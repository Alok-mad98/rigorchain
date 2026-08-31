#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Compact-block concurrency hardening (CVE-2024-52922 class).

Covers a high-bandwidth peer, duplicate reconstruction, slow BLOCKTXN,
and inbound GETBLOCKTXN flooding.
"""

from test_framework.blocktools import (
    DEFAULT_COINBASE_SCRIPT_PUBKEY,
    add_witness_commitment,
    create_block,
    create_coinbase,
)
from test_framework.messages import (
    BlockTransactions,
    BlockTransactionsRequest,
    HeaderAndShortIDs,
    msg_blocktxn,
    msg_cmpctblock,
    msg_getblocktxn,
    msg_headers,
)
from test_framework.p2p import P2PInterface
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


class GetblocktxnCounter(P2PInterface):
    def __init__(self):
        super().__init__()
        self.n_getblocktxn = 0

    def on_getblocktxn(self, message):
        self.n_getblocktxn += 1


class CompactBlockStressTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def next_block(self, node):
        height = node.getblockcount()
        block = create_block(
            tmpl=node.getblocktemplate({"rules": ["segwit"]}),
            coinbase=create_coinbase(height + 1, script_pubkey=DEFAULT_COINBASE_SCRIPT_PUBKEY),
            rpc_for_convergence_root=node,
        )
        add_witness_commitment(block)
        block.solve()
        return block

    def run_test(self):
        node = self.nodes[0]
        self.generate(node, 1)
        tip = int(node.getbestblockhash(), 16)

        self.log.info("High-bandwidth peer: duplicate compact block is ignored")
        peer = node.add_p2p_connection(P2PInterface())
        block = self.next_block(node)
        headers = msg_headers()
        headers.headers = [block]
        peer.send_message(headers)

        hsid = HeaderAndShortIDs()
        hsid.initialize_from_block(block, prefill_list=[0], use_witness=True)
        cmpct = msg_cmpctblock(hsid.to_p2p())
        peer.send_and_ping(cmpct)
        peer.send_and_ping(cmpct)
        assert_equal(int(node.getbestblockhash(), 16), block.sha256)

        self.log.info("Slow BLOCKTXN: node stays connected")
        slow = node.add_p2p_connection(P2PInterface())
        unexpected = msg_blocktxn()
        unexpected.block_transactions = BlockTransactions(block.sha256, [])
        slow.send_and_ping(unexpected)

        self.log.info("Inbound GETBLOCKTXN flood disconnects")
        flood = node.add_p2p_connection(P2PInterface())
        req = msg_getblocktxn()
        req.block_txn_request = BlockTransactionsRequest(tip, [0])
        disconnected = False
        for _ in range(70):
            flood.send_message(req)
            if not flood.is_connected:
                disconnected = True
                break
        if flood.is_connected:
            try:
                flood.wait_for_disconnect(timeout=10)
                disconnected = True
            except Exception:
                disconnected = not flood.is_connected
        assert disconnected, "malicious concurrent getblocktxn should disconnect"

        self.log.info("At most 3 concurrent incomplete compact reconstructions")
        cap = node.add_p2p_connection(GetblocktxnCounter())
        incomplete = []
        for i in range(4):
            blk = self.next_block(node)
            blk.nTime += i + 1
            blk.solve()
            incomplete.append(blk)
            hdr = msg_headers()
            hdr.headers = [blk]
            cap.send_message(hdr)
            hsid = HeaderAndShortIDs()
            hsid.initialize_from_block(blk, prefill_list=[], use_witness=True)
            cap.send_and_ping(msg_cmpctblock(hsid.to_p2p()))
        assert_equal(cap.n_getblocktxn, 3)

        first = incomplete[0]
        resp = msg_blocktxn()
        resp.block_transactions = BlockTransactions(first.sha256, first.vtx)
        cap.send_and_ping(resp)

        fifth = self.next_block(node)
        fifth.nTime += 10
        fifth.solve()
        hdr = msg_headers()
        hdr.headers = [fifth]
        cap.send_message(hdr)
        hsid = HeaderAndShortIDs()
        hsid.initialize_from_block(fifth, prefill_list=[], use_witness=True)
        before = cap.n_getblocktxn
        cap.send_and_ping(msg_cmpctblock(hsid.to_p2p()))
        assert cap.n_getblocktxn == before + 1, "a slot should open after one reconstruction completes"

        self.log.info("Honest compact peer still works after stress")
        honest = node.add_p2p_connection(P2PInterface())
        nxt = self.next_block(node)
        hdr = msg_headers()
        hdr.headers = [nxt]
        honest.send_message(hdr)
        h2 = HeaderAndShortIDs()
        h2.initialize_from_block(nxt, prefill_list=[0], use_witness=True)
        honest.send_and_ping(msg_cmpctblock(h2.to_p2p()))
        assert_equal(int(node.getbestblockhash(), 16), nxt.sha256)


if __name__ == "__main__":
    CompactBlockStressTest().main()

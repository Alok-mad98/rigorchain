#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Witness is mandatory on the RIGOR block relay path.

- A stripped (no-witness) block is disconnected
- A witness block is accepted
- Compact-block reconstruction uses witness serialization
"""

from test_framework.blocktools import (
    DEFAULT_COINBASE_SCRIPT_PUBKEY,
    add_witness_commitment,
    create_block,
    create_coinbase,
)
from test_framework.messages import (
    CInv,
    HeaderAndShortIDs,
    MSG_WITNESS_FLAG,
    msg_block,
    msg_cmpctblock,
    msg_headers,
    msg_inv,
)
from test_framework.p2p import P2PInterface
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


class msg_block_stripped(msg_block):
    def serialize(self):
        return self.block.serialize(with_witness=False)


class WitnessRequiredTest(BitcoinTestFramework):
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

        self.log.info("Witness block is accepted")
        peer = node.add_p2p_connection(P2PInterface())
        good = self.next_block(node)
        peer.send_and_ping(msg_block(good))
        assert_equal(int(node.getbestblockhash(), 16), good.sha256)

        self.log.info("Stripped block is rejected and the peer is disconnected")
        bad_peer = node.add_p2p_connection(P2PInterface())
        stripped = self.next_block(node)
        with node.assert_debug_log(["sent block without required witness; disconnecting"]):
            bad_peer.send_message(msg_block_stripped(stripped))
            bad_peer.wait_for_disconnect()
        assert_equal(int(node.getbestblockhash(), 16), good.sha256)

        self.log.info("Compact block witness reconstruction")
        recon = node.add_p2p_connection(P2PInterface())
        block = self.next_block(node)
        header = msg_headers()
        header.headers = [block]
        recon.send_message(header)
        hsid = HeaderAndShortIDs()
        hsid.initialize_from_block(block, prefill_list=[0], use_witness=True)
        recon.send_and_ping(msg_cmpctblock(hsid.to_p2p()))
        assert_equal(int(node.getbestblockhash(), 16), block.sha256)

        self.log.info("MSG_WITNESS_BLOCK getdata is the fetch type")
        recon.send_message(msg_inv([CInv(2 | MSG_WITNESS_FLAG, block.sha256)]))
        recon.sync_with_ping()


if __name__ == "__main__":
    WitnessRequiredTest().main()

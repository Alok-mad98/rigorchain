#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Compact-block relay during IBD and low-bandwidth announcements (R2A-L01).

Peers on older releases push cmpctblock unsolicited to every compact-block peer.
A node in IBD that receives one whose parent it lacks must request headers through
the normal, rate-limited getheaders path instead of dropping it silently, and the
node itself must not push unsolicited cmpctblock (low-bandwidth BIP152).
"""
import time

from test_framework.address import ADDRESS_BCRT1_UNSPENDABLE
from test_framework.messages import (
    CBlock,
    CBlockHeader,
    CInv,
    HeaderAndShortIDs,
    MSG_BLOCK,
    MSG_CMPCT_BLOCK,
    MSG_TYPE_MASK,
    from_hex,
    msg_block,
    msg_cmpctblock,
    msg_getdata,
    msg_headers,
    msg_sendheaders,
)
from test_framework.p2p import P2PInterface, p2p_lock
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

CHAIN_LEN = 12


class ChainServer(P2PInterface):
    """Peer that holds a chain the node does not know and serves it on request."""

    def __init__(self, blocks):
        super().__init__()
        self.blocks = {b.sha256: b for b in blocks}
        self.chain = blocks
        self.answer_getheaders = False
        self.silent = False
        self.getheaders_count = 0

    def on_getheaders(self, message):
        self.getheaders_count += 1
        if self.silent:
            return
        if not self.answer_getheaders:
            # Same as a peer below its own minimum chain work: an empty reply.
            self.send_message(msg_headers())
            return
        known = set(message.locator.vHave)
        start = 0
        for i, b in enumerate(self.chain):
            if b.sha256 in known:
                start = i + 1
        self.send_message(msg_headers([CBlockHeader(b) for b in self.chain[start:]]))

    def on_getdata(self, message):
        for inv in message.inv:
            if (inv.type & MSG_TYPE_MASK) == MSG_BLOCK and inv.hash in self.blocks:
                self.send_message(msg_block(self.blocks[inv.hash]))

    def push_cmpctblock(self, block):
        cmpct = HeaderAndShortIDs()
        cmpct.initialize_from_block(block, use_witness=True)
        self.send_message(msg_cmpctblock(cmpct.to_p2p()))
        self.sync_with_ping()


class CmpctblockIbdRecoveryTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2

    def setup_network(self):
        self.setup_nodes()

    def getheaders_after(self, peer, before, timeout=10):
        peer.wait_until(lambda: peer.getheaders_count > before, timeout=timeout)

    def assert_no_new_getheaders(self, peer, before, wait=3):
        time.sleep(wait)
        peer.sync_with_ping()
        with p2p_lock:
            assert_equal(peer.getheaders_count, before)

    def run_test(self):
        node, miner = self.nodes
        self.generatetoaddress(miner, CHAIN_LEN, ADDRESS_BCRT1_UNSPENDABLE, sync_fun=self.no_op)
        blocks = []
        for h in range(1, CHAIN_LEN + 1):
            b = from_hex(CBlock(), miner.getblock(miner.getblockhash(h), 0))
            b.rehash()
            blocks.append(b)
        tip = blocks[-1]
        assert node.getblockchaininfo()["initialblockdownload"]

        self.log.info("Sync peer answers the initial getheaders with nothing (peer below its own gate)")
        sync_peer = node.add_p2p_connection(ChainServer(blocks))
        sync_peer.wait_until(lambda: sync_peer.getheaders_count >= 1)
        sync_peer.sync_with_ping()
        assert_equal(node.getblockcount(), 0)

        self.log.info("Unsolicited cmpctblock with unknown parent during IBD triggers getheaders")
        # Leave the request unanswered so the response window stays open (a headers reply resets it).
        sync_peer.silent = True
        before = sync_peer.getheaders_count
        with node.assert_debug_log(expected_msgs=["on unconnecting cmpctblock"]):
            sync_peer.push_cmpctblock(tip)
            self.getheaders_after(sync_peer, before)

        self.log.info("A second cmpctblock inside the response window does not trigger another getheaders")
        before = sync_peer.getheaders_count
        sync_peer.push_cmpctblock(tip)
        self.assert_no_new_getheaders(sync_peer, before)

        self.log.info("Non-sync peers: at most one new headers-sync peer per announced block")
        extra1 = node.add_p2p_connection(ChainServer(blocks))
        extra2 = node.add_p2p_connection(ChainServer(blocks))
        extra1.sync_with_ping()
        extra2.sync_with_ping()
        b1, b2 = extra1.getheaders_count, extra2.getheaders_count
        extra1.push_cmpctblock(tip)
        self.getheaders_after(extra1, b1)
        extra2.push_cmpctblock(tip)
        self.assert_no_new_getheaders(extra2, b2)

        self.log.info("Sync peer disappears; the node still syncs from another peer")
        sync_peer.peer_disconnect()
        sync_peer.wait_for_disconnect()
        extra1.answer_getheaders = True
        extra1.send_message(msg_headers([CBlockHeader(b) for b in blocks]))
        self.wait_until(lambda: node.getblockcount() == CHAIN_LEN, timeout=60)
        assert_equal(node.getbestblockhash(), tip.hash)
        assert not node.getblockchaininfo()["initialblockdownload"]

        self.log.info("A reconnecting peer starts from fresh state")
        again = node.add_p2p_connection(ChainServer(blocks))
        again.sync_with_ping()
        info = [p for p in node.getpeerinfo() if p["inbound"]]
        assert all(not p["compact_block_hb_from"] for p in info)

        self.log.info("Out of IBD the node announces with headers, never an unsolicited cmpctblock")
        for p in (extra1, extra2, again):
            p.send_and_ping(msg_sendheaders())
            with p2p_lock:
                p.last_message.pop("cmpctblock", None)
                p.last_message.pop("headers", None)
        new_hash = int(self.generatetoaddress(node, 1, ADDRESS_BCRT1_UNSPENDABLE, sync_fun=self.no_op)[0], 16)
        for p in (extra1, extra2, again):
            p.wait_until(lambda: "headers" in p.last_message or "inv" in p.last_message, timeout=30)
            p.sync_with_ping()
            with p2p_lock:
                assert "cmpctblock" not in p.last_message
        again.send_message(msg_getdata([CInv(MSG_CMPCT_BLOCK, new_hash)]))
        again.wait_until(lambda: "cmpctblock" in again.last_message, timeout=30)
        with p2p_lock:
            got = again.last_message["cmpctblock"].header_and_shortids.header
            got.calc_sha256()
            assert_equal(got.sha256, new_hash)


if __name__ == '__main__':
    CmpctblockIbdRecoveryTest().main()

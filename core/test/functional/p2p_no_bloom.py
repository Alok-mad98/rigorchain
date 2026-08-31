#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Bloom filter commands are unknown and disconnect.

NODE_BLOOM is not a RIGOR service flag. filterload / filteradd / filterclear /
merkleblock are rejected as unknown commands.
"""

from test_framework.messages import (
    NODE_BLOOM,
    msg_filteradd,
    msg_filterclear,
    msg_filterload,
    msg_merkleblock,
)
from test_framework.p2p import P2PInterface
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


class msg_unknown:
    msgtype = b"xyzzy"

    def serialize(self):
        return b""


class NoBloomTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1

    def test_unknown_disconnect(self, message, name):
        self.log.info(f"{name} is an unknown command and disconnects")
        peer = self.nodes[0].add_p2p_connection(P2PInterface())
        with self.nodes[0].assert_debug_log([f'Unknown command "{name}"']):
            peer.send_message(message)
            peer.wait_for_disconnect()
        assert_equal(self.nodes[0].getconnectioncount(), 0)

    def run_test(self):
        self.log.info("NODE_BLOOM is not advertised")
        peer = self.nodes[0].add_p2p_connection(P2PInterface())
        assert_equal(peer.nServices & NODE_BLOOM, 0)
        info = self.nodes[0].getpeerinfo()
        for p in info:
            assert "BLOOM" not in p.get("servicesnames", [])
        self.nodes[0].disconnect_p2ps()

        self.test_unknown_disconnect(msg_filterload(), "filterload")
        self.test_unknown_disconnect(msg_filteradd(data=b"\xcc"), "filteradd")
        self.test_unknown_disconnect(msg_filterclear(), "filterclear")
        self.test_unknown_disconnect(msg_merkleblock(), "merkleblock")
        self.test_unknown_disconnect(msg_unknown(), "xyzzy")


if __name__ == "__main__":
    NoBloomTest().main()

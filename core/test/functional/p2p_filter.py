#!/usr/bin/env python3
# Copyright (c) 2020-2021 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
BIP37 is not part of the RIGOR public P2P protocol.

- NODE_BLOOM is never advertised
- filterload / filteradd / filterclear / merkleblock disconnect
- MSG_FILTERED_BLOCK getdata disconnects
- -peerbloomfilters is rejected at startup
"""

from test_framework.messages import (
    CInv,
    MSG_FILTERED_BLOCK,
    NODE_BLOOM,
    msg_filteradd,
    msg_filterclear,
    msg_filterload,
    msg_getdata,
    msg_merkleblock,
)
from test_framework.test_node import ErrorMatch
from test_framework.p2p import P2PInterface
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


class FilterTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [[]]

    def test_message_causes_disconnect(self, message):
        peer = self.nodes[0].add_p2p_connection(P2PInterface())
        peer.send_message(message)
        peer.wait_for_disconnect()
        assert_equal(self.nodes[0].getconnectioncount(), 0)

    def run_test(self):
        self.log.info("NODE_BLOOM must not be advertised")
        peer = self.nodes[0].add_p2p_connection(P2PInterface())
        assert_equal(peer.nServices & NODE_BLOOM, 0)
        self.nodes[0].disconnect_p2ps()

        self.log.info("filterload disconnects")
        self.test_message_causes_disconnect(msg_filterload())

        self.log.info("filteradd disconnects")
        self.test_message_causes_disconnect(msg_filteradd(data=b'\xcc'))

        self.log.info("filterclear disconnects")
        self.test_message_causes_disconnect(msg_filterclear())

        self.log.info("merkleblock disconnects")
        self.test_message_causes_disconnect(msg_merkleblock())

        self.log.info("MSG_FILTERED_BLOCK getdata disconnects")
        best = int(self.nodes[0].getbestblockhash(), 16)
        self.test_message_causes_disconnect(msg_getdata(inv=[CInv(MSG_FILTERED_BLOCK, best)]))

        self.log.info("-peerbloomfilters is not a registered argument")
        self.stop_node(0)
        self.nodes[0].assert_start_raises_init_error(
            extra_args=["-peerbloomfilters"],
            expected_msg="Invalid parameter -peerbloomfilters",
            match=ErrorMatch.PARTIAL_REGEX,
        )


if __name__ == '__main__':
    FilterTest().main()

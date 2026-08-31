#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""RIGOR P2P is witness-mandatory.

- Peers without NODE_WITNESS are disconnected at VERSION
- MSG_BLOCK getdata is served as witness-serialized BLOCK
- MSG_TX getdata is served with witness
- Unknown getdata inv types disconnect
"""

import time
from decimal import Decimal

from test_framework.messages import (
    CInv,
    MSG_BLOCK,
    MSG_FILTERED_BLOCK,
    MSG_TX,
    MSG_WITNESS_FLAG,
    NODE_NETWORK,
    msg_getdata,
    msg_version,
)
from test_framework.p2p import (
    P2PInterface,
    P2P_SUBVERSION,
    P2P_VERSION,
    P2P_VERSION_RELAY,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


class WitnessMandatoryTest(BitcoinTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser, legacy=False)

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [["-whitelist=noban@127.0.0.1"]]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        node = self.nodes[0]
        mining_addr = node.getnewaddress(address_type="bech32")
        self.generatetoaddress(node, 101, mining_addr)

        self.log.info("Inbound peer without NODE_WITNESS is disconnected")
        no_witness = node.add_p2p_connection(P2PInterface(), send_version=False, wait_for_verack=False)
        vt = msg_version()
        vt.nVersion = P2P_VERSION
        vt.strSubVer = P2P_SUBVERSION
        vt.relay = P2P_VERSION_RELAY
        vt.nServices = NODE_NETWORK
        with node.assert_debug_log(["missing NODE_WITNESS; disconnecting"]):
            no_witness.send_message(vt)
            no_witness.wait_for_disconnect()

        peer = node.add_p2p_connection(P2PInterface())

        dest = node.getnewaddress(address_type="bech32")
        txid = node.sendtoaddress(dest, Decimal("1"))
        raw_tx = node.getrawtransaction(txid)

        self.log.info("MSG_TX getdata is not stripped")
        # Fresh mempool txs are only served unconditionally after 2 minutes.
        node.setmocktime(int(time.time()) + 150)
        peer.send_message(msg_getdata([CInv(MSG_TX, int(txid, 16))]))
        peer.wait_for_tx(txid)
        served = peer.last_message["tx"].tx
        assert_equal(served.serialize().hex(), raw_tx)

        tip = self.generatetoaddress(node, 1, mining_addr)[0]
        raw_block = node.getblock(tip, False)

        self.log.info("MSG_BLOCK getdata is not stripped")
        peer.send_message(msg_getdata([CInv(MSG_BLOCK, int(tip, 16))]))
        peer.wait_for_block(int(tip, 16))
        msg_block = peer.last_message["block"].block
        peer.send_message(msg_getdata([CInv(MSG_BLOCK | MSG_WITNESS_FLAG, int(tip, 16))]))
        peer.wait_for_block(int(tip, 16))
        wit_block = peer.last_message["block"].block
        assert_equal(msg_block.serialize(), wit_block.serialize())
        assert_equal(wit_block.serialize().hex(), raw_block)

        self.log.info("BIP37 MSG_FILTERED_BLOCK getdata disconnects")
        with node.assert_debug_log(["unsupported getdata type"]):
            peer.send_message(msg_getdata([CInv(MSG_FILTERED_BLOCK, int(tip, 16))]))
            peer.wait_for_disconnect()


if __name__ == "__main__":
    WitnessMandatoryTest().main()

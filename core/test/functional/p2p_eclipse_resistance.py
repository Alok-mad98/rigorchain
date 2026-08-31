#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Eclipse-resistance surfaces: ASMap auto-load, signed seed manifest, Bitcoin v25 reject.
"""

import os
import shutil

from test_framework.messages import NODE_BLOOM, msg_version
from test_framework.p2p import P2PInterface, P2P_SERVICES, P2P_SUBVERSION, P2P_VERSION_RELAY
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

ASMAP = "../../src/test/data/asmap.raw"
DEFAULT_ASMAP_FILENAME = "ip_asn.map"


class EclipseResistanceTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [["-checkaddrman=1"]]

    def run_test(self):
        node = self.nodes[0]
        datadir = node.chain_path

        self.log.info("Bitcoin v25 protocol version is disconnected")
        v25 = node.add_p2p_connection(P2PInterface(), send_version=False, wait_for_verack=False)
        vt = msg_version()
        vt.nVersion = 70016
        vt.strSubVer = P2P_SUBVERSION
        vt.relay = P2P_VERSION_RELAY
        vt.nServices = P2P_SERVICES
        with node.assert_debug_log(["using obsolete version 70016; disconnecting"]):
            v25.send_message(vt)
            v25.wait_for_disconnect()

        self.log.info("NODE_BLOOM is not advertised")
        peer = node.add_p2p_connection(P2PInterface())
        assert_equal(peer.nServices & NODE_BLOOM, 0)
        node.disconnect_p2ps()

        self.log.info("Invalid signed seed manifest is ignored")
        manifest = datadir / "rigor_seed_v1.json"
        manifest.write_text(
            '{"epoch":"2026Q3","expiry":1798761600,"nodes":["203.0.113.9:38732"],'
            '"signature":"' + ("00" * 64) + '"}'
        )
        self.stop_node(0)
        with node.assert_debug_log(["Ignoring signed seed manifest"]):
            self.start_node(0)

        self.log.info("ASMap loads when -asmap is set and ip_asn.map is present")
        asmap_src = os.path.join(os.path.dirname(os.path.realpath(__file__)), ASMAP)
        dest = datadir / DEFAULT_ASMAP_FILENAME
        shutil.copyfile(asmap_src, dest)
        self.stop_node(0)
        with node.assert_debug_log(["Using asmap version"]):
            self.start_node(0, extra_args=["-asmap"])
        os.remove(dest)


if __name__ == "__main__":
    EclipseResistanceTest().main()

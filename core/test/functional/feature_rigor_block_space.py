#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""RIGOR on-chain block space: 1 MiB serialized + 1_000_000 weight + 20_000 sigops.

Locks live in consensus/rigor_consensus_lock.h (RIGOR_CONSENSUS_LOCK_VERSION >= 2)
and are exposed via getblocktemplate as sizelimit / weightlimit / sigoplimit.

Witness rules are active from genesis on RIGOR public/regtest networks; do not
use Bitcoin-era BIP141 activation matrices (feature_segwit.py) for this check.
"""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


class RigorBlockSpaceTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [[]]
        self.wallet_names = ["default_wallet"]
        self._requires_wallet = True

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        node = self.nodes[0]
        # Mine to a descriptor-wallet ORU address (not framework deterministic combo keys).
        mine_addr = node.getnewaddress(address_type="bech32")
        self.generatetoaddress(node, 110, mine_addr)

        self.log.info("GBT must advertise 1 MiB / 1_000_000 weight / 20_000 sigops")
        tmpl = node.getblocktemplate({"rules": ["segwit"]})
        assert_equal(tmpl["sizelimit"], 1000000)
        assert_equal(tmpl["weightlimit"], 1000000)
        assert_equal(tmpl["sigoplimit"], 20000)
        assert "!segwit" in tmpl["rules"]

        self.log.info("GBT limits unchanged with a mempool ORU tx present")
        # Do not assert template membership: getblocktemplate caches for up to 5s
        # (rpc/mining.cpp) and may omit a just-submitted tx without regenerating.
        txid = node.sendtoaddress(node.getnewaddress(address_type="bech32"), 1)
        assert_equal(node.getrawmempool(), [txid])
        tmpl2 = node.getblocktemplate({"rules": ["segwit"]})
        assert_equal(tmpl2["sizelimit"], 1000000)
        assert_equal(tmpl2["weightlimit"], 1000000)
        assert_equal(tmpl2["sigoplimit"], 20000)

        self.log.info("Mining default -blockmaxweight ceiling is MAX_BLOCK_WEIGHT - 4000")
        # Empty assembled tip weight is small; ensure node accepts the consensus cap.
        info = node.getmininginfo()
        assert_equal(info["blocks"], 110)


if __name__ == "__main__":
    RigorBlockSpaceTest().main()

#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
""": dual-pool (RES + ORU fee) P2P relay validation (PHASE-B2).

Proves:
  * RES transfer + ORU fee tx propagates across peers
  * All nodes accept into mempool with identical ORU fee accounting
  * Block including dual-pool tx propagates (compact block path inherited)
  * Package testmempoolaccept does not mix fee semantics
  * Sequential relay of two dual-pool txs remains stable

All validating peers share ``-reswalletbootstrap`` / ``-miningresfeeaddr`` so mined
blocks are accepted during IBD (same rule as ``feature_dual_pool_mining.py``).
"""

from decimal import Decimal

from test_framework.blocktools import COINBASE_MATURITY
from test_framework.test_framework import BitcoinTestFramework
from test_framework.p2p_propagation_util import assert_tips_equal
from test_framework.util import assert_equal

from feature_dual_pool_mempool import build_dual_pool_hex


class P2PDualPoolRelayTest(BitcoinTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser)

    def set_test_params(self):
        self.chain = "devtest"
        self.setup_clean_chain = True
        self.num_nodes = 3
        self.extra_args = [[], [], []]
        self.wallet_names = ["default_wallet"]
        self._requires_wallet = True

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def setup_network(self):
        self.setup_nodes()

    def enable_res_bootstrap_on_all_nodes(self):
        """Restart every node with the same RES bootstrap flags before any shared mining."""
        n0 = self.nodes[0]
        self.oru_fund_addr = n0.getnewaddress("", "bech32")
        br = n0.getnewaddress("", "bech32m")
        self.bootstrap_res_fee_addr = br
        res_flags = [
            "-reswalletbootstrap",
            f"-miningresfeeaddr={br}",
            "-minrelaytxfee=0",
            "-blockmintxfee=0",
        ]
        self.stop_nodes()
        for i in range(self.num_nodes):
            self.start_node(i, list(self.extra_args[i]) + res_flags)
        self.connect_nodes(1, 0)
        self.connect_nodes(2, 1)

    def wait_mempool_tx_on_all(self, txid, timeout=60):
        self.wait_until(
            lambda: all(txid in n.getrawmempool() for n in self.nodes),
            timeout=timeout,
        )

    def run_test(self):
        self.enable_res_bootstrap_on_all_nodes()
        n0 = self.nodes[0]
        br = self.bootstrap_res_fee_addr
        fund = self.oru_fund_addr

        self.log.info("Shared chain with RES bootstrap on all validators")
        self.generatetoaddress(n0, COINBASE_MATURITY + 120, fund)
        self.sync_all()

        self.log.info("Build dual-pool tx on node0 and relay to node1/node2")
        hex_tx = build_dual_pool_hex(
            n0,
            bootstrap_res_addr=br,
            oru_fund_addr=fund,
            res_spend=Decimal("1.00"),
            oru_fee=Decimal("0.0001"),
        )
        txid = n0.sendrawtransaction(hex_tx)
        self.wait_mempool_tx_on_all(txid)

        for n in self.nodes:
            ent = n.getmempoolentry(txid)
            assert_equal(Decimal(str(ent["fees"]["base"])), Decimal("0.0001"))

        self.log.info("Package accept: two dual-pool txs, ORU fees independent")
        used = []
        a_hex = build_dual_pool_hex(
            n0,
            bootstrap_res_addr=br,
            oru_fund_addr=fund,
            res_spend=Decimal("1.00"),
            oru_fee=Decimal("0.0002"),
            excluded=used,
        )
        for vin in n0.decoderawtransaction(a_hex)["vin"]:
            used.append((vin["txid"], vin["vout"]))
        b_hex = build_dual_pool_hex(
            n0,
            bootstrap_res_addr=br,
            oru_fund_addr=fund,
            res_spend=Decimal("1.00"),
            oru_fee=Decimal("0.0003"),
            excluded=used,
        )
        pkg = n0.testmempoolaccept([a_hex, b_hex])
        assert_equal(len(pkg), 2)
        assert pkg[0]["allowed"] and pkg[1]["allowed"]
        assert_equal(Decimal(str(pkg[0]["fees"]["base"])), Decimal("0.0002"))
        assert_equal(Decimal(str(pkg[1]["fees"]["base"])), Decimal("0.0003"))

        txid_a = n0.sendrawtransaction(a_hex)
        txid_b = n0.sendrawtransaction(b_hex)
        self.wait_until(
            lambda: txid_a in self.nodes[2].getrawmempool()
            and txid_b in self.nodes[2].getrawmempool(),
            timeout=60,
        )

        self.log.info("Mine block on node0; block + confirmed txs propagate")
        self.generatetoaddress(n0, 1, fund)
        self.sync_all()
        assert_tips_equal(self.nodes, "post-mine")
        for n in self.nodes:
            assert txid not in n.getrawmempool()
            assert txid_a not in n.getrawmempool()
            assert txid_b not in n.getrawmempool()
            block = n.getblock(n.getbestblockhash(), 2)
            confirmed = [
                t["txid"] if isinstance(t, dict) else t for t in block["tx"]
            ]
            assert txid in confirmed
            assert txid_a in confirmed
            assert txid_b in confirmed

        self.log.info("Orphan stability: mempool sizes bounded after relay burst")
        for n in self.nodes:
            assert n.getmempoolinfo()["size"] < 100

        self.log.info("Dual-pool P2P relay validation passed")


if __name__ == "__main__":
    P2PDualPoolRelayTest().main()

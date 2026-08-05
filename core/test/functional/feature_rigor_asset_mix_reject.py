#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Rigor: spending ORU inputs while creating RES outputs must be rejected (mixed-asset-output).

Consensus: Consensus::CheckTxInputs (see tx_verify.cpp). Unit coverage lives in
transaction_tests.cpp (mixed_asset_outputs_rejected_by_checktxinputs, etc.).

This functional test uses createrawtransaction's {\"amount\", \"asset\":\"RES\"} form (RPC
intent to build the correct witness script; not a serialized CTxOut field) and
testmempoolaccept so the reject-reason matches mempool policy + consensus.

Mixed ORU+RES **inputs** in one tx is covered separately in `src/test/transaction_tests.cpp`
where an on-chain RES prevout is constructed; this test only needs the output-side mismatch.
"""

from decimal import Decimal

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


class RigorAssetMixRejectTest(BitcoinTestFramework):
    def set_test_params(self):
        self.chain = "devtest"
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
        mining_addr = node.get_deterministic_witness_mining_address()
        self.generatetoaddress(node, 120, mining_addr)

        utxos = node.listunspent(minconf=1, query_options={"minimumAmount": Decimal("10.0")})
        assert len(utxos) >= 1
        u = utxos[0]

        # Valid witness v1 RES (32-byte program); derive bech32m via node (checksum depends on HRP).
        res_prog_hex = "ee" * 32
        dec_res = node.decodescript("5120" + res_prog_hex)
        assert_equal(dec_res["type"], "witness_v1_res")
        res_addr = dec_res["address"]

        outputs = {
            res_addr: {"amount": "0.01", "asset": "RES"},
        }
        raw = node.createrawtransaction(
            [{"txid": u["txid"], "vout": u["vout"]}],
            outputs,
        )
        dec = node.decoderawtransaction(raw)
        assert_equal(dec["vout"][0]["asset"], "RES")

        signed = node.signrawtransactionwithwallet(raw)
        assert_equal(signed["complete"], True)

        res = node.testmempoolaccept([signed["hex"]])[0]
        assert_equal(res["allowed"], False)
        assert_equal(res["reject-reason"], "mixed-asset-output")


if __name__ == "__main__":
    RigorAssetMixRejectTest().main()

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
        # Mine to a wallet address so listunspent can sign the mixed-asset spend.
        # The imported deterministic key is not a descriptor-wallet receive script.
        mining_addr = node.getnewaddress()
        self.generatetoaddress(node, 120, mining_addr)

        utxos = node.listunspent(minconf=1, query_options={"minimumAmount": Decimal("10.0")})
        assert len(utxos) >= 1
        u = utxos[0]

 # Scheme A: an off-curve 32-byte v1 program is still classified RES
        # (pure byte predicate), but key_io will not emit a Bech32m address for it.
        off_curve = node.decodescript("5120" + ("ee" * 32))
        assert_equal(off_curve["type"], "witness_v1_res")
        assert not off_curve.get("address")

        res_addr = node.getnewaddress("", "bech32m")
        info = node.getaddressinfo(res_addr)
        assert_equal(info["witness_version"], 1)

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

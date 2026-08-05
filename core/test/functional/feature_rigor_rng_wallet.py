#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""RIGOR devtest: descriptor wallet RES receive (coinbase bootstrap) and send (fee in RES).

Spendable on-chain RES otherwise has a circular dependency (RES bootstrap needs prior RES UTXOs).
For functional coverage, mockable chains support -reswalletbootstrap: each coinbase pays
1.00 RES to -miningresfeeaddr (must be a witness-v1-RES / bech32m wallet address).

This test restarts the node once to apply -miningresfeeaddr and -reswalletbootstrap after
obtaining a bech32m receive address from the wallet.

Fee policy (relay and block assembly) is intentionally relaxed here (-minrelaytxfee=0 and
-blockmintxfee=0) so the script exercises the RES core path (generate, sign, broadcast, mine)
without default feerate floors rejecting or skipping low implicit-RES-fee transactions. Economic
behaviour of implicit RES fees versus miner thresholds belongs in a dedicated test, not in this closure test.
"""

from decimal import Decimal

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than, assert_greater_than_or_equal


class RigorResWalletTest(BitcoinTestFramework):
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
        self.log.info("Obtain bech32m (witness-v1 RES) address, then restart with bootstrap + mining RES fee addr")
        res_fee_addr = node.getnewaddress("", "bech32m")
        info = node.getaddressinfo(res_fee_addr)
        assert info.get("iswitness") and info.get("witness_version") == 1

        self.stop_node(0)
        extra = list(self.extra_args[0]) + [
            "-reswalletbootstrap",
            f"-miningresfeeaddr={res_fee_addr}",
            # Scheme A: do not let default relay / block min feerate interfere with the RES loop test.
            "-minrelaytxfee=0",
            "-blockmintxfee=0",
        ]
        self.start_node(0, extra)

        mining_addr = node.get_deterministic_witness_mining_address()
        self.log.info(
            "Mine past coinbase maturity; each block pays 1.00 RES bootstrap to the fee address "
            "(need height >= 100 + maturity so bootstrap UTXOs are spendable)"
        )
        self.generatetoaddress(node, 220, mining_addr)

        gb0 = node.getbalances()
        assert_equal(gb0["orum"], gb0["mine"])
        assert_greater_than_or_equal(gb0["res"]["trusted"], Decimal("100"))

        res_utxos = [u for u in node.listunspent(minconf=1) if u.get("address") == res_fee_addr]
        total_res = sum(Decimal(str(u["amount"])) for u in res_utxos)
        assert_greater_than_or_equal(total_res, Decimal("100"))
        self.log.info("Wallet holds %s RES across %d UTXOs on %s", total_res, len(res_utxos), res_fee_addr)

        res_recv = node.getnewaddress("", "bech32m")
        res_change = node.getrawchangeaddress("bech32m")
        self.log.info("Build raw RES-domain tx; fee implicit in RES (input - outputs)")
        # Pick a single mature 1.00 RES coinbase output; leave ~0.45 RES as fee (see wallet fee estimate in RES).
        utxo = max(res_utxos, key=lambda u: Decimal(str(u["amount"])))
        send_amt = Decimal("0.55")
        change_amt = Decimal(str(utxo["amount"])) - send_amt - Decimal("0.44")
        assert change_amt > 0
        raw = node.createrawtransaction(
            [{"txid": utxo["txid"], "vout": utxo["vout"]}],
            {
                res_recv: {"amount": format(send_amt, "f"), "asset": "RES"},
                res_change: {"amount": format(change_amt, "f"), "asset": "RES"},
            },
        )
        signed = node.signrawtransactionwithwallet(raw)
        assert signed["complete"]
        txid = node.sendrawtransaction(signed["hex"])
        raw = node.getrawtransaction(txid, True)
        assert any(o.get("asset") == "RES" for o in raw.get("vout", []))

        tip = node.getbestblockhash()
        block = node.getblock(tip, 2)
        assert txid not in [t["txid"] for t in block["tx"]]
        self.generatetoaddress(node, 1, mining_addr)
        tip2 = node.getbestblockhash()
        block2 = node.getblock(tip2, 2)
        assert txid in [t["txid"] for t in block2["tx"]]

        mined = node.getrawtransaction(txid, True, tip2)
        recv_vouts = [o for o in mined["vout"] if o.get("scriptPubKey", {}).get("address") == res_recv]
        assert len(recv_vouts) >= 1
        assert_equal(recv_vouts[0].get("asset"), "RES")

        out = node.listunspent(1, 9999999, [res_recv])
        assert len(out) >= 1
        assert_equal(out[0]["address"], res_recv)
        assert_greater_than_or_equal(Decimal(str(out[0]["amount"])), Decimal("0.54"))

        # sendtoaddress(..., asset=RES) must hit the same wallet IsMine path as sendres:
        # descriptor-expanded witness-v1 RES scripts are in the SPKM cache; no scan.cpp special case.
        res_recv2 = node.getnewaddress("", "bech32m")
        assert_equal(len(node.listunspent(1, 9999999, [res_recv2])), 0)
        gb_pre = node.getbalances()
        assert_equal(gb_pre["orum"], gb_pre["mine"])
        res_trusted_pre = gb_pre["res"]["trusted"]
        txid2 = node.sendtoaddress(
            res_recv2,
            "0.12",
            "",
            "",
            False,
            False,
            None,
            "unset",
            None,
            None,
            False,
            "RES",
        )
        assert len(txid2) == 64
        self.generatetoaddress(node, 1, mining_addr)
        out2 = node.listunspent(1, 9999999, [res_recv2])
        assert len(out2) >= 1
        assert_equal(out2[0]["address"], res_recv2)
        recv2_amt = Decimal(str(out2[0]["amount"]))
        assert_greater_than_or_equal(recv2_amt, Decimal("0.11"))
        gb_post = node.getbalances()
        assert_equal(gb_post["orum"], gb_post["mine"])
        # Wallet-wide res.trusted: confirming this block adds another 1.00 RES coinbase (-reswalletbootstrap)
        # to the fee address while confirming the send (internal move minus fee). Net delta is ~+1 RES minus fee.
        delta_res = gb_post["res"]["trusted"] - res_trusted_pre
        bootstrap_res = Decimal("1.00")
        assert_greater_than(delta_res, bootstrap_res - Decimal("0.4"))
        assert_greater_than(bootstrap_res + Decimal("0.15"), delta_res)
        # The new receive itself (same pattern as "100 + 0.11" for an external credit; here listunspent is authoritative).
        assert_greater_than_or_equal(recv2_amt, Decimal("0.11"))


if __name__ == "__main__":
    RigorResWalletTest().main()

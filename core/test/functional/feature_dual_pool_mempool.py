#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""PHASE-P8: dual-pool (RES spend + ORU fee) mempool admission — ORU-only fee accounting.

Exercises consensus reject `bad-txns-oru-fee-required`, valid dual-pool acceptance,
and package `testmempoolaccept` (fees remain ORU-denominated per vout).

RES bootstrap UTXO selection mirrors ``feature_rigor_rng_wallet``: every devtest coinbase
extra RES pays to the single ``-miningresfeeaddr``. Classify by ``listunspent[].address``,
avoiding ``getrawtransaction`` (needs -txindex for confirmed tx) and ``gettransaction``
(strict RPC result validation can reject extended wallet fields).

ORU funding mirrors wallet descriptor harnesses (e.g. ``feature_wallet_asset_domain`` /
``wallet_descriptor.py``): **mine coinbases to ``getnewaddress(..., \"bech32\")``** so Descriptor
wallet ``listunspent`` rows always carry a stable ``address`` field. Do **not** rely on
``get_deterministic_witness_mining_address()`` for UTXO picking — imported-key coinbases often
won't match that string in ``listunspent``.
"""

from decimal import Decimal

from test_framework.blocktools import COINBASE_MATURITY
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


def find_bootstrap_res_utxo(node, bootstrap_res_addr: str, min_amount: Decimal, excluded=None):
    """Pick smallest mature RES coinbase-to-bootstrap output (same pattern as RigorRngWalletTest)."""
    excluded = excluded or frozenset()
    best = None
    best_amt = None
    for u in node.listunspent(minconf=1):
        pid = (u["txid"], u["vout"])
        if pid in excluded:
            continue
        if u.get("address") != bootstrap_res_addr:
            continue
        amt = Decimal(str(u["amount"]))
        if amt < min_amount:
            continue
        if best is None or amt < best_amt:
            best, best_amt = u, amt
    assert best is not None, f"no RES utxo on bootstrap addr >= {min_amount}"
    return best


def find_wallet_oru_utxo(node, oru_fund_addr: str, min_amount: Decimal, excluded=None):
    """ORU UTXOs on the wallet bech32 address used as generatetoaddress target (descriptor-safe)."""
    excluded = excluded or frozenset()
    best = None
    best_amt = None
    for u in node.listunspent(minconf=1):
        pid = (u["txid"], u["vout"])
        if pid in excluded:
            continue
        if u.get("address") != oru_fund_addr:
            continue
        amt = Decimal(str(u["amount"]))
        if amt < min_amount:
            continue
        if best is None or amt < best_amt:
            best, best_amt = u, amt
    assert best is not None, f"no ORU utxo on wallet fund addr >= {min_amount}"
    return best


def sign_complete(node, raw_hex: str) -> str:
    signed = node.signrawtransactionwithwallet(raw_hex)
    assert_equal(signed["complete"], True)
    return signed["hex"]


def build_dual_pool_hex(
    node,
    *,
    bootstrap_res_addr: str,
    oru_fund_addr: str,
    res_spend: Decimal,
    oru_fee: Decimal,
    excluded=None,
):
    assert res_spend == res_spend.quantize(Decimal("0.01"))
    excluded = set(excluded or [])
    res_u = find_bootstrap_res_utxo(node, bootstrap_res_addr, res_spend, frozenset(excluded))
    oru_u = find_wallet_oru_utxo(
        node, oru_fund_addr, oru_fee + Decimal("0.00000001"), frozenset(excluded)
    )
    res_recv = node.getnewaddress("", "bech32m")
    oru_change_addr = node.getnewaddress("", "bech32")
    oru_in = Decimal(str(oru_u["amount"]))
    oru_change = oru_in - oru_fee
    assert oru_change > 0
    inputs = [
        {"txid": res_u["txid"], "vout": res_u["vout"]},
        {"txid": oru_u["txid"], "vout": oru_u["vout"]},
    ]
    outputs = {
        res_recv: {"amount": format(res_spend, "f"), "asset": "RES"},
        oru_change_addr: format(oru_change, "f"),
    }
    raw = node.createrawtransaction(inputs, outputs)
    return sign_complete(node, raw)


class DualPoolMempoolTest(BitcoinTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser)

    def set_test_params(self):
        self.chain = "devtest"
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [[]]
        self.wallet_names = ["default_wallet"]
        self._requires_wallet = True

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def bootstrap_res_wallet(self):
        node = self.nodes[0]
        # Same pattern as feature_wallet_asset_domain / wallet_descriptor: fund ORU via wallet bech32.
        self.oru_fund_addr = node.getnewaddress("", "bech32")
        self.generatetoaddress(node, COINBASE_MATURITY + 10, self.oru_fund_addr)

        orbis_fee = node.getnewaddress("", "bech32m")
        self.bootstrap_res_fee_addr = orbis_fee
        self.stop_node(0)
        self.start_node(
            0,
            list(self.extra_args[0])
            + [
                "-reswalletbootstrap",
                f"-miningresfeeaddr={orbis_fee}",
                "-minrelaytxfee=0",
                "-blockmintxfee=0",
                "-reindex-chainstate",
            ],
        )
        self.generatetoaddress(node, 120, self.oru_fund_addr)

    def run_test(self):
        self.bootstrap_res_wallet()
        node = self.nodes[0]
        br = self.bootstrap_res_fee_addr
        of = self.oru_fund_addr

        self.log.info("dual-pool: RES spend without ORU input is rejected")
        res_u = find_bootstrap_res_utxo(node, br, Decimal("1.00"))
        res_recv = node.getnewaddress("", "bech32m")
        raw_res_only = node.createrawtransaction(
            [{"txid": res_u["txid"], "vout": res_u["vout"]}],
            {res_recv: {"amount": "1.00", "asset": "RES"}},
        )
        hex_res_only = sign_complete(node, raw_res_only)
        r0 = node.testmempoolaccept([hex_res_only])[0]
        assert_equal(r0["allowed"], False)
        assert_equal(r0["reject-reason"], "bad-txns-oru-fee-required")

        self.log.info("dual-pool: RES + ORU fee path accepted (ORU fees only)")
        hex_ok = build_dual_pool_hex(
            node,
            bootstrap_res_addr=br,
            oru_fund_addr=of,
            res_spend=Decimal("1.00"),
            oru_fee=Decimal("0.0001"),
        )
        r_ok = node.testmempoolaccept([hex_ok])[0]
        assert_equal(r_ok["allowed"], True)
        assert_equal(Decimal(str(r_ok["fees"]["base"])), Decimal("0.0001"))

        self.log.info("dual-pool: two independent dual txs as one package — both accepted")
        used = []
        a_hex = build_dual_pool_hex(
            node,
            bootstrap_res_addr=br,
            oru_fund_addr=of,
            res_spend=Decimal("1.00"),
            oru_fee=Decimal("0.0002"),
            excluded=used,
        )
        for vin in node.decoderawtransaction(a_hex)["vin"]:
            used.append((vin["txid"], vin["vout"]))
        b_hex = build_dual_pool_hex(
            node,
            bootstrap_res_addr=br,
            oru_fund_addr=of,
            res_spend=Decimal("1.00"),
            oru_fee=Decimal("0.0003"),
            excluded=used,
        )
        pkg = node.testmempoolaccept([a_hex, b_hex])
        assert_equal(len(pkg), 2)
        assert pkg[0]["allowed"] and pkg[1]["allowed"]


if __name__ == "__main__":
    DualPoolMempoolTest().main()

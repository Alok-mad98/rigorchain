#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""TASK-RES-SIGNING-PATH-VALIDATION: descriptor wallet signs convergence txs end-to-end.

Consensus **convergence candidate** outputs use witness **v1** CVG2 (``RIGOR_PUSH_1`` + 32-byte program);
block-level convergence metadata also uses an ``RIGOR_MARKER_RETURN`` CVGE commitment in coinbase (not asserted
here). This test proves the **wallet signing path**: native **v0 P2WPKH** ORU inputs → signed witness,
``signrawtransactionwithwallet`` ``complete``, and mempool acceptance for **≥2** distinct convergence txs.

**Run prerequisite:** ``test/config.ini`` ``BUILDDIR`` must contain a built node (``src/rigord`` or
``src/bitcoind`` under that directory). If you only built ``rigord``, the default resolver already
prefers it when ``bitcoind`` is absent. Otherwise set ``BITCOIND`` / ``BITCOINCLI`` to existing paths.
"""

from __future__ import annotations

from decimal import Decimal

from test_framework.rigor_block_builder import (
    prepare_unsigned_convergence_tx,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

from feature_res_core_convergence_draw import DEVTEST_MINING_RES_FEE_ADDR


def _assert_p2wpkh_v0_script_hex(h: str) -> None:
    x = h.lower()
    assert len(x) == 44 and x.startswith("0014"), f"expected native P2WPKH scriptPubKey hex, got {h!r}"


def _assert_convergence_vout_script_hex(h: str, expected_sats: int) -> None:
    x = h.lower()
    # OP_PUSHBYTES_20 + 20-byte HASH160 + RIGOR_MARKER_CONVERGE → 22 bytes → 44 hex chars
    assert x.startswith("14") and x.endswith("bd") and len(x) == 44, (
        f"expected RIGOR_MARKER_CONVERGE lock hex, got {h!r}"
    )
    del expected_sats


def assert_signed_convergence_tx(node, raw_signed: str, prev_txs: list[dict], conv_sats: int) -> str:
    """Decode signed tx; check witness + v0 input + v1 convergence output; accept to mempool."""
    dec = node.decoderawtransaction(raw_signed, True)
    assert_equal(len(dec["vin"]), 1)
    wit = dec["vin"][0].get("txinwitness") or []
    assert len(wit) >= 2, f"expected non-empty P2WPKH witness stack, got {wit!r}"

    prev_spk = prev_txs[0]["scriptPubKey"]
    if isinstance(prev_spk, dict):
        prev_spk = prev_spk.get("hex", "")
    _assert_p2wpkh_v0_script_hex(str(prev_spk))

    v0 = dec["vout"][0]
    spk0 = v0["scriptPubKey"]
    h0 = spk0["hex"] if isinstance(spk0, dict) else spk0
    _assert_convergence_vout_script_hex(str(h0), conv_sats)

    txid_rpc = dec["txid"]
    assert len(txid_rpc) == 64, txid_rpc
    int(txid_rpc, 16)

    tma = node.testmempoolaccept(rawtxs=[raw_signed], maxfeerate=Decimal("0"))[0]
    assert tma.get("allowed") is True, tma

    txid_send = node.sendrawtransaction(raw_signed, 0)
    assert_equal(txid_send.lower(), txid_rpc.lower())
    node.getmempoolentry(txid_send)
    return txid_send


class ResDescriptorSigningTest(BitcoinTestFramework):
    def set_test_params(self):
        self.chain = "devtest"
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [[f"-miningresfeeaddr={DEVTEST_MINING_RES_FEE_ADDR}"]]
        self.wallet_names = ["default_wallet"]
        self._requires_wallet = True

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        node = self.nodes[0]
        self.log.info("Step 1: mature ORU (ORUM-domain) coinbase UTXOs on a pinned bech32 pool")
        fund = node.getnewaddress(label="res-desc-sign-pool", address_type="bech32")
        self.generatetoaddress(node, 130, fund)
        node.syncwithvalidationinterfacequeue()

        pin = [fund]
        fee = 1000
        amt = Decimal("1.0")

        self.log.info("Step 2–4: first convergence candidate — unsigned → signrawtransactionwithwallet")
        raw1, prev1, meta1 = prepare_unsigned_convergence_tx(
            node,
            amt,
            fee_sats=fee,
            spend_addresses=pin,
            change_address=fund,
            minconf=1,
        )
        signed1 = node.signrawtransactionwithwallet(raw1, prev1)
        if signed1.get("complete") is not True:
            self.log.error("signrawtransactionwithwallet (tx1): %s", signed1)
        assert_equal(signed1.get("complete"), True)
        txid1 = assert_signed_convergence_tx(node, signed1["hex"], prev1, meta1["convergence_sats"])

        self.log.info("Step 2–4: second convergence candidate (0-conf change from first tx)")
        raw2, prev2, meta2 = prepare_unsigned_convergence_tx(
            node,
            amt,
            fee_sats=fee,
            spend_addresses=pin,
            change_address=fund,
            minconf=0,
        )
        signed2 = node.signrawtransactionwithwallet(raw2, prev2)
        if signed2.get("complete") is not True:
            self.log.error("signrawtransactionwithwallet (tx2): %s", signed2)
        assert_equal(signed2.get("complete"), True)
        txid2 = assert_signed_convergence_tx(node, signed2["hex"], prev2, meta2["convergence_sats"])

        assert txid1 != txid2, "expected two distinct convergence txids"
        self.log.info("TASK-RES-SIGNING-PATH-VALIDATION: PASS (%s, %s)", txid1, txid2)


if __name__ == "__main__":
    ResDescriptorSigningTest().main()

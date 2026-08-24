#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Consensus maturity: immature ORU coinbase and coinbase OP_RES_MINT UTXOs cannot be spent.

RES origin is the coinbase ``OP_RES_MINT`` output (40.000000 RES at epoch 0). Wallet
``res.trusted`` / immature UI is not the enforcement path — this suite uses raw
transactions and block connection.

See ``docs/security/RES-ORU-MATURITY.md``.
"""

from __future__ import annotations

from decimal import Decimal
from typing import Optional

from test_framework.authproxy import JSONRPCException
from test_framework.rigor_block_builder import create_convergence_tx
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than, assert_raises_rpc_error

from res_security.harness import (
    CENSOR_FEE_DELTA,
    COINBASE_MATURITY,
    DEVTEST_MINING_RES_FEE_ADDR,
    RES_REWARD_ATOMS,
    SATURATING_CONVERGENCE_ORU,
    body_res_mint_rows,
    draw_of,
    fail_context,
    is_res_mint_vout,
    supply_snapshot,
)


RES_REWARD = Decimal("40.000000")
ORU_FEE = Decimal("0.0001")


def wallet_res_program32(node) -> bytes:
    addr = node.getnewaddress("", "bech32m")
    spk = node.getaddressinfo(addr)["scriptPubKey"]
    if isinstance(spk, dict):
        spk = spk["hex"]
    h = str(spk).lower()
    assert h.startswith("5120") and len(h) == 68, h
    return bytes.fromhex(h[4:])


def mint_res_outpoint(node, payout_hash: str) -> tuple[str, int]:
    rows = body_res_mint_rows(node, payout_hash)
    assert_equal(len(rows), 1)
    assert_equal(rows[0][1], RES_REWARD_ATOMS)
    mint_txid = rows[0][0]
    tx = node.getrawtransaction(mint_txid, True, payout_hash)
    for o in tx["vout"]:
        if is_res_mint_vout(o):
            return mint_txid, int(o["n"])
    raise AssertionError("coinbase has no OP_RES_MINT vout")


class ResOruMaturityTest(BitcoinTestFramework):
    def set_test_params(self):
        self.chain = "devtest"
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.rpc_timeout = 240
        self.extra_args = [[
            f"-miningresfeeaddr={DEVTEST_MINING_RES_FEE_ADDR}",
            "-solomining=1",
        ]]
        self.wallet_names = ["default_wallet"]
        self._requires_wallet = True

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def _n(self):
        return self.nodes[0]

    def _mine(self, n: int = 1) -> list[str]:
        return self.generatetoaddress(self._n(), n, self.mining_addr)

    def _dump(self, case_id: str, extra=None) -> str:
        return fail_context(self._n(), case_id, extra)

    def _fund_saturating_utxo(self) -> None:
        node = self._n()
        target = SATURATING_CONVERGENCE_ORU + Decimal("20")
        chunk = Decimal("300000")
        locked = []
        remaining = target
        while remaining > 0:
            amt = min(chunk, remaining)
            node.sendtoaddress(self.large_addr, amt)
            self._mine(1)
            for u in node.listunspent(1, 9999999, [self.large_addr]):
                op = {"txid": u["txid"], "vout": u["vout"]}
                if op not in locked:
                    node.lockunspent(False, [op])
                    locked.append(op)
            remaining -= amt
        node.lockunspent(True)
        node.sendtoaddress(self.large_addr, target)
        self._mine(1)
        fat = [u for u in node.listunspent(1, 9999999, [self.large_addr])
               if Decimal(str(u["amount"])) >= SATURATING_CONVERGENCE_ORU]
        assert fat, self._dump("fund-saturating")
        # Leave unlocked so create_convergence_tx / listunspent can select the fat UTXO.

    def _broadcast_saturating(self) -> str:
        node = self._n()
        tx = create_convergence_tx(
            node, SATURATING_CONVERGENCE_ORU,
            spend_addresses=[self.large_addr], change_address=self.large_addr,
            res_owner_program=self.res_program32,
        )
        txid = node.sendrawtransaction(tx.serialize_with_witness().hex(), 0, format(SATURATING_CONVERGENCE_ORU, "f"))
        assert node.getmempoolentry(txid).get("convergence", {}).get("is_convergence")
        return txid

    def _mine_only(self, include_txids: list[str]) -> str:
        node = self._n()
        mem = set(node.getrawmempool())
        missing = [t for t in include_txids if t not in mem]
        assert not missing, self._dump("mine_only", {"missing": missing})
        for txid in mem - set(include_txids):
            node.prioritisetransaction(txid=txid, fee_delta=CENSOR_FEE_DELTA)
        for txid in include_txids:
            node.prioritisetransaction(txid=txid, fee_delta=-CENSOR_FEE_DELTA * 8)
        bh = self._mine(1)[0]
        in_block = node.getblock(bh, 1)["tx"]
        for txid in include_txids:
            assert txid in in_block, self._dump("mine_only", {"wanted": include_txids, "got": in_block})
        return bh

    def _pick_oru_fee_utxo(self, excluded: Optional[set] = None) -> dict:
        excluded = excluded or set()
        for u in self._n().listunspent(1, 9999999):
            pid = (u["txid"], u["vout"])
            if pid in excluded:
                continue
            if str(u.get("asset", "ORU")).upper() == "RES":
                continue
            if not u.get("spendable", True):
                continue
            if Decimal(str(u["amount"])) >= ORU_FEE + Decimal("0.01"):
                return u
        raise AssertionError(self._dump("no-oru-fee-utxo"))

    def _build_res_spend_hex(self, mint_txid: str, mint_vout: int) -> str:
        node = self._n()
        oru_u = self._pick_oru_fee_utxo({(mint_txid, mint_vout)})
        oru_in = Decimal(str(oru_u["amount"]))
        oru_change = oru_in - ORU_FEE
        res_recv = node.getnewaddress("", "bech32m")
        oru_change_addr = node.getnewaddress("", "bech32")
        raw = node.createrawtransaction(
            [
                {"txid": mint_txid, "vout": mint_vout},
                {"txid": oru_u["txid"], "vout": oru_u["vout"]},
            ],
            {
                res_recv: {"amount": format(RES_REWARD, "f"), "asset": "RES"},
                oru_change_addr: format(oru_change, "f"),
            },
        )
        signed = node.signrawtransactionwithwallet(raw)
        assert signed["complete"], signed
        return signed["hex"]

    def _assert_premature_res(self, hex_tx: str, case_id: str) -> None:
        node = self._n()
        acc = node.testmempoolaccept([hex_tx])[0]
        assert not acc.get("allowed"), self._dump(case_id, acc)
        reason = str(acc.get("reject-reason", ""))
        assert "premature-spend-of-coinbase" in reason or "premature" in reason.lower(), self._dump(case_id, acc)
        assert_raises_rpc_error(-26, "premature-spend-of-coinbase",
                                node.sendrawtransaction, hex_tx)

    def run_test(self):
        node = self._n()
        self.mining_addr = node.getnewaddress(label="mat-miner", address_type="bech32")
        self.large_addr = node.getnewaddress(label="mat-large", address_type="bech32")
        self.small_addr = node.getnewaddress(label="mat-small", address_type="bech32")
        self.res_program32 = wallet_res_program32(node)

        n_fund = COINBASE_MATURITY + 2250
        self.log.info("Mining %d blocks (maturity=%d)", n_fund, COINBASE_MATURITY)
        self._mine(n_fund)
        bals = node.getbalances()
        assert_greater_than(Decimal(str(bals["oru"]["trusted"])), Decimal("2000000"))

        # T-ORU-1: latest coinbase is still immature (raw path).
        self._case_t_oru_1()

        self._fund_saturating_utxo()
        # Fee coin after consolidation: sendtoaddress during fat-funding can consume
        # an earlier small_addr UTXO. create_convergence_tx spends only large_addr.
        node.sendtoaddress(self.small_addr, Decimal("50"))
        self._mine(1)

        ticket = self._broadcast_saturating()
        award = self._mine_only([ticket])
        fr = draw_of(node, award)
        assert fr["draw_winner_txid"] == ticket, self._dump("T-RES-setup", fr)
        payout = self._mine(1)[0]
        mint_txid, mint_vout = mint_res_outpoint(node, payout)
        mint_height = node.getblock(payout)["height"]
        utxo = node.gettxout(mint_txid, mint_vout)
        assert utxo is not None, self._dump("T-RES-setup", {"mint": mint_txid})
        assert utxo.get("res_mint") is True, self._dump("T-RES-setup", utxo)
        assert utxo.get("coinbase") is True, self._dump("T-RES-setup", utxo)
        self.log.info("Mint %s:%d at height %d res_mint=%s", mint_txid, mint_vout, mint_height, utxo.get("res_mint"))

        self._case_t_issue_1(payout, mint_txid)
        spend_hex = self._build_res_spend_hex(mint_txid, mint_vout)

        self.log.info("T-RES-1 / T-RES-6: spend RES mint at depth 1 (raw)")
        self._assert_premature_res(spend_hex, "T-RES-1")

        self._case_t_res_4(spend_hex)
        self._case_t_res_5(payout, spend_hex)

        # T-RES-2: nSpendHeight - H = MATURITY - 1 when tip = H + MATURITY - 2
        need = mint_height + COINBASE_MATURITY - 2 - node.getblockcount()
        if need > 0:
            self._mine(need)
        assert_equal(node.getblockcount(), mint_height + COINBASE_MATURITY - 2)
        self.log.info("T-RES-2: spend at depth %d", COINBASE_MATURITY - 1)
        self._assert_premature_res(spend_hex, "T-RES-2")

        self._mine(1)
        assert_equal(node.getblockcount(), mint_height + COINBASE_MATURITY - 1)
        self.log.info("T-RES-3: spend at depth %d (mempool nSpendHeight = mint+MATURITY)", COINBASE_MATURITY)
        acc = node.testmempoolaccept([spend_hex])[0]
        assert acc.get("allowed"), self._dump("T-RES-3", acc)
        txid = node.sendrawtransaction(spend_hex)
        self._mine(1)
        assert txid in node.getblock(node.getbestblockhash())["tx"], self._dump("T-RES-3-mine")

        # MAT-RES-4: the confirmed spend is a transfer (res_in >= res_out), not a second mint.
        snap = supply_snapshot(node)
        assert_equal(int(snap["mint_count"]), 1)
        self.log.info("All maturity cases PASS; mint_count=%s", snap["mint_count"])

    def _case_t_oru_1(self) -> None:
        node = self._n()
        tip_h = node.getblockcount()
        cb = node.getblock(node.getblockhash(tip_h), 2)["tx"][0]
        oru_vout = None
        oru_amt = None
        mining_spk = node.getaddressinfo(self.mining_addr)["scriptPubKey"]
        if isinstance(mining_spk, dict):
            mining_spk = mining_spk["hex"]
        mining_spk = str(mining_spk).lower()
        for o in cb["vout"]:
            spk = o.get("scriptPubKey", {})
            hx = spk.get("hex", "") if isinstance(spk, dict) else str(spk)
            if str(hx).lower() == mining_spk:
                oru_vout = int(o["n"])
                oru_amt = Decimal(str(o["value"]))
                break
        assert oru_vout is not None, self._dump("T-ORU-1", {"cb": cb["txid"]})
        dest = node.getnewaddress("", "bech32")
        raw = node.createrawtransaction(
            [{"txid": cb["txid"], "vout": oru_vout}],
            {dest: format(oru_amt - Decimal("0.0001"), "f")},
        )
        signed = node.signrawtransactionwithwallet(raw)
        assert signed["complete"], signed
        acc = node.testmempoolaccept([signed["hex"]])[0]
        assert not acc.get("allowed"), self._dump("T-ORU-1", acc)
        assert "premature-spend-of-coinbase" in str(acc.get("reject-reason", "")), self._dump("T-ORU-1", acc)
        self.log.info("T-ORU-1 PASS reject=%s", acc.get("reject-reason"))

    def _case_t_issue_1(self, payout_hash: str, mint_txid: str) -> None:
        node = self._n()
        blk = node.getblock(payout_hash, 2)
        cb = blk["tx"][0]
        assert_equal(cb["txid"], mint_txid)
        rows = body_res_mint_rows(node, payout_hash)
        assert_equal(len(rows), 1)
        assert_equal(rows[0][0], mint_txid)
        assert_equal(rows[0][1], RES_REWARD_ATOMS)
        assert_equal(int(node.getblockheader(payout_hash).get("rigor_res_minted", 0)), 1)
        self.log.info("T-ISSUE-1 PASS coinbase OP_RES_MINT=40.000000")

    def _case_t_res_4(self, spend_hex: str) -> None:
        node = self._n()
        # Drain mempool so generateblock is not fighting automint from extra tickets.
        for txid in list(node.getrawmempool()):
            node.prioritisetransaction(txid=txid, fee_delta=CENSOR_FEE_DELTA)
        try:
            self.generateblock(node, self.mining_addr, [spend_hex])
            raise AssertionError(self._dump("T-RES-4", {"generateblock": "accepted immature RES spend"}))
        except JSONRPCException as exc:
            msg = str(exc.error.get("message", exc))
            if "automint" in msg.lower() or "res mint" in msg.lower():
                self._mine(1)
                try:
                    self.generateblock(node, self.mining_addr, [spend_hex])
                    raise AssertionError(self._dump("T-RES-4-retry", {"generateblock": "accepted"}))
                except JSONRPCException as exc2:
                    msg = str(exc2.error.get("message", exc2))
            assert "premature" in msg.lower() or "res-mint" in msg.lower() or "bad-txns" in msg.lower(), (
                self._dump("T-RES-4", {"msg": msg})
            )
        self.log.info("T-RES-4 PASS block path rejected immature RES spend")

    def _case_t_res_5(self, payout_hash: str, spend_hex: str) -> None:
        node = self._n()
        self._mine(5)
        self._assert_premature_res(spend_hex, "T-RES-5-before-invalidate")
        node.invalidateblock(payout_hash)
        acc = node.testmempoolaccept([spend_hex])[0]
        assert not acc.get("allowed"), self._dump("T-RES-5", acc)
        reason = str(acc.get("reject-reason", ""))
        assert "missing" in reason or "spent" in reason or "premature" in reason or "bad-txns" in reason, (
            self._dump("T-RES-5", acc)
        )
        node.reconsiderblock(payout_hash)
        mint_txid, mint_vout = mint_res_outpoint(node, payout_hash)
        utxo = node.gettxout(mint_txid, mint_vout)
        assert utxo is not None and utxo.get("res_mint") is True, self._dump("T-RES-5-restore", utxo)
        self._assert_premature_res(spend_hex, "T-RES-5-after-reconsider")
        self.log.info("T-RES-5 PASS disconnected mint is not spendable; restore keeps mint immature")


if __name__ == "__main__":
    ResOruMaturityTest().main()

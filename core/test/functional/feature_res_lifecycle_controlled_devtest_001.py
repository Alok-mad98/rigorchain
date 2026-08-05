#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# TASK-RES-LIFECYCLE-CONTROLLED-DEVTEST-001 — controlled devtest RES lifecycle (runtime only).
"""Validate RES draw + mint on mockable devtest: >=6 mints, tri-node sync, reorg drill, artifacts.

Does not modify consensus. Uses the same helpers as feature_res_core_convergence_draw.py.
Set RES_DEVTEST_CAPTURE_DIR to persist JSON artifacts for report generation.
"""
from __future__ import annotations

import json
import os
from decimal import Decimal

from test_framework.rigor_block_builder import create_convergence_tx
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than_or_equal

from feature_res_core_convergence_draw import (
    DEVTEST_MINING_RES_FEE_ADDR,
    RES_REWARD_ATOMS,
    _body_res_mint_rows,
    _pcorb_convergence_txid_hex_from_mint_tx,
    expected_draw_candidate_winner,
)

_CAPTURE_ENV = "RES_DEVTEST_CAPTURE_DIR"
_MIN_MINTS = 6
_REORG_EXTRA_DEPTH = 3


def _canonical(obj):
    if isinstance(obj, dict):
        return {k: _canonical(v) for k, v in obj.items()}
    if isinstance(obj, list):
        return [_canonical(v) for v in obj]
    if isinstance(obj, Decimal):
        return format(obj, "f")
    return obj


class ResLifecycleControlledDevtest001(BitcoinTestFramework):
    def set_test_params(self):
        self.chain = "devtest"
        self.setup_clean_chain = True
        self.num_nodes = 3
        fee = f"-miningresfeeaddr={DEVTEST_MINING_RES_FEE_ADDR}"
        self.extra_args = [[fee], [fee], [fee]]
        self.wallet_names = ["default_wallet", False, False]
        self._requires_wallet = True

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def setup_network(self):
        self.setup_nodes()
        self.connect_nodes(1, 0)
        self.connect_nodes(2, 0)
        self.sync_all()

    def _capture_dir(self) -> str:
        d = os.environ.get(_CAPTURE_ENV)
        if d:
            os.makedirs(d, exist_ok=True)
            return os.path.abspath(d)
        return os.path.abspath(os.path.join(self.options.tmpdir, "RES_devtest_001"))

    def _write_json(self, path: str, obj) -> None:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="ascii") as f:
            json.dump(_canonical(obj), f, indent=2, sort_keys=True)

    def _broadcast_convergence(self, node, pool_addr: str) -> str:
        tx = create_convergence_tx(
            node,
            Decimal("1.0"),
            spend_addresses=[pool_addr],
            change_address=pool_addr,
        )
        txid = node.sendrawtransaction(tx.serialize_with_witness().hex(), 0, "1.0")
        meta = node.getmempoolentry(txid).get("convergence", {})
        assert meta.get("is_convergence") and meta.get("meets_min_convergence")
        return txid

    def _mine_mint_block(self, node, pool_addr: str, n_conv: int) -> dict:
        txids = []
        for _ in range(n_conv):
            txids.append(self._broadcast_convergence(node, pool_addr))
        self.sync_mempools(self.nodes)
        out = self.generatetoaddress(node, 1, pool_addr)
        assert_equal(len(out), 1)
        bh = out[0]
        self.sync_blocks(self.nodes)
        fr = node.getconvergenceresult(bh)

        # SECURITY-001-A8: `bh` awards the draw; the block after it pays the winner. Mine that
        # payout block on an empty mempool so its only body tx is the mint under test.
        payout_hash = self.generatetoaddress(node, 1, pool_addr)[0]
        self.sync_blocks(self.nodes)
        hdr = node.getblockheader(payout_hash)
        rows = _body_res_mint_rows(node, payout_hash)
        assert_equal(_body_res_mint_rows(node, bh), [])

        exp_w = expected_draw_candidate_winner(fr)
        rpc_w = fr.get("draw_winner_txid", "0" * 64).lower()
        mint_row = None
        if exp_w is not None:
            assert_equal(len(rows), 1)
            assert_equal(rows[0][1], RES_REWARD_ATOMS)
            assert_equal(rpc_w, exp_w.lower())
            mint_row = {"mint_txid": rows[0][0], "res_atoms": rows[0][1]}
        return {
            "block_hash": bh,
            "height": fr.get("height"),
            "convergence_txids": txids,
            "convergence_root_set_size": int(fr.get("convergence_root_set_size", 0)),
            "draw_winner_txid": rpc_w,
            "sigma_seed": fr.get("sigma_seed"),
            "payout_block_hash": payout_hash,
            "payout_height": fr.get("payout_height"),
            "draw_candidate_root_committed": fr.get("draw_candidate_root_committed"),
            "draw_candidate_root_computed": fr.get("draw_candidate_root_computed"),
            "rigor_res_minted": hdr.get("rigor_res_minted"),
            "mint_body": mint_row,
            "candidates": fr.get("candidates", []),
        }

    def _explorer_equivalent_audit(self, rec: dict) -> dict:
        """Fields an explorer indexer must surface (RPC contract check)."""
        missing = []
        for k in (
            "block_hash",
            "draw_winner_txid",
            "sigma_seed",
            "payout_height",
            "draw_candidate_root_committed",
        ):
            if rec.get(k) is None:
                missing.append(k)
        ok = rec.get("rigor_res_minted") == 1 and rec.get("mint_body") is not None and not missing
        return {"ok": ok, "missing_fields": missing}

    def _wallet_equivalent_audit(self, node) -> dict:
        snap = node.getreplaysnapshot()
        return {
            "ok": int(snap["mint_count"]) >= _MIN_MINTS and Decimal(str(snap["res_balance"])) > 0,
            "mint_count": snap["mint_count"],
            "res_balance": str(snap["res_balance"]),
            "state_hash": snap["state_hash"],
        }

    def run_test(self):
        cap = self._capture_dir()
        os.makedirs(cap, exist_ok=True)
        node_a = self.nodes[0]
        pool = node_a.getnewaddress(label="devtest001-pool", address_type="bech32")
        self.generatetoaddress(node_a, 130, pool)
        self.sync_blocks(self.nodes)

        mint_ledger: list[dict] = []
        conv_counts = [1, 2, 5, 3, 7, 10, 2, 3]
        for n_conv in conv_counts:
            if len(mint_ledger) >= _MIN_MINTS:
                break
            rec = self._mine_mint_block(node_a, pool, n_conv)
            if rec["rigor_res_minted"] != 1:
                continue
            rec["explorer_audit"] = self._explorer_equivalent_audit(rec)
            assert rec["explorer_audit"]["ok"]
            mint_ledger.append(rec)
            self.log.info(
                "mint %d block=%s |F|=%s winner=%s",
                len(mint_ledger),
                rec["block_hash"][:16],
                rec["convergence_root_set_size"],
                rec["draw_winner_txid"][:16],
            )

        assert_greater_than_or_equal(len(mint_ledger), _MIN_MINTS)

        winners = [m["draw_winner_txid"] for m in mint_ledger]
        assert len(winners) == len(set(winners)), "duplicate draw_winner_txid observed"

        wallet_audit = self._wallet_equivalent_audit(node_a)
        assert wallet_audit["ok"]

        self._write_json(os.path.join(cap, "mint_ledger.json"), mint_ledger)
        self._write_json(os.path.join(cap, "wallet_audit.json"), wallet_audit)

        self.log.info("Reorg drill: 3-block invalidate + reconsider (mint chain preserved)")
        for _ in range(3):
            self.generatetoaddress(node_a, 1, pool)
        self.sync_blocks(self.nodes)
        tip_before = node_a.getbestblockhash()
        height_before = node_a.getblockcount()
        inv_hash = node_a.getblockhash(height_before - 2)
        node_a.invalidateblock(inv_hash)
        height_inv = node_a.getblockcount()
        node_a.reconsiderblock(inv_hash)
        tip_after = node_a.getbestblockhash()
        self.sync_blocks(self.nodes)
        reorg_ok = height_inv < height_before and tip_after == tip_before
        for n in self.nodes:
            assert_equal(n.getbestblockhash(), tip_after)

        common_mints = []
        for rec in mint_ledger:
            bh = rec["block_hash"]
            try:
                h = node_a.getblockheader(bh)["height"]
                if h <= node_a.getblockcount():
                    on_chain = node_a.getblockhash(h) == bh
                else:
                    on_chain = False
            except Exception:
                on_chain = False
            if not on_chain:
                continue
            fr_a = node_a.getconvergenceresult(bh)
            fr_b = self.nodes[1].getconvergenceresult(bh)
            fr_c = self.nodes[2].getconvergenceresult(bh)
            assert_equal(fr_a["draw_winner_txid"], fr_b["draw_winner_txid"])
            assert_equal(fr_a["draw_winner_txid"], fr_c["draw_winner_txid"])
            common_mints.append(bh)

        reorg_report = {
            "tip_before": tip_before,
            "tip_after": tip_after,
            "height_before": height_before,
            "height_after_invalidate": height_inv,
            "height_after_reconsider": node_a.getblockcount(),
            "invalidate_hash": inv_hash,
            "reorg_ok": reorg_ok,
            "mint_blocks_still_canonical": len(common_mints),
            "mint_blocks_total": len(mint_ledger),
        }
        self._write_json(os.path.join(cap, "reorg_report.json"), reorg_report)
        assert reorg_ok
        assert_equal(len(common_mints), len(mint_ledger))

        meta = {
            "task": "TASK-RES-LIFECYCLE-CONTROLLED-DEVTEST-001",
            "mint_count": len(mint_ledger),
            "convergence_tx_total": sum(len(m["convergence_txids"]) for m in mint_ledger),
            "avg_f_size": sum(m["convergence_root_set_size"] for m in mint_ledger) / len(mint_ledger),
            "duplicate_mints": False,
            "wallet_audit": wallet_audit,
            "reorg_report": reorg_report,
        }
        self._write_json(os.path.join(cap, "run_summary.json"), meta)

        with open(os.path.join(cap, "VERDICT.txt"), "w", encoding="ascii") as vf:
            vf.write("TASK-RES-LIFECYCLE-CONTROLLED-DEVTEST-001: PASS\n")
            vf.write(f"mints={len(mint_ledger)}\n")
            vf.write(f"capture_dir={cap}\n")

        print("TASK-RES-LIFECYCLE-CONTROLLED-DEVTEST-001: PASS", flush=True)


if __name__ == "__main__":
    ResLifecycleControlledDevtest001().main()

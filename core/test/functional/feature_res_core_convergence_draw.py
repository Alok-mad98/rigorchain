#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""TASK-RES-CORE-FUNCTIONAL-TEST: RES convergence draw (functional, no consensus edits).

Harness chain is **devtest** (local-only, regtest-class parameters). ORUM-domain
burns are **convergence** transactions (CVG2); RES issuance is the coupled body
mint bound to the draw winner.

Validates protocol-level behavior aligned with ``convergence_logic.cpp`` /
``getconvergenceresult``:

* Sigma digest ``h`` per candidate tx (Sigma-PoW-01 / ``Sigma(txid, C, B_{h-1}, h)``;
  RPC field ``h`` — **not** a naive ``Hash256(txid || block_hash)``; the convergence
  root ``C``, the committing block's own hash, and the payout height participate per
  ``rigor::ComputeSigmaHash`` / ``convergence_logic.cpp``).
* Draw eligibility ``draw_eligible`` matches ``H < T·f`` (512-bit rule, resolved from
  the block's undo data by ``BuildConvergenceDrawSetFromUndo``).
* With ``convergence_root_set_size >= CONVERGENCE_DRAW_MIN_TXS_FOR_RES`` (1) and a
  non-null draw, ``draw_winner_txid`` equals the **argmin** Sigma digest among
  eligible txs, breaking ties by **memcmp on 32-byte txid** (LE byte order).
* SECURITY-001-A8: the draw a block AWARDS is PAID by the block after it. The payout
  coinbase carries **OP_RES_MINT** of **W(h)** (epoch 0: 40.000000 RES = 40,000,000 atoms)
  when a winner exists; ``mint_count`` / ``res_balance`` / ``getchainstatesummary`` /
  ``rigor_res_minted`` stay consistent.

Deliverables under ``--tmpdir``:

* ``task_res_core_draw_<label>.json`` — block hash, full txid list, sorted candidates
  (``txid``, ``h``, ``draw_eligible``), winner, ``state_hash``, RPL2 sidecar name.
* ``task_res_core_draw_<label>_verification.log`` — one-line ``verification_status`` + hashes.

Set ``TASK_RES_CORE_FUNCTIONAL_ARCHIVE_DIR`` to a directory (or run
``scripts/freeze/archive_task_res_core_functional.sh``) to copy the above plus
``MANIFEST.sha256`` and ``ARCHIVE_SEAL.txt`` after PASS (no ``--nocleanup``).
"""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
from decimal import Decimal

from test_framework.rigor_block_builder import create_convergence_tx
from test_framework.messages import ser_uint256
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than_or_equal

from res_security.harness import (
    DEVTEST_MINING_RES_FEE_ADDR,
    RES_REWARD_ATOMS,
    body_res_mint_rows as _body_res_mint_rows,
    paid_ticket_txid_from_payout_block as _pcorb_convergence_txid_hex_from_mint_tx,
)

# Optional: if set, copy ``task_res_core_draw_*.{json,hex,log}`` + ``MANIFEST.sha256`` + ``ARCHIVE_SEAL.txt``
# after a successful run (see ``scripts/freeze/archive_task_res_core_functional.sh``).
_TASK_RES_ARCHIVE_ENV = "TASK_RES_CORE_FUNCTIONAL_ARCHIVE_DIR"


def _txid_le_bytes(txid_hex: str) -> bytes:
    """uint256 txid RPC hex → 32-byte little-endian (memcmp order for tie-break)."""
    return bytes.fromhex(txid_hex)[::-1]


def expected_draw_candidate_winner(fr) -> str | None:
    """Return expected winner txid hex, or None (all-zero) if draw does not run."""
    n_root = int(fr.get("convergence_root_set_size", 0))
    if n_root < 1:
        return None
    eligible = [c for c in fr["candidates"] if c.get("draw_eligible")]
    if not eligible:
        return None
    best_h: int | None = None
    best_tid_le: bytes | None = None
    best_txid_hex: str | None = None
    for c in eligible:
        h = int(c["h"], 16)
        tid_hex = c["txid"]
        tid_le = _txid_le_bytes(tid_hex)
        if best_h is None:
            best_h, best_tid_le, best_txid_hex = h, tid_le, tid_hex
            continue
        if h < best_h or (h == best_h and tid_le < best_tid_le):
            best_h, best_tid_le, best_txid_hex = h, tid_le, tid_hex
    return best_txid_hex


def _json_sanitize(obj):
    """Make RPC-nested structures JSON-serializable (e.g. ``Decimal`` amounts)."""
    if isinstance(obj, dict):
        return {k: _json_sanitize(v) for k, v in obj.items()}
    if isinstance(obj, list):
        return [_json_sanitize(v) for v in obj]
    if isinstance(obj, Decimal):
        return format(obj, "f")
    return obj


def _chainstatesummary_semantic(csm: dict) -> dict:
    """CSM1 fields for audit JSON without embedding full ``chainstatesummary_hex``."""
    out = {k: v for k, v in csm.items() if k != "chainstatesummary_hex"}
    hx = csm.get("chainstatesummary_hex")
    if isinstance(hx, str) and hx:
        out["chainstatesummary_hex_sha256"] = hashlib.sha256(bytes.fromhex(hx)).hexdigest()
    return out


class ResCoreConvergenceDrawTest(BitcoinTestFramework):
    def _archive_task_deliverables_if_requested(self) -> None:
        dest = os.environ.get(_TASK_RES_ARCHIVE_ENV)
        if not dest:
            return
        dest = os.path.abspath(dest)
        os.makedirs(dest, exist_ok=True)
        td = self.options.tmpdir
        copied: list[str] = []
        for name in sorted(os.listdir(td)):
            if not name.startswith("task_res_core_draw_"):
                continue
            if not (
                name.endswith(".json") or name.endswith(".hex") or name.endswith(".log")
            ):
                continue
            shutil.copy2(os.path.join(td, name), os.path.join(dest, name))
            copied.append(name)
        lines: list[str] = []
        for name in sorted(copied):
            path = os.path.join(dest, name)
            with open(path, "rb") as bf:
                digest = hashlib.sha256(bf.read()).hexdigest()
            lines.append(f"{digest}  {name}\n")
        with open(os.path.join(dest, "MANIFEST.sha256"), "w", encoding="ascii") as mf:
            mf.writelines(lines)
        repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
        try:
            head = subprocess.check_output(
                ["git", "-C", repo_root, "rev-parse", "HEAD"],
                stderr=subprocess.DEVNULL,
                text=True,
            ).strip()
        except (OSError, subprocess.CalledProcessError):
            head = "(unknown)"
        seal = os.path.join(dest, "ARCHIVE_SEAL.txt")
        with open(seal, "w", encoding="ascii") as sf:
            sf.write("TASK-RES-CORE-FUNCTIONAL-TEST archive (post-PASS)\n")
            sf.write(f"git HEAD: {head}\n")
            sf.write(f"artifact files: {len(copied)}\n")
            sf.write("verify: (cd this directory && sha256sum -c MANIFEST.sha256)\n")
        self.log.info("Archived TASK deliverables to %s (%d files)", dest, len(copied))

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

    def _run_convergence_tx_helper(self, node_idx: int, *, orum: str = "1.0") -> dict:
        """Build/sign/broadcast like ``contrib/devtools/devtest-create-convergence-tx.py`` but in-process.

        Uses the same ``create_convergence_tx`` path as ``rigor_block_builder`` (wallet RPC), avoiding
        ``rigor-cli`` subprocess + ``signrawtransactionwithwallet`` edge cases on devtest.
        """
        node = self.nodes[node_idx]
        blockcount = node.getblockcount()
        tx = create_convergence_tx(node, Decimal(orum))
        raw_hex = tx.serialize_with_witness().hex()
        txid = node.sendrawtransaction(raw_hex, 0, orum)
        entry = node.getmempoolentry(txid)
        convergence_meta = entry.get("convergence", {})
        if not convergence_meta.get("is_convergence", False):
            raise AssertionError("tx not recognized as convergence by mempool metadata")
        if not convergence_meta.get("meets_min_convergence", False):
            raise AssertionError(
                "convergence amount below required threshold (meets_min_convergence false)"
            )
        prev = tx.vin[0].prevout
        utxo_txid = ser_uint256(prev.hash)[::-1].hex()
        conv_sats = tx.vout[0].nValue
        chg_sats = tx.vout[1].nValue if len(tx.vout) > 1 else 0
        return {
            "height": blockcount,
            "change_address_type": "bech32",
            "txid": txid,
            "raw_unsigned": None,
            "raw_signed": raw_hex,
            "convergence_meta": convergence_meta,
            "no_broadcast": False,
            "utxo_txid": utxo_txid,
            "utxo_vout": int(prev.n),
            "convergence_sats": conv_sats,
            "fee_sats": 1000,
            "fee_sats_requested": 1000,
            "change_sats": chg_sats,
            "mined": False,
        }

    def _mine_convergence_block(self, node, n_convergence: int):
        tx_infos = []
        for _ in range(n_convergence):
            tx_infos.append(self._run_convergence_tx_helper(0))
        txids = {t["txid"] for t in tx_infos}
        assert len(txids) == n_convergence, "convergence tx helper must yield distinct txids"
        self.sync_mempools()
        mining_addr = node.get_deterministic_witness_mining_address()
        out = self.generatetoaddress(node, 1, mining_addr)
        assert_equal(len(out), 1)
        block_hash = out[0]
        blk_tx = node.getblock(block_hash, 1)["tx"]
        for t in tx_infos:
            assert t["txid"] in blk_tx, f"expected convergence tx {t['txid']} in block"
        return block_hash, tx_infos

    def _assert_draw_and_res(self, node, block_hash: str, label: str):
        node.syncwithvalidationinterfacequeue()
        fr = node.getconvergenceresult(block_hash)
        assert_equal(fr["hash"], block_hash)
        # Sigma-PoW-01: the seed is the awarding block itself, and it pays at height + 1.
        assert_equal(fr["sigma_seed"], block_hash)
        assert_equal(int(fr["payout_height"]), int(fr["height"]) + 1)
        n_root = int(fr["convergence_root_set_size"])
        exp_winner = expected_draw_candidate_winner(fr)
        rpc_w = fr.get("draw_winner_txid", "0" * 64).lower()
        if exp_winner is None:
            assert rpc_w == "0" * 64, f"[{label}] expected no draw winner"
        else:
            assert rpc_w == exp_winner.lower(), f"[{label}] winner mismatch vs Python argmin"

        blk1 = node.getblock(block_hash, 1)
        tx_list = list(blk1["tx"])

        # CSM1 covers the awarding block, so capture it before extending the chain.
        csm = node.getchainstatesummary(block_hash)
        assert isinstance(csm, dict) and "error" not in csm, csm
        assert csm["block_hash"] == block_hash, f"[{label}] CSM1 tip must match asserted block"

        # SECURITY-001-A8: the mint settling this draw belongs to the next block. Mine it with
        # an empty mempool so the payout block is the only thing under test here.
        payout_hash = self.generatetoaddress(node, 1, node.get_deterministic_witness_mining_address())[0]
        node.syncwithvalidationinterfacequeue()
        assert_equal(node.getblockheader(payout_hash)["previousblockhash"], block_hash)
        assert_equal(int(node.getblock(payout_hash, 1)["height"]), int(fr["payout_height"]))
        hdr = node.getblockheader(payout_hash)
        res_mint_rows = _body_res_mint_rows(node, payout_hash)

        for c in fr["candidates"]:
            self.log.info(
                "[%s] candidate txid=%s h=%s eligible=%s conv_amt=%s",
                label,
                c["txid"],
                c.get("h"),
                c.get("draw_eligible"),
                c.get("convergence_amount"),
            )
        self.log.info("[%s] block tx order (%d): %s", label, len(tx_list), tx_list)
        self.log.info("[%s] RPC winner (draw_winner_txid)=%s", label, rpc_w)

        snap = node.getreplaysnapshot()
        det = node.getdeterministicstatehash()
        assert snap["state_hash"] == det["state_hash"], f"[{label}] replay vs deterministic state_hash"

        rpl2_name = f"task_res_core_draw_{label}_rpl2.hex"
        rpl2_path = os.path.join(self.options.tmpdir, rpl2_name)
        rpl2_hex = snap["serialized_snapshot"]
        with open(rpl2_path, "w", encoding="ascii") as rf:
            rf.write(rpl2_hex)
        rpl2_raw = bytes.fromhex(rpl2_hex)

        json_path = os.path.join(self.options.tmpdir, f"task_res_core_draw_{label}.json")
        snap_payload = {
            "label": label,
            "block_hash": block_hash,
            "block_height": blk1.get("height"),
            "payout_block_hash": payout_hash,
            "payout_height": fr.get("payout_height"),
            "transaction_ids": tx_list,
            "convergence_root_set_size": n_root,
            "res_probability_target": fr.get("res_probability_target"),
            "sigma_seed": fr.get("sigma_seed"),
            "winner_txid": rpc_w,
            "candidate_hashes": [
                {"txid": c["txid"], "h": c.get("h"), "draw_eligible": c.get("draw_eligible")}
                for c in sorted(fr["candidates"], key=lambda x: x["txid"])
            ],
            "state_hash": snap["state_hash"],
            "deterministic_statehash_rpc": det,
            "replay_snapshot": {
                "height": snap["height"],
                "mint_count": snap["mint_count"],
                "res_balance": str(snap["res_balance"]),
                "utxo_count": snap["utxo_count"],
                "serialized_snapshot_byte_len": len(rpl2_raw),
                "serialized_snapshot_sha256": hashlib.sha256(rpl2_raw).hexdigest(),
                "rpl2_sidecar_file": rpl2_name,
            },
            "body_res_mint": [{"txid": t, "res_atoms": a} for t, a in res_mint_rows],
            "rigor_res_minted": hdr.get("rigor_res_minted"),
            "mint_state_projection": {
                "mint_count": snap["mint_count"],
                "res_balance": str(snap["res_balance"]),
                "utxo_count": snap["utxo_count"],
                "lock_count": snap.get("lock_count"),
                "converge_count": snap.get("converge_count"),
                "other_balance": str(snap.get("other_balance", "")),
            },
            "chainstatesummary_semantic": _chainstatesummary_semantic(csm),
            "verification_status": "PENDING",
        }
        ok_winner = (exp_winner is None and rpc_w == "0" * 64) or (
            exp_winner is not None and rpc_w == exp_winner.lower()
        )
        ok_mint = True
        if exp_winner is not None:
            ok_mint = (
                len(res_mint_rows) == 1
                and res_mint_rows[0][1] == RES_REWARD_ATOMS
                and _pcorb_convergence_txid_hex_from_mint_tx(node, res_mint_rows[0][0], payout_hash)
                == rpc_w
            )
        else:
            ok_mint = len(res_mint_rows) == 0
        snap_payload["verification_status"] = "PASS" if (ok_winner and ok_mint) else "FAIL"
        with open(json_path, "w", encoding="ascii") as f:
            json.dump(_json_sanitize(snap_payload), f, indent=2, sort_keys=True)
        self.log.info("Wrote %s and %s", json_path, rpl2_path)
        vlog = os.path.join(self.options.tmpdir, f"task_res_core_draw_{label}_verification.log")
        with open(vlog, "w", encoding="ascii") as vf:
            vf.write(f"TASK-RES-CORE-FUNCTIONAL-TEST block={block_hash} label={label}\n")
            vf.write(f"verification_status={snap_payload['verification_status']}\n")
            vf.write(f"winner_txid={rpc_w}\n")
            vf.write(f"state_hash={snap['state_hash']}\n")
            vf.write(f"rpl2_sha256={hashlib.sha256(rpl2_raw).hexdigest()}\n")
        self.log.info("Wrote %s", vlog)

        if exp_winner is not None:
            assert_greater_than_or_equal(int(n_root), 1)
            mints = int(snap["mint_count"])
            rb = Decimal(str(snap["res_balance"]))
            assert_greater_than_or_equal(mints, 1)
            assert rb > Decimal(0)
            assert len(res_mint_rows) == 1, f"[{label}] exactly one coinbase OP_RES_MINT in the payout block"
            mint_txid, mint_atoms = res_mint_rows[0]
            assert mint_atoms == RES_REWARD_ATOMS, f"[{label}] RES mint atoms must match consensus lock"
            assert (
                _pcorb_convergence_txid_hex_from_mint_tx(node, mint_txid, payout_hash) == rpc_w
            ), f"[{label}] payout coinbase must pay the parent-draw winner"
            assert hdr.get("rigor_res_minted") == 1, f"[{label}] payout block header flags RES mint"
            assert csm.get("selection_has_winner"), f"[{label}] CSM1 selection_has_winner"
            assert (
                csm.get("selection_winner_txid", "").lower() == rpc_w
            ), f"[{label}] CSM1 winner txid"
        else:
            assert res_mint_rows == [], f"[{label}] no winner ⇒ no coinbase OP_RES_MINT"
            assert hdr.get("rigor_res_minted") == 0, f"[{label}] header: no RES mint without winner"

        return fr, snap

    def _fund_one_utxo(self, node, mining_addr, needed_oru: Decimal) -> Decimal:
        """Consolidate coinbases into a single spendable UTXO of at least `needed_oru`.

        Two stages, because `create_convergence_tx` funds a burn from ONE input and a direct
        consolidation of ~2,100 coinbases would exceed the standard transaction weight limit.
        The second stage names its inputs explicitly rather than leaving the choice to coin
        selection, which is free to prefer many small inputs and blow the same limit.
        """
        per_stage = Decimal("250000.0")
        stages = int(needed_oru / per_stage) + 1
        stage_utxos = []
        for i in range(stages):
            addr = node.getnewaddress(label=f"stage-{i}", address_type="bech32")
            node.sendtoaddress(addr, format(per_stage, "f"))
            self.generatetoaddress(node, 1, mining_addr)
            staged = node.listunspent(1, 9999999, [addr])
            assert_equal(len(staged), 1)
            # Lock it, or the next stage's coin selection will happily fund itself by spending
            # this one and the final consolidation will reference an output that no longer exists.
            node.lockunspent(False, [{"txid": staged[0]["txid"], "vout": staged[0]["vout"]}])
            stage_utxos.extend(staged)
        assert_equal(len(stage_utxos), stages)

        total = sum(Decimal(str(u["amount"])) for u in stage_utxos)
        final_addr = node.getnewaddress(label="saturating-burn", address_type="bech32")
        raw = node.createrawtransaction(
            [{"txid": u["txid"], "vout": u["vout"]} for u in stage_utxos],
            [{final_addr: format(total - Decimal("0.001"), "f")}],
        )
        signed = node.signrawtransactionwithwallet(raw)
        assert signed["complete"], signed
        node.sendrawtransaction(signed["hex"])
        # No unlock: the inputs are spent now, and `lockunspent` refuses to release an output it
        # can no longer see as unspent.
        self.generatetoaddress(node, 1, mining_addr)
        return total - Decimal("0.001")

    def run_test(self):
        node = self.nodes[0]
        self.log.info(
            "Mine maturity + prepare ORUM (ORU on wire) in default_wallet (bech32); "
            "convergence tx helper spends listunspent — mine to the wallet, not only to get_deterministic_witness_mining_address"
        )
        mining_addr = node.getnewaddress(label="res-core-miner", address_type="bech32")
        # devtest pays 1000 ORU per block and coinbases mature after 100.
        #
        # A-9-C divided T by the draw window, which multiplied the amount that saturates
        # eligibility by the same factor: certainty now needs 65536·W ORU rather than 65536. Read W
        # off the node so this tracks the constant instead of restating it.
        self.generatetoaddress(node, 101, mining_addr)
        window = int(node.getconvergenceresult(node.getbestblockhash())["window_size"])
        saturating_oru = Decimal(65536 * window)
        self.log.info(f"draw window W = {window}; saturating burn = {saturating_oru} ORU")
        # Enough mature subsidy to build one UTXO above that, plus headroom for the later cases.
        self.generatetoaddress(node, int(saturating_oru / 1000) + 400, mining_addr)

        self.log.info("Consolidating UTXOs for the saturating convergence tx")
        self._fund_one_utxo(node, mining_addr, saturating_oru)

        snap0 = node.getreplaysnapshot()
        m0 = int(snap0["mint_count"])

        self.log.info("Case A: single convergence tx in block → eligible singleton draw")
        # σ is seeded by the awarding block's own hash, so eligibility is probabilistic below the
        # saturation point. Burn at the saturation point to make this case deterministic.
        self._run_convergence_tx_helper(0, orum=format(saturating_oru, "f"))
        self.sync_mempools()
        self.generatetoaddress(node, 1, node.get_deterministic_witness_mining_address())
        bh_a = node.getbestblockhash()
        fr_a = node.getconvergenceresult(bh_a)
        assert_equal(int(fr_a["convergence_root_set_size"]), 1)
        fr_a, _snap_a = self._assert_draw_and_res(node, bh_a, "single_tx")
        assert expected_draw_candidate_winner(fr_a) is not None

        self.log.info("Case B: two convergence txs → winner + RES mint delta")
        snap_b0 = node.getreplaysnapshot()
        mb0 = int(snap_b0["mint_count"])
        rb0 = Decimal(str(snap_b0["res_balance"]))
        bh_b, _infos = self._mine_convergence_block(node, 2)
        fr_b, snap_b = self._assert_draw_and_res(node, bh_b, "two_tx")
        exp = expected_draw_candidate_winner(fr_b)
        if exp is not None:
            assert_equal(int(snap_b["mint_count"]), mb0 + 1)
            assert Decimal(str(snap_b["res_balance"])) > rb0

        self.log.info("Case C: five convergence txs → deterministic winner (same tie-break rules)")
        snap_c0 = node.getreplaysnapshot()
        mc0 = int(snap_c0["mint_count"])
        bh_c, _ = self._mine_convergence_block(node, 5)
        fr_c, _ = self._assert_draw_and_res(node, bh_c, "five_tx")
        exp_c = expected_draw_candidate_winner(fr_c)
        if exp_c is not None:
            assert_equal(int(node.getreplaysnapshot()["mint_count"]), mc0 + 1)

        self.log.info("Case D: sweep 3 / 7 / 10 convergence txs per block (deterministic winner each)")
        for n_sweep in (3, 7, 10):
            tag = f"sweep_{n_sweep}_tx"
            snap0 = node.getreplaysnapshot()
            m_prev = int(snap0["mint_count"])
            bh, _ = self._mine_convergence_block(node, n_sweep)
            fr_d, _ = self._assert_draw_and_res(node, bh, tag)
            if expected_draw_candidate_winner(fr_d) is not None:
                assert_equal(int(node.getreplaysnapshot()["mint_count"]), m_prev + 1)

        self.log.info("Baseline mint drift since test start: %d → %d", m0, int(node.getreplaysnapshot()["mint_count"]))
        self._archive_task_deliverables_if_requested()
        print("TASK-RES-CORE-FUNCTIONAL-TEST: PASS", flush=True)


if __name__ == "__main__":
    ResCoreConvergenceDrawTest().main()

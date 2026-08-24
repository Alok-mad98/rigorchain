#!/usr/bin/env python3
"""Generate PHASE-3 golden vectors (>=50) aligned with frozen C++ selection tests."""

from __future__ import annotations

import json
import os
import sys
from pathlib import Path
from collections import Counter
from typing import Any, Dict, List

sys.path.insert(0, str(Path(__file__).parent / "spec"))
import rigor_consensus_spec as spec  # noqa: E402

T_MAX_HEX = "f" * 64
T_ONE_HEX = "0000000000000000000000000000000000000000000000000000000000000001"


def v(vid: str, name: str, module: str, inp: Dict[str, Any], exp: Dict[str, Any], note: str = "") -> Dict[str, Any]:
    row = {"id": vid, "name": name, "module": module, "input": inp, "expected": exp}
    if note:
        row["note"] = note
    return row


def txid(seed: int) -> bytes:
    return spec.sha256d(seed.to_bytes(4, "little"))


def sethex(h: str) -> bytes:
    return spec.parse_hex256_sethex(h)


def run_sel(txid_hex: List[str], fees: List[int], height: int, prev_hex: str, t_hex: str) -> Dict[str, Any]:
    txids = [sethex(h) for h in txid_hex]
    prev_block_hash = sethex(prev_hex)
    t_int = spec.res_probability_target_from_bytes(bytes.fromhex(t_hex))
    r = spec.run_selection(txids, fees, height, prev_block_hash, t_int)
    return {
        "convergence_root": r.convergence_root.hex(),
        "sigmas": [s.hex() for s in r.sigma_hashes],
        "eligible": r.eligible,
        "winner_index": r.winner_index,
    }


def main() -> None:
    vectors: List[Dict[str, Any]] = []

    for i, n in enumerate([0, 1, 2, 3, 5, 8, 12]):
        ids = [txid(j) for j in range(n)]
        root = spec.compute_convergence_root_from_txids(ids)
        vectors.append(
            v(f"CR-{i:03d}", f"root_n{n}", "ConvergenceRoot", {"txids_hex": [x.hex() for x in ids]}, {"convergence_root": root.hex()})
        )

    ids3 = [txid(0), txid(1), txid(2)]
    root3 = spec.compute_convergence_root_from_txids(ids3)
    vectors.append(
        v("CR-010", "perm", "ConvergenceRoot", {"txids_hex": [ids3[2].hex(), ids3[0].hex(), ids3[1].hex()]}, {"convergence_root": root3.hex()})
    )

    aa, bb = sethex("aa" * 32), sethex("bb" * 32)
    vectors.append(
        v(
            "CR-011",
            "sethex_pair",
            "ConvergenceRoot",
            {"txids_hex": ["aa" * 32, "bb" * 32], "encoding": "sethex"},
            {"convergence_root": spec.compute_convergence_root_from_txids([aa, bb]).hex()},
        )
    )

    # SIG-003 differs from SIG-002 only in the seed: the σ family must cover B_{h-1}
    # varying independently of every other input.
    for i, (h, height, prev, c) in enumerate(
        [
            (txid(0), 100, sethex("11" * 32), txid(2)),
            (txid(1), 999, sethex("11" * 32), txid(3)),
            (aa, 1, sethex("11" * 32), bytes(32)),
            (aa, 1, sethex("22" * 32), bytes(32)),
            (aa, 50, sethex("11" * 32), spec.compute_convergence_root_from_txids([aa, bb])),
        ]
    ):
        sig = spec.compute_pcobr_hash(h, prev, height, c)
        vectors.append(
            v(
                f"SIG-{i:03d}",
                f"sigma_{i}",
                "SigmaPoW01",
                {
                    "txid_hex": h.hex(),
                    "prev_block_hash_hex": prev.hex(),
                    "height": height,
                    "convergence_root_hex": c.hex(),
                },
                {"sigma": sig.hex()},
            )
        )

    def t_hex(n: int) -> str:
        return n.to_bytes(32, "little").hex()

    ineq = [
        ("0100000000000000000000000000000000000000000000000000000000000000", spec.RES_PROBABILITY_TARGET_INT, 1),
        ("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", spec.RES_PROBABILITY_TARGET_INT, 1),
        ("0100000000000000000000000000000000000000000000000000000000000000", 1, 1),
        ("0100000000000000000000000000000000000000000000000000000000000000", (1 << 256) - 1, 1),
    ]
    for i, (hhex, T, f) in enumerate(ineq):
        h = spec.hash_hex_to_uint256_bytes(hhex)
        vectors.append(
            v(
                f"INQ-{i:03d}",
                f"inq_{i}",
                "Inequality",
                {"h_hex": hhex, "T_hex": t_hex(T), "f": f},
                {"eligible": spec.check_inequality(h, T, f)},
            )
        )

    H64 = lambda c: c * 64  # 64 hex digits -> 32 raw bytes (SetHex input)
    cpp = [
        ("empty", [], [], 1, H64("0"), T_ONE_HEX),
        (
            "sort",
            [H64("f"), H64("0")],
            [1, 1],
            10,
            H64("1"),
            T_MAX_HEX,
        ),
        ("single", [H64("a")], [1], 1, H64("b"), T_MAX_HEX),
        ("nowin", [H64("a"), H64("b")], [1, 1], 5, H64("c"), T_ONE_HEX),
        ("wgt", [H64("a"), H64("b")], [1, 1000], 7, H64("d"), T_MAX_HEX),
        ("tie", [H64("1"), H64("2")], [10, 10], 9, H64("e"), T_MAX_HEX),
        ("big", [H64("f"), H64("e")], [18446744073709551615, 1], 100, H64("a"), T_MAX_HEX),
    ]
    for i, (tag, txs, fees, height, prev, th) in enumerate(cpp):
        vectors.append(
            v(
                f"WIN-{i:03d}",
                tag,
                "RunSelection",
                {"txids_hex": txs, "fees": fees, "height": height, "prev_block_hash_hex": prev, "T_hex": th},
                run_sel(txs, fees, height, prev, th),
            )
        )

    # SECURITY-001-A8: the σ seed is B_{h-1}, so a draw resolved under one parent cannot
    # replay under another. One flipped seed bit must move every σ.
    prev_aa = sethex("aa" * 32)
    for i, bit in enumerate([0, 127, 255]):
        flipped = bytearray(prev_aa)
        flipped[bit // 8] ^= 1 << (bit % 8)
        s1 = spec.compute_pcobr_hash(txid(0), prev_aa, 50, bytes(32))
        s2 = spec.compute_pcobr_hash(txid(0), bytes(flipped), 50, bytes(32))
        vectors.append(v(f"RP-{i:03d}", f"replay_seed_bit_{bit}", "Replay", {"seed_bit": bit}, {"differ": s1 != s2}))

    # The seed is the parent block hash at depth 1, defined for every payout height from 1
    # upward: no bind depth to configure and no bootstrap zero-preimage window. Each vector
    # pins σ at one height so a reintroduced bootstrap branch would fail here.
    prev_bb = sethex("bb" * 32)
    for h in range(1, 13):
        sig = spec.compute_pcobr_hash(txid(0), prev_bb, h, bytes(32))
        vectors.append(
            v(
                f"RO-{h - 1:03d}",
                f"seed_depth_h{h}",
                "ReorgBind",
                {"payout_height": h, "prev_block_hash_hex": prev_bb.hex(), "txid_hex": txid(0).hex()},
                {"seed_depth": spec.CONVERGENCE_DRAW_SEED_DEPTH, "sigma": sig.hex()},
            )
        )

    # RFC-017: the mint amount is now W(height), so each case carries the height it mints at.
    # SUP-002 sits in epoch 1 to exercise a decayed reward; SUP-004 sits one reward short of the
    # cap so the cap rejection path is covered.
    sup_cases = [
        (1000, 0, False, 0),
        (900, spec.RES_REWARD_GENESIS, True, 0),
        (500, 1_000_000, True, spec.RES_REWARD_EPOCH),
        (0, spec.RES_MAX_SUPPLY, False, 0),
        (100, spec.RES_MAX_SUPPLY - 100, True, 0),
    ]
    for i, (oru, res, mint, height) in enumerate(sup_cases):
        w = spec.get_res_mint_reward(height)
        after = res + (w if mint and res + w <= spec.RES_MAX_SUPPLY else 0)
        vectors.append(
            v(
                f"SUP-{i:03d}",
                "supply",
                "SupplyScaffold",
                {"ORU": oru, "RES": res, "mint": mint, "height": height},
                {"RES_after": after},
            )
        )

    for n in range(12):
        if n == 0:
            exp = {"convergence_root": "00" * 64, "winner_index": -1, "sigmas": [], "eligible": []}
        else:
            txs = ["a" * 64] * min(n, 3)
            fees = [spec.MIN_CONVERGENCE_AMOUNT] * len(txs)
            exp = run_sel(txs, fees, 100 + n, "b" * 64, T_MAX_HEX)
        vectors.append(v(f"BLK-{n:03d}", f"blk_{n}", "BlockTrace", {"n": n}, exp))

    # -------------------------------------------------------------------------
    # Post-verdict expansion (A-9-C, encoder, window/reorg/dup/boundary).
    # SHA-256-heavy families default coq_enforce=false so the Coq harness stays
    # a cheap kernel core; Python and C++ consume the full set. Original kernel
    # rows (CR-/SIG-/WIN-/INQ- prefix) omit the key on purpose; the lock lists
    # those 25 ids under surface_splits.omit_coq_enforce_key_enforced.
    # -------------------------------------------------------------------------

    def add(vec, coq_enforce=None):
        if coq_enforce is not None:
            vec["coq_enforce"] = coq_enforce
        vectors.append(vec)

    # Canonical encoder: offsets, LE32 height, length lock.
    enc_txid = bytes(range(32))
    enc_c = bytes(range(32, 64))
    enc_prev = bytes(range(64, 96))
    for i, height in enumerate([0, 1, 255, 256, 0x01020304, 0xFFFFFFFF]):
        pre = spec.encode_sigma_preimage(enc_txid, enc_prev, height, enc_c)
        add(
            v(
                f"ENC-{i:03d}",
                f"encoder_h{height}",
                "SigmaEncoder",
                {
                    "txid_hex": enc_txid.hex(),
                    "convergence_root_hex": enc_c.hex(),
                    "prev_block_hash_hex": enc_prev.hex(),
                    "height": height,
                },
                {
                    "preimage_hex": pre.hex(),
                    "length": 100,
                    "txid": pre[0:32].hex(),
                    "croot": pre[32:64].hex(),
                    "prev": pre[64:96].hex(),
                    "height_le": pre[96:100].hex(),
                    "sigma": spec.sha256d(pre).hex(),
                },
            ),
            coq_enforce=True,
        )

    # A-9-C already_paid exclusion (order-preserving, paid dropped).
    a9c_ids = [txid(i) for i in range(8)]
    a9c_cases = [
        ("none_paid", a9c_ids[:4], []),
        ("first_paid", a9c_ids[:4], [a9c_ids[0]]),
        ("last_paid", a9c_ids[:4], [a9c_ids[3]]),
        ("all_paid", a9c_ids[:4], a9c_ids[:4]),
        ("middle_two", a9c_ids[:5], [a9c_ids[1], a9c_ids[3]]),
        ("paid_not_in_set", a9c_ids[:3], [txid(99)]),
        ("empty_candidates", [], [a9c_ids[0]]),
        ("dup_paid_list", a9c_ids[:3], [a9c_ids[1], a9c_ids[1]]),
        ("perm_paid", a9c_ids[:4], [a9c_ids[2], a9c_ids[0]]),
        ("single_survives", a9c_ids[:1], []),
        ("single_dropped", a9c_ids[:1], a9c_ids[:1]),
        ("window_full_eight", a9c_ids, [a9c_ids[0], a9c_ids[7]]),
    ]
    for i, (tag, cands, paid) in enumerate(a9c_cases):
        surv = spec.exclude_already_paid(cands, paid)
        add(
            v(
                f"A9C-{i:03d}",
                tag,
                "WindowExclusion",
                {
                    "candidates_hex": [x.hex() for x in cands],
                    "already_paid_hex": [x.hex() for x in paid],
                },
                {
                    "surviving_hex": [x.hex() for x in surv],
                    "dropped": len(cands) - len(surv),
                },
            ),
            coq_enforce=True,
        )

    # DrawCandidateRoot: HashConcatenatedSortedTxids. Empty hashes []; C of empty is zeros.
    # Cheap SHA-256d (empty / 32 / 64 bytes). Locks the eligible-subset digest
    # that ConvergenceRoot theorems do not cover.
    dcr0 = txid(0)
    dcr1 = txid(1)
    dcr2 = txid(2)

    def dcr_row(vid, name, txids, elig, note=""):
        root = spec.convergence_draw_eligible_root(txids, elig)
        add(
            v(
                vid,
                name,
                "DrawCandidateRoot",
                {
                    "txids_hex": [x.hex() for x in txids],
                    "eligible": elig,
                },
                {"draw_candidate_root": root.hex()},
                note,
            ),
            coq_enforce=True,
        )

    dcr_row("DCR-000", "empty", [], [], "empty set hashes empty concat, not zeros")
    dcr_row("DCR-001", "singleton_no_zero_shortcut", [dcr0], [True],
            "N=1 hashes the txid; same digest as ConvergenceRoot")
    dcr_row("DCR-002", "two_agrees_with_C", [dcr0, dcr1], [True, True],
            "nonempty F: same digest as ConvergenceRoot")
    dcr_row("DCR-003", "perm_two", [dcr1, dcr0], [True, True],
            "order independence: same set as DCR-002")
    dcr_row("DCR-004", "eligible_subset", [dcr0, dcr1, dcr2], [True, False, True],
            "hashes {txid0, txid2} subset of F")
    dcr_row("DCR-005", "none_eligible", [dcr0, dcr1, dcr2], [False, False, False],
            "all ineligible → empty concat")
    dcr_row("DCR-006", "a9c_all_paid", [dcr0, dcr1], [False, False],
            "A-9-C: both already paid, eligible subset empty")
    dcr_row("DCR-007", "a9c_singleton_remainder", [dcr0, dcr1, dcr2], [False, True, False],
            "A-9-C: one unpaid remainder; hash of that txid, not zeros")

    # Integration + coverage expansion (TASK-FV-MODEL-DCR-INTEGRATION-AND-VECTORS-001).
    empty_dcr = spec.convergence_draw_eligible_root([], []).hex()

    add(
        v(
            "DCR-008",
            "incomplete_window_no_root",
            "DrawCandidateRoot",
            {"kind": "incomplete"},
            {"draw_candidate_root": None},
            "incomplete window is no draw and no root",
        ),
        coq_enforce=True,
    )
    add(
        v(
            "DCR-009",
            "genesis_complete_empty",
            "DrawCandidateRoot",
            {"kind": "genesis"},
            {"draw_candidate_root": empty_dcr},
            "complete empty window hashes empty concat",
        ),
        coq_enforce=True,
    )
    add(
        v(
            "DCR-010",
            "exclusion_all_paid",
            "DrawCandidateRoot",
            {
                "kind": "exclusion",
                "candidates_hex": [dcr0.hex(), dcr1.hex(), dcr2.hex()],
                "already_paid_hex": [dcr0.hex(), dcr1.hex(), dcr2.hex()],
            },
            {"draw_candidate_root": empty_dcr},
            "A-9-C exclude all → empty eligible root",
        ),
        coq_enforce=True,
    )
    ids3 = [dcr0, dcr1, dcr2]
    root3 = spec.compute_convergence_root_from_txids(ids3)
    dcr_row(
        "DCR-011",
        "three_perm_a",
        [dcr2, dcr0, dcr1],
        [True, True, True],
        "|F|=3 perm: same digest as ConvergenceRoot",
    )
    dcr_row(
        "DCR-012",
        "three_perm_b",
        [dcr1, dcr2, dcr0],
        [True, True, True],
        "|F|=3 other perm: order independence",
    )

    def dcr_e2e(vid, name, txids, fees, height, prev, t_hex, note, paid=None):
        work, work_fees = txids, fees
        if paid is not None:
            work = spec.exclude_already_paid(txids, paid)
            work_fees = [fees[txids.index(x)] for x in work]
        r = spec.run_selection(
            work, work_fees, height, prev, spec.res_probability_target_from_bytes(bytes.fromhex(t_hex))
        )
        dcr = spec.convergence_draw_eligible_root(work, r.eligible)
        if r.winner_index is None or r.winner_index < 0:
            orig_win = -1 if r.winner_index is None else r.winner_index
        else:
            orig_win = txids.index(work[r.winner_index])
        inp = {
            "kind": "e2e",
            "txids_hex": [x.hex() for x in txids],
            "fees": fees,
            "height": height,
            "prev_block_hash_hex": prev.hex(),
            "T_hex": t_hex,
        }
        if paid is not None:
            inp["already_paid_hex"] = [x.hex() for x in paid]
        add(
            v(
                vid,
                name,
                "DrawCandidateRoot",
                inp,
                {
                    "draw_candidate_root": dcr.hex(),
                    "convergence_root": r.convergence_root.hex(),
                    "eligible": r.eligible,
                    "winner_index": orig_win,
                },
                note,
            ),
            coq_enforce=True,
        )

    prev11 = bytes.fromhex("11" * 32)
    dcr_e2e(
        "DCR-013",
        "e2e_all_eligible_winner",
        [dcr0, dcr1],
        [1, 1],
        10,
        prev11,
        T_MAX_HEX,
        "e2e: T_max, |F|=2, DCR=C and a winner",
    )
    dcr_e2e(
        "DCR-014",
        "e2e_none_eligible",
        [dcr0, dcr1],
        [1, 1],
        5,
        bytes.fromhex("cc" * 32),
        T_ONE_HEX,
        "e2e: T_one, nobody eligible, DCR=empty concat, no winner",
    )
    dcr_row(
        "DCR-015",
        "subset_perm",
        [dcr2, dcr1, dcr0],
        [True, False, True],
        "eligible {txid2,txid0} permuted vs DCR-004; same set",
    )

    # E2E expansion (TASK-FV-MODEL-DCR-SELECTION-E2E-001): root/σ/winner joint
    # cases, plus A-9-C exclusion with recomputed C.
    prev02 = bytes.fromhex("02" * 32)
    prev22 = bytes.fromhex("22" * 32)
    dcr_e2e(
        "DCR-016",
        "e2e_same_dcr_winner_flips_with_seed",
        [dcr0, dcr1],
        [1, 1],
        10,
        prev02,
        T_MAX_HEX,
        "e2e: same C/DCR as DCR-013 (T_max), prev seed flips winner 1→0",
    )
    dcr_e2e(
        "DCR-017",
        "e2e_two_prev22_baseline",
        [dcr0, dcr1],
        [1, 1],
        10,
        prev22,
        T_MAX_HEX,
        "e2e: |F|=2 prev=22, winner=txid1; baseline for DCR-018 C-change",
    )
    dcr_e2e(
        "DCR-018",
        "e2e_root_change_flips_winner",
        [dcr0, dcr1, dcr2],
        [1, 1, 1],
        10,
        prev22,
        T_MAX_HEX,
        "e2e: adding txid2 changes C vs DCR-017 and flips winner to txid0",
    )
    dcr_e2e(
        "DCR-019",
        "e2e_three_prev11_baseline",
        [dcr0, dcr1, dcr2],
        [1, 1, 1],
        10,
        prev11,
        T_MAX_HEX,
        "e2e: |F|=3 prev=11, winner=txid1; baseline for A-9-C DCR-020",
    )
    dcr_e2e(
        "DCR-020",
        "e2e_exclusion_drops_winner",
        [dcr0, dcr1, dcr2],
        [1, 1, 1],
        10,
        prev11,
        T_MAX_HEX,
        "e2e A-9-C: pay DCR-019 winner; remaining {0,2} new C/DCR, winner=txid0",
        paid=[dcr1],
    )
    dcr_e2e(
        "DCR-021",
        "e2e_exclusion_singleton_hashed",
        [dcr0, dcr1],
        [1, 1],
        10,
        prev11,
        T_MAX_HEX,
        "e2e A-9-C: pay txid1; singleton remainder, C=DCR=Hash256(txid0), winner=txid0",
        paid=[dcr1],
    )
    dcr_e2e(
        "DCR-022",
        "e2e_three_t_one_no_winner",
        [dcr0, dcr1, dcr2],
        [1, 1, 1],
        10,
        prev11,
        T_ONE_HEX,
        "e2e: |F|=3 T_one, C of three, DCR=empty concat, no winner",
    )
    dcr_e2e(
        "DCR-023",
        "e2e_height_flips_winner_same_root",
        [dcr0, dcr1, dcr2],
        [1, 1, 1],
        11,
        prev11,
        T_MAX_HEX,
        "e2e: same C/DCR as DCR-019, height 11 flips winner 1→0",
    )

    # -------------------------------------------------------------------------
    # TASK-FV-VECTOR-DCR-SELECTION-NEGATIVE-001: negative / perturbation
    # counterparts of DCR-000..023. Each row pins that a tamper, exclusion
    # mismatch, duplicate, empty/singleton shape break, or σ-input change
    # (C / B_{h-1} / LE32(h)) actually moves DCR and/or the winner. Coq-enforced.
    # -------------------------------------------------------------------------
    zeros32 = bytes(32)

    def dcr_contrast(vid, name, txids, elig, other_txids, other_elig, note):
        a = spec.convergence_draw_eligible_root(txids, elig)
        b = spec.convergence_draw_eligible_root(other_txids, other_elig)
        if a == b:
            raise SystemExit(f"{vid} contrast unexpectedly equal")
        add(
            v(
                vid,
                name,
                "DrawCandidateRoot",
                {
                    "kind": "contrast",
                    "txids_hex": [x.hex() for x in txids],
                    "eligible": elig,
                    "other_txids_hex": [x.hex() for x in other_txids],
                    "other_eligible": other_elig,
                },
                {
                    "draw_candidate_root": a.hex(),
                    "other_draw_candidate_root": b.hex(),
                    "differ": True,
                },
                note,
            ),
            coq_enforce=True,
        )

    dcr_contrast(
        "DCR-024",
        "neg_tamper_insert",
        [dcr0, dcr1],
        [True, True],
        [dcr0, dcr1, dcr2],
        [True, True, True],
        "neg: inserting txid2 into F changes DCR vs DCR-002",
    )
    dcr_contrast(
        "DCR-025",
        "neg_tamper_delete",
        [dcr0, dcr1, dcr2],
        [True, True, True],
        [dcr0, dcr2],
        [True, True],
        "neg: deleting txid1 from F changes DCR vs DCR-011",
    )
    dcr_contrast(
        "DCR-026",
        "neg_tamper_replace",
        [dcr0, dcr1],
        [True, True],
        [dcr0, dcr2],
        [True, True],
        "neg: replacing txid1 with txid2 changes DCR",
    )
    dcr_contrast(
        "DCR-027",
        "neg_flag_perm_changes_hashed_set",
        [dcr0, dcr1, dcr2],
        [True, False, True],
        [dcr0, dcr1, dcr2],
        [True, True, False],
        "neg: permuting eligibility flags changes the hashed subset vs DCR-004",
    )
    dcr_contrast(
        "DCR-028",
        "neg_perm_of_tampered_still_differs",
        [dcr0, dcr1],
        [True, True],
        [dcr2, dcr1, dcr0],
        [True, True, True],
        "neg: permutation of a tampered 3-set still ≠ honest 2-set DCR (contrast DCR-003)",
    )
    dcr_contrast(
        "DCR-029",
        "neg_duplicate_txid",
        [dcr0],
        [True],
        [dcr0, dcr0],
        [True, True],
        "neg: duplicate txid is hashed twice; not collapsed to the singleton DCR-001",
    )

    empty_root = spec.convergence_draw_eligible_root([], [])
    if empty_root == zeros32:
        raise SystemExit("DCR-030 empty root collapsed to zeros")
    add(
        v(
            "DCR-030",
            "neg_empty_neq_zero",
            "DrawCandidateRoot",
            {"kind": "shape", "txids_hex": [], "eligible": [], "neq_zero": True},
            {"draw_candidate_root": empty_root.hex(), "neq_zero": True},
            "neg: empty eligible set ≠ ConvergenceRoot empty zeros (contrast DCR-000)",
        ),
        coq_enforce=True,
    )
    sing_root = spec.convergence_draw_eligible_root([dcr0], [True])
    sing_c = spec.compute_convergence_root_from_txids([dcr0])
    if sing_root != sing_c or sing_root == zeros32:
        raise SystemExit("DCR-031 singleton DCR must equal hashed C and not be zeros")
    add(
        v(
            "DCR-031",
            "pos_singleton_eq_C",
            "DrawCandidateRoot",
            {
                "kind": "shape",
                "txids_hex": [dcr0.hex()],
                "eligible": [True],
                "neq_zero": True,
                "eq_c": True,
            },
            {"draw_candidate_root": sing_root.hex(), "neq_zero": True, "eq_c": True},
            "pos: singleton DCR = C = Hash256(txid); empty is the only C=zeros case (DCR-030)",
        ),
        coq_enforce=True,
    )

    t_max = spec.res_probability_target_from_bytes(bytes.fromhex(T_MAX_HEX))
    ids3 = [dcr0, dcr1, dcr2]
    paid_w = [dcr1]
    work_ex = spec.exclude_already_paid(ids3, paid_w)
    r_ex = spec.run_selection(work_ex, [1, 1], 10, prev11, t_max)
    r_full = spec.run_selection(ids3, [1, 1, 1], 10, prev11, t_max)
    dcr_ex = spec.convergence_draw_eligible_root(work_ex, r_ex.eligible)
    dcr_full = spec.convergence_draw_eligible_root(ids3, r_full.eligible)
    win_ex = ids3.index(work_ex[r_ex.winner_index])
    if dcr_ex == dcr_full or r_full.winner_index != 1 or win_ex != 0:
        raise SystemExit("DCR-032 exclusion mismatch did not separate root/winner")
    add(
        v(
            "DCR-032",
            "neg_exclusion_root_winner_mismatch",
            "DrawCandidateRoot",
            {
                "kind": "e2e_mismatch",
                "txids_hex": [x.hex() for x in ids3],
                "fees": [1, 1, 1],
                "height": 10,
                "prev_block_hash_hex": prev11.hex(),
                "T_hex": T_MAX_HEX,
                "already_paid_hex": [dcr1.hex()],
            },
            {
                "draw_candidate_root": dcr_ex.hex(),
                "convergence_root": r_ex.convergence_root.hex(),
                "eligible": r_ex.eligible,
                "winner_index": win_ex,
                "wrong_draw_candidate_root": dcr_full.hex(),
                "wrong_winner_index": r_full.winner_index,
                "differ": True,
            },
            "neg: hashing full F but selecting after A-9-C drop of the winner: DCR and winner disagree (contrast DCR-019/020)",
        ),
        coq_enforce=True,
    )

    def dcr_sigma_perturb(vid, name, txids, fees, height, prev, t_hex, alt, note):
        t_int = spec.res_probability_target_from_bytes(bytes.fromhex(t_hex))
        r = spec.run_selection(txids, fees, height, prev, t_int)
        dcr = spec.convergence_draw_eligible_root(txids, r.eligible)
        alt_prev = bytes.fromhex(alt["prev_block_hash_hex"]) if "prev_block_hash_hex" in alt else prev
        alt_h = int(alt.get("height", height))
        if "convergence_root_hex" in alt:
            r_alt = spec.compute_convergence_draw_selection(
                txids, fees, bytes.fromhex(alt["convergence_root_hex"]), alt_prev, alt_h, t_int
            )
        else:
            r_alt = spec.run_selection(txids, fees, alt_h, alt_prev, t_int)
        if r.winner_index == r_alt.winner_index:
            raise SystemExit(f"{vid} σ perturbation did not change the winner")
        inp = {
            "kind": "sigma_perturb",
            "txids_hex": [x.hex() for x in txids],
            "fees": fees,
            "height": height,
            "prev_block_hash_hex": prev.hex(),
            "T_hex": t_hex,
            "alt": alt,
        }
        add(
            v(
                vid,
                name,
                "DrawCandidateRoot",
                inp,
                {
                    "draw_candidate_root": dcr.hex(),
                    "convergence_root": r.convergence_root.hex(),
                    "eligible": r.eligible,
                    "winner_index": r.winner_index,
                    "alt_winner_index": r_alt.winner_index,
                    "differ": True,
                },
                note,
            ),
            coq_enforce=True,
        )

    r013 = spec.run_selection([dcr0, dcr1], [1, 1], 10, prev11, t_max)
    flipped_c = bytearray(r013.convergence_root)
    flipped_c[-1] ^= 1
    dcr_sigma_perturb(
        "DCR-033",
        "neg_sigma_wrong_C",
        [dcr0, dcr1],
        [1, 1],
        10,
        prev11,
        T_MAX_HEX,
        {"convergence_root_hex": bytes(flipped_c).hex()},
        "neg: feeding a 1-bit-flipped C into σ changes the winner vs DCR-013; DCR of F unchanged (T_max)",
    )
    dcr_sigma_perturb(
        "DCR-034",
        "neg_sigma_flip_prev",
        [dcr0, dcr1],
        [1, 1],
        10,
        prev11,
        T_MAX_HEX,
        {"prev_block_hash_hex": prev02.hex()},
        "neg: flipping B_{h-1} (11→02) changes the winner vs DCR-013; same C/DCR (contrast DCR-016)",
    )
    dcr_sigma_perturb(
        "DCR-035",
        "neg_sigma_flip_height",
        [dcr0, dcr1],
        [1, 1],
        10,
        prev11,
        T_MAX_HEX,
        {"height": 11},
        "neg: flipping LE32(h) (10→11) changes the winner vs DCR-013; same C/DCR (contrast DCR-023)",
    )

    # More exclusion pairs: every singleton drop in a 6-candidate window.
    six = [txid(100 + i) for i in range(6)]
    for i in range(6):
        paid = [six[i]]
        surv = spec.exclude_already_paid(six, paid)
        add(
            v(
                f"A9C-{12 + i:03d}",
                f"drop_index_{i}",
                "WindowExclusion",
                {
                    "candidates_hex": [x.hex() for x in six],
                    "already_paid_hex": [x.hex() for x in paid],
                },
                {"surviving_hex": [x.hex() for x in surv], "dropped": 1},
            ),
            coq_enforce=True,
        )

    # Sequential no-double-win: after paying winner, it is absent from the next F.
    seq_cands = [txid(200 + i) for i in range(4)]
    paid_seq: list = []
    for step in range(4):
        surv = spec.exclude_already_paid(seq_cands, paid_seq)
        add(
            v(
                f"A9C-SEQ-{step:03d}",
                f"seq_step_{step}",
                "WindowExclusion",
                {
                    "candidates_hex": [x.hex() for x in seq_cands],
                    "already_paid_hex": [x.hex() for x in paid_seq],
                },
                {"surviving_hex": [x.hex() for x in surv], "dropped": step},
            ),
            coq_enforce=True,
        )
        if surv:
            paid_seq = paid_seq + [surv[0]]

    # Reorg: a side branch does not inherit the other branch's already_paid.
    branch_cands = [txid(300 + i) for i in range(5)]
    paid_a = [branch_cands[0]]
    paid_b: list = []
    surv_a = spec.exclude_already_paid(branch_cands, paid_a)
    surv_b = spec.exclude_already_paid(branch_cands, paid_b)
    add(
        v(
            "REORG-A9C-000",
            "branch_a_paid",
            "WindowExclusion",
            {
                "candidates_hex": [x.hex() for x in branch_cands],
                "already_paid_hex": [x.hex() for x in paid_a],
            },
            {"surviving_hex": [x.hex() for x in surv_a], "dropped": 1},
        ),
        coq_enforce=True,
    )
    add(
        v(
            "REORG-A9C-001",
            "branch_b_unpaid",
            "WindowExclusion",
            {
                "candidates_hex": [x.hex() for x in branch_cands],
                "already_paid_hex": [x.hex() for x in paid_b],
            },
            {"surviving_hex": [x.hex() for x in surv_b], "dropped": 0},
        ),
        coq_enforce=True,
    )
    assert surv_a != surv_b

    # Window-edge draw heights (σ seed + payout height). Coq-unchecked (SHA-256).
    for i, height in enumerate([1, 2, 31, 32, 33, 63, 64, 100000, 2100000]):
        txs = [H64("a"), H64("b")]
        fees = [spec.MIN_CONVERGENCE_AMOUNT, spec.MIN_CONVERGENCE_AMOUNT]
        add(
            v(
                f"WIN-H-{i:03d}",
                f"height_{height}",
                "RunSelection",
                {
                    "txids_hex": txs,
                    "fees": fees,
                    "height": height,
                    "prev_block_hash_hex": H64("1"),
                    "T_hex": T_MAX_HEX,
                },
                run_sel(txs, fees, height, H64("1"), T_MAX_HEX),
            ),
            coq_enforce=False,
        )

    # Reorg/bind: flipping the parent hash must move σ (already RP-*); extra parents.
    for i, prev in enumerate(["11" * 32, "22" * 32, "ff" * 32, "00" * 32, "a5" * 32]):
        prev_b = bytes.fromhex(prev)
        sig = spec.compute_pcobr_hash(txid(0), prev_b, 50, bytes(32))
        add(
            v(
                f"SIG-REORG-{i:03d}",
                f"parent_{i}",
                "SigmaPoW01",
                {
                    "txid_hex": txid(0).hex(),
                    "prev_block_hash_hex": prev,
                    "height": 50,
                    "convergence_root_hex": bytes(32).hex(),
                },
                {"sigma": sig.hex()},
            ),
            coq_enforce=False,
        )

    # Boundary weights around T * f vs 2^256. Production T at min / sat / over-sat.
    Tprod = spec.RES_PROBABILITY_TARGET_INT
    fmin = spec.MIN_CONVERGENCE_AMOUNT
    sat = (1 << 256) // Tprod  # first f where T*f may reach 2^256
    boundary_fs = [
        1,
        fmin - 1,
        fmin,
        fmin + 1,
        fmin * 2,
        spec.CONVERGENCE_MINT_COST,
        fmin * spec.CONVERGENCE_DRAW_WINDOW,
        max(1, sat - 1),
        sat,
        sat + 1 if sat + 1 > 0 else sat,
    ]
    h_low = spec.hash_hex_to_uint256_bytes("01" + "00" * 31)
    h_high = spec.hash_hex_to_uint256_bytes("ff" * 32)
    inq_n = 4
    for f in boundary_fs:
        for tag, hhex, hb in (("low", "01" + "00" * 31, h_low), ("high", "ff" * 32, h_high)):
            add(
                v(
                    f"INQ-B-{inq_n:03d}",
                    f"boundary_{tag}_f{f}",
                    "Inequality",
                    {"h_hex": hhex, "T_hex": t_hex(Tprod), "f": f},
                    {"eligible": spec.check_inequality(hb, Tprod, f)},
                ),
                coq_enforce=True,
            )
            inq_n += 1

    # Extra convergence-root sizes and a permutation family.
    for n in [4, 6, 7, 9, 10, 16, 24, 32]:
        ids = [txid(j + 500) for j in range(n)]
        root = spec.compute_convergence_root_from_txids(ids)
        add(
            v(
                f"CR-N-{n:03d}",
                f"root_n{n}",
                "ConvergenceRoot",
                {"txids_hex": [x.hex() for x in ids]},
                {"convergence_root": root.hex()},
            ),
            coq_enforce=False,
        )
        shuffled = list(reversed(ids))
        add(
            v(
                f"CR-P-{n:03d}",
                f"root_n{n}_rev",
                "ConvergenceRoot",
                {"txids_hex": [x.hex() for x in shuffled]},
                {"convergence_root": root.hex()},
            ),
            coq_enforce=False,
        )

    # Deterministic extra σ samples (hash-kernel agreement surface).
    for i in range(40):
        h = txid(1000 + i)
        prev = txid(2000 + i)
        c = txid(3000 + i) if i % 3 else bytes(32)
        height = 1 + i * 17
        sig = spec.compute_pcobr_hash(h, prev, height, c)
        add(
            v(
                f"SIG-X-{i:03d}",
                f"sigma_extra_{i}",
                "SigmaPoW01",
                {
                    "txid_hex": h.hex(),
                    "prev_block_hash_hex": prev.hex(),
                    "height": height,
                    "convergence_root_hex": c.hex(),
                },
                {"sigma": sig.hex()},
            ),
            coq_enforce=False,
        )

    # Extra winner-selection shapes (weights, ties, empty-adjacent).
    extra_wins = [
        ([H64("1"), H64("2"), H64("3")], [1, 1, 1], 11, H64("a")),
        ([H64("1"), H64("2"), H64("3")], [1, 100, 1], 12, H64("a")),
        ([H64("0"), H64("f")], [fmin, fmin], 13, H64("b")),
        ([H64("c") * 2], [fmin], 14, H64("d")),  # wait that's 2 chars * 2
    ]
    # Fix singleton hex
    extra_wins[3] = ([H64("c")], [fmin], 14, H64("d"))
    extra_wins.extend(
        [
            ([H64("1"), H64("2"), H64("3"), H64("4")], [10, 10, 10, 10], 15, H64("e")),
            ([H64("a"), H64("b"), H64("c")], [fmin, fmin * 10, fmin * 100], 16, H64("f")),
            ([], [], 17, H64("1")),
            ([H64("9")], [1], 18, H64("8")),
        ]
    )
    for i, (txs, fees, height, prev) in enumerate(extra_wins):
        add(
            v(
                f"WIN-X-{i:03d}",
                f"win_extra_{i}",
                "RunSelection",
                {
                    "txids_hex": txs,
                    "fees": fees,
                    "height": height,
                    "prev_block_hash_hex": prev,
                    "T_hex": T_MAX_HEX,
                },
                run_sel(txs, fees, height, prev, T_MAX_HEX),
            ),
            coq_enforce=False,
        )

    # Incomplete-window documentation vectors (no draw). Checked as WindowIncomplete.
    add(
        v(
            "WIN-INCOMPLETE-000",
            "incomplete_is_none",
            "WindowIncomplete",
            {"complete": False, "n_candidates": 3},
            {"draw": None},
        ),
        coq_enforce=False,
    )
    add(
        v(
            "WIN-INCOMPLETE-001",
            "genesis_empty_complete",
            "WindowIncomplete",
            {"complete": True, "n_candidates": 0},
            {"draw": "empty_ok"},
        ),
        coq_enforce=False,
    )

    # Encoder field extremes (layout only in Coq; digest checked in Python/C++).
    for i, (tag, tx, cr, pr, height) in enumerate(
        [
            ("all_zero", bytes(32), bytes(32), bytes(32), 0),
            ("all_ff", b"\xff" * 32, b"\xff" * 32, b"\xff" * 32, 0xFFFFFFFF),
            ("zero_txid_ff_croot", bytes(32), b"\xff" * 32, b"\xaa" * 32, 1),
        ]
    ):
        pre = spec.encode_sigma_preimage(tx, pr, height, cr)
        add(
            v(
                f"ENC-Z-{i:03d}",
                tag,
                "SigmaEncoder",
                {
                    "txid_hex": tx.hex(),
                    "convergence_root_hex": cr.hex(),
                    "prev_block_hash_hex": pr.hex(),
                    "height": height,
                },
                {
                    "preimage_hex": pre.hex(),
                    "length": 100,
                    "txid": pre[0:32].hex(),
                    "croot": pre[32:64].hex(),
                    "prev": pre[64:96].hex(),
                    "height_le": pre[96:100].hex(),
                    "sigma": spec.sha256d(pre).hex(),
                },
            ),
            coq_enforce=True,
        )

    # Duplicate candidates: both copies survive unless the txid is paid.
    dup_ids = [txid(700), txid(700), txid(701)]
    for i, (tag, paid) in enumerate([("dup_unpaid", []), ("dup_paid", [txid(700)])]):
        surv = spec.exclude_already_paid(dup_ids, paid)
        add(
            v(
                f"A9C-DUP-{i:03d}",
                tag,
                "WindowExclusion",
                {
                    "candidates_hex": [x.hex() for x in dup_ids],
                    "already_paid_hex": [x.hex() for x in paid],
                },
                {"surviving_hex": [x.hex() for x in surv], "dropped": len(dup_ids) - len(surv)},
            ),
            coq_enforce=True,
        )

    # Reorg: same F, overlapping already_paid sets (branch-local exclusion).
    reorg_c = [txid(800 + i) for i in range(6)]
    reorg_paid = [
        ("two_prefix", [reorg_c[0], reorg_c[1]]),
        ("overlap_ends", [reorg_c[0], reorg_c[5]]),
        ("middle_only", [reorg_c[2]]),
        ("disjoint_from_window", [txid(9999)]),
    ]
    for i, (tag, paid) in enumerate(reorg_paid):
        surv = spec.exclude_already_paid(reorg_c, paid)
        add(
            v(
                f"REORG-A9C-{2 + i:03d}",
                tag,
                "WindowExclusion",
                {
                    "candidates_hex": [x.hex() for x in reorg_c],
                    "already_paid_hex": [x.hex() for x in paid],
                },
                {"surviving_hex": [x.hex() for x in surv], "dropped": len(reorg_c) - len(surv)},
            ),
            coq_enforce=True,
        )

    # After three sequential pays on branch A, a reorg to unpaid restores F.
    restore_ids = [txid(900 + i) for i in range(5)]
    paid_restore: list = []
    for _ in range(3):
        surv_r = spec.exclude_already_paid(restore_ids, paid_restore)
        if surv_r:
            paid_restore = paid_restore + [surv_r[0]]
    surv_a = spec.exclude_already_paid(restore_ids, paid_restore)
    surv_b = spec.exclude_already_paid(restore_ids, [])
    add(
        v(
            "REORG-A9C-006",
            "branch_a_three_paid",
            "WindowExclusion",
            {
                "candidates_hex": [x.hex() for x in restore_ids],
                "already_paid_hex": [x.hex() for x in paid_restore],
            },
            {"surviving_hex": [x.hex() for x in surv_a], "dropped": 3},
        ),
        coq_enforce=True,
    )
    add(
        v(
            "REORG-A9C-007",
            "branch_b_restored",
            "WindowExclusion",
            {
                "candidates_hex": [x.hex() for x in restore_ids],
                "already_paid_hex": [],
            },
            {"surviving_hex": [x.hex() for x in surv_b], "dropped": 0},
        ),
        coq_enforce=True,
    )
    assert surv_a != surv_b
    assert surv_b == restore_ids

    # Tightening 2026-08-18: small high-risk window/reorg/already_paid/eligibility edges.
    # Do not grow this block into another bulk campaign.
    surv_repay = spec.exclude_already_paid(restore_ids, [restore_ids[0]])
    add(
        v(
            "REORG-A9C-008",
            "restored_then_repay_first",
            "WindowExclusion",
            {
                "candidates_hex": [x.hex() for x in restore_ids],
                "already_paid_hex": [restore_ids[0].hex()],
            },
            {"surviving_hex": [x.hex() for x in surv_repay], "dropped": 1},
        ),
        coq_enforce=True,
    )
    assert restore_ids[0] not in surv_repay

    branch_a_paid = [reorg_c[0], reorg_c[1]]
    branch_b_paid = [reorg_c[4], reorg_c[5]]
    surv_fork_a = spec.exclude_already_paid(reorg_c, branch_a_paid)
    surv_fork_b = spec.exclude_already_paid(reorg_c, branch_b_paid)
    add(
        v(
            "REORG-A9C-009",
            "fork_a_prefix_paid",
            "WindowExclusion",
            {
                "candidates_hex": [x.hex() for x in reorg_c],
                "already_paid_hex": [x.hex() for x in branch_a_paid],
            },
            {"surviving_hex": [x.hex() for x in surv_fork_a], "dropped": 2},
        ),
        coq_enforce=True,
    )
    add(
        v(
            "REORG-A9C-010",
            "fork_b_suffix_paid",
            "WindowExclusion",
            {
                "candidates_hex": [x.hex() for x in reorg_c],
                "already_paid_hex": [x.hex() for x in branch_b_paid],
            },
            {"surviving_hex": [x.hex() for x in surv_fork_b], "dropped": 2},
        ),
        coq_enforce=True,
    )
    assert surv_fork_a != surv_fork_b

    w32 = [txid(1100 + i) for i in range(spec.CONVERGENCE_DRAW_WINDOW)]
    assert len(w32) == 32
    for tag, paid, drop in (
        ("drop_first", [w32[0]], 1),
        ("drop_last", [w32[-1]], 1),
        ("all_paid", w32, 32),
    ):
        surv_w = spec.exclude_already_paid(w32, paid)
        add(
            v(
                f"A9C-W32-{tag}",
                tag,
                "WindowExclusion",
                {
                    "candidates_hex": [x.hex() for x in w32],
                    "already_paid_hex": [x.hex() for x in paid],
                },
                {"surviving_hex": [x.hex() for x in surv_w], "dropped": drop},
            ),
            coq_enforce=True,
        )
    perm_cands = list(reversed(seq_cands))
    paid_perm = [seq_cands[0]]
    surv_perm = spec.exclude_already_paid(perm_cands, paid_perm)
    add(
        v(
            "A9C-NDW-000",
            "paid_excluded_under_permutation",
            "WindowExclusion",
            {
                "candidates_hex": [x.hex() for x in perm_cands],
                "already_paid_hex": [x.hex() for x in paid_perm],
            },
            {"surviving_hex": [x.hex() for x in surv_perm], "dropped": 1},
        ),
        coq_enforce=True,
    )
    assert seq_cands[0] not in surv_perm

    add(
        v(
            "INQ-Z-000",
            "weight_zero_ineligible",
            "Inequality",
            {"h_hex": "01" + "00" * 31, "T_hex": t_hex(Tprod), "f": 0},
            {"eligible": spec.check_inequality(h_low, Tprod, 0)},
        ),
        coq_enforce=True,
    )
    assert spec.check_inequality(h_low, Tprod, 0) is False

    pre_w = spec.encode_sigma_preimage(enc_txid, enc_prev, spec.CONVERGENCE_DRAW_WINDOW, enc_c)
    add(
        v(
            "ENC-W-000",
            "encoder_height_window_w",
            "SigmaEncoder",
            {
                "txid_hex": enc_txid.hex(),
                "convergence_root_hex": enc_c.hex(),
                "prev_block_hash_hex": enc_prev.hex(),
                "height": spec.CONVERGENCE_DRAW_WINDOW,
            },
            {
                "preimage_hex": pre_w.hex(),
                "length": 100,
                "txid": pre_w[0:32].hex(),
                "croot": pre_w[32:64].hex(),
                "prev": pre_w[64:96].hex(),
                "height_le": pre_w[96:100].hex(),
                "sigma": spec.sha256d(pre_w).hex(),
            },
        ),
        coq_enforce=True,
    )

    # Hash-kernel length checks: empty / 1 byte / padding edges / 3-block.
    import hashlib as _hl

    def sha256d_py(data: bytes) -> str:
        return _hl.sha256(_hl.sha256(data).digest()).hexdigest()

    for i, payload in enumerate([b"", b"\x00", b"\xff" * 55, b"\x01" * 100, b"\x02" * 200, bytes(range(256))]):
        add(
            v(
                f"HASH-{i:03d}",
                f"sha256d_len{len(payload)}",
                "HashKernel",
                {"payload_hex": payload.hex()},
                {"sha256d": sha256d_py(payload)},
            ),
            # Empty is one compression pair; keep it in the Coq harness.
            coq_enforce=(i == 0),
        )

    add(
        v(
            "HASH-ABC-000",
            "sha256d_abc",
            "HashKernel",
            {"payload_hex": b"abc".hex()},
            {"sha256d": sha256d_py(b"abc")},
        ),
        coq_enforce=True,
    )

    for i, n in enumerate([63, 65]):
        payload = b"a" * n
        add(
            v(
                f"HASH-PAD-{i:03d}",
                f"sha256d_pad{n}",
                "HashKernel",
                {"payload_hex": payload.hex()},
                {"sha256d": sha256d_py(payload)},
            ),
            coq_enforce=False,
        )

    # FIPS 180-4 / NIST short SHA-256 (single). Padding edges 55/56/63/64/65.
    fips_msgs = [
        ("empty", b""),
        ("abc", b"abc"),
        ("pad55", b"a" * 55),
        ("pad56", b"a" * 56),
        ("block64", b"\x00" * 64),
        ("nist56", b"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
        ("pad63", b"a" * 63),
        ("pad65", b"a" * 65),
    ]
    for i, (tag, payload) in enumerate(fips_msgs):
        add(
            v(
                f"FIPS-{i:03d}",
                tag,
                "Sha256Fips",
                {"payload_hex": payload.hex()},
                {"sha256": _hl.sha256(payload).hexdigest()},
            ),
            # CanonicalHashKernel.v already vm_computes these; keep the harness cheap.
            coq_enforce=False,
        )

    # Bulk fill: deterministic extra draws, roots, and exclusion pairs.
    for i in range(60):
        n = 2 + (i % 6)
        ids = [txid(4000 + i * 10 + j) for j in range(n)]
        fees = [spec.MIN_CONVERGENCE_AMOUNT * (1 + (i + j) % 5) for j in range(n)]
        txs = [x.hex() for x in ids]
        prev = txid(5000 + i).hex()
        add(
            v(
                f"WIN-Y-{i:03d}",
                f"win_bulk_{i}",
                "RunSelection",
                {
                    "txids_hex": txs,
                    "fees": fees,
                    "height": 20 + i,
                    "prev_block_hash_hex": prev,
                    "T_hex": T_MAX_HEX,
                },
                run_sel(txs, fees, 20 + i, prev, T_MAX_HEX),
            ),
            coq_enforce=False,
        )
    for i in range(20):
        ids = [txid(6000 + i * 3 + j) for j in range(3)]
        paid = [ids[i % 3]]
        surv = spec.exclude_already_paid(ids, paid)
        add(
            v(
                f"A9C-Y-{i:03d}",
                f"a9c_bulk_{i}",
                "WindowExclusion",
                {
                    "candidates_hex": [x.hex() for x in ids],
                    "already_paid_hex": [x.hex() for x in paid],
                },
                {"surviving_hex": [x.hex() for x in surv], "dropped": 1},
            ),
            coq_enforce=True,
        )
    for i in range(20):
        payload = bytes([(i + k) % 256 for k in range(16 + i)])
        add(
            v(
                f"HASH-X-{i:03d}",
                f"sha256d_var_{i}",
                "HashKernel",
                {"payload_hex": payload.hex()},
                {"sha256d": spec.sha256d(payload).hex()},
            ),
            coq_enforce=False,
        )

    # -------------------------------------------------------------------------
    # TASK-FV-VECTOR-NEGATIVE-CORE-001: reject paths for dual-pool / mint-shape /
    # hard cap / H-1. Cheap Coq vm_compute (no SHA-256). Every row must be
    # rejected by both the Python oracle and Kernel/NegativeCoreHarness.v.
    # -------------------------------------------------------------------------
    def coin(
        cid: int, vout: int, value: int, asset: str, script: bytes | None = None
    ) -> Dict[str, Any]:
        d: Dict[str, Any] = {"id": cid, "vout": vout, "value": value, "asset": asset}
        if script is not None:
            d["script_hex"] = script.hex()
        return d

    def spend(cid: int, vout: int = 0) -> Dict[str, Any]:
        return {"id": cid, "vout": vout}

    def out(value: int, asset: str, script: bytes | None = None) -> Dict[str, Any]:
        d: Dict[str, Any] = {"value": value, "asset": asset}
        if script is not None:
            d["script_hex"] = script.hex()
        return d

    def mk_tx(tid: int, spends: List[Dict[str, Any]], creates: List[Dict[str, Any]]) -> Dict[str, Any]:
        return {"id": tid, "spends": spends, "creates": creates}

    def add_neg(vid: str, name: str, inp: Dict[str, Any], note: str) -> None:
        exp = spec.eval_negative_core(inp)
        if exp["accepted"]:
            raise SystemExit(f"{vid} unexpectedly accepted by L1 oracle")
        add(v(vid, name, "NegativeCore", inp, exp, note=note), coq_enforce=True)

    w0 = spec.get_res_mint_reward(1)
    w_epoch1 = spec.get_res_mint_reward(spec.RES_REWARD_EPOCH)
    h_after_sched = (spec.RES_REWARD_LAST_EPOCH + 1) * spec.RES_REWARD_EPOCH
    assert w0 == spec.RES_REWARD_GENESIS
    assert w_epoch1 != w0 and w_epoch1 > 0
    assert spec.get_res_mint_reward(h_after_sched) == 0

    add_neg(
        "NEG-000",
        "ordinary_res_two_out_inflation",
        {
            "kind": "dual_pool_tx",
            "height": 1,
            "utxo": [coin(1, 0, 100, "RES")],
            "tx": mk_tx(10, [spend(1)], [out(60, "RES"), out(50, "RES")]),
        },
        "ordinary transfer: 100 RES in, 110 out across two outputs",
    )
    add_neg(
        "NEG-001",
        "ordinary_res_amount_plus_one",
        {
            "kind": "dual_pool_tx",
            "height": 1,
            "utxo": [coin(1, 0, 100, "RES")],
            "tx": mk_tx(10, [spend(1)], [out(101, "RES")]),
        },
        "ordinary transfer: res_out = res_in + 1",
    )
    add_neg(
        "NEG-002",
        "oru_only_creates_res",
        {
            "kind": "dual_pool_tx",
            "height": 1,
            "utxo": [coin(2, 0, 50, "ORU")],
            "tx": mk_tx(10, [spend(2)], [out(1, "RES")]),
        },
        "ORU-only inputs cannot create RES except via mint-shape of W(h)",
    )
    add_neg(
        "NEG-003",
        "res_only_creates_oru",
        {
            "kind": "dual_pool_tx",
            "height": 1,
            "utxo": [coin(1, 0, 100, "RES")],
            "tx": mk_tx(10, [spend(1)], [out(100, "ORU")]),
        },
        "RES-only inputs cannot create ORU (no_res_to_oru)",
    )
    add_neg(
        "NEG-004",
        "mint_W_plus_one",
        {
            "kind": "mint_shape",
            "height": 1,
            "utxo": [coin(2, 0, 50, "ORU")],
            "tx": mk_tx(10, [spend(2)], [out(w0 + 1, "RES")]),
        },
        "ORU-funded mint of W(h)+1",
    )
    add_neg(
        "NEG-005",
        "mint_W_minus_one",
        {
            "kind": "mint_shape",
            "height": 1,
            "utxo": [coin(2, 0, 50, "ORU")],
            "tx": mk_tx(10, [spend(2)], [out(w0 - 1, "RES")]),
        },
        "ORU-funded mint of W(h)-1",
    )
    add_neg(
        "NEG-006",
        "mint_two_res_outputs",
        {
            "kind": "mint_shape",
            "height": 1,
            "utxo": [coin(2, 0, 50, "ORU")],
            "tx": mk_tx(10, [spend(2)], [out(w0 // 2, "RES"), out(w0 - w0 // 2, "RES")]),
        },
        "mint-shape requires exactly one RES output; two outputs summing to W(h) still fail",
    )
    add_neg(
        "NEG-007",
        "mint_genesis_W_at_epoch_1",
        {
            "kind": "mint_shape",
            "height": spec.RES_REWARD_EPOCH,
            "utxo": [coin(2, 0, 50, "ORU")],
            "tx": mk_tx(10, [spend(2)], [out(w0, "RES")]),
        },
        "epoch-1 mint of W(0) rather than W(k=1)",
    )
    add_neg(
        "NEG-008",
        "winner_mint_count_two",
        {
            "kind": "connect_obligation",
            "height": 1,
            "draw_won": True,
            "mint_count": 2,
        },
        "H-1 / winner-mismatch: scored mint count is 2, not 1",
    )
    add_neg(
        "NEG-009",
        "winner_omits_mint",
        {
            "kind": "connect_obligation",
            "height": 1,
            "draw_won": True,
            "mint_count": 0,
        },
        "H-1: winner with W(h)>0 and mint_count=0",
    )
    add_neg(
        "NEG-010",
        "cap_one_atom_overshoot",
        {
            "kind": "connect_cap",
            "height": 1,
            "res_supply": spec.RES_MAX_SUPPLY - w0 + 1,
            "draw_won": True,
            "mint_count": 1,
        },
        "supply + W(h) = cap + 1; equal-to-cap is admitted, one atom over is not",
    )
    add_neg(
        "NEG-011",
        "at_cap_then_mint",
        {
            "kind": "connect_cap",
            "height": 1,
            "res_supply": spec.RES_MAX_SUPPLY,
            "draw_won": True,
            "mint_count": 1,
        },
        "exactly at the hard cap, then mint W(h)",
    )
    add_neg(
        "NEG-012",
        "schedule_exhausted_mint",
        {
            "kind": "connect_obligation",
            "height": h_after_sched,
            "draw_won": True,
            "mint_count": 1,
        },
        "epoch 251: W(h)=0 so a mint is unauthorized, not winner-mismatch",
    )
    add_neg(
        "NEG-013",
        "without_winner_mint",
        {
            "kind": "connect_obligation",
            "height": 1,
            "draw_won": False,
            "mint_count": 1,
        },
        "no draw winner, but a mint is present",
    )
    add_neg(
        "NEG-014",
        "mixed_ordinary_plus_illegal_mint",
        {
            "kind": "apply_block",
            "height": 1,
            "res_supply": 0,
            "draw_won": True,
            "mint_count": 1,
            "utxo": [coin(1, 0, 100, "RES"), coin(2, 0, 50, "ORU")],
            "txs": [
                mk_tx(10, [spend(1)], [out(90, "RES")]),
                mk_tx(11, [spend(2)], [out(w0 + 1, "RES")]),
            ],
        },
        "valid ordinary transfer plus W(h)+1 mint in the same body; apply_block = None",
    )
    add_neg(
        "NEG-015",
        "mixed_inflation_plus_mint_shape",
        {
            "kind": "apply_block",
            "height": 1,
            "res_supply": 0,
            "draw_won": True,
            "mint_count": 1,
            "utxo": [coin(1, 0, 100, "RES"), coin(2, 0, 50, "ORU")],
            "txs": [
                mk_tx(10, [spend(1)], [out(101, "RES")]),
                mk_tx(11, [spend(2)], [out(w0, "RES")]),
            ],
        },
        "ordinary inflation plus a well-shaped mint; body still rejected",
    )

    # -------------------------------------------------------------------------
    # TASK-FV-VECTOR-INVALID-CONVERGENCE-ROOT-001: Invalid Convergence tx
    # (abstract CVG2 form), malformed C / preimage, duplicate txids in F, and
    # Wrong-C claims, including committing zeros / DCR(empty) as C. Complementary to
    # DCR-024..035 (which lock DCR tamper/dup/empty/σ-perturb). Coq-enforced.
    # -------------------------------------------------------------------------
    cvg2 = spec.canonical_cvg2_script()
    res2 = bytes([spec.OP_1, spec.OP_PUSH32]) + spec.RES2_TAG + bytes(28)
    p2wpkh = bytes([spec.OP_0, spec.OP_PUSH20]) + bytes(20)
    amin = spec.MIN_CONVERGENCE_AMOUNT

    def cvg_out(value: int, asset: str, script: bytes) -> Dict[str, Any]:
        return {"value": value, "asset": asset, "script_hex": script.hex()}

    def add_icvg(vid: str, name: str, inp: Dict[str, Any], note: str) -> None:
        exp = spec.eval_invalid_cvg(inp)
        if exp["accepted"]:
            raise SystemExit(f"{vid} unexpectedly accepted by L1 oracle")
        add(v(vid, name, "InvalidCvg", inp, exp, note=note), coq_enforce=True)

    add_icvg(
        "ICVG-000",
        "amount_below_min",
        {
            "kind": "cvg_tx",
            "outs": [cvg_out(amin - 1, "ORU", cvg2)],
        },
        "CVG2/ORU output of MIN-1 sat; IsConvergenceTx may hold (nValue>0) but F excludes it",
    )
    add_icvg(
        "ICVG-001",
        "amount_zero",
        {
            "kind": "cvg_tx",
            "outs": [cvg_out(0, "ORU", cvg2)],
        },
        "CVG2 output with nValue=0 is not counted (C++ nValue>0); not a convergence tx",
    )
    add_icvg(
        "ICVG-002",
        "wrong_asset_res",
        {
            "kind": "cvg_tx",
            "outs": [cvg_out(amin, "RES", cvg2)],
        },
        "abstract: CVG2 container tagged RES; Convergence is ORU-domain",
    )
    add_icvg(
        "ICVG-003",
        "script_res2_not_cvg2",
        {
            "kind": "cvg_tx",
            "outs": [cvg_out(amin, "ORU", res2)],
        },
        "Witness v1 / 32 / RES2 prefix is not CVG2 (abstract form)",
    )
    add_icvg(
        "ICVG-004",
        "script_p2wpkh_not_cvg2",
        {
            "kind": "cvg_tx",
            "outs": [cvg_out(amin, "ORU", p2wpkh)],
        },
        "Witness v0 P2WPKH-shaped script is not a Convergence output",
    )
    add_icvg(
        "ICVG-005",
        "two_cvg2_outputs",
        {
            "kind": "cvg_tx",
            "outs": [cvg_out(amin, "ORU", cvg2), cvg_out(amin, "ORU", cvg2)],
        },
        "IsConvergenceTx requires exactly one CVG2-positive output",
    )

    icvg_a, icvg_b = txid(0), txid(1)
    honest_c = spec.compute_convergence_root_from_txids([icvg_a, icvg_b])
    flipped_c = bytearray(honest_c)
    flipped_c[-1] ^= 1
    add_icvg(
        "ICVG-006",
        "committed_c_bitflip",
        {
            "kind": "committed_c",
            "txids_hex": [icvg_a.hex(), icvg_b.hex()],
            "claimed_hex": bytes(flipped_c).hex(),
        },
        "neg: 1-bit-flipped claimed C ≠ recomputed Convergence_Root (complement DCR-033)",
    )

    sorted_ab = sorted([icvg_a, icvg_b])
    unsorted_ab = list(reversed(sorted_ab))
    unsorted_payload = b"".join(unsorted_ab)
    if unsorted_payload == b"".join(sorted_ab):
        raise SystemExit("ICVG-007 unsorted payload collapsed to sorted concat")
    add_icvg(
        "ICVG-007",
        "unsorted_concat",
        {
            "kind": "malformed_preimage",
            "mode": "unsorted",
            "txids_hex": [icvg_a.hex(), icvg_b.hex()],
            "bad_payload_hex": unsorted_payload.hex(),
        },
        "neg: hashing presentation order without sort ≠ C (CR-010 pins perm of sorted concat)",
    )
    truncated_payload = b"".join(sorted_ab)[:-32]
    add_icvg(
        "ICVG-008",
        "truncated_concat",
        {
            "kind": "malformed_preimage",
            "mode": "truncated",
            "txids_hex": [icvg_a.hex(), icvg_b.hex()],
            "bad_payload_hex": truncated_payload.hex(),
        },
        "neg: dropping the last txid from the sorted concat changes the digest",
    )
    add_icvg(
        "ICVG-009",
        "claimed_root_len_31",
        {
            "kind": "malformed_length",
            "txids_hex": [],
            "claimed_hex": bytes(31).hex(),
        },
        "neg: claimed C of length 31; honest empty C is 32 zero bytes",
    )
    extra_payload = b"".join(sorted_ab) + b"\x00"
    add_icvg(
        "ICVG-010",
        "extra_byte_concat",
        {
            "kind": "malformed_preimage",
            "mode": "extra_byte",
            "txids_hex": [icvg_a.hex(), icvg_b.hex()],
            "bad_payload_hex": extra_payload.hex(),
        },
        "neg: appending one byte to the sorted concat changes the digest",
    )
    add_icvg(
        "ICVG-011",
        "duplicate_txid_not_nlt2",
        {
            "kind": "dup_txid",
            "txids_hex": [icvg_a.hex(), icvg_a.hex()],
            "collapsed_hex": [icvg_a.hex()],
        },
        "neg: C([a,a]) hashes both copies (N=2); not Hash256(a) = C([a]) (complement DCR-029)",
    )
    empty_dcr = spec.convergence_draw_eligible_root([], [])
    add_icvg(
        "ICVG-012",
        "empty_claimed_dcr_as_c",
        {
            "kind": "nlt2_claim",
            "mode": "empty_claimed_dcr",
            "txids_hex": [],
            "claimed_hex": empty_dcr.hex(),
        },
        "neg: committing DCR(empty)=SHA-256d([]) as C of N=0; honest C is zeros (complement DCR-030)",
    )
    add_icvg(
        "ICVG-013",
        "singleton_claimed_zeros_as_c",
        {
            "kind": "nlt2_claim",
            "mode": "singleton_claimed_zeros",
            "txids_hex": [icvg_a.hex()],
            "claimed_hex": zeros32.hex(),
        },
        "neg: committing 32 zeros as C of N=1; honest C is Hash256(txid) (complement DCR-031)",
    )

    # -------------------------------------------------------------------------
    # TASK-FV-VECTOR-SCRIPT-AND-MINT-WITNESS-NEG-001: script classification
    # (Witness v0/v1 mix, non-standard templates) and PCORB mint-witness shape.
    # Abstract form only — not an interpreter. Shape subset (P2PKH/P2SH/v2/
    # v1-len20, spend-CVG2, PCORB sizes) is also an apply_tx conjunct
    # (ScriptShape.v). Script-vs-claimed-asset rows stay harness-only.
    # Dual-pool and mint-shape conjuncts on the two cross rows are L1 extras
    # (the monetary halves are already L2-locked by NegativeCore NEG-002 /
    # mint-shape rows).
    # -------------------------------------------------------------------------
    def add_smw(vid: str, name: str, inp: Dict[str, Any], note: str) -> None:
        exp = spec.eval_script_mint_neg(inp)
        if exp["accepted"]:
            raise SystemExit(f"{vid} unexpectedly accepted by L1 oracle")
        if inp["kind"] == "domain_cross" and not exp.get("dual_pool_reject"):
            raise SystemExit(f"{vid} domain_cross must also fail dual-pool")
        if inp["kind"] == "mint_witness_cross" and not exp.get("mint_shape_would_pass"):
            raise SystemExit(f"{vid} mint_witness_cross must be well-shaped mint + bad PCORB")
        add(v(vid, name, "ScriptMintNeg", inp, exp, note=note), coq_enforce=True)

    p2wpkh_spk = spec.canonical_p2wpkh_script()
    p2wsh_spk = spec.canonical_p2wsh_script()
    res2_spk = spec.canonical_res2_script()
    p2pkh_spk = spec.canonical_p2pkh_script()
    p2sh_spk = spec.canonical_p2sh_script()
    v1_20 = spec.canonical_v1_len20_script()
    v2_32 = spec.canonical_v2_32_script()
    cvg2_spk = spec.canonical_cvg2_script()
    pcorb_ok = spec.canonical_pcorb_stack()

    add_smw(
        "SMW-000",
        "v0_p2wpkh_claimed_res",
        {
            "kind": "script_class",
            "script_hex": p2wpkh_spk.hex(),
            "claimed_asset": "RES",
        },
        "L2: Witness v0 P2WPKH is not IsResScript (ORU domain). L3: C++ IsResScript.",
    )
    add_smw(
        "SMW-001",
        "v1_res2_claimed_oru",
        {
            "kind": "script_class",
            "script_hex": res2_spk.hex(),
            "claimed_asset": "ORU",
        },
        "L2: Witness v1 / 32 / RES2 is IsResScript (RES domain, not ORU). L3: C++ IsResScript.",
    )
    add_smw(
        "SMW-002",
        "v1_program_len_20",
        {
            "kind": "script_class",
            "script_hex": v1_20.hex(),
            "claimed_asset": "RES",
        },
        "L2: v1 program length 20 is not IsAllowedScript. L3: bad-rigor-script-type.",
    )
    add_smw(
        "SMW-003",
        "p2pkh_nonstandard",
        {
            "kind": "script_class",
            "script_hex": p2pkh_spk.hex(),
        },
        "L2: P2PKH byte template is not a witness program. L3: bad-rigor-script-type.",
    )
    add_smw(
        "SMW-004",
        "p2sh_nonstandard",
        {
            "kind": "script_class",
            "script_hex": p2sh_spk.hex(),
        },
        "L2: P2SH byte template is not a witness program. L3: bad-rigor-script-type.",
    )
    add_smw(
        "SMW-005",
        "v0_p2wsh_claimed_res",
        {
            "kind": "script_class",
            "script_hex": p2wsh_spk.hex(),
            "claimed_asset": "RES",
        },
        "L2: Witness v0 32-byte (P2WSH) is ORU, not RES. L3: AssetTypeFromScriptPubKey.",
    )
    add_smw(
        "SMW-006",
        "pcorb_five_items",
        {
            "kind": "pcorb_shape",
            "stack_hex": [x.hex() for x in pcorb_ok[:5]],
        },
        "L2: PCORB stack length 5 < 6. L3: ParseConvergeProof fails.",
    )
    add_smw(
        "SMW-007",
        "pcorb_amount_len_7",
        {
            "kind": "pcorb_shape",
            "stack_hex": [pcorb_ok[0].hex(), bytes(7).hex()] + [x.hex() for x in pcorb_ok[2:]],
        },
        "L2: PCORB item[1] (amount) is 7 bytes, not 8. L3: ParseConvergeProof field-size fail.",
    )
    add_smw(
        "SMW-008",
        "witness_v2_32",
        {
            "kind": "script_class",
            "script_hex": v2_32.hex(),
        },
        "L2: unknown witness version 2 is not IsAllowedScript. L3: bad-rigor-script-type.",
    )
    add_smw(
        "SMW-009",
        "spend_cvg2",
        {
            "kind": "spend_cvg2",
            "script_hex": cvg2_spk.hex(),
        },
        "L2: spending a CVG2 scriptPubKey is rejected (abstract form). L3: bad-txns-spend-convergence.",
    )
    add_smw(
        "SMW-010",
        "domain_v0_tagged_res_oru_to_res",
        {
            "kind": "domain_cross",
            "script_hex": p2wpkh_spk.hex(),
            "claimed_asset": "RES",
            "height": 1,
            "utxo": [coin(2, 0, 50, "ORU")],
            "tx": mk_tx(10, [spend(2)], [out(1, "RES")]),
        },
        "CROSS dual-pool: v0-ORU script tagged RES + ORU-in creating RES-out. "
        "L2: not_res_script. L1 extra: dual_pool mint_wrong_amount (NEG-002 family).",
    )
    add_smw(
        "SMW-011",
        "mint_shape_ok_pcorb_five",
        {
            "kind": "mint_witness_cross",
            "height": 1,
            "utxo": [coin(2, 0, 50, "ORU")],
            "tx": mk_tx(10, [spend(2)], [out(w0, "RES")]),
            "stack_hex": [x.hex() for x in pcorb_ok[:5]],
        },
        "CROSS mint-shape: body is W(h) mint-shape (would pass dual-pool) but PCORB has 5 items. "
        "L2: pcorb_too_few. L1 extra: mint_shape_would_pass=true (NEG mint-shape complement).",
    )

    # -------------------------------------------------------------------------
    # TASK-FV-VECTOR-EDGE-AND-CROSS-FAMILY-001: cross-family combination
    # rejects and finer schedule / window / root boundaries. Coq-enforced.
    # Script+dual-pool and schedule+mint/cap land in NegativeCoreHarness
    # (TxAbs). Malformed-root+mint/cap and window+DCR+selection land in
    # GoldenHarness (Types.Tx / ConvergeRoot / WindowExclusion).
    # -------------------------------------------------------------------------
    def add_xfam(vid: str, name: str, inp: Dict[str, Any], note: str) -> None:
        exp = spec.eval_edge_cross(inp)
        if exp["accepted"]:
            raise SystemExit(f"{vid} unexpectedly accepted by L1 oracle")
        kind = inp["kind"]
        if kind in ("script_dual_pool", "spend_cvg2_dual_pool", "pcorb_dual_pool"):
            if not exp.get("dual_pool_reject") or exp.get("shape_reject") is None:
                raise SystemExit(f"{vid} must fail both script-shape and dual-pool")
        if kind == "window_obligation":
            if not exp.get("obligation_reject"):
                raise SystemExit(f"{vid} window_obligation must pin an obligation reject")
        add(v(vid, name, "EdgeCross", inp, exp, note=note), coq_enforce=True)

    h_epoch_last = spec.RES_REWARD_EPOCH - 1
    h_last_pay = (spec.RES_REWARD_LAST_EPOCH + 1) * spec.RES_REWARD_EPOCH - 1
    w_epoch_last = spec.get_res_mint_reward(h_epoch_last)
    w_last_pay = spec.get_res_mint_reward(h_last_pay)
    assert w_epoch_last == w0
    assert w_epoch1 != w0 and w_epoch1 > 0
    assert w_last_pay > 0
    assert spec.get_res_mint_reward(h_after_sched) == 0

    add_xfam(
        "XFAM-000",
        "p2pkh_plus_ordinary_inflation",
        {
            "kind": "script_dual_pool",
            "script_hex": p2pkh_spk.hex(),
            "claimed_asset": "RES",
            "height": 1,
            "utxo": [coin(1, 0, 100, "RES")],
            "tx": mk_tx(10, [spend(1)], [out(101, "RES", p2pkh_spk)]),
        },
        "CROSS script+dual-pool: created P2PKH (bad-rigor-script-type) and "
        "ordinary RES inflation. Both apply_tx conjuncts fail.",
    )
    add_xfam(
        "XFAM-001",
        "witness_v2_plus_oru_to_res",
        {
            "kind": "script_dual_pool",
            "script_hex": v2_32.hex(),
            "claimed_asset": "RES",
            "height": 1,
            "utxo": [coin(2, 0, 50, "ORU")],
            "tx": mk_tx(10, [spend(2)], [out(1, "RES", v2_32)]),
        },
        "CROSS script+dual-pool: witness v2 created script + ORU-in creating "
        "RES-out (mint_wrong_amount). Shape and pooling both reject.",
    )
    add_xfam(
        "XFAM-002",
        "spend_cvg2_plus_inflation",
        {
            "kind": "spend_cvg2_dual_pool",
            "script_hex": cvg2_spk.hex(),
            "height": 1,
            "utxo": [coin(1, 0, 100, "RES", cvg2_spk)],
            "tx": mk_tx(10, [spend(1)], [out(101, "RES")]),
        },
        "CROSS script+dual-pool: spend-CVG2 (bad-txns-spend-convergence) plus "
        "ordinary RES inflation. Spend-shape and dual-pool both reject.",
    )

    add_xfam(
        "XFAM-003",
        "c_bitflip_plus_mint_W_plus_one",
        {
            "kind": "root_mint",
            "height": 1,
            "root": {
                "kind": "committed_c",
                "txids_hex": [icvg_a.hex(), icvg_b.hex()],
                "claimed_hex": bytes(flipped_c).hex(),
            },
            "mint": {
                "kind": "mint_shape",
                "height": 1,
                "utxo": [coin(2, 0, 50, "ORU")],
                "tx": mk_tx(10, [spend(2)], [out(w0 + 1, "RES")]),
            },
        },
        "CROSS root+mint: committed C bitflip (ICVG-006 family) and ORU-funded "
        "mint of W(h)+1 (NEG-004 family). Both halves reject.",
    )
    add_xfam(
        "XFAM-004",
        "claimed_c_len_31_plus_cap_overshoot",
        {
            "kind": "root_cap",
            "height": 1,
            "root": {
                "kind": "malformed_length",
                "txids_hex": [],
                "claimed_hex": bytes(31).hex(),
            },
            "mint": {
                "kind": "connect_cap",
                "height": 1,
                "res_supply": spec.RES_MAX_SUPPLY - w0 + 1,
                "draw_won": True,
                "mint_count": 1,
            },
        },
        "CROSS root+cap: claimed C length 31 (ICVG-009) and one-atom hard-cap "
        "overshoot (NEG-010). Both halves reject.",
    )
    add_xfam(
        "XFAM-005",
        "unsorted_concat_plus_mint_at_cap",
        {
            "kind": "root_cap",
            "height": 1,
            "root": {
                "kind": "malformed_preimage",
                "mode": "unsorted",
                "txids_hex": [icvg_a.hex(), icvg_b.hex()],
                "bad_payload_hex": unsorted_payload.hex(),
            },
            "mint": {
                "kind": "connect_cap",
                "height": 1,
                "res_supply": spec.RES_MAX_SUPPLY,
                "draw_won": True,
                "mint_count": 1,
            },
        },
        "CROSS root+cap: unsorted concat digest ≠ C (ICVG-007) and mint at the "
        "hard cap (NEG-011). Both halves reject.",
    )

    t_max = spec.res_probability_target_from_bytes(bytes.fromhex(T_MAX_HEX))
    xfam_ids3 = [dcr0, dcr1, dcr2]
    xfam_fees3 = [1, 1, 1]
    add_xfam(
        "XFAM-006",
        "window_all_paid_empty_dcr_no_winner",
        {
            "kind": "window_dcr_sel",
            "mode": "empty_after_pay",
            "txids_hex": [x.hex() for x in xfam_ids3],
            "fees": xfam_fees3,
            "already_paid_hex": [x.hex() for x in xfam_ids3],
            "height": 10,
            "prev_block_hash_hex": prev11.hex(),
            "T_hex": T_MAX_HEX,
        },
        "CROSS window+DCR+selection: A-9-C pays the whole F. Survivors empty, "
        "C is empty-set zeros, DCR is SHA-256d([]) ≠ zeros, no winner.",
    )
    add_xfam(
        "XFAM-007",
        "window_drop_to_singleton_hashed",
        {
            "kind": "window_dcr_sel",
            "mode": "singleton_hashes",
            "txids_hex": [dcr0.hex(), dcr1.hex()],
            "fees": [1, 1],
            "already_paid_hex": [dcr1.hex()],
            "height": 10,
            "prev_block_hash_hex": prev11.hex(),
            "T_hex": T_MAX_HEX,
        },
        "CROSS window+DCR+selection: pay one of two. Singleton remainder has "
        "C=DCR=Hash256(survivor) ≠ zeros, and a survivor winner (contrast unpaid 2-set).",
    )
    r_full_019 = spec.run_selection(xfam_ids3, xfam_fees3, 10, prev11, t_max)
    if r_full_019.winner_index is None or r_full_019.winner_index < 0:
        raise SystemExit("XFAM-008 unpaid 3-set unexpectedly has no winner")
    paid_winner = [xfam_ids3[r_full_019.winner_index]]
    add_xfam(
        "XFAM-008",
        "window_drop_winner_moves_dcr_and_sel",
        {
            "kind": "window_dcr_sel",
            "mode": "drop_winner_moves",
            "txids_hex": [x.hex() for x in xfam_ids3],
            "fees": xfam_fees3,
            "already_paid_hex": [paid_winner[0].hex()],
            "height": 10,
            "prev_block_hash_hex": prev11.hex(),
            "T_hex": T_MAX_HEX,
        },
        "CROSS window+DCR+selection: A-9-C drops the unpaid-set winner. "
        "Surviving list, DCR, and winner all move vs the unpaid 3-set "
        "(complements DCR-020 / DCR-032).",
    )

    add_xfam(
        "XFAM-009",
        "epoch_last_height_mint_next_W",
        {
            "kind": "schedule_mint",
            "height": h_epoch_last,
            "neighbour_height": spec.RES_REWARD_EPOCH,
            "mint": {
                "kind": "mint_shape",
                "height": h_epoch_last,
                "utxo": [coin(2, 0, 50, "ORU")],
                "tx": mk_tx(10, [spend(2)], [out(w_epoch1, "RES")]),
            },
        },
        "CROSS schedule+mint-shape: last height of epoch 0 mints W(k=1) "
        "instead of W(0). W jumps at the next height.",
    )
    add_xfam(
        "XFAM-010",
        "W_zero_mint_last_epoch_amount",
        {
            "kind": "schedule_mint",
            "height": h_after_sched,
            "neighbour_height": h_last_pay,
            "mint": {
                "kind": "mint_shape",
                "height": h_after_sched,
                "utxo": [coin(2, 0, 50, "ORU")],
                "tx": mk_tx(10, [spend(2)], [out(w_last_pay, "RES")]),
            },
        },
        "CROSS schedule+mint-shape: first W(h)=0 height mints the last "
        "positive-epoch amount. Unauthorized, not winner-mismatch.",
    )
    add_xfam(
        "XFAM-011",
        "last_positive_W_mint_plus_one_and_cap",
        {
            "kind": "schedule_cap",
            "height": h_last_pay,
            "shape": {
                "kind": "mint_shape",
                "height": h_last_pay,
                "utxo": [coin(2, 0, 50, "ORU")],
                "tx": mk_tx(10, [spend(2)], [out(w_last_pay + 1, "RES")]),
            },
            "cap": {
                "kind": "connect_cap",
                "height": h_last_pay,
                "res_supply": spec.RES_MAX_SUPPLY - w_last_pay + 1,
                "draw_won": True,
                "mint_count": 1,
            },
        },
        "CROSS schedule+mint+cap: last height with W(h)>0, mint of W(h)+1 "
        "and one-atom cap overshoot together.",
    )

    # TASK-FV-VECTOR-SELECTIVE-DENSIFY-001: eight high-value residual crosses.
    # Not added: SHA-256 family extras, IsResScript vs tag, P1-2 wiring of
    # Selection into cm_draw_won, W=0×cap (attempted amount is 0), coinbase bind.
    opreturn_spk = bytes([spec.OP_RETURN])
    add_xfam(
        "XFAM-012",
        "p2sh_plus_ordinary_inflation",
        {
            "kind": "script_dual_pool",
            "script_hex": p2sh_spk.hex(),
            "claimed_asset": "RES",
            "height": 1,
            "utxo": [coin(1, 0, 100, "RES")],
            "tx": mk_tx(10, [spend(1)], [out(101, "RES", p2sh_spk)]),
        },
        "CROSS script-shape+dual-pool: created P2SH (in-model IsAllowedScript "
        "reject) plus ordinary RES inflation. Complements XFAM-000 P2PKH.",
    )
    add_xfam(
        "XFAM-013",
        "v1_len20_plus_ordinary_inflation",
        {
            "kind": "script_dual_pool",
            "script_hex": v1_20.hex(),
            "claimed_asset": "RES",
            "height": 1,
            "utxo": [coin(1, 0, 100, "RES")],
            "tx": mk_tx(10, [spend(1)], [out(101, "RES", v1_20)]),
        },
        "CROSS script-shape+dual-pool: created v1 program length 20 plus "
        "ordinary RES inflation. Complements SMW-002 (script-only).",
    )
    add_xfam(
        "XFAM-014",
        "opreturn_nonzero_plus_inflation",
        {
            "kind": "script_dual_pool",
            "script_hex": opreturn_spk.hex(),
            "claimed_asset": "RES",
            "height": 1,
            "utxo": [coin(1, 0, 100, "RES")],
            "tx": mk_tx(10, [spend(1)], [out(101, "RES", opreturn_spk)]),
        },
        "CROSS script-shape+dual-pool: nonzero-value OP_RETURN (the other "
        "created-script branch besides IsAllowedScript) plus ordinary inflation.",
    )
    pcorb_tx = mk_tx(10, [spend(1)], [out(101, "RES")])
    pcorb_tx["stack_hex"] = [x.hex() for x in pcorb_ok[:5]]
    add_xfam(
        "XFAM-015",
        "pcorb_five_plus_ordinary_inflation",
        {
            "kind": "pcorb_dual_pool",
            "height": 1,
            "utxo": [coin(1, 0, 100, "RES")],
            "tx": pcorb_tx,
        },
        "CROSS PCORB-shape+dual-pool: empty created-script fixture (shape-ok "
        "on creates) but 5-item PCORB plus ordinary inflation. Complements "
        "SMW-011 (well-shaped mint + bad PCORB).",
    )
    add_xfam(
        "XFAM-016",
        "window_empty_plus_without_winner_mint",
        {
            "kind": "window_obligation",
            "mode": "empty_after_pay",
            "txids_hex": [x.hex() for x in xfam_ids3],
            "fees": xfam_fees3,
            "already_paid_hex": [x.hex() for x in xfam_ids3],
            "height": 10,
            "prev_block_hash_hex": prev11.hex(),
            "T_hex": T_MAX_HEX,
            "draw_won": False,
            "mint_count": 1,
        },
        "CROSS window+obligation: A-9-C empties F (no winner, C=0) and a mint "
        "is still claimed (without_winner). Does not wire Selection into "
        "cm_draw_won (P1-2).",
    )
    add_xfam(
        "XFAM-017",
        "window_drop_winner_plus_h1_omit",
        {
            "kind": "window_obligation",
            "mode": "drop_winner_moves",
            "txids_hex": [x.hex() for x in xfam_ids3],
            "fees": xfam_fees3,
            "already_paid_hex": [paid_winner[0].hex()],
            "height": 10,
            "prev_block_hash_hex": prev11.hex(),
            "T_hex": T_MAX_HEX,
            "draw_won": True,
            "mint_count": 0,
        },
        "CROSS window+obligation: A-9-C drops the unpaid-set winner (survivor "
        "winner exists) and mint_count=0 (H-1). Complements XFAM-008 / NEG-009.",
    )
    add_xfam(
        "XFAM-018",
        "epoch_last_mint_plus_one_and_cap",
        {
            "kind": "schedule_cap",
            "height": h_epoch_last,
            "shape": {
                "kind": "mint_shape",
                "height": h_epoch_last,
                "utxo": [coin(2, 0, 50, "ORU")],
                "tx": mk_tx(10, [spend(2)], [out(w_epoch_last + 1, "RES")]),
            },
            "cap": {
                "kind": "connect_cap",
                "height": h_epoch_last,
                "res_supply": spec.RES_MAX_SUPPLY - w_epoch_last + 1,
                "draw_won": True,
                "mint_count": 1,
            },
        },
        "CROSS first-epoch-last+cap: last height of epoch 0, mint of W(0)+1 "
        "and one-atom cap overshoot. Complements XFAM-011 (last paying height).",
    )
    add_xfam(
        "XFAM-019",
        "epoch_start_mint_plus_one_and_cap",
        {
            "kind": "schedule_cap",
            "height": spec.RES_REWARD_EPOCH,
            "shape": {
                "kind": "mint_shape",
                "height": spec.RES_REWARD_EPOCH,
                "utxo": [coin(2, 0, 50, "ORU")],
                "tx": mk_tx(10, [spend(2)], [out(w_epoch1 + 1, "RES")]),
            },
            "cap": {
                "kind": "connect_cap",
                "height": spec.RES_REWARD_EPOCH,
                "res_supply": spec.RES_MAX_SUPPLY - w_epoch1 + 1,
                "draw_won": True,
                "mint_count": 1,
            },
        },
        "CROSS first-epoch-start+cap: first height of epoch 1, mint of W(1)+1 "
        "and one-atom cap overshoot. Complements NEG-007 (wrong-epoch amount only).",
    )

    assert len(vectors) >= 50
    # Fail-closed: never rewrite phase3_golden_vectors.json unless the living
    # lock still describes this set (TASK-FV-DISCREPANCY-AND-LOCK-MAINTENANCE-001).
    lock_path = Path(__file__).parent / "golden_lock.json"
    lock = json.loads(lock_path.read_text(encoding="utf-8"))
    by_mod = Counter(v["module"] for v in vectors)
    if len(vectors) != lock["shared_vectors"]:
        raise SystemExit(
            f"generated {len(vectors)} vectors != golden_lock.json shared_vectors "
            f"{lock['shared_vectors']}; bump the lock only with a new vector task"
        )
    for mod, exp in lock["families"].items():
        got = by_mod.get(mod, 0)
        if got != exp["vectors"]:
            raise SystemExit(
                f"generated {mod} count {got} != golden_lock.json {exp['vectors']}"
            )
    extra = sorted(set(by_mod) - set(lock["families"]))
    if extra:
        raise SystemExit(f"generated unexpected modules {extra}; bump golden_lock.json")
    flag_true = sum(1 for v in vectors if v.get("coq_enforce") is True)
    if flag_true != lock["json_coq_enforce_true"]:
        raise SystemExit(
            f"generated coq_enforce:true = {flag_true} != lock {lock['json_coq_enforce_true']}"
        )
    omit = lock["surface_splits"]["omit_coq_enforce_key_enforced"]
    vec_by_id = {v["id"]: v for v in vectors}
    for vid in omit:
        if vid not in vec_by_id:
            raise SystemExit(f"omit-key enforced id missing from generated JSON: {vid}")
        if "coq_enforce" in vec_by_id[vid]:
            raise SystemExit(f"{vid} must omit coq_enforce (golden_lock surface_splits)")
    for split in lock["surface_splits"]["rows"]:
        for vid in split["ids"]:
            if vid not in vec_by_id:
                raise SystemExit(f"surface_split {vid} missing from generated JSON")
            if vec_by_id[vid]["module"] != split["family"]:
                raise SystemExit(
                    f"surface_split {vid} module {vec_by_id[vid]['module']!r} != {split['family']}"
                )
            if vec_by_id[vid].get("coq_enforce") is False:
                raise SystemExit(f"surface_split {vid} must stay Coq-enforced")
    for fam, fam_spec in lock["must_enforce_families"].items():
        got = by_mod.get(fam, 0)
        if got != fam_spec["vectors"]:
            raise SystemExit(f"must-enforce {fam} has {got} rows, lock wants {fam_spec['vectors']}")

    out = Path(os.environ["GOLDEN_OUT"]) if "GOLDEN_OUT" in os.environ else Path(__file__).parent / "phase3_golden_vectors.json"
    out.write_text(
        json.dumps(
            {
                "version": "phase3-2.2-tightening",
                "baseline": "c05f1a94beece54df872dc89c08715bd555fd987",
                "tag": lock["tag"],
                "count": len(vectors),
                "vectors": vectors,
            },
            indent=2,
        )
        + "\n"
    )
    print(f"Wrote {len(vectors)} vectors -> {out}")


if __name__ == "__main__":
    main()

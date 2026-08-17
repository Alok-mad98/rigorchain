#!/usr/bin/env python3
"""Generate PHASE-3 golden vectors (>=50) aligned with frozen C++ selection tests."""

from __future__ import annotations

import json
import os
import sys
from pathlib import Path
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

    for i, (oru, res, mint) in enumerate([(1000, 0, False), (900, 10000, True), (500, 1_000_000, False), (0, 21000000 * 100, False), (100, 20999999 * 100, True)]):
        cap = 21000000 * 100
        after = res + (10000 if mint and res + 10000 <= cap else 0)
        vectors.append(v(f"SUP-{i:03d}", "supply", "SupplyScaffold", {"ORU": oru, "RES": res, "mint": mint}, {"RES_after": after}))

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
    # a cheap kernel core; Python and C++ consume the full set.
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
        fmin * 65536,
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

    assert len(vectors) >= 50
    out = Path(os.environ["GOLDEN_OUT"]) if "GOLDEN_OUT" in os.environ else Path(__file__).parent / "phase3_golden_vectors.json"
    out.write_text(
        json.dumps(
            {
                "version": "phase3-2.1-hash-kernel",
                "baseline": "c05f1a94beece54df872dc89c08715bd555fd987",
                "tag": "hash-kernel-cpp-cli-2026-08-16",
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

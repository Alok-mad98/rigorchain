#!/usr/bin/env python3
"""Generate PHASE-3 golden vectors (>=50) aligned with frozen C++ selection tests."""

from __future__ import annotations

import json
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

    assert len(vectors) >= 50
    out = Path(__file__).parent / "phase3_golden_vectors.json"
    out.write_text(
        json.dumps(
            {
                "version": "phase3-1.0",
                "baseline": "c05f1a94beece54df872dc89c08715bd555fd987",
                "tag": "mainnet-freeze-rfc015-2026-05-26",
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

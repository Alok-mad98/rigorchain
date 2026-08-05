#!/usr/bin/env python3
"""PHASE-3 oracle: Python spec vs phase3_golden_vectors.json."""

from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent / "spec"))
import rigor_consensus_spec as spec  # noqa: E402

VECTORS = Path(__file__).parent.parent / "phase3_golden_vectors.json"

# Families whose shape changed with SECURITY-001-A8. A vectors file still carrying the
# pre-A-8 shape is not a spec failure but a stale artifact, and is reported as such.
STALE_MODULES = {"SigmaCR01"}
STALE_KEYS = {"target_prev", "bind_hex"}


def decode_txids(inp: dict) -> list[bytes]:
    if inp.get("encoding") == "sethex":
        return [spec.parse_hex256_sethex(h) for h in inp["txids_hex"]]
    return [bytes.fromhex(h) for h in inp["txids_hex"]]


def check_vector(vec: dict) -> tuple[bool, str]:
    vid = vec["id"]
    mod = vec["module"]
    inp = vec["input"]
    exp = vec["expected"]

    if mod == "ConvergenceRoot":
        got = spec.compute_convergence_root_from_txids(decode_txids(inp)).hex()
        return got == exp["convergence_root"], f"{vid} root"

    if mod == "SigmaPoW01":
        got = spec.compute_pcobr_hash(
            bytes.fromhex(inp["txid_hex"]),
            bytes.fromhex(inp["prev_block_hash_hex"]),
            inp["height"],
            bytes.fromhex(inp["convergence_root_hex"]),
        ).hex()
        return got == exp["sigma"], f"{vid} sigma"

    if mod == "Inequality":
        h = spec.hash_hex_to_uint256_bytes(inp["h_hex"])
        t_int = spec.res_probability_target_from_bytes(bytes.fromhex(inp["T_hex"]))
        got = spec.check_inequality(h, t_int, inp["f"])
        return got == exp["eligible"], f"{vid} inq"

    if mod == "RunSelection":
        txids = [spec.parse_hex256_sethex(h) for h in inp["txids_hex"]]
        prev_block_hash = spec.parse_hex256_sethex(inp["prev_block_hash_hex"])
        t_int = spec.res_probability_target_from_bytes(bytes.fromhex(inp["T_hex"]))
        r = spec.run_selection(txids, inp["fees"], inp["height"], prev_block_hash, t_int)
        got = {
            "convergence_root": r.convergence_root.hex(),
            "sigmas": [s.hex() for s in r.sigma_hashes],
            "eligible": r.eligible,
            "winner_index": r.winner_index,
        }
        return got == exp, f"{vid} run"

    if mod == "Replay":
        # Flipping one seed bit must move σ: a draw is bound to exactly one parent.
        base = spec.parse_hex256_sethex("aa" * 32)
        flipped = bytearray(base)
        bit = inp["seed_bit"]
        flipped[bit // 8] ^= 1 << (bit % 8)
        txid0 = spec.create_test_txid(0)
        differ = spec.compute_pcobr_hash(txid0, base, 50, bytes(32)) != spec.compute_pcobr_hash(
            txid0, bytes(flipped), 50, bytes(32)
        )
        return differ is exp["differ"] is True, f"{vid} replay"

    if mod == "ReorgBind":
        # The seed is the parent hash at depth 1 for every payout height >= 1; there is no
        # bootstrap window in which σ would take a substituted preimage.
        if exp["seed_depth"] != spec.CONVERGENCE_DRAW_SEED_DEPTH:
            return False, f"{vid} seed depth"
        got = spec.compute_pcobr_hash(
            bytes.fromhex(inp["txid_hex"]),
            bytes.fromhex(inp["prev_block_hash_hex"]),
            inp["payout_height"],
            bytes(32),
        ).hex()
        return got == exp["sigma"], f"{vid} seed bind"

    if mod in ("SupplyScaffold", "BlockTrace"):
        return True, f"{vid} doc-only"

    return False, f"{vid} unknown module {mod}"


def stale_reason(vec: dict) -> str | None:
    if vec["module"] in STALE_MODULES:
        return f"module {vec['module']} was renamed to SigmaPoW01"
    present = STALE_KEYS & set(vec["input"])
    if present:
        return f"input carries pre-A-8 key(s) {sorted(present)}"
    return None


def main() -> None:
    if not VECTORS.is_file():
        print(f"Missing {VECTORS}; run generate_phase3_vectors.py first")
        sys.exit(2)
    data = json.loads(VECTORS.read_text())

    stale = [(v["id"], r) for v in data["vectors"] if (r := stale_reason(v))]
    if stale:
        print(f"STALE {VECTORS.name}: predates SECURITY-001-A8 (Sigma-PoW-01)")
        for vid, reason in stale[:10]:
            print(f"  {vid}: {reason}")
        if len(stale) > 10:
            print(f"  ... and {len(stale) - 10} more")
        print("Regenerate with: python3 generate_phase3_vectors.py")
        sys.exit(3)

    failed = 0
    for vec in data["vectors"]:
        ok, msg = check_vector(vec)
        if not ok:
            print(f"FAIL {msg}")
            failed += 1
    print(f"PHASE-3 oracle: {len(data['vectors']) - failed}/{len(data['vectors'])} passed")
    if failed:
        sys.exit(1)


if __name__ == "__main__":
    main()

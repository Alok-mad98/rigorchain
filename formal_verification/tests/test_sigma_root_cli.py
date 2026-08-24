#!/usr/bin/env python3
"""Production C++ σ / ConvergenceRoot CLI vs golden JSON families.

Calls extracted/cpp_sigma_root_cli, which links rigor::ComputeSigmaHash and
Consensus::ComputeConvergenceRootFromEligibilityTxids. Finite vector check,
not a refinement proof and not a shared Coq/C++ binary.

Families: SigmaEncoder, SigmaPoW01, ConvergenceRoot.
Does not exercise ComputeConvergenceRootFromConvergenceSet.
"""
from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VEC = ROOT / "phase3_golden_vectors.json"
CLI = ROOT / "extracted" / "cpp_sigma_root_cli"

sys.path.insert(0, str(ROOT / "spec"))
import rigor_consensus_spec as spec  # noqa: E402


def run_cli(args: list[str]) -> list[str]:
    r = subprocess.run(
        [str(CLI), *args],
        check=True,
        capture_output=True,
        text=True,
    )
    lines = [ln.strip().lower() for ln in r.stdout.splitlines() if ln.strip()]
    if not lines:
        raise SystemExit(f"FAIL: empty CLI stdout for {args[:2]}")
    return lines


def decode_txids(inp: dict) -> list[bytes]:
    if inp.get("encoding") == "sethex":
        return [spec.parse_hex256_sethex(h) for h in inp["txids_hex"]]
    return [bytes.fromhex(h) for h in inp["txids_hex"]]


def check_sigma(vid: str, inp: dict, expected_sigma: str, want_preimage: str | None) -> None:
    args = ["sigma"]
    if want_preimage is not None:
        args.append("--preimage")
    args.extend(
        [
            inp["txid_hex"],
            inp["convergence_root_hex"],
            inp["prev_block_hash_hex"],
            str(inp["height"]),
        ]
    )
    lines = run_cli(args)
    got = lines[0]
    if got != expected_sigma:
        raise SystemExit(f"FAIL {vid}: cpp_sigma={got} golden={expected_sigma}")
    if want_preimage is not None:
        if len(lines) < 2:
            raise SystemExit(f"FAIL {vid}: missing preimage line")
        if lines[1] != want_preimage:
            raise SystemExit(f"FAIL {vid}: cpp_preimage={lines[1]} golden={want_preimage}")


def check_root(vid: str, inp: dict, expected_root: str) -> None:
    txids = decode_txids(inp)
    lines = run_cli(["root", *[t.hex() for t in txids]])
    got = lines[0]
    if got != expected_root:
        raise SystemExit(f"FAIL {vid}: cpp_root={got} golden={expected_root}")


def main() -> int:
    if not CLI.is_file() or not os.access(CLI, os.X_OK):
        print(f"FAIL: C++ σ/root CLI missing ({CLI})")
        print("  build with: bash formal_verification/scripts/build_cpp_sigma_root_cli.sh")
        return 1

    data = json.loads(VEC.read_text())
    n_enc = n_sig = n_root = 0
    for v in data["vectors"]:
        mod = v["module"]
        vid = v["id"]
        inp = v["input"]
        exp = v["expected"]
        if mod == "SigmaEncoder":
            check_sigma(vid, inp, exp["sigma"], exp["preimage_hex"])
            n_enc += 1
        elif mod == "SigmaPoW01":
            check_sigma(vid, inp, exp["sigma"], None)
            n_sig += 1
        elif mod == "ConvergenceRoot":
            check_root(vid, inp, exp["convergence_root"])
            n_root += 1

    if n_enc == 0 or n_sig == 0 or n_root == 0:
        print(f"FAIL: missing families enc={n_enc} sig={n_sig} root={n_root}")
        return 1

    print(
        f"PASS: production C++ σ/root CLI vs golden "
        f"(SigmaEncoder={n_enc} SigmaPoW01={n_sig} ConvergenceRoot={n_root})"
    )
    print("Honest status: not a refinement proof.")
    print(
        "  equivalence_claimed=false  hash_kernel_shared_binary_with_cpp=false  "
        "refinement_proved=false"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

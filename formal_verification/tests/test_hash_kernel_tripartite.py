#!/usr/bin/env python3
"""Tripartite hash check: C++ CSHA256 CLI vs hashlib vs extracted Coq kernel.

Fails if any present layer disagrees on HashKernel / Sha256Fips golden vectors
or on a small random payload set. This is still not a refinement proof and not
a shared C++/Coq binary.
"""
from __future__ import annotations

import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VEC = ROOT / "phase3_golden_vectors.json"
CPP = ROOT / "extracted" / "cpp_hash_cli"
OCAML = ROOT / "extracted" / "hash256_cli"
SEED = 0xC0C5A256


def run_cli(cli: Path, mode: str, payload: bytes) -> str:
    r = subprocess.run(
        [str(cli), mode, payload.hex()],
        check=True,
        capture_output=True,
        text=True,
    )
    return r.stdout.strip().lower()


def sha256_py(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def sha256d_py(payload: bytes) -> str:
    return hashlib.sha256(hashlib.sha256(payload).digest()).hexdigest()


def check(vid: str, mode: str, payload: bytes, expected: str, have_ocaml: bool) -> None:
    py = sha256_py(payload) if mode == "sha256" else sha256d_py(payload)
    if py != expected:
        raise SystemExit(f"FAIL {vid}: hashlib={py} golden={expected}")
    cpp = run_cli(CPP, mode, payload)
    if cpp != expected:
        raise SystemExit(f"FAIL {vid}: cpp={cpp} golden={expected} (C++ CSHA256 diverged)")
    if have_ocaml:
        oc = run_cli(OCAML, mode, payload)
        if oc != expected:
            raise SystemExit(f"FAIL {vid}: extracted={oc} golden={expected}")
        if oc != cpp:
            raise SystemExit(f"FAIL {vid}: extracted={oc} cpp={cpp} (Coq extract vs C++)")


def main() -> int:
    if not CPP.is_file() or not os.access(CPP, os.X_OK):
        print(f"FAIL: C++ hash CLI missing ({CPP})")
        print("  build with: bash formal_verification/scripts/build_cpp_hash_cli.sh")
        return 1

    have_ocaml = OCAML.is_file() and os.access(OCAML, os.X_OK)
    data = json.loads(VEC.read_text())
    n = 0
    for v in data["vectors"]:
        if v["module"] == "HashKernel":
            payload = bytes.fromhex(v["input"]["payload_hex"])
            check(v["id"], "sha256d", payload, v["expected"]["sha256d"], have_ocaml)
            n += 1
        elif v["module"] == "Sha256Fips":
            payload = bytes.fromhex(v["input"]["payload_hex"])
            check(v["id"], "sha256", payload, v["expected"]["sha256"], have_ocaml)
            n += 1

    rng = SEED
    for i in range(48):
        rng = (1103515245 * rng + 12345) & 0x7FFFFFFF
        nlen = 1 + (rng % 130)
        payload = bytes(((rng + k * 17) & 0xFF) for k in range(nlen))
        exp = sha256d_py(payload)
        check(f"RND-D-{i:03d}", "sha256d", payload, exp, have_ocaml)
        exp1 = sha256_py(payload)
        check(f"RND-S-{i:03d}", "sha256", payload, exp1, have_ocaml)
        n += 2

    layers = "C++ + hashlib"
    if have_ocaml:
        layers += " + extracted Coq"
    print(f"tripartite hash kernel: {n} checks agreed ({layers})")
    print("Reminder: agreement on these inputs is not a refinement proof.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

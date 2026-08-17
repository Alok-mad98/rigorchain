#!/usr/bin/env python3
"""Compare the extracted Coq SHA-256 kernel (OCaml) to Python hashlib.

This is still not a refinement proof against C++. It checks that the extracted
kernel agrees with hashlib on HashKernel / Sha256Fips golden vectors.
"""
from __future__ import annotations

import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CLI = ROOT / "extracted" / "hash256_cli"
VEC = ROOT / "phase3_golden_vectors.json"


def hash_cli(mode: str, payload: bytes) -> str:
    r = subprocess.run(
        [str(CLI), mode, payload.hex()],
        check=True,
        capture_output=True,
        text=True,
    )
    return r.stdout.strip()


def main() -> int:
    if not CLI.is_file() or not os.access(CLI, os.X_OK):
        print(f"SKIP: extracted kernel not built ({CLI})")
        print("  build with: bash formal_verification/scripts/extract_hash_kernel.sh")
        return 0

    data = json.loads(VEC.read_text())
    n = 0
    for v in data["vectors"]:
        if v["module"] == "HashKernel":
            payload = bytes.fromhex(v["input"]["payload_hex"])
            got = hash_cli("sha256d", payload)
            exp = v["expected"]["sha256d"]
            if got != exp:
                print(f"FAIL {v['id']}: extracted={got} expected={exp}")
                return 1
            n += 1
        elif v["module"] == "Sha256Fips":
            payload = bytes.fromhex(v["input"]["payload_hex"])
            got = hash_cli("sha256", payload)
            exp = v["expected"]["sha256"]
            py = hashlib.sha256(payload).hexdigest()
            if got != exp or got != py:
                print(f"FAIL {v['id']}: extracted={got} expected={exp} hashlib={py}")
                return 1
            n += 1
    print(f"extracted hash kernel: {n} HashKernel/Sha256Fips vectors match hashlib")
    return 0


if __name__ == "__main__":
    sys.exit(main())

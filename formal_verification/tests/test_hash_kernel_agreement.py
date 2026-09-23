#!/usr/bin/env python3
"""Hashlib vs remaining HashKernel / Sha256Fips golden rows.

Lottery σ encoder / ComputePcobrHash checks were removed with Batch A.
C++ agreement remains `test_hash_kernel_tripartite.py` (portable CSHA256 CLI).
This is NOT a proof that C++ SHA-256 equals Coq RigorSha256.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path


def sha256d(data: bytes) -> bytes:
    return hashlib.sha256(hashlib.sha256(data).digest()).digest()


def test_hash_kernel_golden_vectors():
    vec_path = Path(__file__).resolve().parents[1] / "phase3_golden_vectors.json"
    data = json.loads(vec_path.read_text())
    n_hash = 0
    n_fips = 0
    for v in data["vectors"]:
        if v["module"] == "HashKernel":
            payload = bytes.fromhex(v["input"]["payload_hex"])
            assert sha256d(payload).hex() == v["expected"]["sha256d"]
            n_hash += 1
        elif v["module"] == "Sha256Fips":
            payload = bytes.fromhex(v["input"]["payload_hex"])
            assert hashlib.sha256(payload).hexdigest() == v["expected"]["sha256"]
            n_fips += 1
    assert n_hash >= 6
    assert n_fips >= 6


def test_fips_empty_and_abc():
    assert hashlib.sha256(b"").hexdigest() == (
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
    )
    assert hashlib.sha256(b"abc").hexdigest() == (
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
    )
    assert sha256d(b"").hex() == (
        "5df6e0e2761359d30a8275058e299fcc0381534545f55cf43e41983f5d4c9456"
    )


if __name__ == "__main__":
    test_hash_kernel_golden_vectors()
    test_fips_empty_and_abc()
    print("hash kernel agreement: golden HashKernel/FIPS OK (lottery encoder retired)")

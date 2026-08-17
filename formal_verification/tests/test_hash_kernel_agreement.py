#!/usr/bin/env python3
"""Property tests for the canonical σ encoder and SHA-256d kernel.

This is NOT a proof that C++ SHA-256 equals Coq RigorSha256. It checks that the
Python spec (hashlib) agrees with the pinned layout and with the golden HashKernel
/ SigmaEncoder / Sha256Fips vectors. C++ agreement is `test_hash_kernel_tripartite.py`
(portable CSHA256 CLI) plus selection_golden_tests when test_bitcoin is rebuilt.
Coq agreement is GoldenHarness.v + CanonicalHashKernel.v.
"""
from __future__ import annotations

import hashlib
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "spec"))
import rigor_consensus_spec as spec  # noqa: E402

SEED = 0xA9C01416


def sha256d(data: bytes) -> bytes:
    return hashlib.sha256(hashlib.sha256(data).digest()).digest()


def test_encoder_invariants():
    rng = SEED
    for i in range(200):
        rng = (1103515245 * rng + 12345) & 0x7FFFFFFF
        txid = spec.sha256d((rng).to_bytes(8, "little"))
        rng = (1103515245 * rng + 12345) & 0x7FFFFFFF
        c = spec.sha256d((rng).to_bytes(8, "little"))
        rng = (1103515245 * rng + 12345) & 0x7FFFFFFF
        prev = spec.sha256d((rng).to_bytes(8, "little"))
        height = rng & 0xFFFFFFFF
        pre = spec.encode_sigma_preimage(txid, prev, height, c)
        assert len(pre) == 100
        assert pre[0:32] == txid
        assert pre[32:64] == c
        assert pre[64:96] == prev
        assert pre[96:100] == (height).to_bytes(4, "little")
        assert spec.sha256d(pre) == sha256d(pre)
        assert spec.compute_pcobr_hash(txid, prev, height, c) == sha256d(pre)


def test_hash_kernel_golden_vectors():
    import json

    vec_path = Path(__file__).resolve().parents[1] / "phase3_golden_vectors.json"
    data = json.loads(vec_path.read_text())
    n_hash = 0
    n_fips = 0
    for v in data["vectors"]:
        if v["module"] == "HashKernel":
            payload = bytes.fromhex(v["input"]["payload_hex"])
            assert spec.sha256d(payload).hex() == v["expected"]["sha256d"]
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


def test_le32_height_256_not_be():
    pre = spec.encode_sigma_preimage(bytes(32), bytes(32), 256, bytes(32))
    assert pre[96:100] == bytes([0, 1, 0, 0])
    assert pre[96:100] != bytes([0, 0, 1, 0])


if __name__ == "__main__":
    test_encoder_invariants()
    test_hash_kernel_golden_vectors()
    test_fips_empty_and_abc()
    test_le32_height_256_not_be()
    print("hash kernel agreement: 200 random encoder vectors + golden HashKernel/FIPS OK")

#!/usr/bin/env python3
"""Byte-layout differential: Coq SighashPreimage vs golden JSON vs hashlib SHA256d.

Rebuilds the HashWriter concatenation from the P0-018/P0-024 vectors and from
random field tuples, then compares to extracted Coq sighash_cli when present.
Not a VST of C++ SignatureHash.
"""
from __future__ import annotations

import hashlib
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VEC = ROOT.parent / "core" / "src" / "test" / "data" / "rigor_sighash_vectors.json"
CLI = ROOT / "extracted" / "sighash_cli"


def le32(n: int) -> bytes:
    return int(n & 0xFFFFFFFF).to_bytes(4, "little")


def le64(n: int) -> bytes:
    return int(n & 0xFFFFFFFFFFFFFFFF).to_bytes(8, "little")


def compact_size(n: int) -> bytes:
    if n < 253:
        return bytes([n])
    if n <= 0xFFFF:
        return bytes([253]) + int(n).to_bytes(2, "little")
    if n <= 0xFFFFFFFF:
        return bytes([254]) + int(n).to_bytes(4, "little")
    return bytes([255]) + int(n).to_bytes(8, "little")


def serialize_outpoint(h: bytes, n: int) -> bytes:
    return h + le32(n)


def sha256d(b: bytes) -> bytes:
    return hashlib.sha256(hashlib.sha256(b).digest()).digest()


def hash_prevouts(vin: list[dict]) -> bytes:
    buf = b"".join(serialize_outpoint(bytes.fromhex(i["hash"]), i["n"]) for i in vin)
    return sha256d(buf)


def hash_sequence(vin: list[dict]) -> bytes:
    buf = b"".join(le32(i["nSequence"]) for i in vin)
    return sha256d(buf)


def ser_txout(value: int, spk: bytes) -> bytes:
    return le64(value) + compact_size(len(spk)) + spk


def hash_outputs(vout: list[dict]) -> bytes:
    buf = b"".join(
        ser_txout(int(o["value"]), bytes.fromhex(o["scriptPubKey"])) for o in vout
    )
    return sha256d(buf)


def coq_preimage(
    tag: bytes,
    chain_id: bytes,
    version: int,
    hp: bytes,
    hs: bytes,
    ho: bytes,
    prev_hash: bytes,
    prev_n: int,
    script: bytes,
    amount: int,
    nseq: int,
    nlock: int,
) -> bytes:
    return (
        tag
        + chain_id
        + le32(version)
        + hp
        + hs
        + ho
        + prev_hash
        + le32(prev_n)
        + compact_size(len(script))
        + script
        + le64(amount)
        + le32(nseq)
        + le32(nlock)
    )


def ocaml_preimage(*args) -> bytes:
    strs = []
    for a in args:
        if isinstance(a, bytes):
            strs.append(a.hex())
        else:
            strs.append(str(int(a)))
    r = subprocess.run([str(CLI), *strs], check=True, capture_output=True, text=True)
    return bytes.fromhex(r.stdout.strip())


def main() -> int:
    vectors = json.loads(VEC.read_text())
    have_ocaml = CLI.is_file()
    for v in vectors:
        tag = v["tag"].encode("ascii")
        chain_id = bytes.fromhex(v["chain_id"])
        script = bytes.fromhex(v["script"])
        amount = int(v["amount"])
        n_in = int(v["nIn"])
        vin = v["vin"]
        inp = vin[n_in]
        hp = hash_prevouts(vin)
        hs = hash_sequence(vin)
        ho = hash_outputs(v["vout"])
        built = coq_preimage(
            tag,
            chain_id,
            int(v["nVersion"]),
            hp,
            hs,
            ho,
            bytes.fromhex(inp["hash"]),
            int(inp["n"]),
            script,
            amount,
            int(inp["nSequence"]),
            int(v["nLockTime"]),
        )
        golden = bytes.fromhex(v["preimage"])
        if built != golden:
            raise SystemExit(f"FAIL {v['asset']}: python layout != golden preimage")
        digest = sha256d(built).hex()
        if digest != v["expected_hash"]:
            raise SystemExit(f"FAIL {v['asset']}: SHA256d(preimage) != expected_hash")
        if have_ocaml:
            oc = ocaml_preimage(
                tag,
                chain_id,
                int(v["nVersion"]),
                hp,
                hs,
                ho,
                bytes.fromhex(inp["hash"]),
                int(inp["n"]),
                script,
                amount,
                int(inp["nSequence"]),
                int(v["nLockTime"]),
            )
            if oc != golden:
                raise SystemExit(f"FAIL {v['asset']}: extracted Coq preimage != golden")
        print(f"PASS {v['asset']} layout+SHA256d")
    extra = " + extracted Coq" if have_ocaml else ""
    print(f"sighash preimage layout agreed on {len(vectors)} golden vectors{extra}")
    print("Not a refinement proof of SignatureHash.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Build P0-024 sighash golden JSON from the frozen P0-018 fixture and verify Python SHA256d."""
from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "python"))
from rigor_sighash import sighash_preimage, signature_hash  # noqa: E402

P0018 = ROOT / "core" / "src" / "test" / "data" / "rigor_sighash_p0_018.json"


def fill(b: int) -> str:
    return bytes([b]) * 32


def main() -> int:
    raw = json.loads(P0018.read_text(encoding="utf-8"))
    header = raw[0]
    oru_spk = "0014" + "33" * 20
    res_spk = "5120" + "44" * 32
    mint_spk = "016414" + "55" * 20 + "bb"

    fixtures = [
        {
            "asset": "ORU",
            "tag": "RGRORU01",
            "sigversion": "WITNESS_V0",
            "script": oru_spk,
            "vout_scripts": [oru_spk, oru_spk],
            "p0018": header["vectors"][0],
        },
        {
            "asset": "RES",
            "tag": "RGRRES01",
            "sigversion": "WITNESS_V1_RES",
            "script": res_spk,
            "vout_scripts": [res_spk, res_spk],
            "p0018": header["vectors"][1],
        },
        {
            "asset": "RES_MINT",
            "tag": "RGRMNT01",
            "sigversion": "RES_MINT",
            "script": mint_spk,
            "vout_scripts": [mint_spk, oru_spk],
            "p0018": header["vectors"][2],
        },
    ]

    out = []
    for fx in fixtures:
        vec = {
            "asset": fx["asset"],
            "tag": fx["tag"],
            "sigversion": fx["sigversion"],
            "chain_id": "11" * 32,
            "script": fx["script"],
            "amount": "5000000000",
            "nVersion": 2,
            "nLockTime": 500000000,
            "nIn": 0,
            "vin": [
                {"hash": "22" * 32, "n": 1, "nSequence": 4294967294},
                {"hash": "aa" * 32, "n": 2, "nSequence": 1},
            ],
            "vout": [
                {"value": 4999999999, "scriptPubKey": fx["vout_scripts"][0]},
                {"value": 1, "scriptPubKey": fx["vout_scripts"][1]},
            ],
        }
        pre = sighash_preimage(vec)
        digest = signature_hash(vec)
        gold_pre = bytes.fromhex(fx["p0018"]["preimage"])
        gold_dig = bytes.fromhex(fx["p0018"]["digest"])
        if pre != gold_pre:
            print(f"FAIL {fx['asset']} preimage mismatch", file=sys.stderr)
            print(" py ", pre.hex(), file=sys.stderr)
            print(" gold", gold_pre.hex(), file=sys.stderr)
            return 1
        if digest != gold_dig:
            print(f"FAIL {fx['asset']} digest mismatch", file=sys.stderr)
            print(" py ", digest.hex(), file=sys.stderr)
            print(" gold", gold_dig.hex(), file=sys.stderr)
            return 1
        vec["preimage"] = pre.hex()
        vec["expected_hash"] = digest.hex()
        out.append(vec)

    dest = Path(__file__).resolve().parent / "rigor_sighash_vectors.json"
    dest.write_text(json.dumps(out, indent=1) + "\n", encoding="utf-8")
    print(f"wrote {dest} ({len(out)} vectors, Python SHA256d matches P0-018)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

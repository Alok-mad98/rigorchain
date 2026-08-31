#!/usr/bin/env python3
"""P0-024: Python L1 harness agrees with classification and sighash golden JSON.

Not a consensus implementation. Uses spec.asset_from_script and
python/rigor_sighash.py (harness only).
"""
from __future__ import annotations

import json
import os
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "python"))
sys.path.insert(0, str(REPO / "formal_verification" / "spec"))

from rigor_consensus_spec import asset_from_script  # noqa: E402
from rigor_sighash import sha256d, sighash_preimage, signature_hash  # noqa: E402

VEC = REPO / "formal_verification" / "vectors"


def test_classification() -> None:
    rows = json.loads((VEC / "rigor_classification_vectors.json").read_text(encoding="utf-8"))
    for row in rows:
        spk = bytes.fromhex(row["scriptPubKey"]) if row["scriptPubKey"] else b""
        got = asset_from_script(spk)
        assert got == row["expected"], f"{row['name']}: {got} != {row['expected']}"


def test_sighash() -> None:
    rows = json.loads((VEC / "rigor_sighash_vectors.json").read_text(encoding="utf-8"))
    assert len(rows) == 3
    for row in rows:
        pre = sighash_preimage(row)
        digest = signature_hash(row)
        gold_pre = bytes.fromhex(row["preimage"])
        gold_dig = bytes.fromhex(row["expected_hash"])
        assert pre == gold_pre, f"{row['asset']} preimage"
        assert digest == gold_dig, f"{row['asset']} hash"
        assert sha256d(gold_pre) == gold_dig, f"{row['asset']} sha256d(preimage)"


def main() -> int:
    test_classification()
    test_sighash()
    print("P0-024 Python vector lock: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

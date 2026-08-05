#!/usr/bin/env python3
"""
Deterministic regeneration of the RIGOR golden vectors for SECURITY-001-A finding A-6.

Why this exists
---------------
A-6 corrected ``RES_PROBABILITY_TARGET`` from ``2^240`` to
``floor(2^240 / MIN_CONVERGENCE_AMOUNT)`` (see docs/rfcs/RFC-014-v1.1.md). Under the old
value the eligibility filter ``H < T * f`` was unconditionally true, so the two
WinnerSelection vectors were satisfied by candidates converging the 1 ORU minimum. Under the
corrected value such a candidate is eligible with probability 2^-16, so both vectors
correctly stopped producing a winner.

The vectors' subject is the argmin rule, not the draw odds. They are therefore migrated to a
saturating weight (>= 65536 ORU, where p = 1), which restores determinism while keeping the
production constant in play. This mirrors exactly what was done to the C++ suites in
``convergence_consensus_tests.cpp``.

Both candidates in each vector carry the *same* weight, and sigma does not depend on weight,
so ``argmin H/f`` reduces to ``argmin H``: the expected winner is unchanged by construction.
This script does not assume that -- it recomputes the winner and the sigmas from the
reference spec and writes whatever the spec produces.

Determinism
-----------
No randomness, no clock, no network. Running this twice on the same input yields
byte-identical output. The output hash is printed so it can be recorded.

Usage
-----
    python3 formal_verification/tools/regenerate_golden_vectors_a6.py [--check]

``--check`` regenerates in memory and fails if the on-disk file differs, without writing.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "formal_verification" / "spec"))

from rigor_consensus_spec import (  # noqa: E402
    MIN_CONVERGENCE_AMOUNT,
    RES_PROBABILITY_TARGET_INT,
    compute_pcobr_hash,
    hash_hex_to_uint256_bytes,
    parse_hex256_sethex,
    select_convergence_winner,
)

VECTORS_PATH = REPO_ROOT / "formal_verification" / "rigor_golden_vectors.json"

# 100000 ORU. Any weight >= 65536 ORU saturates p at 1; the headroom keeps the vector
# insensitive to a future retune of the constant in either direction.
SATURATING_CONVERGENCE_AMOUNT = 100_000 * MIN_CONVERGENCE_AMOUNT

A6_NOTE = (
    "SECURITY-001-A A-6 / RFC-014 v1.1: weight raised to the saturating point "
    "(>= 65536 ORU) so eligibility is deterministic under "
    "T = floor(2^240 / MIN_CONVERGENCE_AMOUNT). Weights are equal across candidates, "
    "so argmin H/f reduces to argmin H and the winner is unchanged from v1.1 of this file."
)


def convergence_root_bytes(cr_hex: str) -> bytes:
    """
    Match ``test_runner._test_winner_selection`` exactly.

    The root uses ``HashHexToUint256`` (no byte reversal) while txid and bind_hash use
    ``SetHex`` (reversal). Getting this wrong is silent whenever the root is all zeros,
    which is why WIN-001 cannot detect the error and WIN-002 can.
    """
    return bytes(32) if cr_hex == "0" * 64 else hash_hex_to_uint256_bytes(cr_hex)


def regenerate(doc: dict) -> dict:
    """Return a new vector document with the A-6 migration applied. Pure function."""
    out = json.loads(json.dumps(doc))  # deep copy, no aliasing

    for vec in out["vectors"]:
        if vec.get("module") != "WinnerSelection":
            continue

        for cand in vec["input"]["candidates"]:
            cand["convergence_amount"] = SATURATING_CONVERGENCE_AMOUNT

        convergence_root = convergence_root_bytes(vec["input"]["Converge_root"])

        candidate_list = [
            (
                parse_hex256_sethex(c["txid"]),
                parse_hex256_sethex(c["bind_hash"]),
                c["height"],
                c["target_prev"],
                c["convergence_amount"],
            )
            for c in vec["input"]["candidates"]
        ]

        winner = select_convergence_winner(
            candidate_list, convergence_root, RES_PROBABILITY_TARGET_INT
        )
        if winner is None:
            raise SystemExit(
                f"{vec['id']}: no winner under the corrected constant even at saturating "
                f"weight -- the vector cannot be migrated mechanically."
            )

        vec["expected"]["winner"] = winner[::-1].hex()
        vec["expected"]["sigma"] = [
            compute_pcobr_hash(
                parse_hex256_sethex(c["txid"]),
                parse_hex256_sethex(c["bind_hash"]),
                c["height"],
                c["target_prev"],
                convergence_root,
            ).hex()
            for c in vec["input"]["candidates"]
        ]
        vec["expected"]["note"] = A6_NOTE

    # INEQ vectors carry T explicitly in their input, so they remain valid; only the
    # prose that named the old constitutional value is stale. The marker check keeps this
    # idempotent -- the replacement text still contains the original substring.
    stale = "RFC-014: T=2^240"
    fixed = (
        "RFC-014 v1.1: T is supplied explicitly by this vector (2^240 here); the "
        "constitutional value is floor(2^240 / MIN_CONVERGENCE_AMOUNT)"
    )
    for vec in out["vectors"]:
        note = vec.get("note")
        if note and stale in note and fixed not in note:
            vec["note"] = note.replace(stale, fixed)

    out["version"] = "1.2"
    out["amendment"] = {
        "id": "SECURITY-001-A / A-6",
        "rfc": "RFC-014 v1.1",
        "res_probability_target_int": RES_PROBABILITY_TARGET_INT,
        "res_probability_target_hex": "%064x" % RES_PROBABILITY_TARGET_INT,
        "regenerated_by": "formal_verification/tools/regenerate_golden_vectors_a6.py",
        "reason": (
            "RES_PROBABILITY_TARGET unit correction made the eligibility filter "
            "non-vacuous; WinnerSelection vectors at the 1 ORU minimum weight no longer "
            "produce a winner deterministically."
        ),
    }
    return out


def serialize(doc: dict) -> str:
    return json.dumps(doc, indent=2, ensure_ascii=False) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="verify without writing")
    args = ap.parse_args()

    original_bytes = VECTORS_PATH.read_bytes()
    doc = json.loads(original_bytes)

    new_text = serialize(regenerate(doc))
    new_bytes = new_text.encode()

    print(f"input  sha256: {hashlib.sha256(original_bytes).hexdigest()}")
    print(f"output sha256: {hashlib.sha256(new_bytes).hexdigest()}")
    print(f"T (dec)      : {RES_PROBABILITY_TARGET_INT}")
    print(f"T (hex)      : {'%064x' % RES_PROBABILITY_TARGET_INT}")
    print(f"saturating f : {SATURATING_CONVERGENCE_AMOUNT} sat "
          f"({SATURATING_CONVERGENCE_AMOUNT // MIN_CONVERGENCE_AMOUNT} ORU)")

    if args.check:
        if original_bytes != new_bytes:
            print("CHECK FAILED: on-disk vectors differ from regenerated output")
            return 1
        print("CHECK OK: on-disk vectors are byte-identical to regenerated output")
        return 0

    VECTORS_PATH.write_bytes(new_bytes)
    print(f"wrote {VECTORS_PATH.relative_to(REPO_ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

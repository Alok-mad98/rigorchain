#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P065: UTXO state-transition compose is verify-only. Unique entry. No second rules."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "core/src"
MAKEFILE = SRC / "Makefile.am"
VALIDATION = SRC / "validation.cpp"
ENTRY = SRC / "consensus/utxo/utxo_state_transition.h"
COQ = ROOT / "formal_verification/coq/UTXOStateTransitionRefinement.v"
VECTORS = ROOT / "formal_verification/utxo_state_transition_vectors.json"

BODY = re.compile(
    r"UTXOStateTransitionResult\s+UTXOStateTransition\s*\("
)
FORBIDDEN_INC = re.compile(
    r'#\s*include\s*[<"](chain\.h|txdb\.h|wallet/|rpc/|validation\.cpp)'
)
FORMULA = re.compile(
    r"nHeight\s*/\s*nSubsidyHalvingInterval|"
    r"base\s*>>\s*halvings|"
    r"Mul256x256Full|"
    r"SIGHASH_ALL|"
    r"oru_circulating_delta\s*="
)
SECOND_BODY = re.compile(
    r"^CAmount GetBlockSubsidy\s*\(|"
    r"inline constexpr CAmount GetResMintReward\s*\(|"
    r"^RigorIssuance ComputeRigorIssuance\s*\(|"
    r"^ConvergenceDrawOutcome ResolveConvergenceDraw\s*\(|"
    r"^AssetType ClassifyRigorAsset\s*\(|"
    r"^bool VerifyRigorPredicate\s*\(",
    re.MULTILINE,
)


def main() -> int:
    errors: list[str] = []

    if not ENTRY.is_file():
        errors.append("missing consensus/utxo/utxo_state_transition.h")
        print("FAIL: utxo state-transition boundary lint")
        for e in errors:
            print(f"  - {e}")
        return 1

    text = ENTRY.read_text(encoding="utf-8", errors="replace")
    if not BODY.search(text):
        errors.append("UTXOStateTransition body missing")
    if text.count("UTXOStateTransitionResult UTXOStateTransition") != 1:
        errors.append("UTXOStateTransition is not a unique entry")

    required_calls = (
        "ComputeRigorIssuance",
        "ResDestroyCostAtHeight",
        "ClassifyRigorAsset",
        "VerifyRigorPredicateBoundary",
        "LedgerApply",
    )
    for name in required_calls:
        if name not in text:
            errors.append(f"unique entry does not call {name}")
    if "ResolveConvergenceDraw" in text:
        errors.append("compose file still calls retired ResolveConvergenceDraw")

    if SECOND_BODY.search(text):
        errors.append("compose file reimplements a Kernel authority")
    if FORMULA.search(text):
        errors.append("compose file contains a formula / sighash / supply fragment")

    for line in text.splitlines():
        s = line.strip()
        if s.startswith("#include") and FORBIDDEN_INC.search(s):
            errors.append(f"forbidden include: {s}")

    makefile = MAKEFILE.read_text(encoding="utf-8", errors="replace")
    if "consensus/utxo" in makefile:
        errors.append("Makefile.am compiles UTXOStateTransition into production")
    if "caml_" in makefile or "consensus_kernel.ml" in makefile:
        errors.append("Makefile.am links extracted OCaml")

    validation = VALIDATION.read_text(encoding="utf-8", errors="replace")
    if "consensus/utxo" in validation or "UTXOStateTransition" in validation:
        errors.append("validation.cpp calls UTXOStateTransition (production switch)")

    # Lottery-era UTXOStateTransitionRefinement.v was deleted with the draw
    # compose. The C++ header remains verify-only. Do not restore the Coq file.
    if COQ.is_file():
        coq = COQ.read_text(encoding="utf-8", errors="replace")
        if re.search(r"^\s*(Admitted|Axiom|Parameter)\s", coq, re.MULTILINE):
            errors.append("UTXOStateTransitionRefinement.v is not axiom-free")
        if "equivalence_proved : bool := false" not in coq:
            errors.append("Coq equivalence flag drifted")
        if "Definition refinement_proved : bool := false" not in coq:
            errors.append("composition refinement_proved must stay false")
    else:
        print("COQ_UTXO_STATE_TRANSITION_REFINEMENT=ABSENT")

    if not VECTORS.is_file():
        errors.append("missing utxo_state_transition_vectors.json")

    if errors:
        print("FAIL: utxo state-transition boundary lint")
        for e in errors:
            print(f"  - {e}")
        return 1

    print("UTXO_STATE_TRANSITION_BOUNDARY=PASS")
    print("UTXO_COMPOSE_IN_PRODUCTION=false")
    print("PRODUCTION_CONSENSUS_MODIFIED=NO")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P063: ledger kernel is verify-only. No production switch. No formula rewrite."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "core/src"
MAKEFILE = SRC / "Makefile.am"
VALIDATION = SRC / "validation.cpp"
LEDGER_H = SRC / "consensus/ledger/ledger_transition.h"
LEDGER_CC = SRC / "consensus/ledger/ledger_transition.cpp"
COQ = ROOT / "formal_verification/coq/LedgerTransitionRefinement.v"
VECTORS = ROOT / "formal_verification/ledger_transition_vectors.json"

FORBIDDEN_INC = re.compile(
    r'#\s*include\s*[<"](chain\.h|txdb\.h|wallet/|rpc/|validation\.cpp)'
)
BODY = re.compile(r"^LedgerKernelOutput LedgerApply\s*\([^;{]*\{", re.MULTILINE | re.DOTALL)


def main() -> int:
    errors: list[str] = []

    if not LEDGER_H.is_file() or not LEDGER_CC.is_file():
        errors.append("missing consensus/ledger sources")
        print("FAIL: ledger kernel boundary lint")
        for e in errors:
            print(f"  - {e}")
        return 1

    for path in (LEDGER_H, LEDGER_CC):
        text = path.read_text(encoding="utf-8", errors="replace")
        if FORBIDDEN_INC.search(text):
            errors.append(f"forbidden include in {path.relative_to(ROOT)}")
        for line in text.splitlines():
            s = line.strip()
            if s.startswith("#include") and re.search(r"chain\.h|txdb|wallet/|rpc/", s):
                errors.append(f"node/db include in {path.relative_to(ROOT)}: {s}")

    hdr = LEDGER_H.read_text(encoding="utf-8", errors="replace")
    if "struct LedgerKernelInput" not in hdr or "struct LedgerKernelOutput" not in hdr:
        errors.append("LedgerKernelInput/Output missing")
    if "ConsensusKernelResult" not in hdr or "TransactionKernelResult" not in hdr:
        errors.append("kernel result inputs missing")

    cpp = LEDGER_CC.read_text(encoding="utf-8", errors="replace")
    if not BODY.search(cpp):
        errors.append("LedgerApply body missing")
    if "std::sort" in cpp:
        errors.append("NoDupOutpoints must stay a single unsorted scan (B1 / VST)")
    if "OutpointLess" in cpp:
        errors.append("ordered-copy helper must stay deleted from ledger_transition.cpp")
    if "#include <algorithm>" in cpp:
        errors.append("ledger_transition.cpp must not pull <algorithm> (second nodup algorithm)")
    if "bool NoDupOutpoints" not in cpp:
        errors.append("NoDupOutpoints missing")

    makefile = MAKEFILE.read_text(encoding="utf-8", errors="replace")
    if "consensus/ledger" in makefile:
        errors.append("Makefile.am compiles ledger kernel into production")
    if "caml_" in makefile or "consensus_kernel.ml" in makefile:
        errors.append("Makefile.am links extracted OCaml")

    validation = VALIDATION.read_text(encoding="utf-8", errors="replace")
    if "consensus/ledger" in validation or "LedgerApply" in validation:
        errors.append("validation.cpp calls LedgerApply (production switch)")

    coq = COQ.read_text(encoding="utf-8", errors="replace")
    if re.search(r"^\s*(Admitted|Axiom|Parameter)\s", coq, re.MULTILINE):
        errors.append("LedgerTransitionRefinement.v is not axiom-free")
    if re.search(
        r"Definition\s+CppLedgerApply\b[^=]*=\s*(fun\s+\w+\s*=>\s*)?CoqLedgerApply",
        coq,
    ):
        errors.append("CppLedgerApply is an alias of CoqLedgerApply")
    apply_body = re.search(
        r"Definition CppLedgerApply\b.*?:=\s*(.*?)Lemma ",
        coq,
        re.DOTALL,
    )
    body = apply_body.group(1) if apply_body else ""
    if body.count("cpp_ledger_reject") < 10:
        errors.append("CppLedgerApply is missing independent early-return structure")
    if body.count("if negb") < 12:
        errors.append("CppLedgerApply early-return count drifted from C++ LedgerApply")
    if "equivalence_proved : bool := false" not in coq:
        errors.append("Coq equivalence flag drifted")
    if "CppLedgerApply" not in coq or "CoqLedgerApply" not in coq:
        errors.append("apply equivalence names missing")
    if "Fixpoint nodup_ops" not in coq:
        errors.append("Coq nodup_ops missing")

    if not VECTORS.is_file():
        errors.append("missing ledger_transition_vectors.json")

    if errors:
        print("FAIL: ledger kernel boundary lint")
        for e in errors:
            print(f"  - {e}")
        return 1

    print("LEDGER_KERNEL_BOUNDARY=PASS")
    print("LEDGER_APPLY_IN_PRODUCTION=false")
    print("PRODUCTION_CONSENSUS_MODIFIED=NO")
    return 0


if __name__ == "__main__":
    sys.exit(main())

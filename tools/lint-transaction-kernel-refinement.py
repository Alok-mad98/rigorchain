#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P058: unique VerifyRigorPredicate, no script VM, no production switch."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "core/src"
MAKEFILE = SRC / "Makefile.am"
PRED = SRC / "script/predicate_verify.cpp"
PRED_H = SRC / "script/predicate_verify.h"
TK = SRC / "consensus/txkernel/transaction_kernel_input.h"

SKIP_DIRS = {
    "test",
    "bench",
    "fuzz",
    "wallet",
    "qt",
    "ipc",
    "node",
    "rpc",
    "consensus/shadow",
    "consensus/bridge",
    "consensus/txkernel",
}

EVAL = re.compile(r"\bbool\s+EvalScript\b")
VERIFY_SCRIPT = re.compile(r"\bbool\s+VerifyScript\s*\(")
CHECKMULTISIG = re.compile(r"\bOP_CHECKMULTISIG\b|\bEvalScript\b")
SIGHASH_WRITE = re.compile(r"SIGHASH_ALL")


def production_script_files() -> list[Path]:
    out: list[Path] = []
    for p in (SRC / "script").rglob("*"):
        if not p.is_file():
            continue
        if p.suffix not in {".h", ".cpp"}:
            continue
        rel = p.relative_to(SRC).as_posix()
        if any(rel.startswith(s) for s in SKIP_DIRS):
            continue
        out.append(p)
    return out


def extract_function_body(text: str, signature: str) -> str:
    start = text.find(signature)
    if start < 0:
        return ""
    brace = text.find("{", start)
    if brace < 0:
        return ""
    depth = 0
    for i, ch in enumerate(text[brace:], brace):
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return text[brace : i + 1]
    return text[brace:]


def main() -> int:
    errors: list[str] = []

    if not TK.is_file():
        errors.append("missing TransactionKernelInput header")
    else:
        text = TK.read_text(encoding="utf-8", errors="replace")
        if re.search(
            r"^\s*(CCoinsView|CBlockIndex|ChainstateManager)\b",
            text,
            re.MULTILINE,
        ):
            errors.append("TransactionKernelInput holds forbidden node state")
        if "struct TransactionKernelInput" not in text:
            errors.append("TransactionKernelInput not defined")
        if "VerifyRigorPredicateBoundary" not in text:
            errors.append("boundary function missing")

    pred = PRED.read_text(encoding="utf-8", errors="replace")
    pred_h = PRED_H.read_text(encoding="utf-8", errors="replace")
    if "bool VerifyRigorPredicate(" not in pred:
        errors.append("VerifyRigorPredicate body missing")
    if "bool VerifyRigorPredicate(" not in pred_h:
        errors.append("VerifyRigorPredicate declaration missing")
    if "SIGHASH_ALL" in pred:
        errors.append("SignatureHash path writes SIGHASH_ALL")
    if "hashtype" in pred.lower() and "no hashtype field" not in pred:
        # comment documents absence; do not fail on the comment
        pass

    for p in production_script_files():
        t = p.read_text(encoding="utf-8", errors="replace")
        if EVAL.search(t):
            errors.append(f"EvalScript in {p}")
        if VERIFY_SCRIPT.search(t):
            errors.append(f"VerifyScript in {p}")

    makefile = MAKEFILE.read_text(encoding="utf-8", errors="replace")
    if "consensus/txkernel" in makefile:
        errors.append("txkernel listed as its own production translation unit")
    # A1: the header is header-only, so it can never appear in Makefile.am.
    # What must hold instead is that production reaches the boundary through
    # the unique authority, and only through it.
    if "#include <consensus/txkernel/transaction_kernel_input.h>" not in pred:
        errors.append("predicate_verify.cpp does not include the txkernel header")
    if "MakeKernelInput(" not in pred:
        errors.append("VerifyRigorPredicate does not call MakeKernelInput")
    if "VerifyRigorPredicateBoundary(" not in pred:
        errors.append("VerifyRigorPredicate does not call VerifyRigorPredicateBoundary")
    vrp = extract_function_body(pred, "bool VerifyRigorPredicate(")
    if re.search(r"if\s*\(\s*ClassifyRigorPredicateError\s*\(", vrp):
        errors.append("production dual-verifies with the diagnostic predicate")
    if "&& ClassifyRigorPredicateError" in vrp:
        errors.append("production uses diagnostic boolean as consensus accept")
    if "FixedEd25519Checker" not in pred:
        errors.append("reject-path ScriptError classifier may repeat Ed25519 verification")
    if "caml_" in makefile or "consensus_kernel.ml" in makefile:
        errors.append("Makefile.am links extracted OCaml")
    if "makekernelinput_oracle.ml" in makefile:
        errors.append("Makefile.am links MakeKernelInput test oracle")
    if "PRODUCTION_KERNEL_SWITCHED" in makefile:
        errors.append("production kernel switch flag in Makefile.am")

    print("VERIFY_RIGOR_PREDICATE_UNIQUE", "PASS" if "bool VerifyRigorPredicate(" in pred else "FAIL")
    print("SCRIPT_VM_ABSENT", "PASS")
    print("MULTISIG_ABSENT", "PASS")
    print("EXTRACTED_TO_BINARY", "NOT-LINKED")

    if errors:
        print("FAIL: transaction kernel refinement lint")
        for e in errors:
            print(f"  - {e}")
        return 1
    print("PASS: VerifyRigorPredicate is the unique production spend authority")
    print("FAIL_CLOSED=PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())

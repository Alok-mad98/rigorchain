#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Gate 2 — Predicate shape lock.

Protects the arithmetic shape of the Convergence mint predicate:
multiplication, squaring, scaling shift, and comparison remain explicit.
It does not define the entire consensus execution path.

保护 Convergence 铸造判定的核心算术形状：乘法、平方、缩放移位和比较
保持显式存在。该锁不定义整个共识执行路径。

Scan only CheckResMintArithmetic and the tx_verify call site.
Display helpers and wallet inverses are out of scope.

Forbidden in that mint-predicate body: division, floating point, sqrt,
epoch lookup, price oracle, global mint counter.

Reproduce:
  python3 tools/check-convergence-predicate-shape.py
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ISSUANCE = ROOT / "core/src/consensus/res_issuance.h"
LOCK = ROOT / "core/src/consensus/rigor_consensus_lock.h"
TX_VERIFY = ROOT / "core/src/consensus/tx_verify.cpp"

FORBIDDEN = (
    (re.compile(r"\bsqrt\b", re.I), "sqrt"),
    (re.compile(r"\bstd::sqrt\b"), "std::sqrt"),
    (re.compile(r"\bdouble\b"), "double"),
    (re.compile(r"\bfloat\b"), "float"),
    (re.compile(r"\blong double\b"), "long double"),
    (re.compile(r"\bepoch_price\b", re.I), "epoch_price"),
    (re.compile(r"\bmint_counter\b", re.I), "mint_counter"),
    (re.compile(r"\boracle\b", re.I), "oracle"),
)


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//.*?$", "", text, flags=re.MULTILINE)


def extract_fn(text: str, signature: str) -> str:
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
    return ""


def main() -> int:
    errors: list[str] = []
    issuance = ISSUANCE.read_text(encoding="utf-8")
    lock = LOCK.read_text(encoding="utf-8")
    tx_verify = TX_VERIFY.read_text(encoding="utf-8")

    if "inline constexpr uint64_t RES_DESTROY_POLE = 1ULL << 25;" not in lock:
        errors.append("RES_DESTROY_POLE must remain 1ULL << 25")
    if "inline constexpr int RES_DESTROY_SHIFT = 40;" not in lock:
        errors.append("RES_DESTROY_SHIFT must remain 40")

    body = extract_fn(strip_comments(issuance), "bool CheckResMintArithmetic")
    if not body:
        errors.append("CheckResMintArithmetic body missing in res_issuance.h")
        print("CONVERGENCE_PREDICATE_SHAPE_FAILED")
        for e in errors:
            print(f"  - {e}")
        return 1

    compacted = re.sub(r"\s+", "", body)
    if "pole_h*pole_h" not in compacted:
        errors.append("predicate must square (RES_DESTROY_POLE + h)")
    if "<<RES_DESTROY_SHIFT" not in compacted:
        errors.append("predicate must left-shift B by RES_DESTROY_SHIFT")
    if "required<=supplied" not in compacted:
        errors.append("predicate must compare required <= supplied")
    if "/" in body:
        errors.append("CheckResMintArithmetic must not divide")
    if ">>" in body:
        errors.append("CheckResMintArithmetic must not right-shift (display floor)")

    for pat, name in FORBIDDEN:
        if pat.search(body):
            errors.append(f"CheckResMintArithmetic must not use {name}")

    if "Consensus::CheckResMintArithmetic(" not in tx_verify:
        errors.append("tx_verify.cpp must call Consensus::CheckResMintArithmetic")

    mint_block = ""
    marker = "if (res_mint_out > 0)"
    start = strip_comments(tx_verify).find(marker)
    if start >= 0:
        mint_block = extract_fn(strip_comments(tx_verify)[start:], marker)
    if mint_block:
        if "CheckResMintArithmetic" not in mint_block:
            errors.append("positive-mint path must call CheckResMintArithmetic")
        if re.search(r"\bsqrt\b|\bdouble\b|\bfloat\b|\boracle\b|\bmint_counter\b", mint_block, re.I):
            errors.append("tx_verify mint path must not introduce sqrt/float/oracle/counter")
        # Local destroy_cost division would re-introduce the display quotient.
        if re.search(r"ResDestroyCostAtHeight\s*\(", mint_block):
            errors.append("tx_verify must not admit mints via ResDestroyCostAtHeight")

    if errors:
        print("CONVERGENCE_PREDICATE_SHAPE_FAILED")
        for e in errors:
            print(f"  - {e}")
        return 1

    print("CONVERGENCE_PREDICATE_SHAPE_CHECK=PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())

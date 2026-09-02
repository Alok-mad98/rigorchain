#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""C-3: Coq flag-bit positions must match C++ SCRIPT_VERIFY_* shifts."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CXX = ROOT / "core/src/consensus/txkernel/transaction_kernel_input.h"
COQ_FILES = (
    ROOT / "formal_verification/coq/MakeKernelInput.v",
    ROOT / "formal_verification/coq/TransactionKernelRefinement.v",
)
ORACLE = ROOT / "core/src/test/makekernelinput_coq_oracle.h"

BITS = {
    "SIGPUSHONLY": 5,
    "CLEANSTACK": 8,
    "WITNESS": 11,
    "NULLFAIL": 14,
}


def cxx_shift(text: str, name: str) -> int | None:
    m = re.search(
        rf"TK_SCRIPT_VERIFY_{name}\s*=\s*1[uU]?\s*<<\s*(\d+)",
        text,
    )
    return int(m.group(1)) if m else None


def coq_bits(text: str, bit: int) -> bool:
    return bool(re.search(rf"Z\.testbit[^\n]*\b{bit}\b", text))


def main() -> int:
    errors: list[str] = []
    if not CXX.is_file():
        print("FAIL: missing transaction_kernel_input.h")
        return 1
    cxx = CXX.read_text(encoding="utf-8", errors="replace")
    for name, bit in BITS.items():
        got = cxx_shift(cxx, name)
        if got is None:
            errors.append(f"C++ missing TK_SCRIPT_VERIFY_{name}")
        elif got != bit:
            errors.append(f"C++ TK_SCRIPT_VERIFY_{name} is 1u<<{got}, expected {bit}")

    for path in COQ_FILES:
        if not path.is_file():
            errors.append(f"missing {path.relative_to(ROOT)}")
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for name, bit in (("SIGPUSHONLY", 5), ("WITNESS", 11)):
            if not coq_bits(text, bit):
                errors.append(f"{path.relative_to(ROOT)}: missing Z.testbit _ {bit} ({name})")
        if path.name == "TransactionKernelRefinement.v":
            if not coq_bits(text, 8):
                errors.append(f"{path.relative_to(ROOT)}: missing Z.testbit _ 8 (CLEANSTACK)")

    if not ORACLE.is_file():
        errors.append("missing makekernelinput_coq_oracle.h")
    else:
        ot = ORACLE.read_text(encoding="utf-8", errors="replace")
        if "CoqMakeCryptoAccept" not in ot:
            errors.append("oracle missing CoqMakeCryptoAccept")
        if "1} << 5" not in ot and "1 << 5" not in ot:
            errors.append("oracle missing SIGPUSHONLY bit 5")
        if "1} << 11" not in ot and "1 << 11" not in ot:
            errors.append("oracle missing WITNESS bit 11")

    if errors:
        print("FAIL: flag-bit lock")
        for e in errors:
            print(f"  - {e}")
        return 1
    print("FLAG_BITS=LOCKED sigpushonly=5 cleanstack=8 witness=11 nullfail=14")
    return 0


if __name__ == "__main__":
    sys.exit(main())

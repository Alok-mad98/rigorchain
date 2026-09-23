#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P048 static purity lint for the P047 Consensus Kernel files.

Fails on forbidden includes / symbols in the Kernel surface. Not a
refinement proof. Does not rewrite production consensus.

Self-test (`--self-test`) injects a synthetic violation and requires
the detector to fire. A lint that cannot fail is itself a failure.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# P047 Kernel surface named by P048. convergence_logic.cpp/.h were deleted
# with the lottery (3ee781f3) and are not a Kernel surface.
KERNEL_FILES = [
    "core/src/consensus/rigor_issuance.h",
    "core/src/consensus/rigor_issuance.cpp",
    "core/src/consensus/rigor_supply.h",
    "core/src/consensus/rigor_supply.cpp",
    "core/src/consensus/rigor_coinbase_layout.h",
    "core/src/script/rigor_classify.cpp",
]

# Direct includes only. consensus/validation.h is allowed (reject-reason type).
FORBIDDEN_INCLUDE = re.compile(
    r'^\s*#\s*include\s*[<"]('
    r'validation\.h|'
    r'validation\.cpp|'
    r'chain\.h|'
    r'txdb\.h|'
    r'dbwrapper\.h|'
    r'kernel/chainparams\.h|'
    r'node/[^">]+|'
    r'rpc/[^">]+|'
    r'wallet/[^">]+|'
    r'net\.h|'
    r'net_processing\.h|'
    r'protocol\.h|'
    r'miner\.h'
    r')[>"]',
    re.MULTILINE,
)

FORBIDDEN_SYMBOL = re.compile(
    r"""\b(
        ConnectBlock|
        GetConvergenceDrawSetForBlock|
        CChainState|ChainstateManager|CCoinsViewDB|
        CDBWrapper|
        GetTime(?:Millis|Micros)?|GetSystemTimeInSeconds|GetMockTime|
        GetRand(?:Bytes)?|GetStrongRandBytes|FastRandomContext|InsecureRand|
        thread_local|
        std::getenv|getenv|
        std::filesystem
    )\b""",
    re.VERBOSE,
)

STRING_OR_COMMENT = re.compile(
    r"""
        //.*?$
        | /\*.*?\*/
        | "(?:\\.|[^"\\])*"
        | '(?:\\.|[^'\\])*'
    """,
    re.VERBOSE | re.DOTALL | re.MULTILINE,
)


def strip_noise(text: str) -> str:
    return STRING_OR_COMMENT.sub(" ", text)


def scan_includes(text: str) -> list[str]:
    return [m.group(0).strip() for m in FORBIDDEN_INCLUDE.finditer(text)]


def scan_symbols(text: str) -> list[str]:
    return sorted(set(FORBIDDEN_SYMBOL.findall(strip_noise(text))))


def lint_file(path: Path) -> list[str]:
    text = path.read_text(encoding="utf-8")
    findings: list[str] = []
    for inc in scan_includes(text):
        findings.append(f"forbidden include: {inc}")
    for sym in scan_symbols(text):
        findings.append(f"forbidden symbol: {sym}")
    return findings


def run_self_test() -> int:
    dirty = (
        '#include <validation.h>\n'
        '#include <wallet/wallet.h>\n'
        "void f() { ConnectBlock(); GetTime(); }\n"
    )
    clean = (
        '#include <consensus/validation.h>\n'
        '#include <consensus/params.h>\n'
        "CAmount GetBlockSubsidy(int nHeight, const Consensus::Params& p);\n"
    )
    dirty_inc = scan_includes(dirty)
    dirty_sym = scan_symbols(dirty)
    if not dirty_inc or not dirty_sym:
        print("SELF-TEST FAIL: detector did not fire on synthetic violation", file=sys.stderr)
        print(f"  includes={dirty_inc} symbols={dirty_sym}", file=sys.stderr)
        return 1
    if scan_includes(clean) or scan_symbols(clean):
        print("SELF-TEST FAIL: detector fired on allowed consensus/validation.h", file=sys.stderr)
        return 1
    if "validation.h" not in " ".join(dirty_inc):
        print("SELF-TEST FAIL: missed #include <validation.h>", file=sys.stderr)
        return 1
    print("self-test PASS (synthetic validation.h + ConnectBlock + GetTime detected)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return run_self_test()

    missing = [p for p in KERNEL_FILES if not (ROOT / p).is_file()]
    if missing:
        print("lint-consensus-kernel-purity: missing Kernel files:", file=sys.stderr)
        for p in missing:
            print(f"  {p}", file=sys.stderr)
        return 1

    failed = 0
    for rel in KERNEL_FILES:
        findings = lint_file(ROOT / rel)
        if findings:
            failed = 1
            for f in findings:
                print(f"{rel}: {f}")
        else:
            print(f"{rel}: PASS")
    if failed:
        print(
            "\nlint-consensus-kernel-purity FAIL. "
            "A Kernel file includes or names a forbidden orchestration symbol.",
            file=sys.stderr,
        )
        return 1
    print("lint-consensus-kernel-purity PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())

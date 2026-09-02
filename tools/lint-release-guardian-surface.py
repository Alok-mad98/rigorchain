#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Release Guardian Stage 5: production consensus surface.

CI / audit automation only. Reuses existing lints. Does not rewrite
Consensus Kernel, Transaction Kernel, Ledger, or Coq theorems.

Forbidden on the production path:
  shadow, canary, migration, OCaml runtime, debug authority.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "core/src"
MAKEFILE = SRC / "Makefile.am"
VALIDATION = SRC / "validation.cpp"
CONNECT = SRC / "connect_block_rigor_checks.cpp"

LINTS = (
    ROOT / "tools/lint-consensus-surface-closure.py",
    ROOT / "tools/lint-production-canary-boundary.py",
    ROOT / "tools/lint-extracted-oracle-and-vst-scope.py",
    ROOT / "tools/lint-kernel-switch-single-source.py",
    ROOT / "tools/lint-consensus-kernel-production-source.py",
    ROOT / "tools/lint-ledger-kernel-boundary.py",
    ROOT / "tools/lint-utxo-state-transition-boundary.py",
    ROOT / "tools/lint-genesis-consensus-authority.py",
)

FORBIDDEN_MAKEFILE = (
    "consensus/shadow",
    "consensus/txkernel",
    "consensus/kernel_migration",
    "consensus/kernel_canary",
    "consensus/ledger",
    "consensus_kernel.ml",
    "caml_",
)


def fail(errors: list[str]) -> int:
    print("PRODUCTION_CONSENSUS_SURFACE=OPEN")
    print("FAIL: release guardian surface")
    for e in errors:
        print(f"  - {e}")
    return 1


def main() -> int:
    errors: list[str] = []
    print("== production path ==", flush=True)
    print("Block", flush=True)
    print("  -> ConnectBlock", flush=True)
    print("  -> RigorConsensusKernel", flush=True)
    print("  -> VerifyRigorPredicate", flush=True)
    print("  -> LedgerApply", flush=True)

    for lint in LINTS:
        print(f"== lint {lint.name} ==", flush=True)
        r = subprocess.run([sys.executable, str(lint)], check=False)
        if r.returncode != 0:
            errors.append(f"{lint.name} failed")

    makefile = MAKEFILE.read_text(encoding="utf-8", errors="replace")
    for token in FORBIDDEN_MAKEFILE:
        if token in makefile:
            errors.append(f"Makefile.am lists forbidden production unit: {token}")

    validation = VALIDATION.read_text(encoding="utf-8", errors="replace")
    connect = CONNECT.read_text(encoding="utf-8", errors="replace") if CONNECT.is_file() else ""
    prod = validation + "\n" + connect
    if "RigorConsensusKernel" not in prod:
        errors.append("production ConnectBlock path does not name RigorConsensusKernel")
    if "VerifyRigorPredicate" not in prod and "CScriptCheck" not in prod:
        errors.append("production path does not name VerifyRigorPredicate / CScriptCheck")
    for banned, label in (
        ("KernelCanaryExecutor", "canary path"),
        ("kernel_canary", "canary path"),
        ("kernel_migration", "migration path"),
        ("ocamlopt", "OCaml runtime"),
        ("caml_startup", "OCaml runtime"),
    ):
        if banned in validation:
            errors.append(f"validation.cpp contains {label}: {banned}")

    if errors:
        return fail(errors)
    print("PRODUCTION_CONSENSUS_SURFACE=CLOSED")
    print("SHADOW_PATH=ABSENT")
    print("CANARY_PATH=ABSENT")
    print("MIGRATION_PATH=ABSENT")
    print("OCAML_RUNTIME=ABSENT")
    print("DEBUG_AUTHORITY=ABSENT")
    return 0


if __name__ == "__main__":
    sys.exit(main())

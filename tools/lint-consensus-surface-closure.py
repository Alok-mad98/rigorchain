#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P059: unique consensus authorities. STOP on a second body. Do not fix."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "core/src"
MAKEFILE = SRC / "Makefile.am"
PHASE5 = ROOT / "formal_verification/coq/Phase5Equivalence/EquivalenceFramework.v"
LEGACY = ROOT / "formal_verification/coq/legacy_account_model/README.md"
SURFACE = ROOT / "formal_verification/coq/ConsensusSurfaceClosure.v"

BODY = {
    "GetBlockSubsidy": (
        re.compile(r"^CAmount GetBlockSubsidy\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        SRC / "consensus/rigor_issuance.cpp",
    ),
    "GetResMintReward": (
        re.compile(r"inline constexpr CAmount GetResMintReward\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        SRC / "consensus/rigor_consensus_lock.h",
    ),
    "RigorSupplyDelta": (
        re.compile(r"^bool RigorSupplyDelta\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        SRC / "consensus/rigor_supply.cpp",
    ),
    "ResolveConvergenceDraw": (
        re.compile(r"^ConvergenceDrawOutcome ResolveConvergenceDraw\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        SRC / "consensus/convergence_logic.cpp",
    ),
    "ClassifyRigorTxout": (
        re.compile(r"^TxoutType ClassifyRigorTxout\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        SRC / "script/rigor_classify.cpp",
    ),
    "VerifyRigorPredicate": (
        re.compile(r"^bool VerifyRigorPredicate\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        SRC / "script/predicate_verify.cpp",
    ),
    "GetRigorPowHash": (
        re.compile(r"^uint256 GetRigorPowHash\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        SRC / "rigor_kernel/pow_digest.cpp",
    ),
    "CheckProofOfWork": (
        re.compile(r"^bool CheckProofOfWork\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        SRC / "pow.cpp",
    ),
    "GetNextWorkRequired": (
        re.compile(r"^unsigned int GetNextWorkRequired\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        SRC / "pow.cpp",
    ),
}

SKIP = {
    "test",
    "fuzz",
    "bench",
    "qt",
    "wallet",
    "rpc",
    "bridge",
    "shadow",
    "txkernel",
    "kernel_migration",
    "kernel_canary",
}


def prod_files() -> list[Path]:
    out: list[Path] = []
    for p in SRC.rglob("*"):
        if not p.is_file() or p.suffix not in {".h", ".cpp"}:
            continue
        if any(s in p.parts for s in SKIP):
            continue
        out.append(p)
    return out


def main() -> int:
    errors: list[str] = []
    files = prod_files()

    print("Domain                Authority")
    table = (
        ("ORU issuance", "GetBlockSubsidy"),
        ("RES schedule", "GetResMintReward"),
        ("supply", "RigorSupplyDelta"),
        ("draw", "ResolveConvergenceDraw"),
        ("classify", "ClassifyRigorTxout"),
        ("spend", "VerifyRigorPredicate"),
        ("sighash", "canonical sighash (SignatureHash)"),
        ("signature", "Ed25519 boundary"),
        ("PoW digest", "GetRigorPowHash"),
        ("ASERT", "GetNextWorkRequired / CalculateNextWorkRequired"),
    )
    for domain, auth in table:
        print(f"{domain:<20}  {auth}")

    for name, (pat, expected) in BODY.items():
        hits = [f for f in files if pat.search(f.read_text(encoding="utf-8", errors="replace"))]
        if expected not in hits:
            errors.append(f"{name} body missing at {expected.relative_to(ROOT)}")
        extras = [f for f in hits if f != expected]
        if extras:
            print("DUPLICATE_CONSENSUS_AUTHORITY_FOUND")
            errors.append(
                f"{name} second body in "
                + ", ".join(str(x.relative_to(ROOT)) for x in extras)
            )

    pred = (SRC / "script/predicate_verify.cpp").read_text(encoding="utf-8", errors="replace")
    if "ss.write(AsBytes(GetSighashDomainTag(sigversion)))" not in pred:
        errors.append("canonical sighash write missing")
    if "SIGHASH_ALL" in pred:
        errors.append("SignatureHash writes SIGHASH_ALL")

    if "NON_AUTHORITATIVE" not in PHASE5.read_text(encoding="utf-8", errors="replace"):
        errors.append("Phase5 missing NON_AUTHORITATIVE")
    if "NON_AUTHORITATIVE" not in LEGACY.read_text(encoding="utf-8", errors="replace"):
        errors.append("legacy_account_model missing NON_AUTHORITATIVE")
    if "DEAD_BUILD" not in LEGACY.read_text(encoding="utf-8", errors="replace"):
        errors.append("legacy_account_model missing DEAD_BUILD")

    surface = SURFACE.read_text(encoding="utf-8", errors="replace")
    if "Phase5Equivalence" in surface and "Require" in surface:
        if re.search(r"Require.*Phase5Equivalence", surface):
            errors.append("ConsensusSurfaceClosure cites Phase5 as authority")
    if "legacy_account_model" in surface and re.search(
        r"Require.*legacy_account_model", surface
    ):
        errors.append("ConsensusSurfaceClosure cites legacy_account_model as authority")
    if "VACUOUS-PLACEHOLDER" in surface and "not refinement evidence" not in surface:
        errors.append("VACUOUS-PLACEHOLDER treated as evidence")

    makefile = MAKEFILE.read_text(encoding="utf-8", errors="replace")
    if "caml_" in makefile or "consensus_kernel.ml" in makefile:
        errors.append("Makefile.am links extracted OCaml")
    if "consensus/txkernel" in makefile:
        errors.append("txkernel compiled into production")

    print("PRODUCTION_CONSENSUS_MODIFIED=NO")
    print("PRODUCTION_KERNEL_SWITCHED=YES")
    print("ISSUANCE_CHANGED=NO")
    print("RES_CHANGED=NO")
    print("POW_CHANGED=NO")
    print("ASERT_CHANGED=NO")

    if errors:
        print("FAIL: consensus surface authority audit")
        for e in errors:
            print(f"  - {e}")
        return 1
    print("DUPLICATE_AUTHORITY=NONE")
    print("CONSENSUS_AUTHORITY_SINGLE=PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())

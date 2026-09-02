#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P062: reject audit-scope overclaims. FAIL => AUDIT_SCOPE_OVERCLAIM.

Does not rewrite consensus. Does not flip honesty flags.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from refinement_status import print_status, records_cpp_authority  # noqa: E402
MANIFEST = ROOT / "formal_verification/rigor_consensus_audit_manifest.json"
VECTORS = ROOT / "formal_verification/release_consensus_vectors.json"

# Positive overclaims only. "equivalence_proved = false" is allowed.
# Do-not-set warnings that mention the token are not claims.
OVERCLAIM = (
    re.compile(r'["\']equivalence_proved["\']\s*:\s*true', re.IGNORECASE),
    re.compile(r"\bequivalence_proved\b[^\n]{0,40}:=\s*true", re.IGNORECASE),
    re.compile(r"^\s*EQUIVALENCE_PROVED\s*=\s*TRUE\s*$", re.IGNORECASE | re.MULTILINE),
    re.compile(r"\bfull_node_verified\b"),
    re.compile(r"\bcomplete_protocol_verified\b"),
    re.compile(r"\bCompCert_verified\b"),
    re.compile(r"\bVST_verified\b"),
)

SKIP_DIRS = {
    ".git",
    "node_modules",
    "extracted",
    "__pycache__",
    ".vo",
}
SKIP_SUFFIX = {".vo", ".glob", ".aux", ".o", ".cmx", ".cmi", ".cmo"}
SELF = Path(__file__).name

SCAN_ROOTS = (
    ROOT / "docs",
    ROOT / "formal_verification",
    ROOT / "tools",
    ROOT / "core/src/consensus",
    ROOT / "core/src/script",
    ROOT / ".github",
)


def iter_files() -> list[Path]:
    out: list[Path] = []
    for base in SCAN_ROOTS:
        if not base.exists():
            continue
        for p in base.rglob("*"):
            if not p.is_file():
                continue
            if p.name == SELF:
                continue
            if any(s in p.parts for s in SKIP_DIRS):
                continue
            if p.suffix in SKIP_SUFFIX:
                continue
            if p.suffix not in {".md", ".json", ".v", ".py", ".sh", ".yml", ".yaml", ".cpp", ".h"}:
                continue
            out.append(p)
    return out


def main() -> int:
    errors: list[str] = []

    man = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if man.get("protocol") != "Rigor":
        errors.append("audit manifest protocol")
    ck = man.get("consensus_kernel") or {}
    if ck.get("status") != "closed":
        errors.append("audit manifest kernel status")
    if ck.get("refinement") is not True:
        errors.append("audit manifest refinement")
    if ck.get("coq_model_closure") != "PROVED":
        errors.append("audit manifest coq_model_closure")
    if ck.get("coq_to_cpp") != "VECTOR-TESTED":
        errors.append("audit manifest coq_to_cpp")
    if ck.get("extracted_to_binary") != "NOT-LINKED":
        errors.append("audit manifest extracted_to_binary")
    if ck.get("equivalence") is not False:
        errors.append("audit manifest equivalence must be false")
    auth = man.get("authorities") or {}
    expect = {
        "issuance": "GetBlockSubsidy",
        "res_schedule": "GetResMintReward",
        "supply": "RigorSupplyDelta",
        "draw": "ResolveConvergenceDraw",
        "classification": "ClassifyRigorTxout",
        "spend": "VerifyRigorPredicate",
        "pow": "PowBoundary",
    }
    for k, v in expect.items():
        if auth.get(k) != v:
            errors.append(f"audit manifest authority {k}")
    pk = man.get("production_kernel") or {}
    if pk.get("implementation") != "C++" or pk.get("switched_to_extraction") is not False:
        errors.append("audit manifest production kernel")
    if not records_cpp_authority(pk):
        errors.append("audit manifest must record C++ production authority")
    formal = man.get("formal") or {}
    if formal.get("coq_admitted") or formal.get("coq_axioms") or formal.get("coq_parameters"):
        errors.append("audit manifest formal counts")

    vec = json.loads(VECTORS.read_text(encoding="utf-8"))
    if vec.get("equivalence_proved") is not False:
        errors.append("release vectors equivalence")
    g = vec.get("genesis") or {}
    if g.get("height") != 0 or g.get("oru_actual") != 0 or g.get("res_actual") != 0:
        errors.append("release genesis actuals")
    if g.get("res_schedule") != 40000000 or g.get("draw") != "none":
        errors.append("release genesis schedule/draw")
    heights = [e.get("height") for e in vec.get("epoch") or []]
    if heights != [0, 1, 99, 100, 101, 209999, 210000, 419999]:
        errors.append("release epoch heights")
    tx_ids = [t.get("id") for t in vec.get("transaction") or []]
    if tx_ids != ["valid_spend", "invalid_predicate", "bad_signature", "malformed_witness"]:
        errors.append("release transaction cases")
    draw_ids = [d.get("id") for d in vec.get("draw") or []]
    if draw_ids != ["empty", "single", "weight_tie", "txid_tie_break"]:
        errors.append("release draw cases")

    for path in iter_files():
        text = path.read_text(encoding="utf-8", errors="replace")
        for i, line in enumerate(text.splitlines(), 1):
            if any(pat.search(line) for pat in OVERCLAIM):
                errors.append(f"{path.relative_to(ROOT)}:{i}: {line.strip()}")

    if errors:
        print("AUDIT_SCOPE_OVERCLAIM")
        print("FAIL: audit scope overclaim or package drift")
        for e in errors:
            print(f"  - {e}")
        return 1

    print("AUDIT_SCOPE=PASS")
    print("AUDIT_PACKAGE_STATUS=PASS")
    print_status()
    return 0


if __name__ == "__main__":
    sys.exit(main())

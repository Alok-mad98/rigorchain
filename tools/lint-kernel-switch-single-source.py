#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Retired PRODUCTION_KERNEL_SWITCHED must stay gone.

Status is four independent facts from tools/refinement_status.py.
Reintroducing PRODUCTION_KERNEL_SWITCHED as a bool/echo/JSON field is a
FAIL: that name collapsed "C++ is production" and "extracted is not".
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from refinement_status import (  # noqa: E402
    AUTHORITY,
    HEADER,
    StatusError,
    json_status_fields,
    load_header,
    print_status,
)

COQ_FILES = (
    ROOT / "formal_verification/coq/TransactionKernelRefinement.v",
    ROOT / "formal_verification/coq/MakeKernelInput.v",
    ROOT / "formal_verification/coq/LedgerTransitionRefinement.v",
)

# Status manifests that must exist and carry the four split fields.
# Retired-key scanning is not limited to this list: see iter_json_files.
STATUS_JSON_FILES = (
    ROOT / "formal_verification/rigor_formal_final_manifest.json",
    ROOT / "formal_verification/rigor_consensus_audit_manifest.json",
    ROOT / "formal_verification/consensus_kernel_single_authority.json",
    ROOT / "formal_verification/genesis_consensus_vectors.json",
    ROOT / "formal_verification/ledger_transition_vectors.json",
    ROOT / "formal_verification/utxo_state_transition_vectors.json",
    ROOT / "formal_verification/consensus_kernel_freeze_manifest.json",
    ROOT / "formal_verification/consensus_kernel_freeze_vectors.json",
    ROOT / "formal_verification/kernel_canary_vectors.json",
    ROOT / "formal_verification/kernel_migration_vectors.json",
    ROOT / "formal_verification/consensus_kernel_manifest.json",
    ROOT / "formal_verification/production_consensus_inventory.json",
    ROOT / "formal_verification/consensus_kernel_c_abi.json",
    ROOT / "formal_verification/genesis_consensus_authority_manifest.json",
    ROOT / "formal_verification/genesis_consensus_freeze_manifest.json",
    ROOT / "formal_verification/genesis_consensus_release_audit.json",
)

GATE_SCRIPTS = (
    ROOT / "formal_verification/run_transaction_kernel_refinement.sh",
    ROOT / "formal_verification/run_consensus_kernel_governance.sh",
    ROOT / "formal_verification/run_consensus_mainnet_readiness.sh",
    ROOT / "formal_verification/run_ledger_transition_refinement.sh",
    ROOT / "formal_verification/run_consensus_kernel_refinement_gate.sh",
)

RETIRED = re.compile(r"PRODUCTION_KERNEL_SWITCHED")
RETIRED_ASSIGN = re.compile(
    r"(?:inline\s+constexpr\s+bool\s+)?(?:PRODUCTION_KERNEL_SWITCHED|production_kernel_switched)\s*=(?!=)",
    re.I,
)
RETIRED_JSON_KEY = re.compile(
    r"""["'](?:PRODUCTION_KERNEL_SWITCHED|production_kernel_switched)["']\s*:""",
    re.I,
)
POLICY_WORDS = ("retired", "do not reintroduce", "must not", "forbid")
SKIP_DIRS = {
    ".git",
    "depends",
    "autom4te.cache",
    "__pycache__",
    ".mypy_cache",
    ".libs",
    "logs",
}
SKIP_SUFFIXES = {".o", ".a", ".so", ".pyc", ".vo", ".glob", ".aux"}
SCAN_SUFFIXES = {".py", ".sh", ".h", ".cpp", ".v", ".json", ".md", ".yml", ".yaml", ".am", ".include"}
SCAN_NAMES = {"Makefile", "Makefile.am", "CMakeLists.txt"}
# Files allowed to mention the retired name as a forbidden identifier.
POLICY_FILES = {
    "tools/refinement_status.py",
    "tools/lint-kernel-switch-single-source.py",
    "tools/tests/test_lint_kernel_switch.py",
    "tools/lint-consensus-kernel-freeze.py",
}
COQ_EXTRACTED = re.compile(
    r"Definition\s+extracted_kernel_production\s*:\s*bool\s*:=\s*false\s*\."
)
COQ_COMPCERT = re.compile(
    r"Definition\s+compcert_binary_verified_kernel\s*:\s*bool\s*:=\s*false\s*\."
)
COQ_SWITCHED = re.compile(
    r"Definition\s+production_kernel_switched\s*:"
)


def is_comment_line(rel: str, snippet: str) -> bool:
    s = snippet.lstrip()
    if rel.endswith((".py", ".sh")):
        return s.startswith("#")
    if rel.endswith((".h", ".cpp")):
        return s.startswith("//") or s.startswith("/*") or s.startswith("*")
    if rel.endswith(".v"):
        return s.startswith("(*") or s.startswith("*")
    if rel.endswith(".md"):
        return True
    return False


def retired_line_is_violation(rel: str, snippet: str) -> bool:
    """True iff this line reintroduces the retired flag as a live assignment."""
    if "PRODUCTION_KERNEL_SWITCHED" not in snippet and "production_kernel_switched" not in snippet.lower():
        return False
    rel_posix = rel.replace("\\", "/")
    if rel_posix in POLICY_FILES:
        return False
    lower = snippet.lower()
    if is_comment_line(rel_posix, snippet) and any(w in lower for w in POLICY_WORDS):
        return False
    if rel_posix.endswith(".md") and any(w in lower for w in POLICY_WORDS):
        return False
    return bool(RETIRED_ASSIGN.search(snippet) or RETIRED_JSON_KEY.search(snippet))


def iter_scan_files(root: Path) -> list[Path]:
    out: list[Path] = []
    for p in root.rglob("*"):
        if not p.is_file():
            continue
        if any(part in SKIP_DIRS for part in p.parts):
            continue
        if p.suffix in SKIP_SUFFIXES:
            continue
        if p.suffix in SCAN_SUFFIXES or p.name in SCAN_NAMES:
            out.append(p)
    return out


def iter_json_files(root: Path) -> list[Path]:
    """Every JSON file in the tree, minus SKIP_DIRS. Retired keys are not
    limited to STATUS_JSON_FILES."""
    out: list[Path] = []
    for p in root.rglob("*.json"):
        if not p.is_file():
            continue
        if any(part in SKIP_DIRS for part in p.parts):
            continue
        out.append(p)
    return out


def json_walk(
    obj: object,
) -> tuple[list[str], list[str], list[bool], list[bool], list[str]]:
    """authority, formal, extracted, compcert, retired-keys"""
    authorities: list[str] = []
    formal: list[str] = []
    extracted: list[bool] = []
    compcert: list[bool] = []
    retired: list[str] = []
    if isinstance(obj, dict):
        for k, v in obj.items():
            lk = k.lower()
            if lk in {"production_kernel_switched"}:
                retired.append(k)
            elif lk == "production_consensus_authority":
                authorities.append(str(v))
            elif lk == "formal_refinement_status":
                formal.append(str(v))
            elif lk == "extracted_kernel_production":
                if not isinstance(v, bool):
                    raise ValueError(f"{k} is not a bool")
                extracted.append(v)
            elif lk == "compcert_binary_verified_kernel":
                if not isinstance(v, bool):
                    raise ValueError(f"{k} is not a bool")
                compcert.append(v)
            else:
                a, f, e, c, r = json_walk(v)
                authorities.extend(a)
                formal.extend(f)
                extracted.extend(e)
                compcert.extend(c)
                retired.extend(r)
    elif isinstance(obj, list):
        for x in obj:
            a, f, e, c, r = json_walk(x)
            authorities.extend(a)
            formal.extend(f)
            extracted.extend(e)
            compcert.extend(c)
            retired.extend(r)
    return authorities, formal, extracted, compcert, retired


def main() -> int:
    errors: list[str] = []
    try:
        load_header(HEADER)
    except StatusError as e:
        print(e)
        return 1

    header = HEADER.read_text(encoding="utf-8")
    if "PRODUCTION_KERNEL_SWITCHED =" in header.replace(" ", ""):
        errors.append("genesis_authority.h reintroduced PRODUCTION_KERNEL_SWITCHED")
    if f'PRODUCTION_CONSENSUS_AUTHORITY[] = "{AUTHORITY}"' not in header.replace("  ", " "):
        if f'PRODUCTION_CONSENSUS_AUTHORITY[] = "{AUTHORITY}"' not in header:
            errors.append("genesis_authority.h missing PRODUCTION_CONSENSUS_AUTHORITY string")

    for path in COQ_FILES:
        text = path.read_text(encoding="utf-8")
        if COQ_SWITCHED.search(text):
            errors.append(f"{path.relative_to(ROOT)}: production_kernel_switched must be removed")
        if not COQ_EXTRACTED.search(text):
            errors.append(f"{path.relative_to(ROOT)}: missing extracted_kernel_production := false")
        if not COQ_COMPCERT.search(text):
            errors.append(f"{path.relative_to(ROOT)}: missing compcert_binary_verified_kernel := false")

    expect = json_status_fields()
    for path in iter_json_files(ROOT):
        rel = path.relative_to(ROOT).as_posix()
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, UnicodeDecodeError, json.JSONDecodeError):
            continue
        try:
            _a, _f, _e, _c, retired = json_walk(data)
        except ValueError as e:
            errors.append(f"{rel}: {e}")
            continue
        if retired and rel not in POLICY_FILES:
            errors.append(f"{rel}: retired switch field {retired}")

    for path in STATUS_JSON_FILES:
        if not path.is_file():
            errors.append(f"missing {path.relative_to(ROOT)}")
            continue
        data = json.loads(path.read_text(encoding="utf-8"))
        try:
            authorities, formal, extracted, compcert, retired = json_walk(data)
        except ValueError as e:
            errors.append(f"{path.relative_to(ROOT)}: {e}")
            continue
        if retired:
            errors.append(f"{path.relative_to(ROOT)}: retired switch field {retired}")
        if not authorities:
            errors.append(f"{path.relative_to(ROOT)}: missing production_consensus_authority")
        for a in authorities:
            if a != expect["production_consensus_authority"]:
                errors.append(f"{path.relative_to(ROOT)}: authority={a!r}")
        if not formal:
            errors.append(f"{path.relative_to(ROOT)}: missing formal_refinement_status")
        for f in formal:
            if f != expect["formal_refinement_status"]:
                errors.append(f"{path.relative_to(ROOT)}: formal_refinement_status={f!r}")
        if not extracted:
            errors.append(f"{path.relative_to(ROOT)}: missing extracted_kernel_production")
        for v in extracted:
            if v is not False:
                errors.append(f"{path.relative_to(ROOT)}: extracted_kernel_production={v}")
        if not compcert:
            errors.append(f"{path.relative_to(ROOT)}: missing compcert_binary_verified_kernel")
        for v in compcert:
            if v is not False:
                errors.append(f"{path.relative_to(ROOT)}: compcert_binary_verified_kernel={v}")

    for path in GATE_SCRIPTS:
        text = path.read_text(encoding="utf-8")
        if RETIRED.search(text) and "retired" not in text.lower():
            if "PRODUCTION_KERNEL_SWITCHED=" in text.replace(" ", "") or \
               "PRODUCTION_KERNEL_SWITCHED =" in text:
                errors.append(f"{path.relative_to(ROOT)}: echoes retired PRODUCTION_KERNEL_SWITCHED")
        if "refinement_status.py" not in text:
            errors.append(f"{path.relative_to(ROOT)}: must print status via refinement_status.py")

    canary_h = ROOT / "core/src/test/kernel_runtime_canary.h"
    if canary_h.is_file():
        ch = canary_h.read_text(encoding="utf-8")
        if RETIRED.search(ch) and "retired" not in ch.lower() and "do not reintroduce" not in ch.lower():
            errors.append("kernel_runtime_canary.h still uses PRODUCTION_KERNEL_SWITCHED")

    for path in iter_scan_files(ROOT):
        rel = path.relative_to(ROOT).as_posix()
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for i, snippet in enumerate(text.splitlines(), 1):
            if retired_line_is_violation(rel, snippet):
                errors.append(
                    f"{rel}:{i}: retired PRODUCTION_KERNEL_SWITCHED; "
                    "use tools/refinement_status.py"
                )

    if errors:
        print("FAIL: kernel status semantics are not split")
        for e in errors:
            print(f"  - {e}")
        return 1

    print("KERNEL_STATUS_SEMANTICS=SPLIT")
    print_status()
    return 0


if __name__ == "__main__":
    sys.exit(main())

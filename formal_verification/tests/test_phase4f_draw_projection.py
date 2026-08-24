#!/usr/bin/env python3
"""
Structural validation for PHASE-4F-001D draw winner projection.

Checks projection module/status presence, that the registry still executes,
and that the hardcoded `gtr_golden_match := false` placeholder is gone.
Does not run coqc.
"""

from __future__ import annotations

import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
COQ_DIR = REPO_ROOT / "formal_verification" / "coq" / "Phase4FConsensus"
DOC_DIR = REPO_ROOT / "docs" / "formal" / "phase4f_consensus"

PROJECTION_V = COQ_DIR / "ConsensusDrawProjection.v"
GOLDEN_TRACE_V = COQ_DIR / "ConsensusGoldenTrace.v"
EXECUTION_V = COQ_DIR / "ConsensusTraceExecution.v"
AUDIT_V = COQ_DIR / "ConsensusDrawAudit.v"


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def check_projection_module() -> list[str]:
    errors: list[str] = []
    if not PROJECTION_V.is_file():
        return ["missing ConsensusDrawProjection.v"]
    text = read_text(PROJECTION_V)
    needles = (
        "Inductive DrawProjectionStatus",
        "DPS_Match",
        "DPS_Mismatch",
        "DPS_NotApplicable",
        "Definition project_draw_result",
        "Definition compare_projection_to_golden",
        "Record DrawProjectionResult",
    )
    errors += [f"ConsensusDrawProjection.v missing `{n}`" for n in needles if n not in text]
    return errors


def check_registry_executes() -> list[str]:
    errors: list[str] = []
    if not EXECUTION_V.is_file():
        return ["missing ConsensusTraceExecution.v"]
    text = read_text(EXECUTION_V)
    for n in (
        "Definition execute_registry",
        "registry_projection_results",
        "registry_match_count",
        "registry_mismatch_count",
        "registry_na_count",
    ):
        if n not in text:
            errors.append(f"ConsensusTraceExecution.v missing `{n}`")
    return errors


def check_no_placeholder_false() -> list[str]:
    errors: list[str] = []
    if not GOLDEN_TRACE_V.is_file():
        return ["missing ConsensusGoldenTrace.v"]
    text = read_text(GOLDEN_TRACE_V)
    if "gtr_golden_match := false" in text:
        errors.append("ConsensusGoldenTrace.v: placeholder `gtr_golden_match := false` still present")
    if "meta_golden_match (tc_meta c)" not in text:
        errors.append("ConsensusGoldenTrace.v: gtr_golden_match not derived from meta_golden_match")
    return errors


def check_audit_module() -> list[str]:
    errors: list[str] = []
    if not AUDIT_V.is_file():
        return ["missing ConsensusDrawAudit.v"]
    text = read_text(AUDIT_V)
    for n in (
        "Definition IndependentWinnerSource",
        "Definition ImportedWinnerSource",
        "Inductive WinnerComparisonResult",
        "Definition winner_sources_agree",
        "Definition winner_sources_disagree",
        "Definition recompute_winner",
        "audit_match_count_value",
        "audit_mismatch_count_value",
    ):
        if n not in text:
            errors.append(f"ConsensusDrawAudit.v missing `{n}`")
    return errors


def check_docs() -> list[str] | None:
    """None means SKIP: docs/formal/ is wholly absent from this tree.

    Partial formal trees (e.g. RFC-017-only) skip missing Phase-4F draw pages;
    present pages must still satisfy the section checklist.
    """
    formal_root = REPO_ROOT / "docs" / "formal"
    if not formal_root.is_dir() or not any(formal_root.rglob("*.md")):
        return None
    errors: list[str] = []
    for name, needles in {
        "DRAW_PROJECTION_AUDIT.md": ["## 3. Match counts", "MISMATCH", "N/A"],
        "DRAW_PROJECTION_MATRIX.md": ["Golden Winner", "Coq Winner", "Result"],
        "DRAW_RECOMPUTATION_AUDIT.md": ["Imported Winner", "Recomputed Winner", "Root cause"],
        "CIRCULARITY_ANALYSIS.md": ["Q1 —", "Q2 —", "Q3 —", "Q4 —"],
    }.items():
        path = DOC_DIR / name
        if not path.is_file():
            continue
        text = read_text(path)
        errors += [f"{name}: missing `{n}`" for n in needles if n not in text]
    return errors


def main() -> int:
    all_errors: list[str] = []
    all_errors.extend(check_projection_module())
    all_errors.extend(check_registry_executes())
    all_errors.extend(check_no_placeholder_false())
    all_errors.extend(check_audit_module())
    docs = check_docs()
    if docs is None:
        print("SKIP: docs/formal/ absent (internal stage-gate audits; not part of this tree)")
    else:
        all_errors.extend(docs)

    if all_errors:
        print("FAIL: Phase4F draw projection structural validation")
        for err in all_errors:
            print(f"  - {err}")
        return 1

    print("PASS: Phase4F draw projection + recomputation audit (PHASE-4F-001D/001E)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

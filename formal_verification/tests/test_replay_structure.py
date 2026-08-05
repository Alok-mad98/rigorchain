#!/usr/bin/env python3
"""
Structural validation for PHASE-4B ReplayReorgCore (no cryptography).

TASK-FORMAL-PHASE4B-REPLAY-REORG-CORE-001 — checks Coq module presence,
forbidden imports, and named definitions/theorems. Does not run coqc.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
CORE_DIR = REPO_ROOT / "formal_verification" / "coq" / "ReplayReorgCore"
AUDIT_DOC_4B = REPO_ROOT / "docs" / "formal" / "phase4b_replay" / "REPLAY_REORG_CORE_AUDIT.md"
AUDIT_DOC_4C = REPO_ROOT / "docs" / "formal" / "phase4c_replay" / "REPLAY_REORG_CLOSURE_AUDIT.md"
AUDIT_DOC_4D = REPO_ROOT / "docs" / "formal" / "phase4d_supply" / "SUPPLY_CONSERVATION_AUDIT.md"
AUDIT_DOC_4E = REPO_ROOT / "docs" / "formal" / "phase4e_connectblock" / "CONNECTBLOCK_SUPPLY_AUDIT.md"

FORBIDDEN_IMPORTS = (
    "RigorSha256",
    "ConvergeRoot",
    "RigorHash",
    "SigmaCR01",
    "Execution",
)

MODULE_EXPECTATIONS: dict[str, list[str]] = {
    "Hash32Order.v": [
        "Definition hash32_lt",
        "Theorem hash32_lt_total",
        "Theorem hash32_lt_transitive",
        "Theorem hash32_lt_antisymmetric",
    ],
    "ReplayState.v": [
        "Record ReplayState",
        "Definition UTXOSet",
        "Record BlockAbs",
        "Definition init_replay_state",
    ],
    "StateTransition.v": [
        "Definition apply_block",
        "fold_apply_blocks",
        "valid_block_sequence",
        "valid_block_sequence_fold_some",
    ],
    "ReorgRules.v": [
        "Definition rollback",
        "Definition select_best_fork",
        "Definition better_chain",
    ],
    "ReplayDeterminism.v": [
        "Theorem Replay_Determinism",
        "Theorem fork_resolution_deterministic",
        "Theorem replay_reorg_commutes",
    ],
    "ReplayInvariants.v": [
        "Definition utxo_non_negative",
        "Theorem utxo_spend_removes_output",
        "Theorem utxo_create_adds_output",
        "Theorem rollback_restores_previous_state",
    ],
    "ConnectBlockMint.v": [
        "Definition cb_RES_MINT_AMOUNT",
        "Definition cb_oru_mint_delta",
        "Definition cb_res_mint_delta",
        "Definition apply_connectblock_supply_delta",
        "Definition supply_evolution_eq",
        "Definition protocol_mint_total_delta",
        "Lemma protocol_mint_total_delta_non_negative",
    ],
    "SupplyConservation.v": [
        "Definition total_supply",
        "Theorem apply_block_supply_correct",
        "Theorem supply_deterministic",
        "Theorem supply_monotonic",
        "Theorem rollback_preserves_supply",
        "Theorem reorg_replay_preserves_winning_supply",
        "Theorem valid_sequence_supply_accumulation",
    ],
    "SupplyReplayBridge.v": [
        "Require Export Rigor.ReplayReorgCore.SupplyConservation",
    ],
}

ADMIT_TAG = "ADMIT-PHASE4B"


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def check_forbidden_imports() -> list[str]:
    errors: list[str] = []
    for vfile in CORE_DIR.glob("*.v"):
        text = read_text(vfile)
        for forbidden in FORBIDDEN_IMPORTS:
            if forbidden in text:
                errors.append(f"{vfile.name}: forbidden reference to {forbidden}")
    return errors


def check_definitions() -> list[str]:
    errors: list[str] = []
    for filename, needles in MODULE_EXPECTATIONS.items():
        path = CORE_DIR / filename
        if not path.is_file():
            errors.append(f"missing module file: {filename}")
            continue
        text = read_text(path)
        for needle in needles:
            if needle not in text:
                errors.append(f"{filename}: missing `{needle}`")
    return errors


def check_admits_phase4b_closed() -> list[str]:
    errors: list[str] = []
    path = CORE_DIR / "ReplayDeterminism.v"
    text = read_text(path)
    if ADMIT_TAG in text:
        errors.append(f"ReplayDeterminism.v: stale {ADMIT_TAG} tag must be removed")
    if re.search(r"\bAdmitted\b", text):
        errors.append("ReplayDeterminism.v: PHASE-4B admits must be closed (no Admitted)")
    return errors


def check_audit_doc() -> list[str]:
    errors: list[str] = []
    if not AUDIT_DOC_4B.is_file():
        errors.append(f"missing audit doc: {AUDIT_DOC_4B}")
    if not AUDIT_DOC_4C.is_file():
        errors.append(f"missing audit doc: {AUDIT_DOC_4C}")
    else:
        text = read_text(AUDIT_DOC_4C)
        for section in (
            "## Closed Proofs",
            "## Remaining Admits",
            "## Remaining Axioms",
            "## Counterexamples",
            "## Soundness Assessments",
        ):
            if section not in text:
                errors.append(f"phase4c audit doc missing section: {section}")
    if not AUDIT_DOC_4D.is_file():
        errors.append(f"missing audit doc: {AUDIT_DOC_4D}")
    else:
        text4d = read_text(AUDIT_DOC_4D)
        for section in (
            "## Theorem List",
            "## Proof Status",
            "## Remaining Admits",
            "## Remaining Axioms",
            "## Counterexamples",
            "## Supply Risk Assessment",
        ):
            if section not in text4d:
                errors.append(f"phase4d audit doc missing section: {section}")
    if not AUDIT_DOC_4E.is_file():
        errors.append(f"missing audit doc: {AUDIT_DOC_4E}")
    else:
        text4e = read_text(AUDIT_DOC_4E)
        for section in (
            "## Theorem List",
            "## Proof Status",
            "## Remaining Admits",
            "## Remaining Axioms",
            "## Required Output Fields",
            "## Review Gate",
        ):
            if section not in text4e:
                errors.append(f"phase4e audit doc missing section: {section}")
    return errors


def check_coq_project() -> list[str]:
    errors: list[str] = []
    proj = CORE_DIR / "_CoqProject"
    if not proj.is_file():
        return ["missing _CoqProject"]
    lines = [ln.strip() for ln in read_text(proj).splitlines() if ln.strip()]
    for mod in MODULE_EXPECTATIONS:
        if mod not in lines:
            errors.append(f"_CoqProject: missing {mod}")
    return errors


def main() -> int:
    all_errors: list[str] = []
    all_errors.extend(check_coq_project())
    all_errors.extend(check_forbidden_imports())
    all_errors.extend(check_definitions())
    all_errors.extend(check_admits_phase4b_closed())
    all_errors.extend(check_audit_doc())

    if all_errors:
        print("FAIL: ReplayReorgCore structural validation")
        for err in all_errors:
            print(f"  - {err}")
        return 1

    print("PASS: ReplayReorgCore structural validation (PHASE-4C/4D/4E modules + audit)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

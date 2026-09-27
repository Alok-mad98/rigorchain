#!/usr/bin/env python3
"""
Structural validation for PHASE-4B ReplayReorgCore (no cryptography).

 — checks Coq module presence,
forbidden imports, and named definitions/theorems. Does not run coqc.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
CORE_DIR = REPO_ROOT / "formal_verification" / "archive" / "lottery" / "coq" / "ReplayReorgCore"
AUDIT_DOC_4B = REPO_ROOT / "docs" / "formal" / "phase4b_replay" / "REPLAY_REORG_CORE_AUDIT.md"
AUDIT_DOC_4C = REPO_ROOT / "docs" / "formal" / "phase4c_replay" / "REPLAY_REORG_CLOSURE_AUDIT.md"
AUDIT_DOC_4D = (
    REPO_ROOT
    / "archive"
    / "p0-history"
    / "docs"
    / "formal"
    / "phase4d_supply"
    / "SUPPLY_CONSERVATION_AUDIT.md"
)
AUDIT_DOC_4E = (
    REPO_ROOT
    / "archive"
    / "p0-history"
    / "docs"
    / "formal"
    / "phase4e_connectblock"
    / "CONNECTBLOCK_SUPPLY_AUDIT.md"
)

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
        "tx_id : Hash32",
        "tx_pcorb : option",
    ],
    "StateTransition.v": [
        "Definition apply_block",
        "fold_apply_blocks",
        "valid_block_sequence",
        "valid_block_sequence_fold_some",
        "Definition valid_block_connect",
        "Lemma valid_block_connect_res_mint_obligation",
        "Definition valid_dual_pool_tx",
        "Definition apply_tx_mint_reward",
        "Theorem apply_tx_succeeds_implies_dual_pool",
        "Theorem apply_tx_rejects_without_dual_pool",
        "Theorem apply_tx_succeeds_implies_script_shape",
        "Theorem apply_tx_rejects_without_script_shape",
        "Definition valid_script_shape_tx",
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
        "Theorem utxo_spend_removes_output_distinct_txid",
        "Theorem utxo_create_adds_output",
        "Theorem rollback_restores_previous_state",
        "Definition created_outpoint",
        "Lemma distinct_txid_not_recreated",
    ],
    "ConnectBlockMint.v": [
        "Definition cb_res_mint_amount",
        "Definition cb_oru_mint_delta",
        "Definition cb_res_mint_delta",
        "Definition apply_connectblock_supply_delta",
        "Definition supply_evolution_eq",
        "Definition protocol_mint_total_delta",
        "Lemma protocol_mint_total_delta_non_negative",
        "Definition valid_res_mint_obligation",
    ],
    "ResMintObligation.v": [
        "Theorem h1_valid_connect_zero_mints",
        "Theorem h1_winner_zero_reward_block_valid",
        "Theorem winner_must_mint",
        "Theorem winner_must_mint_exactly_W",
        "Theorem cap_overshoot_rejects_apply_block",
        "Theorem obligation_at_most_one_mint",
        "Theorem obligated_issuance_respects_schedule_cap",
    ],
    "DualPoolConservation.v": [
        "Theorem dual_pool_conservation",
        "Theorem mint_tx_increases_res_by_exactly_W",
        "Theorem mint_tx_cannot_increase_res_by_more_than_W",
        "Theorem mixed_inputs_cannot_mint_extra_res",
        "Theorem ordinary_tx_utxo_res_non_increasing",
        "Theorem mint_tx_utxo_res_plus_W",
        "Theorem apply_tx_ordinary_conserves_res",
        "Theorem apply_tx_res_net_from_scheduled_mint",
        "Theorem apply_block_dual_pool_supply_rollback",
        "Theorem apply_txs_res_net_from_scheduled_mint",
        "Theorem apply_block_res_invariant",
        "Theorem fold_apply_blocks_dual_pool_supply_rollback",
        "Theorem fold_apply_blocks_dual_pool_replay_deterministic",
        "Lemma apply_tx_rejects_res_inflation",
        "Lemma apply_tx_accepts_scheduled_mint",
        "Lemma apply_tx_rejects_h1_unscheduled_mint",
        "Lemma apply_block_body_rejects_res_inflation",
        "Lemma apply_tx_accepts_zero_res_oru_payment",
        "Lemma apply_tx_accepts_multi_res_conserved",
        "Lemma apply_tx_rejects_mint_plus_one_atom",
        "Lemma apply_tx_rejects_mint_minus_one_atom",
        "Lemma apply_txs_accepts_transfer_then_mint",
        "Lemma apply_txs_accepts_two_transfers_then_mint",
        "Lemma apply_block_body_accepts_two_transfers_then_mint",
    ],
    "ScriptShape.v": [
        "Definition created_script_shape_ok",
        "Definition valid_script_shape_on_ins",
        "Lemma valid_script_shape_on_ins_placeholder",
 "",
    ],
    "ScriptShapeLift.v": [
        "Theorem apply_tx_rejects_created_empty_script",
        "Theorem apply_tx_rejects_created_p2pkh",
        "Theorem apply_tx_rejects_created_p2sh",
        "Theorem apply_tx_rejects_created_v2",
        "Theorem apply_tx_rejects_created_v1_len20",
        "Theorem apply_tx_rejects_spend_cvg2",
        "Theorem apply_tx_rejects_pcorb_too_few",
        "Theorem apply_tx_rejects_pcorb_field_size",
        "Theorem allowed_v0_shape_ok",
        "Theorem created_script_shape_ok_not_invalid",
    ],
    "OpaqueBoundary.v": [
        "Theorem script_derives_pooling_class",
        "Theorem we_do_not_claim_cm_draw_won_determined_by_body",
        "Theorem we_do_not_claim_cm_draw_won_is_owner_pcorb",
        "Theorem we_do_not_claim_ci033_mint_authorization",
        "Theorem we_do_not_claim_rs_res_supply_equals_utxo_res",
        "Theorem valid_res_mint_obligation_ignores_body",
        "Theorem apply_tx_does_not_update_rs_res_supply",
        "Theorem apply_connectblock_supply_delta_does_not_update_utxo",
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


def _require_sections(path: Path, sections: tuple[str, ...], label: str) -> list[str]:
    errors: list[str] = []
    text = read_text(path)
    for section in sections:
        if section not in text:
            errors.append(f"{label} audit doc missing section: {section}")
    return errors


def check_audit_doc() -> list[str] | None:
    """None means SKIP: docs/formal/ is wholly absent from this tree.

    Partial trees are allowed: only enforce section checklists for audit pages
    that are present. Unshipped phase4b/4c pages do not fail the suite.
    Lottery-era phase4d + phase4e audits live under archive/p0-history;
    if either is missing while any formal markdown exists, that is a hard error.
    """
    formal_root = REPO_ROOT / "docs" / "formal"
    if not formal_root.is_dir() or not any(formal_root.rglob("*.md")):
        return None
    errors: list[str] = []
    if AUDIT_DOC_4C.is_file():
        errors.extend(
            _require_sections(
                AUDIT_DOC_4C,
                (
                    "## Closed Proofs",
                    "## Remaining Admits",
                    "## Remaining Axioms",
                    "## Counterexamples",
                    "## Soundness Assessments",
                ),
                "phase4c",
            )
        )
    if not AUDIT_DOC_4D.is_file():
        errors.append(f"missing audit doc: {AUDIT_DOC_4D}")
    else:
        errors.extend(
            _require_sections(
                AUDIT_DOC_4D,
                (
                    "## Theorem List",
                    "## Proof Status",
                    "## Remaining Admits",
                    "## Remaining Axioms",
                    "## Counterexamples",
                    "## Supply Risk Assessment",
                ),
                "phase4d",
            )
        )
    if not AUDIT_DOC_4E.is_file():
        errors.append(f"missing audit doc: {AUDIT_DOC_4E}")
    else:
        errors.extend(
            _require_sections(
                AUDIT_DOC_4E,
                (
                    "## Theorem List",
                    "## Proof Status",
                    "## Remaining Admits",
                    "## Remaining Axioms",
                    "## Required Output Fields",
                    "## Review Gate",
                ),
                "phase4e",
            )
        )
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
    docs = check_audit_doc()
    if docs is None:
        print("SKIP: docs/formal/ absent (internal stage-gate audits; not part of this tree)")
    else:
        all_errors.extend(docs)

    if all_errors:
        print("FAIL: ReplayReorgCore structural validation")
        for err in all_errors:
            print(f"  - {err}")
        return 1

    print("PASS: ReplayReorgCore structural validation (PHASE-4C/4D/4E modules + audit)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

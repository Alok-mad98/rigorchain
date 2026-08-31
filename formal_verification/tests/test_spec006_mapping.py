#!/usr/bin/env python3
"""F-3 / A3: SPEC-006 backtick Coq names must exist in the checked-in corpus.

Invented family names from P0-016 C-3 stay forbidden. Remaining identifier-like
backticks must resolve to a Definition/Theorem/… in some _CoqProject module,
or sit on an explicit non-Coq allowlist.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SPEC = REPO / "docs" / "specification" / "SPEC-006-RUNTIME-FORMAL-MAPPING.md"
COQ = REPO / "formal_verification" / "coq"
FULL = COQ / "_CoqProject"

FORBIDDEN = (
    "chain_validity_theorems",
    "determinism_theorems",
    "state_transition_theorems",
    "supply_integrity_theorems",
    "replay_determinism_theorems",
    "reorg_consistency_theorems",
    "freeze_preservation_theorems",
    "chain_continuity_theorems",
    "fork_choice_theorems",
    "utxo_existence_theorems",
    "utxo_uniqueness_theorems",
    "oru_nonnegativity_theorems",
    "oru_supply_equation_theorems",
    "res_supply_equation_theorems",
    "res_cap_theorems",
    "single_mint_theorems",
    "draw_determinism_theorems",
    "selection_uniqueness_theorems",
    "freeze_target_theorems",
    "draw_mint_coherence_theorems",
    "parameter_freeze_theorems",
    "monetary_policy_theorems",
    "protocol_freeze_theorems",
    "ReplayStateEncodingTheorem",
    "ReplayEquivalence",
    "AntiEquivalenceSafety",
    "THM-05",
    "THM-04",
    "THM-03",
    "THM-02",
    "PoWState",
    "cb_oru_burn_delta",
)

REQUIRED = (
    "max_issuance_within_cap",
    "cap_overshoot_rejects_apply_block",
    "convergence_root_order_independent",
    "select_winner_order_independent",
    "apply_block_res_net_bounded_by_scheduled_mints",
    "oru_supply_bounded_by_block_count",
    "supply_deterministic",
    "rollback_restores_previous_state",
    "we_do_not_claim_ci033_mint_authorization",
    "NOT VERIFIED",
    "_CoqProject.protocol-core",
)

# Not Coq names. Each entry is a SPEC-006 backtick that must not be grepped
# as a theorem. Keep this list small; prefer a real Coq identifier.
ALLOW_NON_COQ = {
    "_CoqProject.protocol-core",
    "CheckProofOfWork",
    "ConnectBlock",
    "ConnectEvaluateCoinbaseResMint",
    "ExpandCompact",
    "GetResMintReward",
    "RIGOR_FORMAL_STATE_MODEL",
    "CONSENSUS_TRANSITION_SYSTEM",
    "ChainTip",
    "S_ORU",
    "S_RES",
    "UTXO",
    "nBits",
    "pow_tests",
    "res_proof_math_tests",
    "feature_rigor_convergence_rng_consensus.py",
    # Runtime / FVR observational keys (SPEC-006 §4–§8). Not Coq.
    "block_hash",
    "parent_hash",
    "height",
    "oru_supply_cumulative",
    "res_mint_cumulative",
    "res_max_supply",
    "res_reward_amount",
    "oru_subsidy_block",
    "oru_converge_burn_block",
    "res_mint_block",
    "res_mint_count_block",
    "chas_oru_state",
    "chas_res_state",
    "Comparable",
    "PartiallyComparable",
    "Hash256",
    "bind_commit",
}

IDENT_RE = re.compile(r"`([^`]+)`")
COQ_NAME_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_']*$")
NAMED_RE = re.compile(
    r"^\s*(?:Definition|Theorem|Lemma|Corollary|Proposition|Remark|Fact|"
    r"Example|Record|Inductive|Fixpoint|CoFixpoint|Class|Instance|"
    r"Axiom|Parameter|Structure)\s+([A-Za-z_][A-Za-z0-9_']*)\b",
    re.MULTILINE,
)
FIELD_RE = re.compile(r"^\s+([A-Za-z_][A-Za-z0-9_']*)\s*:", re.MULTILINE)
CTOR_RE = re.compile(r"^\s*\|\s*([A-Za-z_][A-Za-z0-9_']*)\b", re.MULTILINE)


def project_v_entries(path: Path) -> list[str]:
    out: list[str] = []
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or line.startswith("-"):
            continue
        if line.endswith(".v"):
            out.append(line)
    return out


def named_in_corpus(modules: list[str]) -> set[str]:
    names: set[str] = set()
    for rel in modules:
        names.add(Path(rel).stem)
        text = re.sub(r"\(\*.*?\*\)", " ", (COQ / rel).read_text(encoding="utf-8"), flags=re.DOTALL)
        names.update(NAMED_RE.findall(text))
        names.update(FIELD_RE.findall(text))
        names.update(CTOR_RE.findall(text))
    return names


def extract_backticks(text: str) -> list[str]:
    # Drop fenced code so example traces are not treated as mappings.
    stripped = re.sub(r"```.*?```", " ", text, flags=re.DOTALL)
    return IDENT_RE.findall(stripped)


def main() -> int:
    if not SPEC.is_file():
        print(f"FAIL: missing {SPEC}")
        return 1
    text = SPEC.read_text(encoding="utf-8")
    errors: list[str] = []
    for name in FORBIDDEN:
        if name in text:
            errors.append(f"invented identifier still present: {name}")
    for name in REQUIRED:
        if name not in text:
            errors.append(f"missing required identifier: {name}")

    modules = project_v_entries(FULL)
    basenames = {Path(rel).name for rel in modules}
    corpus = named_in_corpus(modules)

    for tok in extract_backticks(text):
        if tok in FORBIDDEN:
            continue
        if tok in ALLOW_NON_COQ:
            continue
        if tok.endswith(".v"):
            if tok not in basenames and tok not in modules:
                errors.append(f"SPEC-006 names missing Coq module: {tok}")
            continue
        if "." in tok or "/" in tok or " " in tok:
            continue
        if not COQ_NAME_RE.match(tok):
            continue
        if tok not in corpus:
            errors.append(f"SPEC-006 Coq identifier not in _CoqProject corpus: {tok}")

    if errors:
        print("FAIL: SPEC-006 mapping (F-3/A3)")
        for e in errors:
            print(f"  - {e}")
        return 1
    print("PASS: SPEC-006 maps to real Coq identifiers (F-3/A3)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

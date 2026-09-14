#!/usr/bin/env python3
"""Apply the P0-130 independent-audit reclassification to the theorem tables.

Idempotent. Rewrites release/TheoremClassification.md and the
evidence/ mirror so both carry the same Category A name-set.

Two kinds of edit:

  DEMOTIONS   theorems the audit found to be projections of their own
              hypothesis, definitional layering between clone records,
              or guarded by a mutation-only flag. They are kept, not
              deleted, but they stop counting as public-claim evidence.

  ADDITIONS   theorems written while closing C-1/C-2/M-4/M-7. The ones
              in A carry the claims the demoted rows used to carry.
"""
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

# name -> new category. Rationale is recorded in
# ../../independent_audit/IndependentAuditVerdict.md.
DEMOTIONS = {
    # projection between field-for-field clone records (proof: exact Hinv)
    "execution_projection_preserves_invariants": "C",
    # lower layer is DEFINED as the upper layer's step, so the
    # refinement holds by construction; the real obligation now lives
    # in machine_step_refines_executable_step
    "concrete_step_refines_abstract_step": "C",
    "executable_step_refines_concrete_step": "C",
    "runtime_step_refines_executable_step": "C",
    # determinism guarded by a *_step_unique constant that exists only
    # to be flipped by a mutant
    "kernel_transition_deterministic": "B",
    "executable_transition_deterministic": "B",
    "runtime_step_deterministic": "B",
    # true, but shallow algebra over a definition, not a consensus
    # invariant
    "oru_fees_are_transfer_only": "B",
    "oru_domain_conservation": "B",
    "burn_is_monotonic_supply_decrease": "B",
    "fees_only_transfer": "B",
    "burn_only_removal": "B",
    # normalisation restated as preservation
    "script_execution_preserves_class": "B",
    "concrete_state_preserves_invariant": "B",
}

# (name, file, domain, category)
ADDITIONS = [
    # --- C-2: the runtime layer is now an independent bounded-word
    # machine, so refinement is a real simulation obligation ---
    ("machine_step_refines_executable_step", "Runtime/MachineState.v",
     "Runtime refinement", "A"),
    ("machine_invariants_imply_representable", "Runtime/MachineState.v",
     "Runtime boundary", "A"),
    ("machine_step_failure_is_overflow", "Runtime/MachineState.v",
     "Runtime boundary", "A"),
    ("machine_refinement_is_partial", "Runtime/MachineState.v",
     "Runtime refinement", "B"),
    ("machine_decode_is_partial", "Runtime/MachineState.v",
     "Runtime boundary", "B"),
    ("representable_does_not_imply_valid", "Runtime/MachineState.v",
     "Runtime boundary", "B"),
    ("machine_step_deterministic_real", "Runtime/MachineState.v",
     "Runtime refinement", "B"),
    ("w_add_overflow_reachable", "Runtime/MachineWord.v",
     "Runtime integer domain", "B"),
    ("w_add_negative_result_rejected", "Runtime/MachineWord.v",
     "Runtime integer domain", "B"),
    ("word_domain_strictly_smaller_than_Z", "Runtime/MachineWord.v",
     "Runtime integer domain", "B"),
    ("runtime_state_can_be_unrepresentable", "Runtime/RuntimeState.v",
     "Runtime boundary", "B"),

    # --- execution boundary: invariant preservation, which the layer
    # never actually stated ---
    ("executable_step_preserves_invariants",
     "ExecutionBoundary/ExecutableInvariance.v", "Execution boundary", "A"),
    ("executable_preservation_hypotheses_are_inhabited",
     "ExecutionBoundary/ExecutableInvariance.v", "Execution boundary", "B"),
    ("oru_flow_condition_is_necessary",
     "ExecutionBoundary/ExecutableInvariance.v", "Execution boundary", "B"),
    ("sample_state_satisfies_invariants",
     "ExecutionBoundary/ExecutableInvariance.v", "Execution boundary", "C"),
    ("sample_input_is_accepted",
     "ExecutionBoundary/ExecutableInvariance.v", "Execution boundary", "C"),

    # --- C-1/M-7: the checker decides instead of receiving the answer ---
    ("check_executable_kernel_reflects",
     "CheckerContract/CheckerSpecification.v", "Checker contract", "A"),
    ("checker_decides_kernel_validity",
     "CheckerContract/CheckerSpecification.v", "Checker contract", "A"),
    ("checker_rejects_unrepresentable_state",
     "CheckerContract/CheckerSpecification.v", "Checker contract", "A"),
    ("check_executable_kernel_false_iff",
     "CheckerContract/CheckerSpecification.v", "Checker contract", "B"),
    ("checker_is_not_constant",
     "CheckerContract/CheckerSpecification.v", "Checker contract", "B"),
    ("checker_survives_machine_step",
     "CheckerContract/CheckerSpecification.v", "Checker contract", "B"),
    ("checker_accepts_something",
     "CheckerContract/CheckerSpecification.v", "Checker contract", "C"),
    ("checker_rejects_something",
     "CheckerContract/CheckerSpecification.v", "Checker contract", "C"),

    # --- M-4: transaction validity from a decision procedure ---
    ("check_value_flow_reflects", "Kernel/Transaction/Validity.v",
     "Transaction", "A"),
    ("transaction_single_output_bounded", "Kernel/Transaction/Validity.v",
     "Transaction", "A"),
    ("transaction_fee_bounded_by_inputs", "Kernel/Transaction/Validity.v",
     "Transaction", "A"),
    ("check_value_flow_false_iff", "Kernel/Transaction/Validity.v",
     "Transaction", "B"),
    ("transaction_totals_nonneg", "Kernel/Transaction/Validity.v",
     "Transaction", "B"),
    ("empty_transaction_has_no_fee", "Kernel/Transaction/Validity.v",
     "Transaction", "B"),
    ("check_value_flow_is_not_constant", "Kernel/Transaction/Validity.v",
     "Transaction", "B"),
    ("check_value_flow_accepts_something", "Kernel/Transaction/Validity.v",
     "Transaction", "C"),
    ("check_value_flow_rejects_overspend", "Kernel/Transaction/Validity.v",
     "Transaction", "C"),

    # --- economic: chain issuance instead of a definitional identity ---
    ("check_econ_reflects", "Kernel/EconomicInvariant/Economic.v",
     "Economic", "A"),
    ("economic_net_issuance_is_subsidy", "Kernel/EconomicInvariant/Economic.v",
     "Economic", "A"),
    ("econ_chain_no_supply_loss", "Kernel/EconomicInvariant/Economic.v",
     "Economic", "A"),
    ("check_econ_false_iff", "Kernel/EconomicInvariant/Economic.v",
     "Economic", "B"),
    ("check_econ_is_not_constant", "Kernel/EconomicInvariant/Economic.v",
     "Economic", "B"),
    ("check_econ_accepts_something", "Kernel/EconomicInvariant/Economic.v",
     "Economic", "C"),
    ("check_econ_rejects_negative_subsidy", "Kernel/EconomicInvariant/Economic.v",
     "Economic", "C"),

    # --- C-1: integer boundary derived from real domains ---
    ("target_domain_restriction_is_necessary", "RuntimeModel/IntegerBoundary.v",
     "Runtime integer domain", "B"),
    ("time_error_domain_restriction_is_necessary",
     "RuntimeModel/IntegerBoundary.v", "Runtime integer domain", "B"),

    # --- C-1: extraction interface is a bijection, not a total function ---
    ("extraction_interface_injective", "ExtractionBoundary/ExtractionModel.v",
     "Extraction boundary", "A"),
    ("extraction_interface_surjective", "ExtractionBoundary/ExtractionModel.v",
     "Extraction boundary", "A"),
    ("extraction_boundary_aligned", "ExtractionBoundary/ExtractionModel.v",
     "Extraction boundary", "B"),
    ("collapsing_boundary_is_not_injective",
     "ExtractionBoundary/ExtractionModel.v", "Extraction boundary", "B"),
    ("collapsing_boundary_is_not_surjective",
     "ExtractionBoundary/ExtractionModel.v", "Extraction boundary", "B"),

    # --- runtime failure path without the mutation-only switch ---
    ("runtime_rejects_iff_some_flag_down", "Runtime/RuntimeFailure.v",
     "Runtime failure", "A"),
    ("runtime_accepts_kernel_valid_input", "Runtime/RuntimeFailure.v",
     "Runtime failure", "A"),
    ("runtime_rejection_is_not_constant", "Runtime/RuntimeFailure.v",
     "Runtime failure", "B"),
]

ROW = re.compile(r"^\|\s*`([^`]+)`\s*\|\s*`([^`]+)`\s*\|\s*([^|]*?)\s*\|\s*$")
SECTION = re.compile(r"^##+\s+(?:Category|Class)\s+([ABC])\b", re.I)


def split_sections(text: str):
    """Return (preamble, {cat: [lines]}, order) preserving everything else."""
    lines = text.splitlines()
    head: list[str] = []
    blocks: dict[str, list[str]] = {}
    cat = None
    for line in lines:
        m = SECTION.match(line.strip())
        if m:
            cat = m.group(1).upper()
            blocks[cat] = [line]
            continue
        (blocks[cat] if cat else head).append(line)
    return head, blocks


def apply(path: pathlib.Path) -> tuple[int, int]:
    text = path.read_text()
    head, blocks = split_sections(text)
    if set(blocks) != {"A", "B", "C"}:
        sys.exit(f"{path}: expected Category/Class A/B/C sections, got {sorted(blocks)}")

    # Keyed by (name, file): decode_encode_identity legitimately exists
    # in two modules, and collapsing them onto the name silently moves
    # one of them between categories.
    rows: dict[tuple[str, str], str] = {}   # (name, file) -> domain
    home: dict[tuple[str, str], str] = {}   # (name, file) -> category
    keep: dict[str, list[str]] = {c: [] for c in "ABC"}

    for cat, lines in blocks.items():
        for line in lines:
            m = ROW.match(line)
            if m and m.group(1) != "Theorem":
                name, rel, domain = m.group(1), m.group(2), m.group(3)
                rows[(name, rel)] = domain
                home[(name, rel)] = cat
            else:
                keep[cat].append(line)

    demoted = 0
    for name, target in DEMOTIONS.items():
        for key in list(home):
            if key[0] == name and home[key] != target:
                home[key] = target
                demoted += 1

    added = 0
    for name, rel, domain, cat in ADDITIONS:
        key = (name, rel)
        if key not in rows:
            added += 1
        rows[key] = domain
        home[key] = cat

    out = list(head)
    for cat in "ABC":
        body = [l for l in keep[cat]]
        # rebuild the table right after its header line
        table = sorted(k for k in rows if home[k] == cat)
        rendered = [f"| `{k[0]}` | `{k[1]}` | {rows[k]} |" for k in table]
        # find where the header separator sits in the kept lines
        insert_at = len(body)
        for i, l in enumerate(body):
            if l.strip().startswith("|---"):
                insert_at = i + 1
                break
        out += body[:insert_at] + rendered + body[insert_at:]
    path.write_text("\n".join(out).rstrip() + "\n")
    return demoted, added


def main() -> int:
    targets = [
        ROOT / "release" / "TheoremClassification.md",
        ROOT / "evidence" / "TheoremClassification.md",
    ]
    for p in targets:
        if not p.is_file():
            sys.exit(f"missing {p}")
        d, a = apply(p)
        print(f"{p.relative_to(ROOT)}: demoted={d} added={a}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

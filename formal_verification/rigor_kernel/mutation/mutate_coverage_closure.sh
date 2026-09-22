#!/usr/bin/env bash
# M51-M65 — closing Category A mutant coverage to 1:1 (frontier 1.1).
#
# Measuring the old suite with the isolating harness showed 43 of 65
# Category A theorems actually killed by some mutant. The declared
# expected-failure names had never been checked, so the gap was not
# visible. These mutants close it.
#
# Two kinds of mutation appear here.
#
# Most relax a real rule in a definition: the checker stops consulting
# its decision procedure, the RES clamp disappears, a domain code
# collides, an overflow guard is dropped.
#
# A few instead perturb the STATEMENT of a lemma that an exported
# theorem consumes. That is deliberate, not a shortcut. When a theorem
# is proved by rewriting with a conservation law or applying a range
# lemma, no change to any definition can make its proof script fail:
# the harness admits the broken lemma first, and an admitted lemma is
# assumed, so the proof goes through. The only way to show such a
# theorem depends on the shape it claims to depend on is to state that
# shape wrongly and watch the proof stop working. These model a
# mis-stated law, which is a real defect class for a spec.
#
# A few theorems get no mutant at all. Their proofs are nothing but
# `exact`/`apply` of other results — runtime_domain_closed bundles four
# range lemmas, money_range_safe forwards to integer_range_safe — and
# no mutation can make such a proof fail, because the harness admits
# the broken component first and an admitted lemma is assumed. They are
# discharged by scripts/check_mutation_coverage.py, which follows the
# deferral chain and requires every lemma at the end of it to carry a
# mutant of its own.
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# --- checker: unrepresentable states get accepted --------------------
# A machine state whose words do not fit the runtime domain is waved
# through instead of rejected.
run_mutation M51 "checker_rejects_unrepresentable_state checker_decides_kernel_validity" \
  CheckerContract/CheckerSpecification.v \
  '  | None => false' \
  '  | None => true' \
  CheckerContract/CheckerSpecification || true

# --- checker: stops consulting the decision procedure ----------------
# Every decodable state is accepted, so acceptance no longer implies
# the kernel invariant.
run_mutation M52 "checker_accepts_valid_kernel_state checker_rejects_invalid_kernel_state checker_survives_machine_step" \
  CheckerContract/CheckerSpecification.v \
  '  | Some e => check_executable_kernel e' \
  '  | Some e => true' \
  CheckerContract/CheckerSpecification || true

# --- economics: conservation law stated with the wrong fee sign ------
# Fees are subtracted from the chain total instead of added, so net
# issuance no longer equals the subsidy sum.
run_mutation M53 "econ_chain_no_supply_loss economic_net_issuance_is_subsidy economic_no_hidden_creation" \
  Kernel/EconomicInvariant/Economic.v \
  '      = init + sum_subsidy es + sum_fees es - sum_burn es.' \
  '      = init + sum_subsidy es - sum_fees es - sum_burn es.' \
  Kernel/EconomicInvariant/Economic || true

# --- serialization: two input domains share a code -------------------
# DomRES and DomORU both encode to 0, so decoding cannot recover which
# one was written, and two states differing only in domain now share a
# wire form -- which is what `encode_oru_injective` forbids.
run_mutation M54 "decode_encode_identity encode_oru_injective" \
  Kernel/SerializationBoundary.v \
  '  | DomRES => 1' \
  '  | DomRES => 0' \
  Kernel/SerializationBoundary || true

# --- supply: zero scheduled-grant prefix is no longer below the cap ---
# res_max_issuance_for_epoch is unused by the fold; flip the bound.
run_mutation M55 epoch_supply_transition_sound Supply/SupplyTransition.v \
  '    supply_after_epochs n <= RES_MAX_SUPPLY.' \
  '    supply_after_epochs n <= -1.' \
  Supply/SupplyTransition || true

# --- executable step: RES clamp missing from the step's field law ----
# Stepping is described as adding the mint with no cap, so the stepped
# state is no longer below its own reserve cap.
run_mutation M56 executable_step_preserves_invariants \
  ExecutionBoundary/ExecutableInvariance.v \
  '         Z.min (ek_res_cap e) (ek_res_stock e + Z.max 0 (ki_res_mint i));' \
  '         ek_res_stock e + Z.max 0 (ki_res_mint i);' \
  ExecutionBoundary/ExecutableInvariance || true

# --- ASERT: exponent clamp given a negative bound --------------------
run_mutation M57 exp_q_clamped Pow/ASERTModel.v \
  'Definition ASERT_EXP_CLAMP : Z := ASERT_RADIX * 64.' \
  'Definition ASERT_EXP_CLAMP : Z := - (ASERT_RADIX * 64).' \
  proofs/Pow_Correctness/ASERTCorrectness || true

# --- ASERT: factor monotonicity stated the wrong way round -----------
# Weakening the top end instead is not enough here: both endpoint
# lemmas are computations, so admitting them hands the range proof
# exactly what it needs. Reversing the direction of the monotonicity
# law is what the range argument cannot survive.
run_mutation M58 factor_range proofs/Pow_Correctness/ASERTCorrectness.v \
  '    factor e1 <= factor e2.' \
  '    factor e2 <= factor e1.' \
  proofs/Pow_Correctness/ASERTCorrectness || true

# --- runtime: the wrong flag defines an invalid input ----------------
# Invalidity is pinned to the script flag while acceptance still turns
# on the UTXO flag, so rejection and validity stop lining up.
run_mutation M59 "invalid_kernel_input_rejected runtime_failure_preserved" \
  Runtime/RuntimeFailure.v \
  '  ki_utxo_ok i = false.' \
  '  ki_script_ok i = false.' \
  Runtime/RuntimeFailure || true

# --- refinement: concrete layer drops the RES delta ------------------
# The concrete step stops tracking reserve movement, so it no longer
# refines the abstract step.
run_mutation M60 kernel_step_refinement_composes Refinement/KernelComposition.v \
  '     kc_res := kc_res c + ks_res_delta s;' \
  '     kc_res := kc_res c;' \
  Refinement/KernelComposition || true

# --- machine: rejection leaves fuel behind ---------------------------
run_mutation M61 machine_state_preserves_invariant Execution/KernelMachine.v \
  '  | KI_Reject => {| km_ok := false; km_fuel := 0 |}' \
  '  | KI_Reject => {| km_ok := false; km_fuel := 1 |}' \
  Execution/KernelMachine || true

# --- machine words: subsidy escapes the domain check -----------------
# A subsidy outside the word domain no longer stops the step, so a
# failed step is no longer evidence of overflow.
run_mutation M62 machine_step_failure_is_overflow Runtime/MachineState.v \
  '  if negb (andb (wf_wordb (ki_fees i))
            (andb (wf_wordb (ki_burn i)) (wf_wordb (ki_subsidy i))))' \
  '  if negb (andb (wf_wordb (ki_fees i)) (wf_wordb (ki_burn i)))' \
  Runtime/MachineState || true

# --- ORU: conservation stated with the fee on the wrong side ---------
run_mutation M63 no_hidden_inflation proofs/ORU_Conservation/ORUConservation.v \
  '    oru_input tx = oru_output tx + fee tx.' \
  '    oru_input tx = oru_output tx - fee tx.' \
  proofs/ORU_Conservation/ORUConservation || true

# --- transactions: validity stops requiring non-negative values ------
run_mutation M64 "transaction_no_negative_value empty_transaction_has_no_fee transaction_totals_nonneg" Kernel/Transaction/Validity.v \
  '  values_nonneg t /\ outputs_bounded_by_inputs t.' \
  '  outputs_bounded_by_inputs t.' \
  Kernel/Transaction/Validity || true

# --- UTXO: application stops consulting transaction validity ---------
# Every transaction applies, valid or not.
run_mutation M65 utxo_soundness proofs/UTXO_Soundness/UTXOSoundness.v \
  '  if valid_tx_b t then Some (apply_body u t) else None.' \
  '  Some (apply_body u t).' \
  proofs/UTXO_Soundness/UTXOSoundness || true

# --- bundle components ------------------------------------------------
# runtime_domain_closed and extraction_interface_bijective are covered
# by their parts (see scripts/check_mutation_coverage.py), which means
# the parts have to carry real mutants of their own.

# Block heights may go negative for the same reason.
run_mutation M67 height_range_safe RuntimeModel/IntegerBoundary.v \
  '  0 <= h <= 21000000.' \
  '  - 21000000 <= h <= 21000000.' \
  RuntimeModel/IntegerBoundary || true

# The ASERT parent timestamp loses its upper bound, so the time error
# is no longer bounded by the runtime word.
run_mutation M68 time_error_range_safe RuntimeModel/IntegerBoundary.v \
  '  t_par <= 2 ^ 40 /\' \
  '  t_par <= 2 ^ 80 /\' \
  RuntimeModel/IntegerBoundary || true

# Two validators are declared to land on the same extracted entry, so
# the boundary no longer sits where the interface says it does.
run_mutation M69 extraction_boundary_aligned \
  ExtractionBoundary/ExtractionInterface.v \
  '  | CK_RES => kernel_call_boundary f = EK_RES' \
  '  | CK_RES => kernel_call_boundary f = EK_ORU' \
  ExtractionBoundary/ExtractionModel || true

#!/usr/bin/env bash
# M138-M141 — the statements that say a rule is not vacuous.
#
# `check_econ_is_not_constant`, `check_value_flow_is_not_constant` and
# `checker_is_not_constant` exist because a checker that answers `true`
# everywhere satisfies every soundness theorem in the tree: "if the
# check passes then the invariant holds" is free when the check is
# `fun _ => true`. They are the guard against that reading, and until
# now nothing tested them -- the guard itself was unguarded.
#
# The mutation is the thing each one denies. Replace the checker by the
# constant it claims not to be, and the claim has to fall. The
# `*_false_iff` results fall with them, since a constant checker never
# returns false.
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

run_mutation M138 "check_econ_reflects check_econ_rejects_negative_subsidy" \
  Kernel/EconomicInvariant/Economic.v \
  'Definition check_econ (e : EconomicStep) : bool :=
  andb (0 <=? es_subsidy e)
    (andb (es_subsidy e <=? ORU_INITIAL_BLOCK_SUBSIDY)
      (andb (0 <=? es_fees e)
        (andb (0 <=? es_burn e) (es_burn e <=? es_fees e)))).' \
  'Definition check_econ (e : EconomicStep) : bool := true.' \
  Kernel/EconomicInvariant/Economic || true

run_mutation M139 "check_value_flow_reflects check_value_flow_rejects_overspend" \
  Kernel/Transaction/Validity.v \
  'Definition check_value_flow (t : Transaction) : bool :=
  andb (all_in_nonneg (tx_ins t))
    (andb (all_out_nonneg (tx_outs t))
      (andb (0 <=? Fee t)
            (outputs_total t + Fee t <=? inputs_total t))).' \
  'Definition check_value_flow (t : Transaction) : bool := true.' \
  Kernel/Transaction/Validity || true

run_mutation M140 "check_executable_kernel_reflects checker_rejects_something" \
  ExecutionBoundary/ConcreteExecutionState.v \
  'Definition check_executable_kernel (e : ExecutableKernelState) : bool :=
  andb (ek_utxo_flag e)
    (andb (Z.leb 0 (ek_oru_stock e))
      (andb (Z.leb 0 (ek_res_stock e))
        (andb (Z.leb (ek_res_stock e) (ek_res_cap e))
          (andb (ek_script_ok e)
            (andb (ek_pow_ok e) (ek_asert_ok e)))))).' \
  'Definition check_executable_kernel (e : ExecutableKernelState) : bool := true.' \
  ExecutionBoundary/ConcreteExecutionState || true

# The same three checkers pinned to `false`.
#
# One direction is not enough. `check_econ_is_not_constant` is the pair
# "something is accepted" and "something is rejected", and a checker
# stuck at `true` only falsifies the second half. Measuring the
# constant-`true` mutants showed exactly that: the rejection witnesses
# fell, the acceptance witnesses did not, and the non-constancy claim
# kept proving from the half that survived. A statement that a function
# is not constant is only tested when both constants are tried.

run_mutation M142 "check_econ_reflects check_econ_accepts_something" \
  Kernel/EconomicInvariant/Economic.v \
  'Definition check_econ (e : EconomicStep) : bool :=
  andb (0 <=? es_subsidy e)
    (andb (es_subsidy e <=? ORU_INITIAL_BLOCK_SUBSIDY)
      (andb (0 <=? es_fees e)
        (andb (0 <=? es_burn e) (es_burn e <=? es_fees e)))).' \
  'Definition check_econ (e : EconomicStep) : bool := false.' \
  Kernel/EconomicInvariant/Economic || true

run_mutation M143 "check_value_flow_reflects check_value_flow_accepts_something" \
  Kernel/Transaction/Validity.v \
  'Definition check_value_flow (t : Transaction) : bool :=
  andb (all_in_nonneg (tx_ins t))
    (andb (all_out_nonneg (tx_outs t))
      (andb (0 <=? Fee t)
            (outputs_total t + Fee t <=? inputs_total t))).' \
  'Definition check_value_flow (t : Transaction) : bool := false.' \
  Kernel/Transaction/Validity || true

run_mutation M144 "check_executable_kernel_reflects checker_accepts_something" \
  ExecutionBoundary/ConcreteExecutionState.v \
  'Definition check_executable_kernel (e : ExecutableKernelState) : bool :=
  andb (ek_utxo_flag e)
    (andb (Z.leb 0 (ek_oru_stock e))
      (andb (Z.leb 0 (ek_res_stock e))
        (andb (Z.leb (ek_res_stock e) (ek_res_cap e))
          (andb (ek_script_ok e)
            (andb (ek_pow_ok e) (ek_asert_ok e)))))).' \
  'Definition check_executable_kernel (e : ExecutableKernelState) : bool := false.' \
  ExecutionBoundary/ConcreteExecutionState || true

# The machine word becomes unbounded, so no addition is ever rejected
# and the overflow the runtime boundary exhibits is unreachable.
run_mutation M141 "w_add_overflow_reachable w_add_negative_result_rejected w_add_some_inv w_add_of_wf machine_refinement_is_partial machine_chain_completes_on_idle machine_chain_refinement_is_partial" \
  Runtime/MachineWord.v \
  '  if wf_wordb (a + b) then Some (a + b) else None.' \
  '  Some (a + b).' \
  Runtime/MachineWord || true

# The extraction boundary stops collapsing: every Coq entry point gets
# its own extracted name. The two theorems that record the collapse as
# a real loss of information have nothing left to point at.
run_mutation M145 "collapsing_boundary_is_not_injective collapsing_boundary_is_not_surjective" \
  ExtractionBoundary/ExtractionModel.v \
  'Definition collapsing_boundary (f : CoqKernelFunction)
  : ExtractedKernelInterface := EK_UTXO.' \
  'Definition collapsing_boundary (f : CoqKernelFunction)
  : ExtractedKernelInterface :=
  match f with
  | CK_UTXO => EK_UTXO
  | CK_ORU => EK_ORU
  | CK_RES => EK_RES
  | CK_Script => EK_Script
  | CK_PoW => EK_PoW
  | CK_ASERT => EK_ASERT
  end.' \
  ExtractionBoundary/ExtractionModel || true

# Decoding becomes total, so no machine state is outside the model and
# the gap between what the runtime can hold and what the kernel accepts
# disappears.
run_mutation M146 "machine_decode_is_partial runtime_state_can_be_unrepresentable machine_decode_some_inv machine_step_refines_executable_step machine_refinement_is_partial machine_invariants_imply_representable representable_does_not_imply_valid machine_chain_refinement_is_partial" \
  Runtime/MachineState.v \
  '  if machine_words_wf m then Some (machine_to_executable m) else None.' \
  '  Some (machine_to_executable m).' \
  Runtime/MachineState || true

# The runtime integer is widened past the point where the ASERT time
# error can leave its range.
#
# It does not reach `target_domain_restriction_is_necessary`. Compact
# encoding carries an exponent, so `compact_decode` outruns any fixed
# widening of the word; killing that statement needs a mutation of
# `compact_decode` itself, which is not in this batch. The declaration
# below is what was measured, not what was intended.
run_mutation M147 "time_error_domain_restriction_is_necessary" \
  RuntimeModel/IntegerBoundary.v \
  'Definition runtime_signed_max : Z := 2 ^ 62 - 1.' \
  'Definition runtime_signed_max : Z := 2 ^ 126 - 1.' \
  RuntimeModel/IntegerBoundary || true

echo "MUTATE_NON_VACUITY_DONE fail=$EXPECT_FAIL_COUNT pass=$MUTATION_PASS_COUNT invalid=$MUTATION_INVALID_COUNT"

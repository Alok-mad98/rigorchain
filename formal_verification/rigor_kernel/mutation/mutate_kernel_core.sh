#!/usr/bin/env bash
# M42–M50 — Category A theorems that previously had no mutant.
#
# Audit finding M-1 recorded 24 of 62 Category A theorems with no
# dedicated mutation, including kernel_sound (the strongest theorem in
# the package), subsidy_is_only_creation_source (the real anti-inflation
# statement) and the whole integer-boundary result. These are those
# mutants, plus coverage for the theorems added while closing C-1/C-2.
#
# Every mutation below relaxes a real rule. None flips a switch.
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# --- kernel_sound: bypass a domain validator entirely ---------------
# validate_kernel accepts every RES input without consulting the RES
# validator, so a passing check no longer implies the RES invariant.
run_mutation M42 kernel_sound Kernel/KernelSoundness.v \
  '| KI_RES s => RESValidator.check_res s' \
  '| KI_RES s => true' \
  Kernel/KernelSoundness || true

# --- kernel_step_preserves_invariants: remove the RES clamp ---------
# Without Z.min the RES stock can exceed its cap after a step.
run_mutation M43 step_of_preserves \
  Kernel/UnifiedTransition.v \
  '{| res_stock := Z.min (res_cap r) (res_stock r + Z.max 0 (ki_res_mint i));' \
  '{| res_stock := res_stock r + Z.max 0 (ki_res_mint i);' \
  proofs/KernelTransitionSoundness || true

# --- subsidy_is_only_creation_source: add a second creation term ----
# ORU stock grows by one unit per step with no subsidy behind it.
run_mutation M44 subsidy_is_only_creation_source \
  Economic/ORU/ORUTransition.v \
  '       oru_stock pre + ovf_subsidy f + ovf_incoming f
         - ovf_outgoing f - ovf_burn f |}.' \
  '       oru_stock pre + ovf_subsidy f + ovf_incoming f
         - ovf_outgoing f - ovf_burn f + 1 |}.' \
  Economic/ORU/ORUSoundness || true

# --- target_range_safe: widen the compact-bits domain ---------------
# A size-40 compact word decodes far past the 63-bit runtime bound.
run_mutation M45 target_range_safe RuntimeModel/IntegerBoundary.v \
  '  0 <= nbits /\ compact_size nbits <= 7.' \
  '  0 <= nbits /\ compact_size nbits <= 40.' \
  RuntimeModel/IntegerBoundary || true

# --- integer_range_safe: shrink the documented word bound -----------
# MAX_MONEY no longer fits, so the domain closure is false.
run_mutation M46 integer_range_safe RuntimeModel/IntegerBoundary.v \
  'Definition runtime_signed_max : Z := 2 ^ 62 - 1.' \
  'Definition runtime_signed_max : Z := 2 ^ 32 - 1.' \
  RuntimeModel/IntegerBoundary || true

# --- machine_step_refines_executable_step: drop the overflow check --
# The RES accumulator is computed unbounded, so the stepped machine
# state can leave the word domain and no longer decodes.
run_mutation M47 machine_step_refines_executable_step \
  Runtime/MachineState.v \
  '    match w_add (mk_res_stock m) (Z.max 0 (ki_res_mint i)) with' \
  '    match Some (mk_res_stock m + Z.max 0 (ki_res_mint i)) with' \
  Runtime/MachineState || true

# --- machine_invariants_imply_representable: drop a word bound ------
# Without the RES cap bound the invariant no longer guarantees that a
# state can be represented at all.
run_mutation M48 "machine_invariants_imply_representable runtime_projection_preserves_kernel_domain" \
  Runtime/MachineState.v \
  '  mk_res_cap m <= word_max /\' \
  '  True /\' \
  Runtime/MachineState || true

# --- check_value_flow_reflects: drop the conservation check ---------
# The transaction decision procedure stops checking that outputs plus
# fee are covered by inputs, so it no longer reflects ValueFlow.
run_mutation M49 check_value_flow_reflects \
  Kernel/Transaction/Validity.v \
  '            (outputs_total t + Fee t <=? inputs_total t))).' \
  '            true)).' \
  Kernel/Transaction/Validity || true

# --- economic_no_hidden_creation: unbounded block subsidy -----------
# valid_econ stops bounding the per-block subsidy, so chain issuance is
# no longer bounded by n * ORU_INITIAL_BLOCK_SUBSIDY.
run_mutation M50 "check_econ_reflects subsidy_schedule_bound" \
  Kernel/EconomicInvariant/Economic.v \
  '  0 <= es_subsidy e <= ORU_INITIAL_BLOCK_SUBSIDY /\' \
  '  0 <= es_subsidy e /\' \
  Kernel/EconomicInvariant/Economic || true

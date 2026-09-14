#!/usr/bin/env bash
# M18–M23 — completeness-hardening mutations
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# Allow outputs to exceed inputs + fee.
run_mutation M18 "transaction_value_conservation check_value_flow_reflects transaction_fee_separation transaction_single_output_bounded transaction_fee_bounded_by_inputs" Kernel/Transaction/Validity.v \
  'Definition outputs_bounded_by_inputs (t : Transaction) : Prop :=
  inputs_total t >= outputs_total t + Fee t.' \
  'Definition outputs_bounded_by_inputs (t : Transaction) : Prop :=
  True.' \
  Kernel/Transaction/Validity || true

# Drop the PoW conjunct from model-level block acceptance.
run_mutation M19 block_acceptance_sound Kernel/Block/Acceptance.v \
  'Definition block_accepted (b : BlockTransition) : Prop :=
  accept_utxo b /\ require_pow b /\ accept_supply b /\ accept_script b.' \
  'Definition block_accepted (b : BlockTransition) : Prop :=
  accept_utxo b /\ accept_supply b /\ accept_script b.' \
  Kernel/Block/Acceptance || true

# Fees mint an extra coin term.
run_mutation M20 fees_only_transfer Kernel/EconomicInvariant/Economic.v \
  'Definition apply_econ (e : EconomicStep) : Z :=
  es_old e + es_subsidy e + es_fees e - es_burn e.' \
  'Definition apply_econ (e : EconomicStep) : Z :=
  es_old e + es_subsidy e + es_fees e - es_burn e + es_fees e.' \
  Kernel/EconomicInvariant/Economic || true

# Execution stops reclassifying: a step leaves the script in whatever
# class it was already carrying. Previously this mutant flipped a
# kernel_step_unique constant that existed only to be flipped.
run_mutation M21 script_execution_preserves_class Script/Semantics/Execution.v \
  '    exec_step (ES_Run scr cls) (ES_Run scr (classify_script scr))' \
  '    exec_step (ES_Run scr cls) (ES_Run scr cls)' \
  Script/Semantics/Execution || true

# Invert the pipeline target comparison.
run_mutation M22 pow_pipeline_sound Pow/Acceptance.v \
  '  0 < target /\ digest_as_Z digest <= target.' \
  '  0 < target /\ digest_as_Z digest > target.' \
  Pow/Acceptance || true

# Change matrix division from Z.quot to Z.div.
run_mutation M23 asert_boundary_complete Pow/ASERTMatrix.v \
  '  Z.quot (te * ASERT_RADIX) tau.' \
  '  Z.div (te * ASERT_RADIX) tau.' \
  Pow/ASERTMatrix || true

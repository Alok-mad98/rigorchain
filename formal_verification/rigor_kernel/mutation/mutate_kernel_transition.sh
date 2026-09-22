#!/usr/bin/env bash
# M29–M30 — unified kernel transition / UTXO–economic coupling
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# The kernel steps on inputs it is supposed to reject: the guard comes
# off the only constructor of the transition relation. Previously this
# mutant flipped a kernel_step_unique constant that existed only to be
# flipped, which made determinism look like a property of the rules.
run_mutation M29 kernel_apply_is_graph Kernel/UnifiedTransition.v \
  '| kapp_fun : forall s i,
    input_ok s i ->
    kernel_apply s i (kernel_step_of s i).' \
  '| kapp_fun : forall s i,
    kernel_apply s i (kernel_step_of s i).' \
  Kernel/UnifiedTransition || true

# Create value without input or subsidy.
run_mutation M30 transaction_value_flow_preserves_economic_state Kernel/UnifiedTransition.v \
  '  ki_subsidy i + ki_incoming i - ki_outgoing i - ki_burn i.' \
  '  ki_subsidy i + ki_incoming i - ki_outgoing i - ki_burn i + 1.' \
  Economic/UTXOEconomicBridge || true

# Invert the computed UTXO decision. The bridge theorem is that a
# passing Path A check yields ki_utxo_ok = true; negating the result
# keeps the field computed and makes input_ok_of_checks fail.
run_mutation M129 input_ok_of_checks Kernel/KernelInputBridge.v \
  'ki_utxo_ok   := UTXOValidator.check_utxo (kw_tx w);' \
  'ki_utxo_ok   := negb (UTXOValidator.check_utxo (kw_tx w));' \
  Kernel/KernelInputBridge || true

# Drop one numeric conjunct from the boolean form of input_ok. The
# Prop is unchanged, so input_ok_reflect right-to-left fails.
run_mutation M130 input_ok_reflect Kernel/UnifiedTransition.v \
  '(andb (Z.leb 0 (ki_incoming i))' \
  '(andb true' \
  Kernel/UnifiedTransition || true

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

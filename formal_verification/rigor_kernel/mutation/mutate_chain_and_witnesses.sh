#!/usr/bin/env bash
# M154-M158 — chain composition, the machine graph, and two witnesses.
#
# These reach the roots the Category B debt was resting on rather than
# the theorems on top of them. `kernel_chain_app` and
# `executable_preservation_hypotheses_are_inhabited` are recognised
# deferrals, but they forward to `chain_ok_app` / `chain_step_of_app`
# and to the two `sample_*` lemmas, and nothing was testing those, so
# the whole stack counted as unchecked. Killing the root retires the
# root and everything standing on it.
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# The weakening the source itself names. `chain_ok` judges each input
# at the state its predecessors produced; the comment above it says
# that relaxing this to judge every input at the start state "would
# make every theorem below easier and none of them true of a real
# chain". Nothing was checking that, so here it is.
run_mutation M154 "chain_ok_app" \
  Kernel/Chain/ChainTransition.v \
  '  | i :: rest => input_ok s i /\ chain_ok (kernel_step_of s i) rest' \
  '  | i :: rest => input_ok s i /\ chain_ok s rest' \
  Kernel/Chain/ChainTransition || true

# The run stops after its first input instead of folding through the
# list, so composing two runs is no longer the same as running the
# concatenation.
run_mutation M155 "chain_step_of_app" \
  Kernel/Chain/ChainTransition.v \
  '  | i :: rest => chain_step_of (kernel_step_of s i) rest' \
  '  | i :: rest => kernel_step_of s i' \
  Kernel/Chain/ChainTransition || true

# The step relation relates a state to every successor rather than to
# the one the transition function computes, so it stops being that
# function'"'"'s graph. This is the shape a second, unconstrained
# constructor would give it.
run_mutation M156 "machine_step_is_graph" \
  Execution/KernelMachine.v \
  '| mstep_fun : forall s i,
    machine_step s i (KernelTransition s i).' \
  '| mstep_fun : forall s i s'"'"',
    machine_step s i s'"'"'.' \
  Execution/KernelMachine || true

# The compact exponent saturates at a value that still fits a machine
# word, which is exactly the assumption
# `target_domain_restriction_is_necessary` denies. M147 tried to reach
# this statement by widening `runtime_signed_max` and could not: the
# encoding carries an exponent and outruns any fixed widening, so the
# mutation has to be on the decoder.
run_mutation M157 "target_domain_restriction_is_necessary" \
  Pow/CompactBits.v \
  '  else Z.shiftl word (8 * (size - 3)).' \
  '  else Z.shiftl word (8 * (Z.min size 6 - 3)).' \
  Pow/CompactBits || true

# ORU deltas can no longer be negative, so no input can draw the stock
# down and the flow condition stops being load-bearing.
run_mutation M158 "oru_flow_condition_is_necessary" \
  Kernel/UnifiedTransition.v \
  '  ki_subsidy i + ki_incoming i - ki_outgoing i - ki_burn i.' \
  '  Z.max 0 (ki_subsidy i + ki_incoming i - ki_outgoing i - ki_burn i).' \
  Kernel/UnifiedTransition || true

echo "MUTATE_CHAIN_AND_WITNESSES_DONE fail=$EXPECT_FAIL_COUNT pass=$MUTATION_PASS_COUNT invalid=$MUTATION_INVALID_COUNT"

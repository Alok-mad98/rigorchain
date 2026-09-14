#!/usr/bin/env bash
# M77-M89 — chain-level induction (frontier 2.1).
#
# Before 2.1 every consensus theorem in this package was about a
# single step, so there was nothing here to mutate. The mutants below
# attack the two things a chain layer can get wrong.
#
# The first is the fold itself: run the inputs in the wrong order,
# forget to thread the state from one block into the next, drop the
# intermediate states from the trace, or let a failed machine step be
# treated as a success. Each of those is a plausible implementation
# bug and each one is caught here.
#
# The second is subtler and is the reason this file exists. [chain_ok]
# judges every input against the state its predecessors produced,
# because [input_ok] reads the ORU stock and the RES cap out of the
# state. Evaluating it at the initial state instead would be an easy
# mistake to make and would silently weaken every chain theorem above
# it. M77 makes exactly that mistake and watches the graph theorem
# stop working.
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# --- acceptability judged at the wrong state -------------------------
# Each block is checked against the chain's starting state instead of
# the state it is actually applied to.
run_mutation M77 "kernel_chain_is_graph" \
  Kernel/Chain/ChainTransition.v \
  '  | i :: rest => input_ok s i /\ chain_ok (kernel_step_of s i) rest' \
  '  | i :: rest => input_ok s i /\ chain_ok s rest' \
  Kernel/Chain/ChainTransition || true

# --- the run is folded in the wrong order ----------------------------
run_mutation M78 "kernel_chain_preserves_invariants" \
  Kernel/Chain/ChainTransition.v \
  '  | i :: rest => chain_step_of (kernel_step_of s i) rest' \
  '  | i :: rest => kernel_step_of (chain_step_of s rest) i' \
  Kernel/Chain/ChainTransition || true

# --- intermediate states dropped from the trace ----------------------
# The endpoint is still reported; the states the chain passed through
# are not, so nothing constrains them.
run_mutation M79 "kernel_chain_states_invariant" \
  Kernel/Chain/ChainTransition.v \
  '  | i :: rest => s :: chain_states (kernel_step_of s i) rest' \
  '  | i :: rest => chain_states (kernel_step_of s i) rest' \
  Kernel/Chain/ChainTransition || true

# --- a block's value flow vanishes from the total --------------------
run_mutation M80 "chain_oru_conservation" \
  Kernel/Chain/ChainTransition.v \
  '  | i :: rest => oru_delta i + sum_oru_delta rest' \
  '  | i :: rest => sum_oru_delta rest' \
  Kernel/Chain/ChainTransition || true

# --- burn stops being counted in the chain identity ------------------
run_mutation M81 "chain_no_hidden_creation" \
  Kernel/Chain/ChainTransition.v \
  '  | i :: rest => ki_burn i + sum_burn_in rest' \
  '  | i :: rest => sum_burn_in rest' \
  Kernel/Chain/ChainTransition || true

# --- the cap drifts upward as the chain runs -------------------------
# A single step still respects whatever cap it is handed; the chain is
# what notices that the ceiling is being raised block by block.
run_mutation M82 "chain_res_cap_fixed" \
  Kernel/UnifiedTransition.v \
  '     res_cap := res_cap r |}.' \
  '     res_cap := res_cap r + 1 |}.' \
  Kernel/UnifiedTransition || true

# --- issuance can run backwards --------------------------------------
run_mutation M83 "chain_res_monotone" \
  Kernel/UnifiedTransition.v \
  '  {| res_stock := Z.min (res_cap r) (res_stock r + Z.max 0 (ki_res_mint i));' \
  '  {| res_stock := Z.min (res_cap r) (res_stock r - Z.max 0 (ki_res_mint i));' \
  Kernel/UnifiedTransition || true

# --- concrete layer stops applying its inputs ------------------------
run_mutation M84 "concrete_chain_commute" \
  Kernel/Chain/ChainRefinement.v \
  '  | i :: rest => concrete_chain_step_of (concrete_step_of c i) rest' \
  '  | i :: rest => concrete_chain_step_of c rest' \
  Kernel/Chain/ChainRefinement || true

# --- executable layer stops applying its inputs ----------------------
run_mutation M85 "executable_chain_commute" \
  Kernel/Chain/ChainRefinement.v \
  '  | i :: rest => executable_chain_step_of (executable_step_of e i) rest' \
  '  | i :: rest => executable_chain_step_of e rest' \
  Kernel/Chain/ChainRefinement || true

# --- the per-step projection law is mis-stated -----------------------
# Claiming the projection is unchanged by a step breaks the transport
# that carries the abstract acceptability hypothesis down the chain.
run_mutation M86 "executable_chain_preserves_invariants" \
  Kernel/Chain/ChainRefinement.v \
  '      = kernel_step_of (kernel_projection (execution_projection e)) i.' \
  '      = kernel_projection (execution_projection e).' \
  Kernel/Chain/ChainRefinement || true

# --- machine run does not thread its own result ----------------------
# Every block after the first is applied to the state before it.
run_mutation M87 "machine_chain_refines_executable_chain" \
  Kernel/Chain/ChainRefinement.v \
  "      | Some m' => machine_chain_step_of m' rest" \
  "      | Some m' => machine_chain_step_of m rest" \
  Kernel/Chain/ChainRefinement || true

# --- the running stock stops gating a spend --------------------------
# With a unit of slack the overspend witness is accepted, so the
# rejection half of the acceptability condition is gone.
run_mutation M88 "chain_ok_rejects_overspend" \
  Kernel/UnifiedTransition.v \
  '    <= oru_stock (ks_oru s) + ki_subsidy i + ki_incoming i /\' \
  '    <= oru_stock (ks_oru s) + ki_subsidy i + ki_incoming i + 1 /\' \
  Kernel/UnifiedTransition || true

# --- machine overflow reported as success ----------------------------
# A run that the word domain cannot complete returns the last good
# state instead of failing, which would hide the partiality the
# runtime refinement is built on.
run_mutation M89 "machine_chain_refinement_is_partial" \
  Kernel/Chain/ChainRefinement.v \
  '      | None => None' \
  '      | None => Some m' \
  Kernel/Chain/ChainRefinement || true

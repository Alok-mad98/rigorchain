#!/usr/bin/env bash
# M38–M41 — Coq runtime semantic / checker contract
#
# M39 and M41 previously flipped a boolean constant that existed only
# so a mutant could flip it (force_checker_accept, failure_path_enabled).
# Those constants are gone; both mutants now relax a real validator
# conjunct.
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# Break the runtime → machine projection: the invariant no longer
# establishes representability.
run_mutation M38 runtime_projection_preserves_kernel_domain \
  Runtime/RuntimeState.v \
  'Definition runtime_project_utxo (r : RuntimeKernelState) : bool :=
  rk_utxo_flag r.' \
  'Definition runtime_project_utxo (r : RuntimeKernelState) : bool :=
  negb (rk_utxo_flag r).' \
  Runtime/RuntimeState || true

# Relax the executable decision procedure: drop the RES stock <= cap
# conjunct, so the checker accepts states violating the kernel
# invariant and the reflection lemma fails.
run_mutation M39 check_executable_kernel_reflects \
  ExecutionBoundary/ConcreteExecutionState.v \
  '(andb (Z.leb (ek_res_stock e) (ek_res_cap e))' \
  '(andb true' \
  CheckerContract/CheckerSpecification || true

# The runtime layer steps on inputs the kernel rejects: the guard comes
# off the only constructor. Previously this mutant flipped a
# runtime_step_unique constant that existed only to be flipped.
run_mutation M40 runtime_apply_is_graph \
  Runtime/RuntimeTransition.v \
  '| rapp_fun : forall r i,
    input_ok (kernel_projection (execution_projection (runtime_projection r))) i ->
    runtime_apply r i (runtime_step_of r i).' \
  '| rapp_fun : forall r i,
    runtime_apply r i (runtime_step_of r i).' \
  Runtime/RuntimeTransition || true

# Drop the ASERT flag from the runtime rejection predicate: an input
# with asert_ok = false is no longer rejected, so the two-directional
# rejection contract fails.
run_mutation M41 "runtime_rejects_iff_some_flag_down runtime_accepts_kernel_valid_input runtime_rejection_is_not_constant" \
  Runtime/RuntimeFailure.v \
  '      (andb (ki_pow_ok i) (ki_asert_ok i))).' \
  '      (ki_pow_ok i)).' \
  Runtime/RuntimeFailure || true

#!/usr/bin/env bash
# M33–M35 — executable kernel / serialization boundary
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# Break the executable → concrete projection.
run_mutation M33 execution_projection_preserves_invariants \
  ExecutionBoundary/ConcreteExecutionState.v \
  'Definition exec_project_utxo (e : ExecutableKernelState) : bool :=
  ek_utxo_flag e.' \
  'Definition exec_project_utxo (e : ExecutableKernelState) : bool :=
  negb (ek_utxo_flag e).' \
  ExecutionBoundary/ConcreteExecutionState || true

# Corrupt the Coq-model encoding of a flag.
run_mutation M34 decode_encode_flag \
  SerializationBoundary/KernelSerialization.v \
  'Definition encode_flag (b : bool) : Z :=
  if b then 1 else 0.' \
  'Definition encode_flag (b : bool) : Z :=
  if b then 2 else 0.' \
  SerializationBoundary/KernelSerialization || true

# The executable layer steps on inputs the kernel rejects: the guard
# comes off the only constructor. Previously this mutant flipped an
# executable_step_unique constant that existed only to be flipped.
run_mutation M35 executable_apply_is_graph \
  ExecutionBoundary/ExecutableTransition.v \
  '| ekapp_fun : forall e i,
    input_ok (kernel_projection (execution_projection e)) i ->
    executable_kernel_apply e i (executable_step_of e i).' \
  '| ekapp_fun : forall e i,
    executable_kernel_apply e i (executable_step_of e i).' \
  ExecutionBoundary/ExecutableTransition || true

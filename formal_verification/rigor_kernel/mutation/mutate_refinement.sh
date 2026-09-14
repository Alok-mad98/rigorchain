#!/usr/bin/env bash
# M24, M25 — refinement relation / machine-step uniqueness
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# Break the observation half of refines (ok bits no longer agree).
run_mutation M24 refinement_preservation Refinement/RefinementRelation.v \
  '  conc_ok c = abs_ok a.' \
  '  conc_ok c = negb (abs_ok a).' \
  Refinement/TransitionRefinement || true

# A no-op instruction quietly marks the machine as accepting.
# Previously this mutant flipped a machine_step_unique constant that
# existed only to be flipped.
run_mutation M25 machine_state_preserves_invariant Execution/KernelMachine.v \
  '  | KI_Nop => s' \
  '  | KI_Nop => {| km_ok := true; km_fuel := km_fuel s |}' \
  Execution/KernelMachine || true

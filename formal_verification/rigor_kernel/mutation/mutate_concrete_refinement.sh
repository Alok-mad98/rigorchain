#!/usr/bin/env bash
# M31–M32 — Coq abstract → Coq concrete refinement
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# Break the projection (flip the UTXO flag).
run_mutation M31 concrete_state_preserves_invariant \
  Refinement/ConcreteKernel/ConcreteKernelMapping.v \
  'Definition project_utxo_flag (c : ConcreteKernelState) : bool :=
  ck_utxo_flag c.' \
  'Definition project_utxo_flag (c : ConcreteKernelState) : bool :=
  negb (ck_utxo_flag c).' \
  Refinement/ConcreteKernel/ConcreteKernelMapping || true

# Concrete transition adds ORU that the abstract step does not. The
# edit now lands on the transition itself; it used to go through a
# concrete_oru_bypass constant that was defined as 0 and existed only
# so that a mutant could set it to 1.
run_mutation M32 projection_step_commute \
  Refinement/ConcreteKernel/ConcreteKernelTransition.v \
  "     ck_oru_stock := oru_stock (ks_oru a');" \
  "     ck_oru_stock := oru_stock (ks_oru a') + 1;" \
  Refinement/ConcreteKernel/ConcreteKernelTransition || true

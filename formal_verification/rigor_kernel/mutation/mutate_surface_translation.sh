#!/usr/bin/env bash
# M137-M138 — faithfulness of the cone-to-surface translation.
#
# `surface_check_oru_eq` says the surface copy of the ORU checker
# agrees with the cone copy on the translated state. M131-M134 reach
# four of the six surface equalities by changing a checker, but this
# one forwards through `valid_oru_tx_b_eq`, so a mutated checker is
# caught one level below it and it never falls.
#
# What it asserts on its own is that `oru_of_cone` carries the state
# across unchanged. That is the claim this mutant breaks: the checkers
# are left alone and the translation is made to lose a field, which is
# the failure mode a hand-written surface copy actually has.
#
# The same edit to `utxo_of_cone` was tried and dropped. It kills
# `valid_tx_b_eq`, which M135 already covers, and never reaches
# `surface_check_utxo_eq`, whose proof is `unfold` plus `apply
# valid_tx_b_eq` and carries nothing of its own.
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# The surface sees ORU output where the cone has input, so the
# `oru_out <= oru_in` conjunct is evaluated on a different pair.
run_mutation M137 "surface_check_oru_eq" \
  Kernel/SurfaceCorrespondence.v \
  'SurfKTx.kt_oru_out := kt_oru_out s' \
  'SurfKTx.kt_oru_out := kt_oru_in s' \
  Kernel/SurfaceCorrespondence || true

echo "MUTATE_SURFACE_TRANSLATION_DONE fail=$EXPECT_FAIL_COUNT pass=$MUTATION_PASS_COUNT invalid=$MUTATION_INVALID_COUNT"

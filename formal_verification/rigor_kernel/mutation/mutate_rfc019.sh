#!/usr/bin/env bash
# M95-M99 — RFC-019 deterministic mechanism.
#
# Unit-price monotonicity, 128-bit product envelope, block-local mint
# sum, derived-cap identity, and reorg replay. Each mutant targets one
# new Category A theorem; the isolation loop must observe that theorem
# failing (Frontier 1.1 / stop rule 4).
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# Flip RATE monotonicity on the authorized ray.
run_mutation M95 res_height_monotonic Supply/SupplySchedule.v \
  '    RATE h1 <= RATE h2.' \
  '    RATE h2 <= RATE h1.' \
  Supply/SupplySchedule || true

# Shrink the 128-bit product bound below the legal envelope.
run_mutation M96 res_mint_products_fit_uint128 Supply/SupplySchedule.v \
  'Definition UINT128_MAX : Z := 2 ^ 128 - 1.' \
  'Definition UINT128_MAX : Z := 2 ^ 64 - 1.' \
  Supply/SupplySchedule || true

# Drop the current mint and keep folding. Stays recursive so the
# non-recursive Fixpoint warning cannot steal the isolation hit.
run_mutation M97 res_block_mint_sum proofs/RES_Issuance/RESIssuance.v \
  '| tx :: rest => apply_res_mints (apply_res_delta s h (mint_amount tx)) h rest' \
  '| tx :: rest => apply_res_mints s h rest' \
  proofs/RES_Issuance/RESIssuance || true

# Break the derived-cap identity (sum equals pin).
run_mutation M98 res_derived_cap_identity Supply/SupplySchedule.v \
  '  res_schedule_total = RES_MAX_SUPPLY.' \
  '  res_schedule_total = 0.' \
  Supply/SupplySchedule || true

# Orphaned mints leak into the surviving replay.
run_mutation M99 res_reorg_replay_eq proofs/RES_Issuance/RESIssuance.v \
  '      = apply_res_mints s h (kept ++ suffix).' \
  '      = apply_res_mints s h (kept ++ suffix ++ orphans).' \
  proofs/RES_Issuance/RESIssuance || true

#!/usr/bin/env bash
# M2, M10 — RES issuance
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# Destroy-cost inequality becomes strict in the soundness conclusion.
run_mutation M2 "res_issuance_soundness check_res_sound" proofs/RES_Issuance/RESIssuance.v \
  '    /\ mint_amount tx * res_destroy_pole_sq h
       <= Z.shiftl (burn_oru tx) RES_DESTROY_SHIFT.' \
  '    /\ mint_amount tx * res_destroy_pole_sq h
       < Z.shiftl (burn_oru tx) RES_DESTROY_SHIFT.' \
  proofs/RES_Issuance/RESIssuance || true

# Drop the body-tx conjunct from res_created.
run_mutation M10 "res_issuance_soundness res_mint_bounded_by_genesis" proofs/RES_Issuance/RESIssuance.v \
  'Definition res_created (tx : res_tx) : Prop :=
  coinbase tx = false /\
  valid_res_mint tx /\
  0 < mint_amount tx /\
  mint_amount tx <= RES_MAX_SUPPLY /\
  res_mint_authorized (rt_height tx) = true /\
  0 < res_destroy_cost_at_height (rt_height tx) /\
  res_mint_fits_rate tx.' \
  'Definition res_created (tx : res_tx) : Prop :=
  valid_res_mint tx /\
  0 < mint_amount tx /\
  mint_amount tx <= RES_MAX_SUPPLY /\
  res_mint_authorized (rt_height tx) = true /\
  0 < res_destroy_cost_at_height (rt_height tx) /\
  res_mint_fits_rate tx.' \
  proofs/RES_Issuance/RESIssuance || true

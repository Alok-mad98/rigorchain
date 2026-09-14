#!/usr/bin/env bash
# M108-M112 — GetBlockProof = 2^256 / (target+1) after SetCompact
# guards (frontier expansion Phase A).
#
# The formula lives in Pow/BlockProof.v; suffix work is derived from
# nBits in Kernel/Chain/ForkChoice.v. Not ActivateBestChain, not OP-R1.
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# Drop the +1. ~T/(T+1) is one less than 2^256/(T+1).
run_mutation M108 block_proof_eq_div Pow/BlockProof.v \
  '  else (u256_not T) / (T + 1) + 1.' \
  '  else (u256_not T) / (T + 1).' \
  Pow/BlockProof || true

# Bad targets (T <= 0 or T >= 2^256) contribute work 1 instead of 0.
run_mutation M109 block_proof_zero_on_bad_target Pow/BlockProof.v \
  '  if (T <=? 0) || (u256_mod <=? T) then 0' \
  '  if (T <=? 0) || (u256_mod <=? T) then 1' \
  Pow/BlockProof || true

# SetCompact failure returns 1 instead of the C++ early-return 0.
run_mutation M110 block_proof_zero_on_compact_fail Pow/BlockProof.v \
  '          || (compact_decode nbits =? 0)
  then 0' \
  '          || (compact_decode nbits =? 0)
  then 1' \
  Pow/BlockProof || true

# Valid targets contribute no work.
run_mutation M111 block_proof_positive_on_valid Pow/BlockProof.v \
  '  else (u256_not T) / (T + 1) + 1.' \
  '  else 0.' \
  Pow/BlockProof || true

# Free increment: ignore nBits / GetBlockProof.
run_mutation M112 chain_work_uses_block_proof Kernel/Chain/ForkChoice.v \
  'Definition wb_work (b : work_block) : Z := get_block_proof (wb_nbits b).' \
  'Definition wb_work (b : work_block) : Z := 1.' \
  Kernel/Chain/ForkChoice || true

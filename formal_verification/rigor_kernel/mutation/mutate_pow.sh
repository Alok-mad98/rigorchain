#!/usr/bin/env bash
# M5, M6, M8, M15, M16 — ASERT / compact / PoW comparison
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# Swap toward-zero quot and floor div in one mutant.
run_mutation M5 negative_time_error_preserved Pow/ASERTModel.v \
  '  Z.quot (te * ASERT_RADIX) tau.' \
  '  Z.div (te * ASERT_RADIX) tau.' \
  Pow/ASERTModel || true

# Claim the compact round-trip is an identity.
run_mutation M6 compact_decode_lower_bound Pow/CompactBits.v \
  '    compact_decode (compact_encode t) <= t.' \
  '    compact_decode (compact_encode t) = t.' \
  Pow/CompactBits || true

# Invert the PoW target comparison.
run_mutation M8 "pow_acceptance_monotonic pow_valid_rejects_above_target" Pow/AbstractHash.v \
  '  0 < target /\ digest_as_Z d <= target.' \
  '  0 < target /\ digest_as_Z d > target.' \
  Pow/PowModel || true

# Reverse the named target comparison (does not touch hash_le_target).
run_mutation M15 target_monotonic Pow/TargetRelation.v \
  '  0 < target /\ digest_as_Z d <= target.' \
  '  0 < target /\ digest_as_Z d > target.' \
  Pow/TargetRelation || true

# Replace Z.quot with Z.div on the boundary quotient (not exp_q_raw).
run_mutation M16 time_error_direction_preserved Pow/ASERTBoundary.v \
  '  Z.quot (te * ASERT_RADIX) tau.' \
  '  Z.div (te * ASERT_RADIX) tau.' \
  Pow/ASERTBoundary || true

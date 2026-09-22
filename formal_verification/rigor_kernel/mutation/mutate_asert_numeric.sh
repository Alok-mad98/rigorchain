#!/usr/bin/env bash
# M90-M94 — ASERT fixed-point width and wrap bounds (frontier 2.4).
#
# The definitions live in Pow/ASERTModel.v; the theorems these mutants
# are aimed at live in proofs/Pow_Correctness/ASERTCorrectness.v, so the
# compile target is the latter.
#
# Scope note. The factor range, monotonicity and clamp bounds were
# already proved before 2.4 and already have mutants elsewhere; M5 in
# mutate_pow.sh covers the quot/div operator split. What 2.4 added, and
# what these mutants exist for, is the polynomial's sign and width --
# the two unchecked casts in the C++ -- and the exactness of the
# factor at the shift wrap.
#
# Each declared kill was derived with MUTATION_KILL_SET_MODE=1 rather
# than predicted, and running it changed the proofs. The first version
# of the 2.4 theorems evaluated the polynomial's endpoints in lemmas of
# their own. Every coefficient mutant killed those lemmas first, the
# isolation loop admitted them, and the two cast-safety theorems then
# proved against the admitted -- wrong -- values. Both were killed by no
# mutant at all while looking perfectly well tested. Inlining the
# evaluated endpoints into the proofs that need them fixed it.
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# --- cubic coefficient off by one ------------------------------------
# One unit in the e^3 term is one unit in 2^48 at the top of the domain,
# which is both enough to move the floor at the wrap and enough to push
# the polynomial past the unsigned 64-bit ceiling it currently clears by
# less than one part in 10^5.
run_mutation M90 "asert_poly_fits_uint64 factor_wrap_continuous factor_wrap_step_is_one" \
  Pow/ASERTModel.v \
  'Definition ASERT_POLY_P3 : Z := 5127.' \
  'Definition ASERT_POLY_P3 : Z := 5128.' \
  proofs/Pow_Correctness/ASERTCorrectness || true

# --- rounding bias dropped -------------------------------------------
# poly(0) becomes 0, so the polynomial no longer clears zero and the
# unsigned cast of poly >> 48 loses the precondition it depends on.
run_mutation M91 "asert_poly_pos" \
  Pow/ASERTModel.v \
  'Definition ASERT_POLY_BIAS : Z := 2 ^ 47.' \
  'Definition ASERT_POLY_BIAS : Z := 0.' \
  proofs/Pow_Correctness/ASERTCorrectness || true

# --- fixed-point scale off by one bit --------------------------------
run_mutation M92 "factor_wrap_continuous" \
  Pow/ASERTModel.v \
  '  Z.shiftr (asert_poly e) 48 + ASERT_RADIX.' \
  '  Z.shiftr (asert_poly e) 47 + ASERT_RADIX.' \
  proofs/Pow_Correctness/ASERTCorrectness || true

# --- leading coefficient mistranscribed ------------------------------
# A dropped digit in a 15-digit constant. The polynomial then fits in an
# int64 comfortably, which is the point: the theorem asserting it does
# not is what notices.
run_mutation M93 "asert_poly_exceeds_int64 factor_wrap_continuous" \
  Pow/ASERTModel.v \
  'Definition ASERT_POLY_P1 : Z := 195766423245049.' \
  'Definition ASERT_POLY_P1 : Z := 19576642324504.' \
  proofs/Pow_Correctness/ASERTCorrectness || true

# --- cubic term dropped ----------------------------------------------
# "Simplified" to a quadratic. Still positive, still monotone, still in
# range; only the exactness at the wrap point notices.
run_mutation M94 "factor_wrap_continuous" \
  Pow/ASERTModel.v \
  '  ASERT_POLY_P1 * e + ASERT_POLY_P2 * e * e + ASERT_POLY_P3 * e * e * e + ASERT_POLY_BIAS.' \
  '  ASERT_POLY_P1 * e + ASERT_POLY_P2 * e * e + ASERT_POLY_BIAS.' \
  proofs/Pow_Correctness/ASERTCorrectness || true

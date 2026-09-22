#!/usr/bin/env bash
# M131-M136 — the six executable validators.
#
# No mutation touched Kernel/*Validator.v, and none touched the two
# functions the aliases there forward to. The `check_*` family is what
# `validate_kernel` dispatches to and what `Extract.v` extracts, so
# every `*_extract_reflects_*`, every `surface_check_*_eq` and every
# `check_*_false_iff` stood on definitions no mutant could change.
#
# Each mutant drops exactly one conjunct by replacing it with `true`,
# which leaves the term structure alone and changes only the rule.
#
# The declared lists are the measured kill sets, not predictions. That
# distinction produced the useful result here: the guess named
# `check_res_sound`, `check_utxo_valid`, `check_oru_valid` and the
# `*_extract_reflects_*` results, and the harness showed all of them
# still proving once the lemma below them was admitted. They are real
# deferrals, and the load-bearing statements are one level down --
# `check_res_reflect`, `valid_tx_b_reflect`, `valid_oru_tx_b_reflect`
# for the three that reflect, and `check_*_sound` itself for the three
# that do not. Covering that level covers the chains above it.
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# RES may be minted by a coinbase transaction, so a passing check no
# longer implies `res_created`. The surface copy is now a different
# function, which is what `surface_check_res_eq` is there to notice.
run_mutation M131 "check_res_reflect surface_check_res_eq" \
  Kernel/RESValidator.v \
  'andb (negb (coinbase s))' \
  'andb true' \
  Kernel/RESValidator || true

# A header passes without its digest meeting the target: the
# proof-of-work rule itself.
run_mutation M132 "check_pow_sound surface_check_pow_eq" \
  Kernel/PowValidator.v \
  '(pow_digest_z h <=? pow_target h)' \
  'true' \
  Kernel/PowValidator || true

# ASERT accepts a non-negative time error. `asert_fail_flag_down` falls
# with it: the witness it exhibits is a header the checker was supposed
# to reject.
run_mutation M133 "check_asert_sound surface_check_asert_eq asert_fail_flag_down" \
  Kernel/ASERTValidator.v \
  'andb (asert_te i <? 0)' \
  'andb true' \
  Kernel/ASERTValidator || true

# The script check stops comparing the classifier against the expected
# type and accepts everything, including the golden negative vectors.
run_mutation M134 "check_script_sound surface_check_script_eq golden_negative_vectors_rejected" \
  Kernel/ScriptValidator.v \
  'script_type_eqb (classify_script (ss_script s)) (ss_expected s)' \
  'true' \
  Kernel/ScriptValidator || true

# A transaction may spend the same input twice and still be `valid_tx_b`.
run_mutation M135 "valid_tx_b_reflect valid_tx_b_eq" \
  proofs/UTXO_Soundness/UTXOSoundness.v \
  '(andb (nodupb (tx_in t))' \
  '(andb true' \
  Kernel/UTXOValidator || true

# ORU output may exceed ORU input. This is the one negative vector
# `invalid002_rejected_by_coq` pins, so it falls too.
run_mutation M136 "valid_oru_tx_b_reflect valid_oru_tx_b_eq invalid002_rejected_by_coq golden_negative_vectors_rejected" \
  Kernel/ORUValidator.v \
  '(andb (oru_out t <=? oru_in t)' \
  '(andb true' \
  Kernel/ORUValidator || true

echo "MUTATE_VALIDATOR_SOUNDNESS_DONE fail=$EXPECT_FAIL_COUNT pass=$MUTATION_PASS_COUNT invalid=$MUTATION_INVALID_COUNT"

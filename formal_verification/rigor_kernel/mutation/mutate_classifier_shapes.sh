#!/usr/bin/env bash
# M148-M150 — the shape ladder in Script/ScriptClassifier.v.
#
# Seven mutants already target this file and all of them aim at the
# recognisers: whether a given byte string is a mint, a converge or a
# null-data script. None of them touches the three properties the
# ladder itself has to have, so those went untested:
#
#   ladder_order_immaterial     the order the shapes are tried in is
#                               free, because they do not overlap
#   no_shape_is_nonstandard     anything matching none of them is
#                               T_NONSTANDARD, never silently a class
#
# Two neighbours of these are not reachable by mutation and stay in
# the Category B debt as measured forwards. `shape_hits_le_one` is the
# three pairwise-disjointness theorems counted instead of conjoined,
# so every mutation that breaks disjointness breaks one of them first
# and the count then follows from the admitted one; two different
# attempts are recorded at M149. `classify_payload_independent` is
# `mint_shape_classifies` applied twice, and falls the same way.
#
# Overlapping shapes are the failure mode that matters here: two
# recognisers accepting one script makes the class depend on ladder
# order, and consensus would then turn on the order two nodes happened
# to compile.
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# A script matching no shape is classified as null-data instead of
# nonstandard, so the fallback no longer refuses to guess.
run_mutation M148 "no_shape_is_nonstandard recognizer_pairwise_disjoint classify_script_eq" \
  Script/ScriptClassifier.v \
  '      if is_mint_script s then T_MINT
      else if is_converge_script s then T_CONVERGE
      else if is_null_data s then T_NULL_DATA
      else T_NONSTANDARD
  end.' \
  '      if is_mint_script s then T_MINT
      else if is_converge_script s then T_CONVERGE
      else if is_null_data s then T_NULL_DATA
      else T_NULL_DATA
  end.' \
  Script/ScriptClassifier || true

# OP_RETURN and PUSHBYTES_20 become the same byte, so a converge
# script and a null-data script start the same way and the two shapes
# overlap.
#
# The obvious mutation -- `is_null_data` accepting every non-empty
# script -- was tried and does not reach `shape_hits_le_one`. It breaks
# `null_data_first_byte` first, and disjointness is proved *from* the
# first-byte lemmas, so once the harness admits that one the rest go
# through. Colliding the two constants leaves every first-byte lemma
# true and removes only the fact that separates the shapes, which is
# the thing being claimed.
run_mutation M149 "converge_not_null_data is_null_data_eq classify_script_eq" \
  Script/ScriptClassifier.v \
  'Definition RIGOR_MARKER_RETURN : byte := 106%nat.' \
  'Definition RIGOR_MARKER_RETURN : byte := 20%nat.' \
  Script/ScriptClassifier || true

# The class of a mint script now turns on the first byte of the hash it
# commits to, which is the dependence `classify_payload_independent`
# denies.
run_mutation M150 "mint_shape_classifies ladder_order_immaterial classify_total classify_range_exact classify_never_legacy_p2pkh_p2sh classify_type_total recognizer_pairwise_disjoint classify_script_eq" \
  Script/ScriptClassifier.v \
  '      if is_mint_script s then T_MINT
      else if is_converge_script s then T_CONVERGE' \
  '      if is_mint_script s then
        (match s with
         | _ :: _ :: b :: _ => if Nat.eqb b 0%nat then T_MINT else T_NONSTANDARD
         | _ => T_NONSTANDARD
         end)
      else if is_converge_script s then T_CONVERGE' \
  Script/ScriptClassifier || true

echo "MUTATE_CLASSIFIER_SHAPES_DONE fail=$EXPECT_FAIL_COUNT pass=$MUTATION_PASS_COUNT invalid=$MUTATION_INVALID_COUNT"

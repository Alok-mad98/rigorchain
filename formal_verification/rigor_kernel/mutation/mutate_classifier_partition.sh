#!/usr/bin/env bash
# M70-M76 — script classifier totality and partition (frontier 1.4).
#
# Before 1.4 the classifier's exported statements were
# `exists t, classify_script s = t` and
# `classify_asset s = classify_asset s`. Nothing could break them, so
# nothing here needed a mutant. They now say which types come out,
# that each of them does come out, and that the three shape
# recognizers cannot fire together. Those are falsifiable, and these
# are the mutants that falsify them.
#
# Two of the seven perturb a lemma STATEMENT rather than a definition.
# `mint_not_null_data` and `converge_not_null_data` are proved by
# pinning the first byte of each shape and observing the bytes differ;
# the harness admits a broken first-byte lemma before reaching them,
# and an admitted lemma is assumed, so no change to `is_null_data`
# itself can make either proof fail. Mis-stating which byte a shape
# pins is the defect these proofs are actually sensitive to, and it is
# a defect a spec can really have.
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# --- range: a reachable type drops out of the declared range ---------
# T_NONSTANDARD is no longer listed, so the residual class escapes the
# range the classifier is claimed to have.
run_mutation M70 "classify_total" \
  Script/ScriptClassifier.v \
  '   T_NONSTANDARD].' \
  '   T_MINT].' \
  Script/ScriptClassifier || true

# --- range: a class loses its witness --------------------------------
# The converge witness carries a 19-byte hash, so nothing exhibits
# T_CONVERGE and the range stops being exact.
run_mutation M71 "classify_range_exact" \
  Script/ScriptClassifier.v \
  '  RIGOR_PUSHBYTES_20 :: repeat_byte 20 7%nat ++ [RIGOR_MARKER_CONVERGE].' \
  '  RIGOR_PUSHBYTES_20 :: repeat_byte 19 7%nat ++ [RIGOR_MARKER_CONVERGE].' \
  Script/ScriptClassifier || true

# --- asset range: A_INVALID drops out --------------------------------
# Nonstandard scripts still classify as A_INVALID, so the asset range
# no longer covers what the classifier produces.
run_mutation M72 "classify_type_total" \
  proofs/Script_Correctness/ScriptCorrectness.v \
  '  [A_ORU; A_RES; A_NONE; A_INVALID].' \
  '  [A_ORU; A_RES; A_NONE; A_ORU].' \
  proofs/Script_Correctness/ScriptCorrectness || true

# --- disjointness: mint's first byte no longer excludes converge -----
# The mint prefix is claimed to be at most 20 instead of at most 4, so
# it stops separating mint from the PUSHBYTES_20 that starts converge.
run_mutation M73 "mint_not_converge" \
  Script/ScriptClassifier.v \
  '    exists b tl, s = b :: tl /\ (b <= 4)%nat.' \
  '    exists b tl, s = b :: tl /\ (b <= 20)%nat.' \
  Script/ScriptClassifier || true

# --- disjointness: null-data claimed to start with the mint prefix ---
run_mutation M74 "mint_not_null_data" \
  Script/ScriptClassifier.v \
  '    exists tl, s = RIGOR_MARKER_RETURN :: tl.' \
  '    exists tl, s = RIGOR_PUSH_0 :: tl.' \
  Script/ScriptClassifier || true

# --- disjointness: converge claimed to start with OP_RETURN ----------
run_mutation M75 "converge_not_null_data" \
  Script/ScriptClassifier.v \
  '    exists tl, s = RIGOR_PUSHBYTES_20 :: tl.' \
  '    exists tl, s = RIGOR_MARKER_RETURN :: tl.' \
  Script/ScriptClassifier || true

# --- payload independence: mint stops recognizing its own shape ------
# The mint recognizer looks for the converge marker, so a script built
# from an arbitrary 20-byte payload is no longer a mint script. This is
# where payload independence gets its content: everything above it
# forwards to this lemma.
run_mutation M76 "mint_script_of_is_mint" \
  Script/ScriptClassifier.v \
  '                | op :: nil => Nat.eqb op RIGOR_MARKER_RES_MINT' \
  '                | op :: nil => Nat.eqb op RIGOR_MARKER_CONVERGE' \
  Script/ScriptClassifier || true

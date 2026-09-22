#!/usr/bin/env bash
# M151-M153 — the two integer models.
#
# `Pow/Uint256Model.v` and `Runtime/MachineWord.v` carry the arithmetic
# the PoW path and the runtime bound run on, and nothing was mutating
# either of them. The three properties below are the ones with a
# failure mode a mutation can express:
#
#   u256_shr_le                 a right shift does not increase a
#                               non-negative value
#   u256_add_wrap_ok            wrap-add lands inside [0, 2^256)
#   word_domain_strictly_smaller_than_Z
#                               the runtime word domain is a proper
#                               subset of Z, so some integer is out of
#                               range and the bound is not vacuous
#
# M153 turned out to carry the whole partiality family with it:
# removing the upper bound also kills `w_add_overflow_reachable`,
# `machine_decode_is_partial`, `runtime_state_can_be_unrepresentable`
# and both `*_refinement_is_partial`. Every claim that the runtime
# model is a genuine partial map rests on the word domain being
# bounded, and nothing was testing that it is.
#
# `u256_add_comm` and `u256_add_assoc` are deliberately absent. In this
# model `u256` is `Z` and `u256_add` is `Z.add`, so those two are
# `Z.add_comm` and `Z.add_assoc` under another name; no mutation of a
# kernel rule breaks them without redefining addition to be something
# other than addition. They are reclassified rather than mutated.
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# Right shift becomes left shift: the classic transcription bug, and
# the one thing `u256_shr_le` rules out.
run_mutation M151 "u256_shr_le" \
  Pow/Uint256Model.v \
  'Definition u256_shr (a : u256) (n : Z) : u256 := Z.shiftr a n.' \
  'Definition u256_shr (a : u256) (n : Z) : u256 := Z.shiftl a n.' \
  Pow/Uint256Model || true

# Wrap-add drops its reduction, so the "wrapping" combinator no longer
# wraps and its result leaves the 256-bit range.
run_mutation M152 "u256_add_wrap_ok" \
  Pow/Uint256Model.v \
  'Definition u256_add_wrap (a b : u256) : u256 := Z.modulo (a + b) u256_mod.' \
  'Definition u256_add_wrap (a b : u256) : u256 := a + b.' \
  Pow/Uint256Model || true

# The word domain loses its upper bound and becomes all of the
# non-negative integers, which is the case the runtime bound exists to
# exclude.
run_mutation M153 "word_domain_strictly_smaller_than_Z wf_wordb_true_iff w_add_overflow_reachable machine_refinement_is_partial machine_decode_is_partial runtime_state_can_be_unrepresentable machine_chain_refinement_is_partial" \
  Runtime/MachineWord.v \
  'Definition wf_word (z : Z) : Prop := 0 <= z <= word_max.' \
  'Definition wf_word (z : Z) : Prop := 0 <= z.' \
  Runtime/MachineWord || true

echo "MUTATE_INTEGER_MODEL_DONE fail=$EXPECT_FAIL_COUNT pass=$MUTATION_PASS_COUNT invalid=$MUTATION_INVALID_COUNT"

#!/usr/bin/env bash
# Run all kernel mutations. Each must yield EXPECTED_FAILURE,
# and a mutation now only counts when the theorems it names are the ones
# that actually stop proving. See lib.sh.
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=lib.sh
source "$DIR/lib.sh"

EXPECT_FAIL_COUNT=0
MUTATION_PASS_COUNT=0

collect() {
  local out
  out="$(bash "$1")"
  printf '%s\n' "$out"
  local nfail npass
  nfail="$(printf '%s\n' "$out" | grep -c '^EXPECTED_FAILURE' || true)"
  npass="$(printf '%s\n' "$out" | grep -cE '^(mutation_passed|mutation_invalid|RESTORE_LEAK)' || true)"
  EXPECT_FAIL_COUNT=$((EXPECT_FAIL_COUNT + nfail))
  MUTATION_PASS_COUNT=$((MUTATION_PASS_COUNT + npass))
}

EXPECT_FAIL_COUNT=0
MUTATION_PASS_COUNT=0
collect "$DIR/mutate_supply.sh"
collect "$DIR/mutate_oru.sh"
collect "$DIR/mutate_res.sh"
collect "$DIR/mutate_pow.sh"
collect "$DIR/mutate_utxo.sh"
collect "$DIR/mutate_completeness.sh"
collect "$DIR/mutate_refinement.sh"
collect "$DIR/mutate_oru_domain.sh"
collect "$DIR/mutate_kernel_transition.sh"
collect "$DIR/mutate_concrete_refinement.sh"
collect "$DIR/mutate_execution_boundary.sh"
collect "$DIR/mutate_extraction_hardening.sh"
collect "$DIR/mutate_runtime_boundary.sh"
collect "$DIR/mutate_kernel_core.sh"
collect "$DIR/mutate_coverage_closure.sh"
collect "$DIR/mutate_classifier_partition.sh"
collect "$DIR/mutate_chain_induction.sh"
collect "$DIR/mutate_asert_numeric.sh"
collect "$DIR/mutate_rfc019.sh"
collect "$DIR/mutate_reorg_replay.sh"
collect "$DIR/mutate_fork_choice.sh"
collect "$DIR/mutate_rfc020.sh"
collect "$DIR/mutate_block_proof.sh"
collect "$DIR/mutate_maturity.sh"
collect "$DIR/mutate_script_spend.sh"

echo "---- mutation summary ----"
echo "EXPECTED_FAILURES=$EXPECT_FAIL_COUNT"
echo "MUTATION_PASSES=$MUTATION_PASS_COUNT"

if [[ "$MUTATION_PASS_COUNT" -ne 0 ]]; then
  echo "FAIL: mutation_passed is forbidden" >&2
  exit 1
fi
if [[ "$EXPECT_FAIL_COUNT" -ne 127 ]]; then
  echo "FAIL: expected 127 EXPECTED_FAILURE lines, got $EXPECT_FAIL_COUNT" >&2
  exit 1
fi

# Restore a clean tree after the last remake.
(
  cd "$KERNEL_ROOT"
  make -s -j"$(nproc 2>/dev/null || echo 2)"
)

# Frontier 1.1: every Category A theorem must be named by a mutation
# that provably kills it.
python3 "$KERNEL_ROOT/scripts/check_mutation_coverage.py" || exit 1

echo "MUTATION_TEST_COMPLETE=TRUE"
echo "EXPECTED_FAILURES_CONFIRMED=TRUE"

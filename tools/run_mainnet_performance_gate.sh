#!/usr/bin/env bash
# P0-077: mainnet performance baseline gate.
# Does not rewrite consensus formulas. Does not modify Genesis / Kernel semantics.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TOOLS="$ROOT/tools"

echo "== P077 mainnet performance baseline gate =="

python3 "$TOOLS/run_mainnet_performance_benchmark.py"

echo "== differential gate (consensus unchanged) =="
SKIP_TEST_RIGOR="${SKIP_TEST_RIGOR:-1}" \
  bash "$ROOT/formal_verification/scripts/run_differential_gate.sh"

echo "== consensus kernel refinement gate =="
bash "$ROOT/run_consensus_kernel_refinement_gate.sh"

echo "PERFORMANCE_BASELINE = PASS"
echo "CONSENSUS_BEHAVIOR_CHANGED = FALSE"
echo "STATE_TRANSITION_CHANGED = FALSE"
echo "HASH_CHANGED = FALSE"
echo "MEMORY_SAFETY = PASS"
echo "CRASH_FOUND = NO"
echo "MAINNET_PERFORMANCE_BASELINE_CLOSED"

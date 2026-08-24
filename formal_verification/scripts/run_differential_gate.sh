#!/usr/bin/env bash
# Differential gate — single entry for Coq ↔ Python ↔ portable-C++ draw/hash checks.
#
# This gate does NOT claim refinement or equivalence. It fails if the shared
# golden vectors, the tripartite hash check, or the σ/root CLI disagree.
#
# Usage (from anywhere):
#   bash formal_verification/scripts/run_differential_gate.sh
#   bash formal_verification/scripts/run_differential_gate.sh --help
#
# Environment:
#   COQ=1               also compile Kernel/NegativeCoreHarness.v + Kernel/GoldenHarness.v (requires coqc 8.15)
#   EXTRACT=1           re-extract the OCaml hash kernel (requires coqc + ocamlopt)
#   TEST_BITCOIN=path   Boost suites; default core/src/test/test_bitcoin if executable
#   SKIP_TEST_BITCOIN=1 do not run test_bitcoin (portable CLI still covers CSHA256)
#
# Coverage (hash / σ / root):
#   Step 2b golden lock  = pinned 422/215 + NegativeCore/InvalidCvg/ScriptMintNeg/DCR-neg/EdgeCross + surface_splits
#   Step 3 portable CLI  = authoritative CSHA256 Transform / FIPS / HashKernel check
#   Step 4 σ/root CLI    = production ComputeSigmaHash + ComputeConvergenceRootFromEligibilityTxids
#   test_bitcoin         = BOOST integration (encoder, σ, A-9-C hex, optional FIPS)
#   STALE binary warning = BOOST lacks sha256_fips_golden; rebuild test_bitcoin
#
# Status: docs/verification/FV-RELEASE-CHECKLIST.md (gate internals: FORMAL-VERIFICATION-STATUS.md)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  sed -n '2,25p' "$0" | sed 's/^# \?//'
  exit 0
fi

cd "$ROOT/formal_verification"

COV=()
cov() { COV+=("$1"); }

fail_diff() {
  local label="$1"
  local tree="$2"
  local gen="$3"
  echo "FAIL: $label does not match the generator (working-tree vs regenerated)."
  echo "      tree: $tree"
  echo "      regen: $gen"
  echo "      Re-run the generators from repo root and keep the working-tree files."
  echo "        python3 formal_verification/generate_phase3_vectors.py"
  echo "        python3 formal_verification/gen_coq_harness.py"
  echo "        python3 formal_verification/tools/gen_cpp_bridge_header.py"
  if command -v diff >/dev/null; then
    diff -u "$tree" "$gen" | head -n 80 || true
  fi
  exit 1
}

echo "== 1. Python golden oracle (full vector set) =="
python3 tests/test_phase3_oracle.py
python3 tests/test_golden_vectors.py
python3 tests/test_cpp_selection_golden.py
python3 tests/test_hash_kernel_agreement.py
cov "Python oracle: PASS"

echo "== 2. Regenerated artifacts must match the working tree =="
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

GOLDEN_OUT="$tmp/phase3_golden_vectors.json" python3 generate_phase3_vectors.py
if ! cmp -s phase3_golden_vectors.json "$tmp/phase3_golden_vectors.json"; then
  fail_diff "phase3_golden_vectors.json" phase3_golden_vectors.json "$tmp/phase3_golden_vectors.json"
fi

HARNESS_OUT="$tmp/GoldenHarness.v" HARNESS_INDEX="$tmp/coq_harness_index.json" \
  NEG_HARNESS_OUT="$tmp/NegativeCoreHarness.v" python3 gen_coq_harness.py
if ! cmp -s coq/Kernel/GoldenHarness.v "$tmp/GoldenHarness.v"; then
  fail_diff "coq/Kernel/GoldenHarness.v" coq/Kernel/GoldenHarness.v "$tmp/GoldenHarness.v"
fi
if ! cmp -s coq/Kernel/NegativeCoreHarness.v "$tmp/NegativeCoreHarness.v"; then
  fail_diff "coq/Kernel/NegativeCoreHarness.v" \
    coq/Kernel/NegativeCoreHarness.v "$tmp/NegativeCoreHarness.v"
fi
if ! cmp -s coq_harness_index.json "$tmp/coq_harness_index.json"; then
  fail_diff "coq_harness_index.json" coq_harness_index.json "$tmp/coq_harness_index.json"
fi

BRIDGE_OUT="$tmp/selection_golden_bridge.inc" python3 tools/gen_cpp_bridge_header.py
if ! cmp -s "$ROOT/core/src/test/selection_golden_bridge.inc" "$tmp/selection_golden_bridge.inc"; then
  fail_diff "selection_golden_bridge.inc" \
    "$ROOT/core/src/test/selection_golden_bridge.inc" "$tmp/selection_golden_bridge.inc"
fi
echo "generated artifacts match working tree"
cov "Generated artifacts match working tree: PASS"
python3 "$ROOT/formal_verification/scripts/check_golden_lock.py"
cov "Golden lock (422/215 + NegativeCore/InvalidCvg/ScriptMintNeg/DCR-neg/EdgeCross + surface_splits): PASS"

echo "== 3. Tripartite hash: portable C++ CSHA256 vs hashlib vs extracted Coq =="
if command -v g++ >/dev/null || command -v c++ >/dev/null; then
  bash "$ROOT/formal_verification/scripts/build_cpp_hash_cli.sh"
else
  echo "FAIL: g++/c++ required to build the C++ hash CLI"
  exit 1
fi

EXTRACTED="$ROOT/formal_verification/extracted/hash256_cli"
if [[ "${EXTRACT:-0}" == "1" ]]; then
  if ! command -v coqc >/dev/null || ! command -v ocamlopt >/dev/null; then
    echo "FAIL: EXTRACT=1 but coqc/ocamlopt is missing"
    exit 1
  fi
  bash "$ROOT/formal_verification/scripts/extract_hash_kernel.sh"
fi
if [[ -x "$EXTRACTED" ]]; then
  :
elif command -v ocamlopt >/dev/null && [[ -f extracted/hash256_kernel.ml ]]; then
  echo "building hash256_cli from committed extraction"
  (cd extracted && rm -f hash256_kernel.mli && ocamlopt -w -3 -o hash256_cli hash256_kernel.ml hash256_cli.ml)
else
  echo "note: no hash256_cli; tripartite test will still require C++ vs hashlib"
fi
python3 tests/test_extracted_hash_kernel.py
python3 tests/test_hash_kernel_tripartite.py
if [[ -x "$EXTRACTED" ]]; then
  cov "Tripartite hash (C++ CLI + hashlib + extracted Coq): PASS"
else
  cov "Hash check (C++ CLI + hashlib; extracted Coq absent): PASS"
fi
echo "    Hash FIPS / CSHA256 for this gate: portable CLI above (not SIMD, not test_bitcoin)."

echo "== 4. Portable σ / ConvergenceRoot CLI vs golden families =="
if command -v g++ >/dev/null || command -v c++ >/dev/null; then
  bash "$ROOT/formal_verification/scripts/build_cpp_sigma_root_cli.sh"
else
  echo "FAIL: g++/c++ required to build the C++ σ/root CLI"
  exit 1
fi
python3 tests/test_sigma_root_cli.py
cov "Production C++ σ/root CLI vs SigmaEncoder/SigmaPoW01/ConvergenceRoot: PASS"
echo "    ComputeSigmaHash + ComputeConvergenceRootFromEligibilityTxids (not the coin-view wrapper)."
echo "    Does not replace a refinement proof; honesty flags stay false."

echo "== 5. Optional Coq harness rebuild =="
if [[ "${COQ:-0}" == "1" ]]; then
  if ! command -v coqc >/dev/null; then
    echo "FAIL: COQ=1 but coqc is not on PATH"
    exit 1
  fi
  cd "$ROOT/formal_verification/coq"
  echo "compiling ReplayReorgCore/ReplayInvariants.v (txid outpoints)"
  coqc -q -R . Rigor ReplayReorgCore/ReplayInvariants.v
  echo "compiling Kernel/NegativeCoreHarness.v"
  coqc -q -R . Rigor Kernel/NegativeCoreHarness.v
  echo "compiling Kernel/GoldenHarness.v"
  coqc -q -R . Rigor Kernel/GoldenHarness.v
  echo "Coq ReplayInvariants.v + NegativeCoreHarness.v + GoldenHarness.v OK"
  cd "$ROOT/formal_verification"
  cov "Coq ReplayInvariants.v + NegativeCoreHarness.v + GoldenHarness.v: PASS"
else
  echo "skipped (set COQ=1 to compile ReplayInvariants.v + Kernel/NegativeCoreHarness.v + Kernel/GoldenHarness.v)"
  cov "Coq ReplayInvariants.v + NegativeCoreHarness.v + GoldenHarness.v: SKIP (COQ=1 to compile)"
fi

echo "== 6. Optional C++ Boost suites (test_bitcoin) =="
if [[ "${SKIP_TEST_BITCOIN:-0}" == "1" ]]; then
  echo "skipped (SKIP_TEST_BITCOIN=1). Portable CLIs still cover CSHA256/FIPS (step 3) and σ/root (step 4)."
  cov "test_bitcoin: SKIP (SKIP_TEST_BITCOIN=1)"
else
  if [[ -z "${TEST_BITCOIN:-}" && -x "$ROOT/core/src/test/test_bitcoin" ]]; then
    TEST_BITCOIN="$ROOT/core/src/test/test_bitcoin"
    echo "auto-detected TEST_BITCOIN=$TEST_BITCOIN"
  fi
  if [[ -n "${TEST_BITCOIN:-}" && -x "${TEST_BITCOIN}" ]]; then
    if "${TEST_BITCOIN}" --list_content 2>/dev/null | grep -q sha256_fips_golden; then
      echo "test_bitcoin includes sha256_fips_golden (BOOST FIPS present)"
      cov "test_bitcoin BOOST: current (sha256_fips_golden present)"
    else
      echo "WARN: test_bitcoin is STALE (no sha256_fips_golden)."
      echo "      Covered anyway: CSHA256/FIPS via portable CLI (step 3); σ/root via step 4."
      echo "      Not covered: BOOST test sha256_fips_golden until you rebuild:"
      echo "        cd $ROOT/core && make -j\$(nproc)   # operator full build"
      echo "        ./src/test/test_bitcoin --run_test=selection_golden_tests,convergence_consensus_tests"
      echo "      Skip this step: SKIP_TEST_BITCOIN=1"
      cov "test_bitcoin BOOST: STALE (no sha256_fips_golden; portable CLI covers hash FIPS)"
    fi
    "${TEST_BITCOIN}" --run_test=selection_golden_tests
    "${TEST_BITCOIN}" --run_test=convergence_consensus_tests
  else
    echo "skipped (no test_bitcoin). Portable CLIs still cover CSHA256/FIPS (step 3) and σ/root (step 4)."
    echo "  After operator make: TEST_BITCOIN=$ROOT/core/src/test/test_bitcoin"
    cov "test_bitcoin: SKIP (binary absent)"
  fi
fi

echo
echo "======== coverage summary ========"
for line in "${COV[@]}"; do
  echo "  $line"
done
echo "================================="
echo "differential gate: PASS"
echo "Honest status: not a refinement proof."
echo "  equivalence_claimed=false  hash_kernel_shared_binary_with_cpp=false  refinement_proved=false"
echo "See docs/verification/FORMAL-VERIFICATION-STATUS.md"

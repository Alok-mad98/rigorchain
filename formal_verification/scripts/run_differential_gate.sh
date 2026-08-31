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
#   TEST_RIGOR=path     Boost suites; default core/src/test/test_rigor if executable
#                       (TEST_BITCOIN remains a one-release alias)
#   SKIP_TEST_RIGOR=1   do not run test_rigor (portable CLI still covers CSHA256)
#                       (SKIP_TEST_BITCOIN remains a one-release alias)
#
# Coverage (hash / σ / root):
#   Step 2b golden lock  = pinned 433/226 + NegativeCore/InvalidCvg/ScriptMintNeg/ScriptAsset/DCR-neg/EdgeCross + surface_splits
#   Step 3 portable CLI  = authoritative CSHA256 Transform / FIPS / HashKernel check
#   Step 4 σ/root CLI    = production ComputeSigmaHash + ComputeConvergenceRootFromEligibilityTxids
#   test_rigor           = BOOST integration (encoder, σ, A-9-C hex, optional FIPS)
#   STALE binary warning = BOOST lacks sha256_fips_golden; rebuild test_rigor
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
cov "Golden lock (433/226 + NegativeCore/InvalidCvg/ScriptMintNeg/ScriptAsset/DCR-neg/EdgeCross + surface_splits): PASS"

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
  bash "$ROOT/formal_verification/scripts/extract_classify_supply.sh"
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
echo "    Hash FIPS / CSHA256 for this gate: portable CLI above (not SIMD, not test_rigor)."

echo "== 3b. Level 1/2 classify / sighash layout / supply shadow =="
if command -v g++ >/dev/null || command -v c++ >/dev/null; then
  bash "$ROOT/formal_verification/scripts/build_cpp_classify_cli.sh"
else
  echo "FAIL: g++/c++ required to build the C++ classify CLI"
  exit 1
fi
if [[ "${EXTRACT:-0}" != "1" ]]; then
  if ! command -v ocamlopt >/dev/null; then
    echo "FAIL: ocamlopt required to build classify/supply/sighash CLIs from committed kernels"
    exit 1
  fi
  for k in classify_kernel.ml supply_kernel.ml sighash_kernel.ml \
           classify_cli.ml supply_cli.ml sighash_cli.ml; do
    if [[ ! -f extracted/$k ]]; then
      echo "FAIL: extracted/$k missing (committed extraction)"
      exit 1
    fi
  done
  echo "building classify/supply/sighash CLIs from committed extraction"
  (cd extracted && rm -f classify_kernel.mli supply_kernel.mli sighash_kernel.mli \
    && ocamlopt -w -3 -o classify_cli classify_kernel.ml classify_cli.ml \
    && ocamlopt -w -3 -o supply_cli supply_kernel.ml supply_cli.ml \
    && ocamlopt -w -3 -o sighash_cli sighash_kernel.ml sighash_cli.ml)
fi
for cli in classify_cli supply_cli sighash_cli cpp_classify_cli; do
  if [[ ! -x extracted/$cli ]]; then
    echo "FAIL: extracted/$cli is not executable"
    exit 1
  fi
done
python3 tests/classify_triple_diff.py
python3 tests/sighash_preimage_diff.py
python3 tests/supply_shadow_diff.py
cov "Level 1/2 classify/sighash/supply (C++ + Python + extracted Coq): PASS"

echo "== 3c. Consensus Kernel extracted oracle ↔ C++ Kernel (P050/P051/P052/P053) =="
if [[ -x extracted/consensus_kernel_cli && -x extracted/cpp_consensus_kernel_cli ]]; then
  python3 tests/test_consensus_kernel_extraction.py
  python3 tests/test_consensus_kernel_production_integration.py
  python3 tests/test_consensus_kernel_bridge.py
  python3 "$ROOT/tools/lint-consensus-kernel-production-source.py"
  if [[ -x extracted/consensus_kernel_refinement_cli ]]; then
    python3 "$ROOT/tools/run_consensus_kernel_refinement.py"
  else
    echo "P053 refinement CLI absent (run formal_verification/run_consensus_kernel_refinement_gate.sh)"
  fi
  python3 "$ROOT/tools/lint-consensus-kernel-c-api.py"
  python3 "$ROOT/tools/lint-consensus-kernel-single-authority.py"
  python3 "$ROOT/tools/lint-consensus-kernel-shadow.py"
  python3 "$ROOT/tools/run_consensus_kernel_shadow.py"
  bash "$ROOT/formal_verification/run_consensus_kernel_formal_refinement.sh"
  bash "$ROOT/formal_verification/run_connectblock_kernel_boundary.sh"
  bash "$ROOT/formal_verification/run_transaction_kernel_refinement.sh"
  bash "$ROOT/formal_verification/run_consensus_surface_closure.sh"
  if [[ -x extracted/consensus_kernel_c_cli && -x extracted/consensus_kernel_c_abi_cli ]]; then
    python3 "$ROOT/formal_verification/test_consensus_kernel_c_abi.py"
  else
    echo "P053 C ABI CLIs absent (run formal_verification/extract_consensus_kernel_c.sh)"
  fi
  cov "Consensus Kernel oracle differential + production source lint: PASS"
else
  echo "skipped (run formal_verification/extract_consensus_kernel.sh first)"
  cov "Consensus Kernel oracle differential: SKIP (CLIs absent)"
fi

echo "== 3d. Consensus Kernel governance authority lock (P060) =="
python3 "$ROOT/tools/lint-consensus-authority.py"
cov "Consensus Kernel governance authority lock: PASS"

echo "== 3e. Consensus Kernel mainnet release-build audit (P061) =="
python3 "$ROOT/tools/audit-consensus-release-build.py"
cov "Consensus Kernel mainnet release-build audit: PASS"

echo "== 3f. Consensus Kernel audit-scope lock (P062) =="
python3 "$ROOT/tools/lint-audit-scope.py"
cov "Consensus Kernel audit-scope lock: PASS"

echo "== 3g. UTXO ledger transition boundary (P063) =="
python3 "$ROOT/tools/lint-ledger-kernel-boundary.py"
cov "UTXO ledger transition boundary: PASS"
echo "    fuzz ≠ theorem ≠ refinement. refinement_proved stays false."

echo "== 3h. UTXO state-transition boundary (P065) =="
python3 "$ROOT/tools/lint-utxo-state-transition-boundary.py"
cov "UTXO state-transition boundary: PASS"

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

echo "== 6. Optional C++ Boost suites (test_rigor) =="
SKIP_TEST_RIGOR="${SKIP_TEST_RIGOR:-${SKIP_TEST_BITCOIN:-0}}"
TEST_RIGOR="${TEST_RIGOR:-${TEST_BITCOIN:-}}"
if [[ "${SKIP_TEST_RIGOR}" == "1" ]]; then
  echo "skipped (SKIP_TEST_RIGOR=1). Portable CLIs still cover CSHA256/FIPS (step 3) and σ/root (step 4)."
  cov "test_rigor: SKIP (SKIP_TEST_RIGOR=1)"
else
  if [[ -z "${TEST_RIGOR}" && -x "$ROOT/core/src/test/test_rigor" ]]; then
    TEST_RIGOR="$ROOT/core/src/test/test_rigor"
    echo "auto-detected TEST_RIGOR=$TEST_RIGOR"
  elif [[ -z "${TEST_RIGOR}" && -x "$ROOT/core/src/test/test_bitcoin" ]]; then
    TEST_RIGOR="$ROOT/core/src/test/test_bitcoin"
    echo "auto-detected leftover TEST_RIGOR=$TEST_RIGOR (rebuild as test_rigor)"
  fi
  if [[ -n "${TEST_RIGOR}" && -x "${TEST_RIGOR}" ]]; then
    if "${TEST_RIGOR}" --list_content 2>/dev/null | grep -q sha256_fips_golden; then
      echo "test_rigor includes sha256_fips_golden (BOOST FIPS present)"
      cov "test_rigor BOOST: current (sha256_fips_golden present)"
    else
      echo "WARN: test_rigor is STALE (no sha256_fips_golden)."
      echo "      Covered anyway: CSHA256/FIPS via portable CLI (step 3); σ/root via step 4."
      echo "      Not covered: BOOST test sha256_fips_golden until you rebuild:"
      echo "        cd $ROOT/core && make -j\$(nproc)   # operator full build"
      echo "        ./src/test/test_rigor --run_test=selection_golden_tests,convergence_consensus_tests"
      echo "      Skip this step: SKIP_TEST_RIGOR=1"
      cov "test_rigor BOOST: STALE (no sha256_fips_golden; portable CLI covers hash FIPS)"
    fi
    "${TEST_RIGOR}" --run_test=selection_golden_tests
    "${TEST_RIGOR}" --run_test=convergence_consensus_tests
    "${TEST_RIGOR}" --run_test=rigor_classification_vector_tests,rigor_sighash_vector_tests,connectblock_res_mint_p0_041_tests,w32_reorg_stability_chain_tests
  else
    echo "skipped (no test_rigor). Portable CLIs still cover CSHA256/FIPS (step 3) and σ/root (step 4)."
    echo "  After operator make: TEST_RIGOR=$ROOT/core/src/test/test_rigor"
    cov "test_rigor: SKIP (binary absent)"
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

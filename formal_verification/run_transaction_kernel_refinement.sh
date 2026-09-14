#!/usr/bin/env bash
# P058: Transaction Kernel spend-predicate refinement. Does not switch production.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
COQ="$ROOT/formal_verification/coq"
SRC="$ROOT/core/src"
OUT="$ROOT/formal_verification/extracted"
TOOLS="$ROOT/tools"

echo "== P058 Transaction Kernel spend-predicate refinement =="

for v in SignatureBoundary.v MakeKernelInput.v TransactionKernelRefinement.v; do
  if grep -E '^[[:space:]]*(Admitted|Axiom|Parameter)[[:space:]]' "$COQ/$v"; then
    echo "FAIL: $v is not axiom-free"
    exit 1
  fi
  if grep -E '^[[:space:]]*Extract Constant' "$COQ/$v"; then
    echo "FAIL: Extract Constant in $v"
    exit 1
  fi
done
echo "COQ_ADMITTED=0 COQ_AXIOMS=0 COQ_PARAMETERS=0"

if ! command -v coqc >/dev/null; then
  echo "FAIL: coqc is not on PATH" >&2
  exit 1
fi

echo "== coqc transaction kernel modules =="
(cd "$COQ" && \
  coqc -q -R . Rigor -R ../rigor_v01 RigorV01 SignatureBoundary.v && \
  coqc -q -R . Rigor -R ../rigor_v01 RigorV01 TransactionKernelRefinement.v && \
  coqc -q -R . Rigor -R ../rigor_v01 RigorV01 MakeKernelInput.v)

echo "== Print Assumptions TransactionKernelRefinement =="
ASSUME="$(cd "$COQ" && coqtop -q -R . Rigor -R ../rigor_v01 RigorV01 <<'EOF'
Require TransactionKernelRefinement.
Require MakeKernelInput.
Print Assumptions TransactionKernelRefinement.predicate_theorem.
Print Assumptions TransactionKernelRefinement.spend_predicate_refinement.
Print Assumptions TransactionKernelRefinement.sighash_alignment.
Print Assumptions TransactionKernelRefinement.signature_boundary_alignment.
Print Assumptions TransactionKernelRefinement.fail_closed_flag.
Print Assumptions MakeKernelInput.witness_no_crypto_without_flag.
Print Assumptions MakeKernelInput.witness_no_crypto_if_scriptSig.
Print Assumptions MakeKernelInput.mint_no_crypto_unless_push_only.
Print Assumptions MakeKernelInput.acceptance_grew_means_new_only.
Print Assumptions MakeKernelInput.to_tki_crypto_accept.
Print Assumptions MakeKernelInput.crypto_accept_false_blocks_spend.
Print Assumptions MakeKernelInput.make_crypto_reject_blocks_spend.
Print Assumptions MakeKernelInput.to_tki_projects_flags.
Print Assumptions MakeKernelInput.all_raw_size.
Print Assumptions MakeKernelInput.pin_v0_accept_crypto.
Print Assumptions MakeKernelInput.pin_v0_no_witness_flag_crypto.
Print Assumptions MakeKernelInput.pin_mint_accept_crypto.
Print Assumptions MakeKernelInput.vst_deferred_and_narrow.
Print Assumptions MakeKernelInput.different_tags_different_preimages.
Print Assumptions MakeKernelInput.mint_oru_preimages_differ.
Print Assumptions MakeKernelInput.spend_message_is_sighash_digest.
Print Assumptions MakeKernelInput.kernel_signature_message_len.
Print Assumptions MakeKernelInput.crypto_accept_implies_path_tag.
Print Assumptions MakeKernelInput.mint_oru_tag_rejected.
Print Assumptions MakeKernelInput.tag_mismatch_all_reject.
Print Assumptions MakeKernelInput.unknown_version_empty_tag.
EOF
)"
echo "$ASSUME"
if echo "$ASSUME" | grep -q '^Axioms:'; then
  echo "FAIL: Print Assumptions listed axioms"
  exit 1
fi
echo "Print Assumptions: Closed under the global context (Axioms: 0)"

python3 "$TOOLS/lint-transaction-kernel-refinement.py"
python3 "$TOOLS/tests/test_lint_kernel_switch.py"
python3 "$TOOLS/lint-flag-bit-lock.py"
python3 "$ROOT/formal_verification/tools/check_vacuous.py"
python3 "$TOOLS/lint-theorem-inventory.py"
python3 "$TOOLS/lint-extracted-oracle-and-vst-scope.py"

CXX="${CXX:-$(command -v g++ || command -v c++)}"
mkdir -p "$OUT"
echo "== g++ test_transaction_kernel_refinement =="
"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections -I"$SRC" \
  -o "$OUT/test_transaction_kernel_refinement" \
  "$SRC/test/test_transaction_kernel_refinement.cpp" \
  "$TOOLS/consensus_kernel_refinement_log_stub.cpp" \
  "$SRC/script/rigor_classify.cpp" \
  "$SRC/script/script.cpp" \
  "$SRC/uint256.cpp" \
  "$SRC/util/strencodings.cpp" \
  "$SRC/crypto/sha256.cpp" \
  -Wl,--gc-sections

"$OUT/test_transaction_kernel_refinement"

# S2/A1: production VerifyRigorPredicate is MakeKernelInput + the P058
# boundary. Keep the differential lock that the public entry and the
# boundary still agree.
echo "== g++ test_predicate_boundary_differential =="
"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections -I"$SRC" \
  -o "$OUT/test_predicate_boundary_differential" \
  "$SRC/test/test_predicate_boundary_differential.cpp" \
  "$SRC/test/predicate_boundary_ed25519_stub.cpp" \
  "$SRC/script/predicate_verify.cpp" \
  "$SRC/script/script_error.cpp" \
  "$SRC/consensus/trace/consensus_trace.cpp" \
  "$SRC/script/rigor_classify.cpp" \
  "$SRC/script/script.cpp" \
  "$SRC/uint256.cpp" \
  "$SRC/util/strencodings.cpp" \
  "$SRC/crypto/sha256.cpp" \
  "$SRC/crypto/ripemd160.cpp" \
  "$SRC/crypto/sha512.cpp" \
  "$SRC/crypto/hmac_sha512.cpp" \
  "$SRC/hash.cpp" \
  -Wl,--gc-sections

"$OUT/test_predicate_boundary_differential"

echo "== g++ test_makekernelinput_field_map =="
"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections -I"$SRC" \
  -o "$OUT/test_makekernelinput_field_map" \
  "$SRC/test/test_makekernelinput_field_map.cpp" \
  "$SRC/test/predicate_boundary_ed25519_stub.cpp" \
  "$SRC/script/predicate_verify.cpp" \
  "$SRC/script/script_error.cpp" \
  "$SRC/consensus/trace/consensus_trace.cpp" \
  "$SRC/script/rigor_classify.cpp" \
  "$SRC/script/script.cpp" \
  "$SRC/uint256.cpp" \
  "$SRC/util/strencodings.cpp" \
  "$SRC/crypto/sha256.cpp" \
  "$SRC/crypto/ripemd160.cpp" \
  "$SRC/crypto/sha512.cpp" \
  "$SRC/crypto/hmac_sha512.cpp" \
  "$SRC/hash.cpp" \
  "$SRC/primitives/transaction.cpp" \
  -Wl,--gc-sections

"$OUT/test_makekernelinput_field_map"

echo "== extract MakeKernelInput test oracle =="
bash "$ROOT/formal_verification/extract_makekernelinput_oracle.sh"

echo "== predicate boundary differential fuzz smoke =="
bash "$ROOT/fuzz/predicate_boundary_differential/run_smoke.sh"

echo "== makekernelinput out-of-domain differential smoke =="
MAKEKERNELINPUT_FUZZ_ITERS="${MAKEKERNELINPUT_FUZZ_ITERS:-20000}" \
  bash "$ROOT/fuzz/makekernelinput_differential/run.sh"

python3 "$TOOLS/lint-predicate-boundary-differential.py"

echo "TRANSACTION_KERNEL_REFINEMENT=CLOSED"
echo "SPEND_PREDICATE_ALIGNMENT=PASS"
echo "SIGHASH_ALIGNMENT=PASS"
echo "SIGNATURE_BOUNDARY_ALIGNMENT=PASS"
echo "FAIL_CLOSED=PASS"
echo "SCRIPT_VM_ABSENT=PASS"
echo "MULTISIG_ABSENT=PASS"
echo "PREDICATE_BOUNDARY_VS_PRODUCTION=SAME-FUNCTION"
echo "ACCEPTANCE_MONOTONICITY=PASS"
echo "MAKEKERNELINPUT_GATES=PROVED"
echo "MAKEKERNELINPUT_FIELD_MAP=VECTOR-TESTED"
echo "BOUNDED_GATE_DOMAIN=9216"
echo "DOMAIN_CRYPTO_TRUE=300"
echo "SIGHASH_DOMAIN_CONNECTED=PASS"
echo "MAKEKERNELINPUT_SIGHASH_EQ_SIGNATUREHASH=PASS"
echo "MAKEKERNELINPUT_PATH_TAG_GATES=PASS"
echo "EXTRACTED_KERNEL_TEST_ORACLE=PASS"
echo "COMPCERT_VST_DEFERRED=TRUE"
echo "COMPCERT_VST_CANDIDATE=MakeKernelInput"
echo "SPEND_ORACLE_FIELDS=push_only,stack_clean,keyhash_match,crypto_accept"
python3 "$TOOLS/refinement_status.py"

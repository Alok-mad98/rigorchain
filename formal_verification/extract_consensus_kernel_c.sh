#!/usr/bin/env bash
# C ABI extraction + verify-only C ABI CLI. Not a production switch.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
COQ="$ROOT/formal_verification/coq"
OUT="$ROOT/formal_verification/extracted"
TOOLS="$ROOT/formal_verification/tools"
SRC="$ROOT/core/src"
V="$COQ/ExtractConsensusKernelC.v"

if ! command -v coqc >/dev/null; then
  echo "FAIL: coqc is not on PATH" >&2
  exit 1
fi
if ! command -v ocamlopt >/dev/null; then
  echo "FAIL: ocamlopt is not on PATH" >&2
  exit 1
fi

ver="$(coqc --version | head -n1)"
case "${ver}" in
  *8.15.*) ;;
  *)
    echo "error: extraction requires Coq 8.15.x, got: ${ver}" >&2
    exit 1
    ;;
esac

echo "== Print Extraction inventory (Coq 8.15 has no Print Extraction vernacular) =="
sed -n '/^Extraction Language/,/^  ledger_oru_delta./p' "$V"

if grep -E '^[[:space:]]*(Admitted|Axiom|Parameter)[[:space:]]' "$V"; then
  echo "FAIL: ExtractConsensusKernelC.v contains Axiom/Parameter/Admitted" >&2
  exit 1
fi
echo "COQ_ADMITTED=0 COQ_AXIOMS=0 COQ_PARAMETERS=0"

mkdir -p "$OUT"
cd "$COQ"
echo "== coqc ExtractConsensusKernelC.v =="
coqc -q -R . Rigor -R ../rigor_v01 RigorV01 ExtractConsensusKernelC.v

for f in consensus_kernel_c.ml consensus_kernel_c.mli; do
  if [[ -f "$COQ/$f" ]]; then
    mv -f "$COQ/$f" "$OUT/$f"
  fi
done
rm -f "$COQ/ExtractConsensusKernelC.vo" "$COQ/ExtractConsensusKernelC.glob" "$COQ/.ExtractConsensusKernelC.aux"

if [[ ! -f "$OUT/consensus_kernel_c.ml" ]]; then
  echo "FAIL: missing $OUT/consensus_kernel_c.ml" >&2
  exit 1
fi
if grep -E 'Obj\.magic|Extract Constant' "$OUT/consensus_kernel_c.ml" >/dev/null; then
  echo "FAIL: extracted OCaml contains Obj.magic or Extract Constant residue" >&2
  exit 1
fi

cd "$OUT"
rm -f consensus_kernel_c.mli
echo "== ocamlopt consensus_kernel_c_cli =="
ocamlopt -w -3 -o consensus_kernel_c_cli consensus_kernel_c.ml "$TOOLS/consensus_kernel_c_cli.ml"

CXX="${CXX:-$(command -v g++ || command -v c++)}"
echo "== g++ consensus_kernel_c_abi_cli (-DRIGOR_KERNEL_ABI_VERIFY_ONLY) =="
"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections \
  -DRIGOR_KERNEL_ABI_VERIFY_ONLY \
  -I"$SRC" \
  -o "$OUT/consensus_kernel_c_abi_cli" \
  "$TOOLS/consensus_kernel_c_abi_cli.cpp" \
  "$ROOT/tools/consensus_kernel_refinement_log_stub.cpp" \
  "$SRC/consensus/bridge/consensus_kernel_c_api.cpp" \
  "$SRC/consensus/bridge/kernel_marshalling.cpp" \
  "$SRC/consensus/bridge/issuance_bridge.cpp" \
  "$SRC/consensus/bridge/classify_bridge.cpp" \
  "$SRC/consensus/bridge/supply_bridge.cpp" \
  "$SRC/consensus/rigor_issuance.cpp" \
  "$SRC/consensus/rigor_supply.cpp" \
  "$SRC/script/rigor_classify.cpp" \
  "$SRC/script/script.cpp" \
  "$SRC/uint256.cpp" \
  "$SRC/util/strencodings.cpp" \
  "$SRC/crypto/sha256.cpp" \
  "$SRC/arith_uint256.cpp" \
  -Wl,--gc-sections

echo "== g++ cpp_consensus_kernel_cli =="
"$CXX" -O2 -std=c++17 -Wall -Wextra \
  -I"$SRC" \
  -o "$OUT/cpp_consensus_kernel_cli" \
  "$TOOLS/cpp_consensus_kernel_cli.cpp" \
  "$SRC/consensus/rigor_issuance.cpp" \
  "$SRC/script/rigor_classify.cpp" \
  "$SRC/script/script.cpp" \
  "$SRC/uint256.cpp" \
  "$SRC/util/strencodings.cpp"

echo "extracted: $OUT/consensus_kernel_c_cli"
echo "c abi:     $OUT/consensus_kernel_c_abi_cli"
echo "cpp adapter: $OUT/cpp_consensus_kernel_cli"
"$OUT/consensus_kernel_c_cli" issuance 0
"$OUT/consensus_kernel_c_abi_cli" issuance 0
"$OUT/cpp_consensus_kernel_cli" res-price 0

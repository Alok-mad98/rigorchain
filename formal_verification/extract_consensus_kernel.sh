#!/usr/bin/env bash
# P050: extract the P049 Consensus Kernel and build the executable CLI.
# Not a refinement. Requires Coq 8.15.x and ocamlopt.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
COQ="$ROOT/formal_verification/coq"
OUT="$ROOT/formal_verification/extracted"
TOOLS="$ROOT/formal_verification/tools"
SRC="$ROOT/core/src"

if ! command -v coqc >/dev/null; then
  echo "FAIL: coqc is not on PATH" >&2
  exit 1
fi
if ! command -v ocamlopt >/dev/null; then
  echo "FAIL: ocamlopt is not on PATH" >&2
  exit 1
fi

echo "== versions =="
coqc --version | head -n1
ocamlopt -vnum
echo "CXX=${CXX:-$(command -v g++ || command -v c++ || echo missing)}"

ver="$(coqc --version | head -n1)"
case "${ver}" in
  *8.15.*) ;;
  *)
    echo "error: extraction requires Coq 8.15.x, got: ${ver}" >&2
    exit 1
    ;;
esac

mkdir -p "$OUT"
cd "$COQ"

echo "== coqc ExtractConsensusKernel.v =="
t0=$(date +%s)
coqc -q -R . Rigor -R fee-burn Rigor.FeeBurn -R ../rigor_v01 RigorV01 ExtractConsensusKernel.v
t1=$(date +%s)
echo "extract_coqc_seconds=$((t1 - t0))"

for f in consensus_kernel.ml consensus_kernel.mli; do
  if [[ -f "$COQ/$f" ]]; then
    mv -f "$COQ/$f" "$OUT/$f"
  fi
done
rm -f "$COQ/ExtractConsensusKernel.vo" "$COQ/ExtractConsensusKernel.glob" "$COQ/.ExtractConsensusKernel.aux"

if [[ ! -f "$OUT/consensus_kernel.ml" ]]; then
  echo "FAIL: extraction did not write $OUT/consensus_kernel.ml" >&2
  exit 1
fi

if grep -E 'Obj\.magic|Extract Constant' "$OUT/consensus_kernel.ml" >/dev/null; then
  echo "FAIL: extracted OCaml contains Obj.magic or Extract Constant residue" >&2
  exit 1
fi

cd "$OUT"
rm -f consensus_kernel.mli
echo "== ocamlopt consensus_kernel_cli =="
t2=$(date +%s)
ocamlopt -w -3 -o consensus_kernel_cli consensus_kernel.ml "$TOOLS/consensus_kernel_cli.ml"
t3=$(date +%s)
echo "ocamlopt_seconds=$((t3 - t2))"

CXX="${CXX:-$(command -v g++ || command -v c++)}"
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

echo "extracted: $OUT/consensus_kernel_cli"
echo "cpp adapter: $OUT/cpp_consensus_kernel_cli"
echo "OCaml startup:"
time -p "$OUT/consensus_kernel_cli" epoch 0
echo "C++ startup:"
time -p "$OUT/cpp_consensus_kernel_cli" epoch 0

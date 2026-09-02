#!/usr/bin/env bash
# F-4: extract MakeKernelInput.v as a test-only OCaml oracle.
# Does not switch production. Does not link into rigord.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
COQ="$ROOT/formal_verification/coq"
OUT="$ROOT/formal_verification/extracted"
TOOLS="$ROOT/formal_verification/tools"
V="$COQ/ExtractMakeKernelInput.v"

if ! command -v coqc >/dev/null; then
  echo "FAIL: coqc is not on PATH" >&2
  exit 1
fi
if ! command -v ocamlopt >/dev/null; then
  echo "FAIL: ocamlopt is not on PATH" >&2
  exit 1
fi

if grep -E '^[[:space:]]*(Admitted|Axiom|Parameter)[[:space:]]' "$V"; then
  echo "FAIL: ExtractMakeKernelInput.v is not axiom-free" >&2
  exit 1
fi
if grep -E '^[[:space:]]*Extract Constant' "$V"; then
  echo "FAIL: Extract Constant in ExtractMakeKernelInput.v" >&2
  exit 1
fi
if grep -E '^[[:space:]]*Extract Inductive' "$V"; then
  echo "FAIL: Extract Inductive override in ExtractMakeKernelInput.v" >&2
  exit 1
fi

mkdir -p "$OUT"
cd "$COQ"
echo "== coqc ExtractMakeKernelInput.v =="
coqc -q -R . Rigor -R fee-burn Rigor.FeeBurn -R ../rigor_v01 RigorV01 ExtractMakeKernelInput.v

for f in makekernelinput_oracle.ml makekernelinput_oracle.mli; do
  if [[ -f "$COQ/$f" ]]; then
    mv -f "$COQ/$f" "$OUT/$f"
  fi
done
rm -f "$COQ/ExtractMakeKernelInput.vo" "$COQ/ExtractMakeKernelInput.glob" "$COQ/.ExtractMakeKernelInput.aux"

if [[ ! -f "$OUT/makekernelinput_oracle.ml" ]]; then
  echo "FAIL: missing $OUT/makekernelinput_oracle.ml" >&2
  exit 1
fi
if grep -E 'Obj\.magic|Extract Constant' "$OUT/makekernelinput_oracle.ml" >/dev/null; then
  echo "FAIL: extracted OCaml contains Obj.magic or Extract Constant residue" >&2
  exit 1
fi

cd "$OUT"
rm -f makekernelinput_oracle.mli
echo "== ocamlopt makekernelinput_oracle_cli =="
ocamlopt -w -3 -o makekernelinput_oracle_cli makekernelinput_oracle.ml "$TOOLS/makekernelinput_oracle_cli.ml"

echo "extracted oracle: $OUT/makekernelinput_oracle_cli"
"$OUT/makekernelinput_oracle_cli"
echo "EXTRACTED_TO_BINARY=NOT-LINKED"

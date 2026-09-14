#!/usr/bin/env bash
# Extract classify / apply_delta / sighash_preimage and build CLIs.
# Not a refinement proof. Requires coqc 8.15 and ocamlopt.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
COQ="$ROOT/formal_verification/coq"
OUT="$ROOT/formal_verification/extracted"

if ! command -v coqc >/dev/null; then
  echo "FAIL: coqc is not on PATH" >&2
  exit 1
fi
if ! command -v ocamlopt >/dev/null; then
  echo "FAIL: ocamlopt is not on PATH" >&2
  exit 1
fi

mkdir -p "$OUT"
cd "$COQ"
coqc -q -R . Rigor -R ../rigor_v01 RigorV01 ExtractClassifySupply.v

for f in classify_kernel.ml classify_kernel.mli supply_kernel.ml supply_kernel.mli \
         sighash_kernel.ml sighash_kernel.mli; do
  if [[ -f "$COQ/$f" ]]; then
    mv -f "$COQ/$f" "$OUT/$f"
  fi
done
rm -f "$COQ/ExtractClassifySupply.vo" "$COQ/ExtractClassifySupply.glob" "$COQ/.ExtractClassifySupply.aux"

cd "$OUT"
rm -f classify_kernel.mli supply_kernel.mli sighash_kernel.mli
ocamlopt -w -3 -o classify_cli classify_kernel.ml "$ROOT/formal_verification/extracted/classify_cli.ml"
ocamlopt -w -3 -o supply_cli supply_kernel.ml "$ROOT/formal_verification/extracted/supply_cli.ml"
ocamlopt -w -3 -o sighash_cli sighash_kernel.ml "$ROOT/formal_verification/extracted/sighash_cli.ml"
echo "extracted: $OUT/classify_cli $OUT/supply_cli $OUT/sighash_cli"

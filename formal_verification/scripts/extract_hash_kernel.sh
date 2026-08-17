#!/usr/bin/env bash
# Extract Coq RigorSha256 to OCaml and build hash256_cli.
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
coqc -q -R . Rigor ExtractHashKernel.v

# Coq 8.15 writes Extraction output next to the .v file.
for f in hash256_kernel.ml hash256_kernel.mli; do
  if [[ -f "$COQ/$f" ]]; then
    mv -f "$COQ/$f" "$OUT/$f"
  fi
done
rm -f "$COQ/ExtractHashKernel.vo" "$COQ/ExtractHashKernel.glob" "$COQ/.ExtractHashKernel.aux"

if [[ ! -f "$OUT/hash256_kernel.ml" ]]; then
  echo "FAIL: extraction did not write $OUT/hash256_kernel.ml" >&2
  ls -la "$OUT" "$COQ" | head
  exit 1
fi

cd "$OUT"
rm -f hash256_kernel.mli
# Coq 8.15 emits Pervasives.*; -w -3 keeps the deprecation alerts from failing the build.
ocamlopt -w -3 -o hash256_cli hash256_kernel.ml hash256_cli.ml
echo "extracted hash kernel: $OUT/hash256_cli"

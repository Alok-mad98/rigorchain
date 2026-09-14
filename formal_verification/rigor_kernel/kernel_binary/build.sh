#!/usr/bin/env bash
# Build rigor-kernel-checker from extracted OCaml.
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
EXT="$(cd "$DIR/../extraction" && pwd)"
cd "$DIR"

if [[ ! -f "$EXT/ExtractedKernel.ml" ]]; then
  echo "FAIL: ExtractedKernel.ml missing; run extraction/build.sh first" >&2
  exit 1
fi

# Compile the extracted interface first so .cmi sits beside .mli.
(
  cd "$EXT"
  ocamlopt -alert -deprecated -c ExtractedKernel.mli ExtractedKernel.ml
)
ocamlopt -alert -deprecated -I "$EXT" "$EXT/ExtractedKernel.cmx" main.ml \
  -o rigor-kernel-checker

if [[ ! -x rigor-kernel-checker ]]; then
  echo "FAIL: rigor-kernel-checker was not produced" >&2
  exit 1
fi

echo "BINARY_ARTIFACT_BUILT=TRUE"

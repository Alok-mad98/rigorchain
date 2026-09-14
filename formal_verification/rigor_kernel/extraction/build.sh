#!/usr/bin/env bash
# Extract the kernel validators to OCaml.
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
KERNEL_ROOT="$(cd "$DIR/.." && pwd)"
cd "$DIR"

if [[ ! -f "$KERNEL_ROOT/Kernel/KernelSoundness.vo" ]]; then
  echo "FAIL: Kernel.KernelSoundness.vo missing; run make first" >&2
  exit 1
fi

coqc -R "$KERNEL_ROOT" RigorKernel ExtractKernel.v

if [[ ! -f ExtractedKernel.ml ]]; then
  echo "FAIL: ExtractedKernel.ml was not produced" >&2
  exit 1
fi

echo "KERNEL_EXTRACTION=PASS"
echo "EXTRACTION_SUCCESS=TRUE"

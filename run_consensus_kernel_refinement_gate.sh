#!/usr/bin/env bash
# Thin wrapper. Implementation lives next to extract_consensus_kernel.sh.
set -euo pipefail
exec "$(cd "$(dirname "$0")" && pwd)/formal_verification/run_consensus_kernel_refinement_gate.sh"

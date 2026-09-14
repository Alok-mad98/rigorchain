#!/usr/bin/env bash
# Phase 10 orchestrator. Subshell sidecar + cone 8.15 docs.
# Does not flip flags. Does not take Level 5.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=sidecar_lib.sh
. "${SCRIPT_DIR}/sidecar_lib.sh"

print_license_banner
assert_cone_coq_untouched

"${SCRIPT_DIR}/compile_phase10_nopipeline.sh"
"${SCRIPT_DIR}/compile_phase10_stages.sh"
"${SCRIPT_DIR}/compile_phase10_doc.sh"

echo "PHASE10=ABORT_LEVEL5"
echo "PHASE10_PARTIAL=true"
echo "MATURITY_STAYS=4.9"
echo "COMPILER_CORRECTNESS=false"
echo "verified_compiler_in_current_tcb=false"
echo "PARENT_CCOMP=$(command -v ccomp || echo ABSENT)"
echo "CONE_COQ=$(/usr/bin/coqc --print-version | awk '{print $1}')"

#!/usr/bin/env bash
# Phase 10 cone 8.15 documentation theorems. No CertiCoq plugin.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=sidecar_lib.sh
. "${SCRIPT_DIR}/sidecar_lib.sh"

PROOF="${CERTICOQ_ROOT}/proofs"
CONE_COQC="${CONE_COQC:-/usr/bin/coqc}"
CONE_COQTOP="${CONE_COQTOP:-/usr/bin/coqtop}"

print_license_banner
assert_cone_coq_untouched

if command -v ccomp >/dev/null 2>&1; then
  echo "SIDECAR_LEAK: ccomp on host PATH" >&2
  exit 1
fi

(
  export PATH="/usr/bin:/bin"
  cd "${PROOF}"
  echo "PHASE10_DOC_COQC=${CONE_COQC}"
  "${CONE_COQC}" -q -R . RigorCertiCoqProofs Phase10Flags.v
  "${CONE_COQC}" -q -R . RigorCertiCoqProofs CodegenUpstream.v
  ASSUME_OUT="$(mktemp)"
  "${CONE_COQTOP}" -q -R . RigorCertiCoqProofs >"${ASSUME_OUT}" 2>&1 <<'EOF'
From RigorCertiCoqProofs Require Import Phase10Flags.
From RigorCertiCoqProofs Require Import CodegenUpstream.
Print Assumptions phase10_no_mem_clight.
Print Assumptions phase10_no_transf_on_mem_clight.
Print Assumptions phase10_sidecar_cps_exp.
Print Assumptions phase10_no_well_scoped_or_correct.
Print Assumptions phase10_codegen_still_unproved.
Print Assumptions phase10_level5_not_taken.
Print Assumptions follow_upstream_codegen.
Print Assumptions no_handwritten_codegen.
EOF
  cat "${ASSUME_OUT}"
  closed="$(grep -c 'Closed under the global context' "${ASSUME_OUT}" || true)"
  rm -f "${ASSUME_OUT}"
  if [ "${closed}" -lt 8 ]; then
    echo "PHASE10_DOC_ASSUMPTIONS_NOT_CLOSED: got ${closed}" >&2
    exit 1
  fi
  echo "PHASE10_DOC_ASSUMPTIONS=CLOSED"
)

echo "CONE_COQ=8.15.0"
echo "PHASE10=ABORT_LEVEL5"
echo "PHASE10_PARTIAL=true"
echo "COMPILER_CORRECTNESS=false"
echo "verified_compiler_in_current_tcb=false"
echo "MATURITY_STAYS=4.9"

#!/usr/bin/env bash
# Phase 9 cone 8.15 documentation theorems. No CertiCoq plugin.
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
  echo "PHASE9_DOC_COQC=${CONE_COQC}"
  "${CONE_COQC}" -q -R . RigorCertiCoqProofs CompCertAST.v
  "${CONE_COQC}" -q -R . RigorCertiCoqProofs LambdaANFInstance.v
  ASSUME_OUT="$(mktemp)"
  "${CONE_COQTOP}" -q -R . RigorCertiCoqProofs >"${ASSUME_OUT}" 2>&1 <<'EOF'
From RigorCertiCoqProofs Require Import CompCertAST.
From RigorCertiCoqProofs Require Import LambdaANFInstance.
Print Assumptions transf_c_program_not_computed.
Print Assumptions ccomp_success_is_not_transf_c_program.
Print Assumptions no_check_oru_lambdaanf_exp.
Print Assumptions codegen_still_unproved.
EOF
  cat "${ASSUME_OUT}"
  closed="$(grep -c 'Closed under the global context' "${ASSUME_OUT}" || true)"
  rm -f "${ASSUME_OUT}"
  if [ "${closed}" -lt 4 ]; then
    echo "PHASE9_DOC_ASSUMPTIONS_NOT_CLOSED: got ${closed}" >&2
    exit 1
  fi
  echo "PHASE9_DOC_ASSUMPTIONS=CLOSED"
)

echo "CONE_COQ=8.15.0"
echo "PHASE9=ABORT"
echo "COMPILER_CORRECTNESS=false"
echo "verified_compiler_in_current_tcb=false"
echo "MATURITY_STAYS=4.9"

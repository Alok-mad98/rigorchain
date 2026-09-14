#!/usr/bin/env bash
# TASK-018 cone 8.15 documentation flags. No CertiCoq / CompCert.
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
  "${CONE_COQC}" -q -R . RigorCertiCoqProofs Task018Flags.v
  ASSUME_OUT="$(mktemp)"
  "${CONE_COQTOP}" -q -R . RigorCertiCoqProofs >"${ASSUME_OUT}" 2>&1 <<'EOF'
From RigorCertiCoqProofs Require Import Task018Flags.
Print Assumptions clight_to_cminor_closed_six_checkers.
Print Assumptions clight_to_asm_not_closed.
Print Assumptions no_closed_compcert_check_oru.
Print Assumptions task018_does_not_claim_level_5.
EOF
  cat "${ASSUME_OUT}"
  closed="$(grep -c 'Closed under the global context' "${ASSUME_OUT}" || true)"
  rm -f "${ASSUME_OUT}"
  if [ "${closed}" -lt 4 ]; then
    echo "TASK018_DOC_ASSUMPTIONS_NOT_CLOSED: got ${closed}" >&2
    exit 1
  fi
  echo "TASK018_DOC_ASSUMPTIONS=CLOSED"
)

echo "CONE_COQ=8.15.0"
echo "CLIGHT_TO_CMINOR_CLOSED=true"
echo "CLIGHT_TO_ASM_CLOSED=false"
echo "LEVEL_5=NOT_CLAIMED"
echo "COMPILER_CORRECTNESS=false"
echo "verified_compiler_in_current_tcb=false"
echo "REFINEMENT_PROVED=false"
echo "CPP_KERNELINPUT_EQUIVALENCE=NOT_PROVED"

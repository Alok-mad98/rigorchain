#!/usr/bin/env bash
# TASK 3.1A cone 8.15 documentation flags. No CompCert.
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
  "${CONE_COQC}" -q -R . RigorCertiCoqProofs Task31AFlags.v
  ASSUME_OUT="$(mktemp)"
  "${CONE_COQTOP}" -q -R . RigorCertiCoqProofs >"${ASSUME_OUT}" 2>&1 <<'EOF'
From RigorCertiCoqProofs Require Import Task31AFlags.
Print Assumptions task31a_route_blocked_by_tcb.
Print Assumptions task31a_asm_not_closed.
Print Assumptions task31a_no_closed_compcert_check_oru.
Print Assumptions task31a_does_not_claim_level_5.
EOF
  cat "${ASSUME_OUT}"
  closed="$(grep -c 'Closed under the global context' "${ASSUME_OUT}" || true)"
  rm -f "${ASSUME_OUT}"
  if [ "${closed}" -lt 4 ]; then
    echo "TASK31A_DOC_ASSUMPTIONS_NOT_CLOSED: got ${closed}" >&2
    exit 1
  fi
  echo "TASK31A_DOC_ASSUMPTIONS=CLOSED"
)
echo "COMPILER_CLOSED_ROUTE=BLOCKED_BY_TCB"
echo "LEVEL_5=NOT_CLAIMED"
echo "CLIGHT_TO_CMINOR_CLOSED=true"
echo "CLIGHT_TO_ASM_CLOSED=false"

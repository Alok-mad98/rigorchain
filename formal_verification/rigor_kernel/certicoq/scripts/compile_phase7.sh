#!/usr/bin/env bash
# Phase 7: compile correspondence / composition docs on cone 8.15.
# Does not load the CertiCoq plugin. Does not flip flags.
# Print Assumptions must be Closed (no Axiom from pipeline.v).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=sidecar_lib.sh
. "${SCRIPT_DIR}/sidecar_lib.sh"

PROOF="${CERTICOQ_ROOT}/proofs"
CONE_COQC="${CONE_COQC:-/usr/bin/coqc}"
CONE_COQTOP="${CONE_COQTOP:-/usr/bin/coqtop}"

print_license_banner
assert_cone_coq_untouched
refuse_host_path_install

if command -v ccomp >/dev/null 2>&1; then
  echo "SIDECAR_LEAK: ccomp on host PATH" >&2
  exit 1
fi
if [ ! -x "${CONE_COQC}" ]; then
  echo "cone coqc missing" >&2
  exit 1
fi

(
  export PATH="/usr/bin:/bin"
  cd "${PROOF}"
  echo "PHASE7_COQC=${CONE_COQC}"
  echo "PHASE7_COQ=$("${CONE_COQC}" --print-version | awk '{print $1}')"
  "${CONE_COQC}" -q -R . RigorCertiCoqProofs CertiCoqCorrespondence.v
  "${CONE_COQC}" -q -R . RigorCertiCoqProofs CompiledCheckerSoundness.v
  echo "PHASE7_COMPILE=PASS"
  echo "PRINT_ASSUMPTIONS=START"
  ASSUME_OUT="$(mktemp)"
  "${CONE_COQTOP}" -q -R . RigorCertiCoqProofs >"${ASSUME_OUT}" 2>&1 <<'EOF'
From RigorCertiCoqProofs Require Import CertiCoqCorrespondence.
From RigorCertiCoqProofs Require Import CompiledCheckerSoundness.
Print Assumptions certicoq_applies_to_check_oru.
Print Assumptions rewrite_check_eq_vacuous.
Print Assumptions compcert_applies_to_generated_clight.
Print Assumptions compiled_driver_not_the_checker.
Print Assumptions we_do_not_claim_node_from_compiled_checker.
Print Assumptions we_do_not_claim_refinement_proved.
EOF
  cat "${ASSUME_OUT}"
  if grep -q 'Error:' "${ASSUME_OUT}"; then
    echo "PRINT_ASSUMPTIONS=FAIL" >&2
    rm -f "${ASSUME_OUT}"
    exit 1
  fi
  closed="$(grep -c 'Closed under the global context' "${ASSUME_OUT}" || true)"
  if [ "${closed}" -lt 6 ]; then
    echo "PRINT_ASSUMPTIONS_NOT_CLOSED: expected 6 Closed, got ${closed}" >&2
    rm -f "${ASSUME_OUT}"
    exit 1
  fi
  rm -f "${ASSUME_OUT}"
  echo "PRINT_ASSUMPTIONS=CLOSED"
  echo "PRINT_ASSUMPTIONS=DONE"
)

if [ "$(cone_coq_version)" != "8.15.0" ]; then
  echo "CONE_COQ_REGRESSION after Phase 7" >&2
  exit 1
fi
if command -v ccomp >/dev/null 2>&1; then
  echo "SIDECAR_LEAK: ccomp on host PATH after Phase 7" >&2
  exit 1
fi

echo "CONE_COQ=8.15.0"
echo "PHASE7=PASS"
echo "PHASE7_PIPELINE_INSTANTIATED=false"
echo "COMPILER_CORRECTNESS=false"
echo "verified_compiler_in_current_tcb=false"
echo "BINARY_SEMANTIC_EQUIVALENCE=false"
echo "NOT_A_KERNEL_CLAIM=true"
echo "MATURITY_STAYS=4.9"

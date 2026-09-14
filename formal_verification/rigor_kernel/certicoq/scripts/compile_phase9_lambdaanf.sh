#!/usr/bin/env bash
# Phase 9: load LambdaANF published lemmas in sidecar 8.19.
# Does not Require Compiler.pipeline.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=sidecar_lib.sh
. "${SCRIPT_DIR}/sidecar_lib.sh"

OPAMROOT="${CERTICOQ_ROOT}/.opam-root"
OPAMBIN="${CERTICOQ_ROOT}/bin/opam"
SWITCH=rigor-certicoq-8.19
TRY="${CERTICOQ_ROOT}/proofs/sidecar819"
CERTICOQ_R="${OPAMROOT}/${SWITCH}/lib/coq/user-contrib/CertiCoq"
COMPCERT_R="${OPAMROOT}/${SWITCH}/lib/coq/user-contrib/compcert"
METACOQ_R="${OPAMROOT}/${SWITCH}/lib/coq/user-contrib"

print_license_banner
assert_cone_coq_untouched

if command -v ccomp >/dev/null 2>&1; then
  echo "SIDECAR_LEAK: ccomp on host PATH" >&2
  exit 1
fi

(
  export OPAMROOT
  export OPAMROOTISOK=1
  eval "$("${OPAMBIN}" env --switch="${SWITCH}" --set-switch)"
  if [ "$(command -v coqc)" = "/usr/bin/coqc" ]; then
    echo "SIDECAR_LEAK: Phase 9 LambdaANF used cone coqc" >&2
    exit 1
  fi
  echo "PHASE9_LANF_COQC=$(command -v coqc)"
  echo "PHASE9_LANF_COQ=$(coqc --print-version | awk '{print $1}')"
  cd "${TRY}"
  # user-contrib is already on the sidecar load path via COQPATH / default
  set +e
  coqc -q LambdaANFTry.v >"${TRY}/lambdaanf_try.log" 2>&1
  RC=$?
  set -e
  echo "PHASE9_LANF_TRY_RC=${RC}"
  tail -40 "${TRY}/lambdaanf_try.log" || true
  if [ "${RC}" -ne 0 ]; then
    echo "PHASE9_LANF_TRY=FAIL_COMPILE"
    exit 0
  fi
  echo "PHASE9_LANF_TRY=PASS"
)

if command -v ccomp >/dev/null 2>&1; then
  echo "SIDECAR_LEAK: ccomp on host PATH after LambdaANF try" >&2
  exit 1
fi
echo "CONE_COQ=$(/usr/bin/coqc --print-version | awk '{print $1}')"

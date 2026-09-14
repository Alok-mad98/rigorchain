#!/usr/bin/env bash
# Phase 10: quote check_oru and run CertiCoq pipeline in sidecar 8.19
# Gallina (no print_Clight). Subshell only.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=sidecar_lib.sh
. "${SCRIPT_DIR}/sidecar_lib.sh"

OPAMROOT="${CERTICOQ_ROOT}/.opam-root"
OPAMBIN="${CERTICOQ_ROOT}/bin/opam"
SWITCH=rigor-certicoq-8.19
TRY="${CERTICOQ_ROOT}/proofs/sidecar819"
SURF="${CERTICOQ_ROOT}/generated/surface-8.19"
COMPCERT_R="${OPAMROOT}/${SWITCH}/lib/coq/user-contrib/compcert"

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
    echo "SIDECAR_LEAK: Phase 10 used cone coqc" >&2
    exit 1
  fi
  echo "PHASE10_COQC=$(command -v coqc)"
  echo "PHASE10_COQ=$(coqc --print-version | awk '{print $1}')"
  cd "${TRY}"
  echo "PHASE10_QUOTE=START"
  set +e
  timeout 180 coqc -q -R "${SURF}" RigorSurface \
    -R "${COMPCERT_R}" compcert \
    CheckOruMemClight.v >"${TRY}/mem_clight.log" 2>&1
  RC=$?
  set -e
  echo "PHASE10_QUOTE_RC=${RC}"
  tail -50 "${TRY}/mem_clight.log" || true
)

echo "PARENT_COQC=$(command -v /usr/bin/coqc)"
if command -v ccomp >/dev/null 2>&1; then
  echo "SIDECAR_LEAK after Phase 10" >&2
  exit 1
fi
echo "CONE_COQ=$(/usr/bin/coqc --print-version | awk '{print $1}')"

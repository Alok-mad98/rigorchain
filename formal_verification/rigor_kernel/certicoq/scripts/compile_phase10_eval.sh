#!/usr/bin/env bash
# Native-compute in-memory CertiCoq compile of check_oru.
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

(
  export OPAMROOT
  export OPAMROOTISOK=1
  eval "$("${OPAMBIN}" env --switch="${SWITCH}" --set-switch)"
  cd "${TRY}"
  echo "PHASE10_EVAL=START"
  coqc -q -R "${SURF}" RigorSurface \
    -R "${COMPCERT_R}" compcert \
    -R . CheckOruMem \
    CheckOruMemClight.v
  set +e
  timeout 600 coqc -q -R "${SURF}" RigorSurface \
    -R "${COMPCERT_R}" compcert \
    -R . CheckOruMem \
    CheckOruMemEval.v >"${TRY}/mem_eval.log" 2>&1
  echo "PHASE10_EVAL_RC=$?"
  set -e
  wc -c "${TRY}/mem_eval.log"
  tail -80 "${TRY}/mem_eval.log" || true
)

echo "CONE_COQ=$(/usr/bin/coqc --print-version | awk '{print $1}')"
if command -v ccomp >/dev/null 2>&1; then
  echo "SIDECAR_LEAK" >&2
  exit 1
fi

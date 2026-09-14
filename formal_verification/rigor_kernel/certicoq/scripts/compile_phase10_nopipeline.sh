#!/usr/bin/env bash
# Phase 10: quote + staged CertiCoq pipeline without Compiler.pipeline.
# Subshell only. Parent PATH stays cone 8.15; host ccomp stays ABSENT.
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

  coqc_r() {
    coqc -q -R "${SURF}" RigorSurface \
      -R "${COMPCERT_R}" compcert \
      -R . Phase10 \
      "$@"
  }

  echo "PHASE10_QUOTE=START"
  set +e
  timeout 180 coqc_r CheckOruQuote.v >"${TRY}/quote.log" 2>&1
  echo "PHASE10_QUOTE_RC=$?"
  set -e
  tail -20 "${TRY}/quote.log" || true

  echo "PHASE10_NOPIPELINE=START"
  set +e
  timeout 180 coqc_r CheckOruNoPipeline.v >"${TRY}/nopipeline.log" 2>&1
  echo "PHASE10_NOPIPELINE_RC=$?"
  set -e
  tail -40 "${TRY}/nopipeline.log" || true
)

echo "PARENT_CCOMP=$(command -v ccomp || echo ABSENT)"
echo "CONE_COQ=$(/usr/bin/coqc --print-version | awk '{print $1}')"
if command -v ccomp >/dev/null 2>&1; then
  echo "SIDECAR_LEAK after Phase 10 nopipeline" >&2
  exit 1
fi

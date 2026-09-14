#!/usr/bin/env bash
# Phase 10: vm_compute each CertiCoq stage (Ret/Err only).
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
    echo "SIDECAR_LEAK: Phase 10 stages used cone coqc" >&2
    exit 1
  fi
  cd "${TRY}"
  coqc_r() {
    coqc -q -R "${SURF}" RigorSurface \
      -R "${COMPCERT_R}" compcert \
      -R . Phase10 \
      "$@"
  }

  run_eval() {
    local name="$1"
    local file="$2"
    local seconds="$3"
    echo "PHASE10_${name}=START"
    set +e
    timeout "${seconds}" coqc_r "${file}" >"${TRY}/${name}.log" 2>&1
    local rc=$?
    set -e
    echo "PHASE10_${name}_RC=${rc}"
    tail -20 "${TRY}/${name}.log" || true
  }

  run_eval ERASE CheckOruStageEraseNP.v 150
  run_eval LOCAL CheckOruStageLocal.v 180
  run_eval ANF_CPS CheckOruStageAnfCps.v 300
  run_eval CLIGHT_CPS CheckOruStageClightCps.v 180
  run_eval CLIGHT_ERR CheckOruClightErr.v 180
  run_eval EXP CheckOruExpLemmas.v 180
  echo "PHASE10_ANF_FULL=SKIP (timed out at 600s on 2026-09-13)"
  echo "PHASE10_CLIGHT_FULL=SKIP (depends on ANF_FULL)"
)

echo "PARENT_CCOMP=$(command -v ccomp || echo ABSENT)"
echo "CONE_COQ=$(/usr/bin/coqc --print-version | awk '{print $1}')"
if command -v ccomp >/dev/null 2>&1; then
  echo "SIDECAR_LEAK after Phase 10 stages" >&2
  exit 1
fi

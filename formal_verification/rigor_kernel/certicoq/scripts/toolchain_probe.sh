#!/usr/bin/env bash
# Phase 1 step 3. Print sidecar / cone versions without exporting
# sidecar PATH into the caller or into run_kernel_critical.sh.
#
# Required tokens on success:
#   CERTICOQ=PRESENT
#   CCOMP=PRESENT
#   CONE_COQ=8.15.0
#
# Usage:
#   formal_verification/rigor_kernel/certicoq/scripts/toolchain_probe.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=sidecar_lib.sh
. "${SCRIPT_DIR}/sidecar_lib.sh"

OPAMROOT="${CERTICOQ_ROOT}/.opam-root"
SWITCH=rigor-certicoq-8.19
SIDECAR_BIN="${OPAMROOT}/${SWITCH}/bin"
FAIL=0

print_license_banner
echo "PROBE=toolchain_probe"
echo "BACKEND=local"
echo "OPAMROOT=${OPAMROOT}"
echo "SIDECAR_SWITCH=${SWITCH}"

# Host isolation: sidecar must not already be on PATH.
if command -v ccomp >/dev/null 2>&1; then
  echo "SIDECAR_LEAK: ccomp on host PATH ($(command -v ccomp))" >&2
  echo "HOST_CCOMP=LEAKED"
  FAIL=1
else
  echo "HOST_CCOMP=ABSENT"
fi

if ! assert_cone_coq_untouched; then
  FAIL=1
fi

# Resolve sidecar tools by absolute path only. Do not eval $(opam env).
if [ -x "${SIDECAR_BIN}/ccomp" ]; then
  CCOMP_VER="$("${SIDECAR_BIN}/ccomp" -version 2>/dev/null | head -n 1 || true)"
  echo "SIDECAR_CCOMP=${SIDECAR_BIN}/ccomp"
  echo "SIDECAR_CCOMP_VERSION=${CCOMP_VER}"
  case "${CCOMP_VER}" in
    *3.13*) echo "CCOMP=PRESENT" ;;
    *)
      echo "CCOMP=WRONG_VERSION"
      FAIL=1
      ;;
  esac
else
  echo "SIDECAR_CCOMP=ABSENT"
  echo "CCOMP=ABSENT"
  FAIL=1
fi

if [ -x "${SIDECAR_BIN}/coqc" ]; then
  SIDECAR_COQ="$("${SIDECAR_BIN}/coqc" --print-version 2>/dev/null | awk '{print $1}')"
  echo "SIDECAR_COQ=${SIDECAR_COQ}"
  if [ "${SIDECAR_COQ}" = "8.19.2" ]; then
    echo "SIDECAR_COQ_PIN=OK"
  else
    echo "SIDECAR_COQ_PIN=UNEXPECTED"
    FAIL=1
  fi
else
  echo "SIDECAR_COQ=ABSENT"
  FAIL=1
fi

# CertiCoq plugin / theories — package presence, not a kernel compile.
CERTICOQ_VO=""
for cand in \
  "${OPAMROOT}/${SWITCH}/lib/coq/user-contrib/CertiCoq/Plugin.vo" \
  "${OPAMROOT}/${SWITCH}/lib/coq/user-contrib/CertiCoq/CertiCoq.vo" \
  "${OPAMROOT}/${SWITCH}/lib/coq-core/../coq/user-contrib/CertiCoq/CertiCoq.vo"
do
  if [ -f "${cand}" ]; then
    CERTICOQ_VO="${cand}"
    break
  fi
done
if [ -z "${CERTICOQ_VO}" ]; then
  CERTICOQ_VO="$(find "${OPAMROOT}/${SWITCH}" -path '*CertiCoq*CertiCoq.vo' -print -quit 2>/dev/null || true)"
fi

if [ -n "${CERTICOQ_VO}" ]; then
  echo "SIDECAR_CERTICOQ_VO=${CERTICOQ_VO}"
  echo "CERTICOQ=PRESENT"
else
  echo "SIDECAR_CERTICOQ_VO=ABSENT"
  echo "CERTICOQ=ABSENT"
  FAIL=1
fi

echo "COMPILER_CORRECTNESS=false"
echo "verified_compiler_in_current_tcb=false"

# Isolation re-check after probing: PATH must still be the cone.
if command -v ccomp >/dev/null 2>&1; then
  echo "SIDECAR_LEAK: probe exported ccomp" >&2
  FAIL=1
fi
if [ "$(cone_coq_version)" != "8.15.0" ]; then
  echo "CONE_COQ_REGRESSION after probe" >&2
  FAIL=1
fi

if [ "${FAIL}" -ne 0 ]; then
  echo "TOOLCHAIN_PROBE=FAIL"
  exit 1
fi
echo "TOOLCHAIN_PROBE=PASS"

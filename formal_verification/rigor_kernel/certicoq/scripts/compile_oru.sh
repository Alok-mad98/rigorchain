#!/usr/bin/env bash
# Phase 4: CertiCoq Compile check_oru → generated Clight.
# Sidecar only. Does not overwrite cone 8.15 .vo. Does not run ccomp
# (that is Phase 6). Does not treat generated C as source-of-truth.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=sidecar_lib.sh
. "${SCRIPT_DIR}/sidecar_lib.sh"

OPAMROOT="${CERTICOQ_ROOT}/.opam-root"
OPAMBIN="${CERTICOQ_ROOT}/bin/opam"
SWITCH=rigor-certicoq-8.19
SIDECAR_BIN="${OPAMROOT}/${SWITCH}/bin"
SRC="${CERTICOQ_ROOT}/surface"
SURF819="${CERTICOQ_ROOT}/generated/surface-8.19"
OUT="${CERTICOQ_ROOT}/generated/clight-oru"
PIN="${CERTICOQ_ROOT}/generated/SHA256"

print_license_banner
assert_cone_coq_untouched
refuse_host_path_install

if [ ! -x "${SIDECAR_BIN}/coqc" ]; then
  echo "sidecar missing; run toolchain_probe.sh first" >&2
  exit 1
fi
if command -v ccomp >/dev/null 2>&1; then
  echo "SIDECAR_LEAK: ccomp on host PATH" >&2
  exit 1
fi

if [ ! -f "${SURF819}/Kernel/ORU.vo" ]; then
  echo "8.19 surface missing; building"
  "${SCRIPT_DIR}/build_surface_819.sh"
fi

rm -rf "${OUT}"
mkdir -p "${OUT}"
cp "${SRC}/ExtractCertiCoq.v" "${OUT}/ExtractCertiCoq.v"

(
  export OPAMROOT
  export OPAMROOTISOK=1
  eval "$("${OPAMBIN}" env --switch="${SWITCH}" --set-switch)"
  if [ "$(command -v coqc)" = "/usr/bin/coqc" ]; then
    echo "SIDECAR_LEAK: ORU compile used cone coqc" >&2
    exit 1
  fi
  SIDECAR_COQ="$(coqc --print-version 2>/dev/null | awk '{print $1}')"
  if [ "${SIDECAR_COQ}" != "8.19.2" ]; then
    echo "ORU_COQ_REGRESSION: expected 8.19.2, got ${SIDECAR_COQ}" >&2
    exit 1
  fi
  echo "ORU_COQC=$(command -v coqc)"
  echo "ORU_COQ=${SIDECAR_COQ}"
  echo "CERTICOQ_COMPILE=START"
  cd "${OUT}"
  coqc -q -R "${SURF819}" RigorSurface ExtractCertiCoq.v
  echo "CERTICOQ_COMPILE=DONE"
  echo "=== generated files ==="
  ls -la
)

C_FILE=""
for cand in "${OUT}/ExtractCertiCoq.f.c" "${OUT}/check_oru.f.c" "${OUT}/check_oru.c"; do
  if [ -f "${cand}" ]; then
    C_FILE="${cand}"
    break
  fi
done
if [ -z "${C_FILE}" ]; then
  C_FILE="$(find "${OUT}" -name '*.c' -print -quit || true)"
fi
if [ -z "${C_FILE}" ] || [ ! -f "${C_FILE}" ]; then
  echo "CERTICOQ_CLIGHT=ABSENT"
  echo "ORU_CLIGHT=FAIL"
  exit 1
fi
echo "CERTICOQ_CLIGHT=${C_FILE}"
echo "CERTICOQ_CLIGHT=PRESENT"

if ! grep -q 'gc_stack.h' "${C_FILE}"; then
  echo "CLIGHT_NO_GC: generated C does not include gc_stack.h" >&2
  echo "ORU_CLIGHT=FAIL"
  exit 1
fi
echo "CLIGHT_GC=gc_stack.h"

if grep -qE 'draw_ok|res_W\(|fragment_checker' "${C_FILE}"; then
  echo "CLIGHT_LOOKS_LIKE_FRAGMENT: generated C resembles fragment_checker.c" >&2
  echo "ORU_CLIGHT=FAIL"
  exit 1
fi
echo "CLIGHT_NOT_FRAGMENT=TRUE"

# Pin is a build output. Generated C stays gitignored.
{
  echo "# certicoq/generated/SHA256"
  echo "# sha256 of CertiCoq-generated Clight. Not hand-edited C."
  echo "# Replay: certicoq/scripts/compile_oru.sh then compile_phase5.sh"
  echo "#"
  (cd "${CERTICOQ_ROOT}/generated" && sha256sum clight-*/*.c clight-*/*.h 2>/dev/null || sha256sum clight-oru/*.c)
} > "${PIN}"
echo "CLIGHT_PIN=${PIN}"
echo "CLIGHT_PIN=WRITTEN"

if [ "$(cone_coq_version)" != "8.15.0" ]; then
  echo "CONE_COQ_REGRESSION after ORU Clight" >&2
  exit 1
fi
if command -v ccomp >/dev/null 2>&1; then
  echo "SIDECAR_LEAK: ccomp on host PATH after ORU Clight" >&2
  exit 1
fi

echo "CONE_COQ=8.15.0"
echo "ORU_CLIGHT=PASS"
echo "COMPILER_CORRECTNESS=false"
echo "verified_compiler_in_current_tcb=false"
echo "NOT_A_KERNEL_CLAIM=true"
echo "PHASE6_CCOMP_NOT_RUN=true"

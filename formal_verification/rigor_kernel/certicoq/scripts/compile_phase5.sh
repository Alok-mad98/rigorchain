#!/usr/bin/env bash
# Phase 5: CertiCoq Compile remaining checkers + dispatcher.
# Order: utxo → pow → asert → res → script → validate (§4.3).
# Sidecar only. Does not overwrite cone 8.15 .vo. Does not run
# ccomp (Phase 6). Does not treat generated C as source-of-truth.

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

if [ ! -f "${SURF819}/Kernel/Validate.vo" ]; then
  echo "8.19 surface missing; building"
  "${SCRIPT_DIR}/build_surface_819.sh"
fi

if [ ! -f "${CERTICOQ_ROOT}/generated/clight-oru/RigorSurface.Kernel.ORU.check_oru.c" ]; then
  echo "Phase 4 ORU Clight missing; compiling"
  "${SCRIPT_DIR}/compile_oru.sh"
fi

compile_one() {
  local name="$1"
  local extract="$2"
  local const="$3"
  local out="${CERTICOQ_ROOT}/generated/clight-${name}"

  rm -rf "${out}"
  mkdir -p "${out}"
  cp "${SRC}/${extract}" "${out}/${extract}"

  (
    export OPAMROOT
    export OPAMROOTISOK=1
    eval "$("${OPAMBIN}" env --switch="${SWITCH}" --set-switch)"
    if [ "$(command -v coqc)" = "/usr/bin/coqc" ]; then
      echo "SIDECAR_LEAK: ${name} compile used cone coqc" >&2
      exit 1
    fi
    SIDECAR_COQ="$(coqc --print-version 2>/dev/null | awk '{print $1}')"
    if [ "${SIDECAR_COQ}" != "8.19.2" ]; then
      echo "${name}_COQ_REGRESSION: expected 8.19.2, got ${SIDECAR_COQ}" >&2
      exit 1
    fi
    echo "${name}_COQC=$(command -v coqc)"
    echo "${name}_COQ=${SIDECAR_COQ}"
    echo "CERTICOQ_COMPILE_${name}=START"
    cd "${out}"
    coqc -q -R "${SURF819}" RigorSurface "${extract}"
    echo "CERTICOQ_COMPILE_${name}=DONE"
    echo "=== generated ${name} ==="
    ls -la
  )

  local c_file=""
  c_file="$(find "${out}" -name '*.c' -print -quit || true)"
  if [ -z "${c_file}" ] || [ ! -f "${c_file}" ]; then
    echo "CERTICOQ_CLIGHT_${name}=ABSENT"
    echo "${name}_CLIGHT=FAIL"
    exit 1
  fi
  echo "CERTICOQ_CLIGHT_${name}=${c_file}"
  echo "CERTICOQ_CLIGHT_${name}=PRESENT"

  if ! grep -q 'gc_stack.h' "${c_file}"; then
    echo "CLIGHT_NO_GC: ${name} C does not include gc_stack.h" >&2
    echo "${name}_CLIGHT=FAIL"
    exit 1
  fi
  echo "CLIGHT_GC_${name}=gc_stack.h"

  if grep -qE 'draw_ok|res_W\(|fragment_checker' "${c_file}"; then
    echo "CLIGHT_LOOKS_LIKE_FRAGMENT: ${name}" >&2
    echo "${name}_CLIGHT=FAIL"
    exit 1
  fi
  echo "CLIGHT_NOT_FRAGMENT_${name}=TRUE"

  if [ "${name}" = "asert" ]; then
    if ! grep -qE 'BinInt|Zquot|quot' "${c_file}"; then
      echo "ASERT_NOT_BOXED_Z: expected BinInt/quot in Clight" >&2
      echo "asert_CLIGHT=FAIL"
      exit 1
    fi
    if grep -qE 'check_asert\(long long' "${c_file}"; then
      echo "ASERT_LOWERED_TO_LONGLONG: forbidden" >&2
      echo "asert_CLIGHT=FAIL"
      exit 1
    fi
    echo "ASERT_BOXED_Z=TRUE"
  fi

  if [ "${name}" = "res" ]; then
    if grep -qE 'draw_ok' "${c_file}"; then
      echo "RES_HAS_DRAW_OK: forbidden RFC-019 rehearsal" >&2
      echo "res_CLIGHT=FAIL"
      exit 1
    fi
    echo "RES_NO_DRAW_OK=TRUE"
  fi

  echo "${name}_CLIGHT=PASS"
}

# §4.3 remaining order. ORU already Closed in Phase 4.
compile_one utxo ExtractUtxo.v check_utxo
compile_one pow ExtractPow.v check_pow
compile_one asert ExtractAsert.v check_asert
compile_one res ExtractRes.v check_res
compile_one script ExtractScript.v check_script
compile_one validate ExtractValidate.v validate_kernel

{
  echo "# certicoq/generated/SHA256"
  echo "# sha256 of CertiCoq-generated Clight. Not hand-edited C."
  echo "# Replay: certicoq/scripts/compile_oru.sh then compile_phase5.sh"
  echo "#"
  (cd "${CERTICOQ_ROOT}/generated" && sha256sum clight-*/*.c clight-*/*.h)
} > "${PIN}"
echo "CLIGHT_PIN=${PIN}"
echo "CLIGHT_PIN=WRITTEN"

need=(
  clight-oru
  clight-utxo
  clight-pow
  clight-asert
  clight-res
  clight-script
  clight-validate
)
for d in "${need[@]}"; do
  if ! find "${CERTICOQ_ROOT}/generated/${d}" -name '*.c' | grep -q .; then
    echo "PHASE5_MISSING=${d}" >&2
    echo "PHASE5_CLIGHT=FAIL"
    exit 1
  fi
  echo "PHASE5_UNIT=${d}=PRESENT"
done

if [ "$(cone_coq_version)" != "8.15.0" ]; then
  echo "CONE_COQ_REGRESSION after Phase 5 Clight" >&2
  exit 1
fi
if command -v ccomp >/dev/null 2>&1; then
  echo "SIDECAR_LEAK: ccomp on host PATH after Phase 5" >&2
  exit 1
fi

echo "CONE_COQ=8.15.0"
echo "PHASE5_CLIGHT=PASS"
echo "COMPILER_CORRECTNESS=false"
echo "verified_compiler_in_current_tcb=false"
echo "NOT_A_KERNEL_CLAIM=true"
echo "PHASE6_CCOMP_NOT_RUN=true"

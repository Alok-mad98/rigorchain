#!/usr/bin/env bash
# Phase 6: ccomp each CertiCoq Clight unit; clang-build GC; link
# trusted vector driver. Sidecar only. Does not put ccomp on the
# host PATH. Does not compile checker C with gcc/clang.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=sidecar_lib.sh
. "${SCRIPT_DIR}/sidecar_lib.sh"

OPAMROOT="${CERTICOQ_ROOT}/.opam-root"
OPAMBIN="${CERTICOQ_ROOT}/bin/opam"
SWITCH=rigor-certicoq-8.19
SIDECAR_BIN="${OPAMROOT}/${SWITCH}/bin"
RUNTIME="${OPAMROOT}/${SWITCH}/lib/coq/user-contrib/CertiCoq/Plugin/runtime"
SRC="${CERTICOQ_ROOT}/surface"
SURF819="${CERTICOQ_ROOT}/generated/surface-8.19"
OUT="${CERTICOQ_ROOT}/generated/ccomp"
DRIVER_SRC="${CERTICOQ_ROOT}/driver/vector_driver.c"
REPORT="${KERNEL_ROOT}/evidence/CertiCoqCompCertReport.md"
CLANG="${CLANG:-/usr/bin/clang}"

print_license_banner
assert_cone_coq_untouched
refuse_host_path_install

if command -v ccomp >/dev/null 2>&1; then
  echo "SIDECAR_LEAK: ccomp on host PATH" >&2
  exit 1
fi
if [ ! -x "${SIDECAR_BIN}/ccomp" ]; then
  echo "CCOMP=ABSENT"
  echo "PHASE6=SKIP"
  echo "not PASS: sidecar ccomp missing (do not treat as rehearsal PASS)" >&2
  exit 2
fi
if [ ! -x "${CLANG}" ]; then
  echo "CLANG=ABSENT (needed for GC TCB object only)" >&2
  exit 1
fi

if [ ! -f "${CERTICOQ_ROOT}/generated/clight-oru/RigorSurface.Kernel.ORU.check_oru.c" ]; then
  echo "Phase 5 Clight missing; compiling"
  "${SCRIPT_DIR}/compile_phase5.sh"
fi
if [ ! -f "${SURF819}/Observe.vo" ]; then
  echo "8.19 Observe.vo missing; building"
  "${SCRIPT_DIR}/build_surface_819.sh"
fi

mkdir -p "${OUT}"
mkdir -p "$(dirname "${REPORT}")"

UNITS=(oru utxo pow asert res script validate)

ccomp_unit() {
  local name="$1"
  local c_file
  c_file="$(find "${CERTICOQ_ROOT}/generated/clight-${name}" -name '*.c' -print -quit)"
  if [ -z "${c_file}" ] || [ ! -f "${c_file}" ]; then
    echo "CCOMP_UNIT_${name}=ABSENT"
    echo "PHASE6=FAIL"
    exit 1
  fi
  echo "CCOMP_UNIT_${name}_SRC=${c_file}"
  echo "CCOMP_${name}_S=START"
  ccomp -I "${RUNTIME}" -I "$(dirname "${c_file}")" \
    -S -o "${OUT}/${name}.s" "${c_file}"
  echo "CCOMP_${name}_S=DONE"
  echo "CCOMP_${name}_C=START"
  ccomp -I "${RUNTIME}" -I "$(dirname "${c_file}")" \
    -c -o "${OUT}/${name}.o" "${c_file}"
  echo "CCOMP_${name}_C=DONE"
  echo "CCOMP_UNIT_${name}=PASS"
}

(
  export OPAMROOT
  export OPAMROOTISOK=1
  eval "$("${OPAMBIN}" env --switch="${SWITCH}" --set-switch)"
  if [ "$(command -v ccomp)" = "/usr/bin/ccomp" ] || [ -z "$(command -v ccomp)" ]; then
    echo "SIDECAR_LEAK_OR_MISSING: ccomp" >&2
    exit 1
  fi
  echo "CCOMP=$(command -v ccomp)"
  echo "CCOMP=PRESENT"
  echo "CCOMP_VERSION=$("$(command -v ccomp)" -version | head -n 1)"
  if [ "$(command -v coqc)" = "/usr/bin/coqc" ]; then
    echo "SIDECAR_LEAK: Phase 6 used cone coqc" >&2
    exit 1
  fi

  for u in "${UNITS[@]}"; do
    ccomp_unit "${u}"
  done

  P6="${CERTICOQ_ROOT}/generated/clight-phase6"
  rm -rf "${P6}"
  mkdir -p "${P6}"
  cp "${SRC}/ExtractPhase6.v" "${P6}/ExtractPhase6.v"
  echo "CERTICOQ_COMPILE_phase6=START"
  (
    cd "${P6}"
    coqc -q -R "${SURF819}" RigorSurface ExtractPhase6.v
  )
  echo "CERTICOQ_COMPILE_phase6=DONE"
  P6C="$(find "${P6}" -name '*.c' -print -quit)"
  if [ -z "${P6C}" ] || [ ! -f "${P6C}" ]; then
    echo "PHASE6_CLIGHT=ABSENT"
    exit 1
  fi
  echo "PHASE6_CLIGHT=${P6C}"
  ccomp -I "${RUNTIME}" -I "$(dirname "${P6C}")" \
    -S -o "${OUT}/phase6.s" "${P6C}"
  ccomp -I "${RUNTIME}" -I "$(dirname "${P6C}")" \
    -c -o "${OUT}/phase6.o" "${P6C}"
  echo "CCOMP_UNIT_phase6=PASS"
)

# GC / prims / driver: clang. Not the checker. TCB.
echo "CLANG=${CLANG}"
echo "GC_BUILD=clang_TCB"
"${CLANG}" -c -I "${RUNTIME}" -o "${OUT}/gc_stack.o" "${RUNTIME}/gc_stack.c"
"${CLANG}" -c -I "${RUNTIME}" -o "${OUT}/prim_int63.o" "${RUNTIME}/prim_int63.c"
"${CLANG}" -c -I "${RUNTIME}" -o "${OUT}/prim_floats.o" "${RUNTIME}/prim_floats.c"
"${CLANG}" -c -I "${RUNTIME}" -o "${OUT}/coq_c_ffi.o" "${RUNTIME}/coq_c_ffi.c"
"${CLANG}" -c -I "${RUNTIME}" -o "${OUT}/vector_driver.o" "${DRIVER_SRC}"
echo "GC=LINKED"
echo "GC_OBJECT=${OUT}/gc_stack.o"

LINK_OBJS=(
  "${OUT}/phase6.o"
  "${OUT}/gc_stack.o"
  "${OUT}/prim_int63.o"
  "${OUT}/prim_floats.o"
  "${OUT}/coq_c_ffi.o"
  "${OUT}/vector_driver.o"
)
echo "LINK_OBJECTS=${LINK_OBJS[*]}"
# -lm is for prim_floats (TCB). Not a gcc compile of checker C.
"${CLANG}" -o "${OUT}/vector_driver" "${LINK_OBJS[@]}" -lm
echo "PHASE6_BINARY=${OUT}/vector_driver"

echo "PHASE6_RUN=START"
set +e
"${OUT}/vector_driver"
RUN_RC=$?
set -e
echo "PHASE6_RUN_RC=${RUN_RC}"
if [ "${RUN_RC}" -ne 0 ]; then
  echo "PHASE6_VECTORS=FAIL"
  echo "PHASE6=FAIL"
  exit 1
fi

if [ "$(cone_coq_version)" != "8.15.0" ]; then
  echo "CONE_COQ_REGRESSION after Phase 6" >&2
  exit 1
fi
if command -v ccomp >/dev/null 2>&1; then
  echo "SIDECAR_LEAK: ccomp on host PATH after Phase 6" >&2
  exit 1
fi

echo "CONE_COQ=8.15.0"
echo "HOST_CCOMP=ABSENT"
echo "CCOMP=PRESENT"
echo "GC=LINKED"
echo "PHASE6=PASS"
echo "COMPILER_CORRECTNESS=false"
echo "verified_compiler_in_current_tcb=false"
echo "NOT_A_KERNEL_CLAIM=true"
echo "CERTICOQ_GCC_SMOKE=not_run"

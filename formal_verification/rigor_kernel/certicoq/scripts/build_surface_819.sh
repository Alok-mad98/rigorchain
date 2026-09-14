#!/usr/bin/env bash
# Phase 3: compile the six-checker surface with sidecar Coq 8.19.2.
# Writes only under certicoq/generated/surface-8.19 (gitignored).
# Does not overwrite cone 8.15 surface .vo. Does not load CertiCoq.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=sidecar_lib.sh
. "${SCRIPT_DIR}/sidecar_lib.sh"

OPAMROOT="${CERTICOQ_ROOT}/.opam-root"
OPAMBIN="${CERTICOQ_ROOT}/bin/opam"
SWITCH=rigor-certicoq-8.19
SIDECAR_BIN="${OPAMROOT}/${SWITCH}/bin"
SRC="${CERTICOQ_ROOT}/surface"
OUT="${CERTICOQ_ROOT}/generated/surface-8.19"

print_license_banner
assert_cone_coq_untouched
refuse_host_path_install

if [ ! -x "${SIDECAR_BIN}/coqc" ]; then
  echo "sidecar missing; run toolchain_probe.sh / build_sidecar.sh first" >&2
  exit 1
fi
if command -v ccomp >/dev/null 2>&1; then
  echo "SIDECAR_LEAK: ccomp on host PATH" >&2
  exit 1
fi

rm -rf "${OUT}"
mkdir -p "${OUT}"
# Sources only. Never copy 8.15 .vo into the 8.19 tree.
cp "${SRC}/_CoqProject" "${OUT}/_CoqProject"
for d in Core Supply Script Pow Kernel; do
  mkdir -p "${OUT}/${d}"
  cp "${SRC}/${d}"/*.v "${OUT}/${d}/"
done
cp "${SRC}/Observe.v" "${OUT}/Observe.v"

# Sidecar env stays in this subshell. Parent PATH is unchanged.
(
  export OPAMROOT
  export OPAMROOTISOK=1
  eval "$("${OPAMBIN}" env --switch="${SWITCH}" --set-switch)"
  if [ "$(command -v coqc)" = "/usr/bin/coqc" ]; then
    echo "SIDECAR_LEAK: 8.19 build used cone coqc" >&2
    exit 1
  fi
  SIDECAR_COQ="$(coqc --print-version 2>/dev/null | awk '{print $1}')"
  if [ "${SIDECAR_COQ}" != "8.19.2" ]; then
    echo "SURFACE_819_COQ_REGRESSION: expected 8.19.2, got ${SIDECAR_COQ}" >&2
    exit 1
  fi
  echo "SURFACE_819_COQC=$(command -v coqc)"
  echo "SURFACE_819_COQ=${SIDECAR_COQ}"
  cd "${OUT}"
  rm -f Makefile .coqdeps.d
  coq_makefile -f _CoqProject -o Makefile
  make -j"$(nproc 2>/dev/null || echo 2)"
  echo "SURFACE_819_COMPILE_OBSERVE=START"
  coqc -q -R . RigorSurface Observe.v
  echo "SURFACE_819_COMPILE_OBSERVE=DONE"
)

if [ "$(cone_coq_version)" != "8.15.0" ]; then
  echo "CONE_COQ_REGRESSION after 8.19 surface" >&2
  exit 1
fi
if command -v ccomp >/dev/null 2>&1; then
  echo "SIDECAR_LEAK: ccomp on host PATH after 8.19 surface" >&2
  exit 1
fi

echo "CONE_COQ=8.15.0"
echo "SURFACE_819_BUILD=PASS"
echo "COMPILER_CORRECTNESS=false"
echo "verified_compiler_in_current_tcb=false"
echo "NOT_A_KERNEL_CLAIM=true"

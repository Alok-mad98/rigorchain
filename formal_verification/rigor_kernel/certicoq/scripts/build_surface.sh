#!/usr/bin/env bash
# Phase 2: compile the six-checker surface with cone Coq 8.15.0.
# Does not load CertiCoq. Does not put the sidecar on PATH.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=sidecar_lib.sh
. "${SCRIPT_DIR}/sidecar_lib.sh"

export PATH="/usr/bin:/bin"
CONE_COQC="${CONE_COQC:-/usr/bin/coqc}"
SURF="${CERTICOQ_ROOT}/surface"

print_license_banner
assert_cone_coq_untouched
refuse_host_path_install

if [ ! -x "${CONE_COQC}" ]; then
  echo "missing ${CONE_COQC}" >&2
  exit 1
fi
if command -v ccomp >/dev/null 2>&1; then
  echo "SIDECAR_LEAK: ccomp on host PATH" >&2
  exit 1
fi

COQC_VER="$("${CONE_COQC}" --print-version 2>/dev/null | awk '{print $1}')"
if [ "${COQC_VER}" != "8.15.0" ]; then
  echo "SURFACE_COQ_REGRESSION: expected 8.15.0, got ${COQC_VER}" >&2
  exit 1
fi

echo "SURFACE_COQC=${CONE_COQC}"
echo "SURFACE_COQ=${COQC_VER}"

cd "${SURF}"
rm -f Makefile .coqdeps.d
/usr/bin/coq_makefile -f _CoqProject -o Makefile
make -j"$(nproc 2>/dev/null || echo 2)"

echo "SURFACE_BUILD=PASS"
echo "COMPILER_CORRECTNESS=false"
echo "verified_compiler_in_current_tcb=false"

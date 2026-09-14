#!/usr/bin/env bash
# Phase 3: observational equality — same Observe.v on 8.15 and 8.19.
# Not a cross-version metatheorem. Not CertiCoq.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=sidecar_lib.sh
. "${SCRIPT_DIR}/sidecar_lib.sh"

SURF="${CERTICOQ_ROOT}/surface"
OUT815="${CERTICOQ_ROOT}/generated/observe-8.15"
CONE_COQC="${CONE_COQC:-/usr/bin/coqc}"

print_license_banner
assert_cone_coq_untouched
refuse_host_path_install

if command -v ccomp >/dev/null 2>&1; then
  echo "SIDECAR_LEAK: ccomp on host PATH" >&2
  exit 1
fi

echo "== 8.15 surface + Observe.v =="
# Keep 8.15 .vo in surface/ (cone correspondence). Observe.vo is gitignored.
(
  export PATH="/usr/bin:/bin"
  cd "${SURF}"
  if [ ! -f Kernel/Validate.vo ]; then
    echo "8.15 surface .vo missing; run build_surface.sh first" >&2
    exit 1
  fi
  "${CONE_COQC}" -q -R . RigorSurface Observe.v
)
echo "OBSERVE_8_15=PASS"

echo "== 8.19 surface + Observe.v =="
"${SCRIPT_DIR}/build_surface_819.sh"
echo "OBSERVE_8_19=PASS"

# Both hosts closed the same Example list. Record that as agreement.
echo "OBSERVE_AGREE=PASS"
echo "OBSERVE_KIND=host_vm_compute"
echo "NOT_CROSS_VERSION_METATHEOREM=true"
echo "COMPILER_CORRECTNESS=false"
echo "verified_compiler_in_current_tcb=false"
echo "CONE_COQ=8.15.0"

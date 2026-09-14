#!/usr/bin/env bash
# Run a command *inside* the sidecar. Host PATH is unchanged.
#
#   ./certicoq/scripts/enter_sidecar.sh
#   ./certicoq/scripts/enter_sidecar.sh ccomp -version
#   ./certicoq/scripts/enter_sidecar.sh bash -lc 'coqc --version'

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=sidecar_lib.sh
. "${SCRIPT_DIR}/sidecar_lib.sh"

print_license_banner
assert_cone_coq_untouched

if ! command -v docker >/dev/null 2>&1; then
  echo "enter_sidecar.sh requires Docker. For local OPAMROOT, run commands" >&2
  echo "under OPAMROOT=${CERTICOQ_ROOT}/.opam-root inside a subshell only." >&2
  exit 1
fi

if ! docker image inspect "${SIDECAR_IMAGE}" >/dev/null 2>&1; then
  echo "image ${SIDECAR_IMAGE} missing; run build_sidecar.sh first" >&2
  exit 1
fi

if [ "$#" -eq 0 ]; then
  set -- bash
fi

# Mount the kernel tree read-write for later phases; compilers stay in-image.
exec docker run --rm -i \
  -e COMPCERT_LICENSE=INRIA_NON_COMMERCIAL \
  -e COMPCERT_PATH=L-RESEARCH-SIDECAR-2026-09-12 \
  -e RIGOR_SIDECAR=1 \
  -v "${KERNEL_ROOT}:/work:rw" \
  -w /work \
  "${SIDECAR_IMAGE}" \
  "$@"

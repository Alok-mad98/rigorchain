#!/usr/bin/env bash
# Build the Phase 1 step 2 research sidecar.
# Does not replace /usr/bin/coqc and does not export sidecar PATH
# into the caller. CompCert is not vendored into git.
#
# Usage (from anywhere):
#   formal_verification/rigor_kernel/certicoq/scripts/build_sidecar.sh
#   RIGOR_SIDECAR_BACKEND=docker|local  (default: docker if present)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=sidecar_lib.sh
. "${SCRIPT_DIR}/sidecar_lib.sh"

BACKEND="${RIGOR_SIDECAR_BACKEND:-}"
if [ -z "${BACKEND}" ]; then
  if command -v docker >/dev/null 2>&1 && docker info >/dev/null 2>&1; then
    BACKEND=docker
  else
    BACKEND=local
  fi
fi

print_license_banner
cat "${CERTICOQ_ROOT}/ci/NOTICE.compcert"
echo "BACKEND=${BACKEND}"
echo "SIDECAR_IMAGE=${SIDECAR_IMAGE}"
refuse_host_path_install
assert_cone_coq_untouched

case "${BACKEND}" in
  docker)
    if ! command -v docker >/dev/null 2>&1; then
      echo "docker: ABSENT" >&2
      exit 1
    fi
    docker build \
      --progress=plain \
      -t "${SIDECAR_IMAGE}" \
      -f "${CERTICOQ_ROOT}/ci/Dockerfile" \
      "${CERTICOQ_ROOT}"
    echo "SIDECAR_IMAGE_BUILT=${SIDECAR_IMAGE}"
    ;;
  local)
    echo "LOCAL sidecar uses OPAMROOT=${CERTICOQ_ROOT}/.opam-root" 
    echo "and a private opam binary under ${CERTICOQ_ROOT}/bin/."
    echo "It still must not be eval'd into the cone shell."
    "${SCRIPT_DIR}/build_sidecar_local.sh"
    ;;
  *)
    echo "unknown RIGOR_SIDECAR_BACKEND=${BACKEND}" >&2
    exit 1
    ;;
esac

assert_cone_coq_untouched
# Host PATH must still not see sidecar ccomp.
if command -v ccomp >/dev/null 2>&1; then
  echo "SIDECAR_LEAK: ccomp appeared on host PATH ($(command -v ccomp))" >&2
  exit 1
fi
echo "HOST_CCOMP=ABSENT"
echo "SIDECAR_BUILD=PASS"
echo "COMPILER_CORRECTNESS=false"

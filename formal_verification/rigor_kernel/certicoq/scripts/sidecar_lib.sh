# Shared by certicoq/scripts only. Do not source from run_kernel_critical.sh.
# L-RESEARCH-SIDECAR-2026-09-12

CERTICOQ_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KERNEL_ROOT="$(cd "${CERTICOQ_ROOT}/.." && pwd)"
SIDECAR_IMAGE="${RIGOR_SIDECAR_IMAGE:-rigor-certicoq:8.19-compcert-3.13.1}"
CONE_COQC="${CONE_COQC:-/usr/bin/coqc}"

print_license_banner() {
  echo "COMPCERT_LICENSE=INRIA_NON_COMMERCIAL"
  echo "COMPCERT_PATH=L-RESEARCH-SIDECAR-2026-09-12"
  echo "COMPILER_CORRECTNESS=false"
  echo "verified_compiler_in_current_tcb=false"
}

cone_coq_version() {
  if [ -x "${CONE_COQC}" ]; then
    "${CONE_COQC}" --print-version 2>/dev/null | awk '{print $1}'
  else
    echo "ABSENT"
  fi
}

assert_cone_coq_untouched() {
  local v
  v="$(cone_coq_version)"
  if [ "${v}" != "8.15.0" ]; then
    echo "CONE_COQ_REGRESSION: expected 8.15.0 from ${CONE_COQC}, got ${v}" >&2
    return 1
  fi
  if command -v coqc >/dev/null 2>&1; then
    local onpath
    onpath="$(command -v coqc)"
    if [ "${onpath}" != "${CONE_COQC}" ] && [ -n "${RIGOR_SIDECAR:-}" ]; then
      echo "SIDECAR_LEAK: coqc on PATH is ${onpath} (sidecar must not leak)" >&2
      return 1
    fi
  fi
  echo "CONE_COQ=${v}"
}

refuse_host_path_install() {
  echo "This script will not install ccomp, coqc 8.19, or opam onto the host PATH."
  echo "Sidecar lives in Docker image ${SIDECAR_IMAGE} or in ${CERTICOQ_ROOT}/.opam-root."
}

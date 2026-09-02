#!/usr/bin/env bash
# Stage 1: fail-closed environment probe for the Genesis Release Guardian.
# Missing required tools => ENVIRONMENT_GATE_FAILED. Does not change protocol.
set -euo pipefail

fail_env() {
  echo "ENVIRONMENT_GATE_FAILED"
  echo "FAIL: $*"
  exit 1
}

echo "== Stage 1 ENVIRONMENT_PROBE =="
echo "OS=$(uname -s)"
echo "ARCH=$(uname -m)"
echo "KERNEL=$(uname -r)"
if [[ -f /etc/os-release ]]; then
  # shellcheck disable=SC1091
  . /etc/os-release
  echo "DISTRO=${PRETTY_NAME:-${ID:-unknown}}"
else
  echo "DISTRO=unknown"
fi

cxx="$(command -v g++ || command -v c++ || true)"
if [[ -z "${cxx}" ]]; then
  fail_env "C++ compiler (g++/clang++) is not on PATH"
fi
echo "COMPILER=${cxx}"
echo "COMPILER_VERSION=$(${cxx} --version 2>/dev/null | head -n1)"
if command -v gcc >/dev/null 2>&1; then
  echo "GCC_VERSION=$(gcc --version 2>/dev/null | head -n1)"
else
  echo "GCC_VERSION=missing"
fi
if command -v clang >/dev/null 2>&1; then
  echo "CLANG_VERSION=$(clang --version 2>/dev/null | head -n1)"
else
  echo "CLANG_VERSION=missing"
fi

command -v python3 >/dev/null 2>&1 || fail_env "python3 is not on PATH"
echo "PYTHON_VERSION=$(python3 --version 2>/dev/null)"

command -v coqc >/dev/null 2>&1 || fail_env "coqc is not on PATH (need Coq 8.15.0; see tools/ci/setup-coq-8.15.0.sh)"
coq_ver="$(coqc --version 2>/dev/null | head -n1)"
echo "COQ_VERSION=${coq_ver}"
case "${coq_ver}" in
  *8.15.*) ;;
  *) fail_env "Coq 8.15.x required, got: ${coq_ver}" ;;
esac

command -v ocamlopt >/dev/null 2>&1 || fail_env "ocamlopt is not on PATH (required for extracted oracle build)"
echo "OCAML_VERSION=$(ocamlopt -vnum 2>/dev/null)"

echo "ENVIRONMENT_PROBE=PASS"
echo "ENVIRONMENT_GATE=PASS"

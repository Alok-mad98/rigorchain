#!/usr/bin/env bash
# Install pinned Coq 8.15.0 + OCaml 4.13.1 via opam on the GitHub-hosted runner.
#
# Do not apt-install Ubuntu `coq` (version is not 8.15.0).
# Do not use job-level `container:` or coqorg/coq:8.15.0 (that Docker tag
# is not published on Docker Hub; 8.15 tags resolve to 8.15.2).
#
# CI infrastructure only. Does not modify Consensus Kernel, Transaction
# Kernel, Ledger, Coq theorems, refinement proofs, or authority manifests.
set -euo pipefail

PIN_OCAML="4.13.1"
PIN_COQ="8.15.0"
PIN_OPAM="2.2.1"
SWITCH_NAME="rigor-coq-${PIN_COQ}-ocaml-${PIN_OCAML}"

ci_fail() {
  echo "CI_TOOLCHAIN_SETUP_FAILURE"
  echo "FAIL: $*"
  exit 1
}

docker_fail() {
  echo "CI_DOCKER_ENVIRONMENT_FAILURE"
  echo "FAIL: $*"
  exit 1
}

if [[ "${GITHUB_ACTIONS:-}" == "true" && -f /.dockerenv ]]; then
  docker_fail "job-level container: is forbidden; use this script on ubuntu-latest"
fi

if [[ -n "${CI_DOCKER_IMAGE:-}" ]]; then
  ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
  bash "${ROOT}/tools/ci/docker-pull-or-fail.sh" "${CI_DOCKER_IMAGE}"
  exit 0
fi

export_opam_env() {
  eval "$(opam env --switch="${SWITCH_NAME}" --set-switch)"
  if [[ -n "${GITHUB_PATH:-}" ]]; then
    echo "$(opam var bin)" >> "${GITHUB_PATH}"
  fi
  if [[ -n "${GITHUB_ENV:-}" ]]; then
    echo "OPAMSWITCH=${SWITCH_NAME}" >> "${GITHUB_ENV}"
    echo "OPAM_SWITCH_PREFIX=$(opam var prefix)" >> "${GITHUB_ENV}"
  fi
}

coq_pin_ok() {
  command -v coqc >/dev/null 2>&1 || return 1
  local ver
  ver="$(coqc --version | head -n1)"
  [[ "${ver}" == *"${PIN_COQ}"* ]]
}

if command -v opam >/dev/null 2>&1 && opam switch list --short 2>/dev/null | grep -qx "${SWITCH_NAME}"; then
  export_opam_env
  if coq_pin_ok; then
    echo "Using cached opam switch ${SWITCH_NAME}"
    coqc --version | head -n1
    ocamlopt -vnum
    exit 0
  fi
fi

if coq_pin_ok && [[ "${CI_FORCE_OPAM_SETUP:-}" != "1" ]]; then
  echo "Using existing Coq ${PIN_COQ} on PATH"
  if [[ -n "${GITHUB_PATH:-}" ]]; then
    echo "$(dirname "$(command -v coqc)")" >> "${GITHUB_PATH}"
  fi
  coqc --version | head -n1
  exit 0
fi

if [[ "${EUID}" -eq 0 ]]; then
  APT="apt-get"
else
  command -v sudo >/dev/null 2>&1 || ci_fail "sudo is required to install build dependencies"
  APT="sudo apt-get"
fi

export DEBIAN_FRONTEND=noninteractive
${APT} update -y
${APT} install -y --no-install-recommends \
  build-essential bubblewrap m4 unzip rsync curl ca-certificates \
  libgmp-dev pkg-config python3 g++ make git findutils

install_opam_binary() {
  local arch opam_arch
  arch="$(uname -m)"
  case "${arch}" in
    x86_64|amd64) opam_arch="x86_64" ;;
    aarch64|arm64) opam_arch="arm64" ;;
    *) ci_fail "unsupported runner architecture ${arch}" ;;
  esac
  local url="https://github.com/ocaml/opam/releases/download/${PIN_OPAM}/opam-${PIN_OPAM}-${opam_arch}-linux"
  echo "Installing opam ${PIN_OPAM} from ${url}"
  curl -fsSL "${url}" -o /tmp/opam-bin
  if [[ "${EUID}" -eq 0 ]]; then
    install -m 0755 /tmp/opam-bin /usr/local/bin/opam
  else
    sudo install -m 0755 /tmp/opam-bin /usr/local/bin/opam
  fi
}

if ! command -v opam >/dev/null 2>&1; then
  install_opam_binary
fi

if [[ ! -d "${HOME}/.opam" ]]; then
  opam init --bare --disable-sandboxing --yes --no-setup \
    || ci_fail "opam init failed"
fi

if ! opam switch list --short | grep -qx "${SWITCH_NAME}"; then
  opam switch create "${SWITCH_NAME}" "${PIN_OCAML}" --yes \
    || ci_fail "opam switch create ${SWITCH_NAME} (${PIN_OCAML}) failed"
fi

export_opam_env

if ! opam list --installed --short | grep -qx coq; then
  opam install -y "coq.${PIN_COQ}" \
    || ci_fail "opam install coq.${PIN_COQ} failed"
fi

coq_pin_ok || ci_fail "expected Coq ${PIN_COQ}, got: $(coqc --version | head -n1)"
command -v ocamlopt >/dev/null 2>&1 || ci_fail "ocamlopt is not on PATH after opam setup"

echo "Coq toolchain ready"
coqc --version | head -n1
echo "ocamlopt $(ocamlopt -vnum)"

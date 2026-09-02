#!/usr/bin/env bash
# Print pinned toolchain facts for every GitHub Actions job.
# Missing tools print "missing" rather than failing this step.
set -u

echo "=== CI_REPRODUCIBILITY ==="
echo "RUNNER_OS=$(uname -s)"
echo "RUNNER_ARCH=$(uname -m)"
echo "RUNNER_KERNEL=$(uname -r)"
if [[ -f /etc/os-release ]]; then
  # shellcheck disable=SC1091
  . /etc/os-release
  echo "RUNNER_DISTRO=${PRETTY_NAME:-${ID:-unknown}}"
else
  echo "RUNNER_DISTRO=unknown"
fi
if [[ -n "${ImageOS:-}" || -n "${ImageVersion:-}" ]]; then
  echo "GITHUB_IMAGE_OS=${ImageOS:-unknown}"
  echo "GITHUB_IMAGE_VERSION=${ImageVersion:-unknown}"
fi

cxx="$(command -v g++ || command -v c++ || true)"
if [[ -n "${cxx}" ]]; then
  echo "COMPILER_VERSION=$(${cxx} --version 2>/dev/null | head -n1)"
else
  echo "COMPILER_VERSION=missing"
fi

if command -v coqc >/dev/null 2>&1; then
  echo "COQ_VERSION=$(coqc --version 2>/dev/null | head -n1)"
else
  echo "COQ_VERSION=missing"
fi

if command -v ocamlopt >/dev/null 2>&1; then
  echo "OCAML_VERSION=$(ocamlopt -vnum 2>/dev/null)"
elif command -v ocamlc >/dev/null 2>&1; then
  echo "OCAML_VERSION=$(ocamlc -vnum 2>/dev/null)"
else
  echo "OCAML_VERSION=missing"
fi

if command -v python3 >/dev/null 2>&1; then
  echo "PYTHON_VERSION=$(python3 --version 2>/dev/null)"
else
  echo "PYTHON_VERSION=missing"
fi

if [[ -n "${CI_DOCKER_IMAGE:-}" ]] && command -v docker >/dev/null 2>&1; then
  digest="$(docker image inspect --format '{{index .RepoDigests 0}}' "${CI_DOCKER_IMAGE}" 2>/dev/null || echo unavailable)"
  echo "DOCKER_IMAGE=${CI_DOCKER_IMAGE}"
  echo "DOCKER_IMAGE_DIGEST=${digest}"
else
  echo "DOCKER_IMAGE=not_used"
  echo "DOCKER_IMAGE_DIGEST=not_used"
fi
echo "=== END CI_REPRODUCIBILITY ==="

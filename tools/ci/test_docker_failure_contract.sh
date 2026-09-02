#!/usr/bin/env bash
# Contract: Docker environment failures must print CI_DOCKER_ENVIRONMENT_FAILURE
# and must not be silent ContainerId errors.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
HELPER="${ROOT}/tools/ci/docker-pull-or-fail.sh"

expect_fail_token() {
  local label="$1"
  shift
  local out rc
  set +e
  out="$("$@" 2>&1)"
  rc=$?
  set -e
  if [[ "${rc}" -eq 0 ]]; then
    echo "FAIL: ${label} exited 0"
    echo "${out}"
    exit 1
  fi
  if ! grep -q "CI_DOCKER_ENVIRONMENT_FAILURE" <<<"${out}"; then
    echo "FAIL: ${label} did not print CI_DOCKER_ENVIRONMENT_FAILURE"
    echo "${out}"
    exit 1
  fi
  echo "PASS: ${label}"
}

expect_fail_token "empty image" bash "${HELPER}" ""
expect_fail_token "untagged image" bash "${HELPER}" "coqorg/coq"
expect_fail_token "latest tag" bash "${HELPER}" "coqorg/coq:latest"

echo "CI_DOCKER_FAILURE_CONTRACT=PASS"

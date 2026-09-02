#!/usr/bin/env bash
# Fail closed if this job is running inside a GitHub Actions job container.
#
# Job-level `container:` pulls the image before any step runs. A missing tag
# or Hub outage then surfaces as:
#   Docker pull failed with exit code 1
#   Value cannot be null. (Parameter 'ContainerId')
# and CI_DOCKER_ENVIRONMENT_FAILURE cannot be printed.
#
# Coq / build jobs must use ubuntu-latest + tools/ci/setup-coq-8.15.0.sh.
set -euo pipefail

if [[ "${GITHUB_ACTIONS:-}" == "true" && -f /.dockerenv ]]; then
  echo "CI_DOCKER_ENVIRONMENT_FAILURE"
  echo "FAIL: job-level container: is forbidden on GitHub Actions"
  echo "FAIL: docker pull/create errors become Parameter 'ContainerId' and skip gates"
  echo "FAIL: install pinned Coq 8.15.0 via tools/ci/setup-coq-8.15.0.sh instead"
  exit 1
fi

if [[ -n "${CI_DOCKER_IMAGE:-}" ]]; then
  ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
  bash "${ROOT}/tools/ci/docker-pull-or-fail.sh" "${CI_DOCKER_IMAGE}"
fi

echo "CI_DOCKER_JOB_CONTAINER=forbidden"
echo "CI_ENVIRONMENT_PREFLIGHT=PASS"

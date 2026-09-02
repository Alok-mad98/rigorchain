#!/usr/bin/env bash
# Pull a public Docker image with a pinned tag, or fail closed.
#
# GitHub Actions job-level `container:` swallows docker-pull failures as:
#   Value cannot be null. (Parameter 'ContainerId')
# and never runs job steps. This helper is the only allowed Docker path:
# print CI_DOCKER_ENVIRONMENT_FAILURE and stop. Do not continue gates.
#
# Forbidden: untagged names and :latest. This TASK does not change protocol.
set -u

docker_fail() {
  echo "CI_DOCKER_ENVIRONMENT_FAILURE"
  echo "FAIL: $*"
  exit 1
}

IMAGE="${1:-}"
if [[ -z "${IMAGE}" ]]; then
  docker_fail "no Docker image specified"
fi

if [[ "${IMAGE}" == *@sha256:* ]]; then
  : # digest pin is acceptable
elif [[ "${IMAGE}" != *:* ]]; then
  docker_fail "Docker image must use a pinned tag (not latest): ${IMAGE}"
else
  tag="${IMAGE##*:}"
  if [[ -z "${tag}" || "${tag}" == "latest" ]]; then
    docker_fail "Docker image must use a pinned tag (not latest): ${IMAGE}"
  fi
fi

if ! command -v docker >/dev/null 2>&1; then
  docker_fail "docker is not on PATH (cannot pull ${IMAGE})"
fi

echo "docker pull ${IMAGE}"
docker pull "${IMAGE}"
pull_rc=$?
if [[ "${pull_rc}" -ne 0 ]]; then
  docker_fail "docker pull failed for ${IMAGE} (exit ${pull_rc})"
fi

CID="$(docker create "${IMAGE}" true 2>/tmp/ci-docker-create.err)"
create_rc=$?
if [[ "${create_rc}" -ne 0 || -z "${CID}" ]]; then
  err="$(cat /tmp/ci-docker-create.err 2>/dev/null || true)"
  docker_fail "docker create failed for ${IMAGE} (ContainerId empty, exit ${create_rc}) ${err}"
fi
docker rm -f "${CID}" >/dev/null 2>&1 || true

DIGEST="$(docker image inspect --format '{{index .RepoDigests 0}}' "${IMAGE}" 2>/dev/null || echo unknown)"
echo "DOCKER_IMAGE=${IMAGE}"
echo "DOCKER_IMAGE_DIGEST=${DIGEST}"
echo "CI_DOCKER_ENVIRONMENT=PASS"

#!/usr/bin/env bash
set -euo pipefail
ALPHA_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${ALPHA_DIR}/../.." && pwd)"
RUNTIME="${REPO_ROOT}/.runtime/alpha-local-stack"
LMS="${REPO_ROOT}/contrib/testing/local_mainnet_simulation"

for pidf in pool-pilot.pid explorer.pid; do
  if [[ -f "${RUNTIME}/${pidf}" ]]; then
    pid="$(cat "${RUNTIME}/${pidf}")"
    kill "${pid}" 2>/dev/null || true
    rm -f "${RUNTIME}/${pidf}"
  fi
done

bash "${LMS}/stop_network.sh" || true
echo "[alpha-stack] stopped"

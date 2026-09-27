#!/usr/bin/env bash
# Shared paths/env for
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
export LMS_ROOT="${ROOT}"
export LMS_DIR="${SCRIPT_DIR}"
export LMS_RUNTIME="${ROOT}/.runtime/local-mainnet-simulation"
export LMS_OUT="${SCRIPT_DIR}/out"
export LMS_DOCS="${ROOT}/docs/release/local_simulation"
export LMS_RIGORD="${RIGORD:-${ROOT}/core/src/rigord}"
export LMS_CLI="${RIGOR_CLI:-${ROOT}/core/src/rigor-cli}"
export LMS_QT="${RIGOR_QT:-${ROOT}/core/src/qt/rigor-qt}"

export LMS_RPC_USER="${LMS_RPC_USER:-lmsim}"
export LMS_RPC_PASS="${LMS_RPC_PASS:-lmsim-local-only}"
export LMS_CHAIN=regtest

# Node-0 miner+wallet | Node-1 validator | Node-2 observer
export LMS_NODE0_P2P=27801
export LMS_NODE0_RPC=27811
export LMS_NODE1_P2P=27802
export LMS_NODE1_RPC=27812
export LMS_NODE2_P2P=27803
export LMS_NODE2_RPC=27813

lms_require_binaries() {
  local missing=0
  for b in "${LMS_RIGORD}" "${LMS_CLI}"; do
    if [[ ! -x "$b" ]]; then
      echo "ERROR: missing executable: $b" >&2
      missing=1
    fi
  done
  if [[ "$missing" -ne 0 ]]; then
    echo "Build first: cd ${ROOT}/core && make -j\$(nproc)" >&2
    exit 1
  fi
}

lms_cli_node0() {
  "${LMS_CLI}" -regtest -rpcconnect=127.0.0.1 -rpcport="${LMS_NODE0_RPC}" \
    -rpcuser="${LMS_RPC_USER}" -rpcpassword="${LMS_RPC_PASS}" \
    -rpcclienttimeout=180 "$@"
}

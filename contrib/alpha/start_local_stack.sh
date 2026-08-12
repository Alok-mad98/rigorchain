#!/usr/bin/env bash
# TASK-MAINNET-PRODUCT-ALPHA-001 — start local Node + Wallet RPC + Miner path + Pool + Explorer hooks
set -euo pipefail

ALPHA_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${ALPHA_DIR}/../.." && pwd)"
LMS="${REPO_ROOT}/contrib/testing/local_mainnet_simulation"
RUNTIME="${REPO_ROOT}/.runtime/alpha-local-stack"
mkdir -p "${RUNTIME}"

# Prefer alpha-staged binaries when present
export RIGORD="${RIGORD:-${REPO_ROOT}/dist/alpha/linux/bin/rigord}"
export RIGOR_CLI="${RIGOR_CLI:-${REPO_ROOT}/dist/alpha/linux/bin/rigor-cli}"
export RIGOR_QT="${RIGOR_QT:-${REPO_ROOT}/dist/alpha/linux/bin/rigor-wallet}"
export RIGOR_MINER="${RIGOR_MINER:-${REPO_ROOT}/dist/alpha/linux/bin/rigor-miner}"
[[ -x "${RIGORD}" ]] || RIGORD="${REPO_ROOT}/core/src/rigord"
[[ -x "${RIGOR_CLI}" ]] || RIGOR_CLI="${REPO_ROOT}/core/src/rigor-cli"
[[ -x "${RIGOR_QT}" ]] || RIGOR_QT="${REPO_ROOT}/core/src/qt/rigor-qt"
[[ -x "${RIGOR_MINER}" ]] || RIGOR_MINER="${REPO_ROOT}/cpuminer/minerd"

export RIGORD RIGOR_CLI RIGOR_QT

log() { printf '[alpha-stack] %s\n' "$*"; }

log "Starting local multi-node network (Node0 miner/wallet, Node1 validator, Node2 observer)…"
bash "${LMS}/start_network.sh" "$@"

# shellcheck source=/dev/null
source "${LMS}/common.sh"
lms_require_binaries

# Mine a few ORU blocks so wallet/miner/pool have tip
log "Mining bootstrap ORU blocks…"
bash "${LMS}/mine_oru.sh" 8 || true

# Pool pilot (optional — build if missing)
POOL_BIN="${REPO_ROOT}/pool/pilot/target/release/rigor-pool-pilot"
if [[ ! -x "${POOL_BIN}" ]]; then
  if command -v cargo >/dev/null 2>&1; then
    log "Building pool pilot…"
    (cd "${REPO_ROOT}/pool/pilot" && cargo build --release 2>&1 | tail -20) || log "Pool pilot build failed — continuing without pool"
  fi
fi

if [[ -x "${POOL_BIN}" ]]; then
  log "Starting rigor-pool-pilot on Stratum 127.0.0.1:3333…"
  export RIGOR_RPC_URL="http://127.0.0.1:${LMS_NODE0_RPC}/"
  export RIGOR_RPC_USER="${LMS_RPC_USER}"
  export RIGOR_RPC_PASSWORD="${LMS_RPC_PASS}"
  export STRATUM_LISTEN="127.0.0.1:3333"
  export MINT_AUTO_SUBMIT="${MINT_AUTO_SUBMIT:-true}"
  export RIGOR_POOL_PROFILE="${RIGOR_POOL_PROFILE:-devtest}"
  export PILOT_STATUS_FILE="${RUNTIME}/pool-pilot-status.json"
  nohup "${POOL_BIN}" >"${RUNTIME}/pool-pilot.log" 2>&1 &
  echo $! >"${RUNTIME}/pool-pilot.pid"
  log "Pool PID $(cat "${RUNTIME}/pool-pilot.pid") — log ${RUNTIME}/pool-pilot.log"
else
  log "Pool pilot binary missing — skip Stratum (solo miner still valid)"
fi

# Explorer: write env pointing at Node0; start only if Postgres ready + START_EXPLORER=1
cat > "${RUNTIME}/explorer.env" <<EOF
RIGOR_RPC_URL=http://127.0.0.1:${LMS_NODE0_RPC}/
RIGOR_RPC_USER=${LMS_RPC_USER}
RIGOR_RPC_PASSWORD=${LMS_RPC_PASS}
RIGOR_CLI_BIN=${RIGOR_CLI}
PORT=8080
EOF

if [[ "${START_EXPLORER:-0}" == "1" ]]; then
  if pg_isready -h 127.0.0.1 -p 5432 >/dev/null 2>&1; then
    log "Starting explorer-indexer-mvp on :8080…"
    (
      cd "${REPO_ROOT}/explorer-indexer-mvp"
      set -a
      # shellcheck disable=SC1091
      source "${RUNTIME}/explorer.env"
      set +a
      nohup npm run dev >"${RUNTIME}/explorer.log" 2>&1 &
      echo $! >"${RUNTIME}/explorer.pid"
    )
    log "Explorer PID $(cat "${RUNTIME}/explorer.pid" 2>/dev/null || echo '?')"
  else
    log "Postgres not ready — explorer not started (set START_EXPLORER=1 after pg_isready)"
  fi
else
  log "Explorer deferred (START_EXPLORER=0). Env written to ${RUNTIME}/explorer.env"
fi

cat > "${RUNTIME}/STACK_INFO.txt" <<EOF
RIGOR Alpha Local Stack
=======================
Node0 RPC:  http://127.0.0.1:${LMS_NODE0_RPC}  user=${LMS_RPC_USER}
Node1 RPC:  http://127.0.0.1:${LMS_NODE1_RPC}
Node2 RPC:  http://127.0.0.1:${LMS_NODE2_RPC}
Stratum:    127.0.0.1:3333 (if pool pilot running)
Explorer:   http://127.0.0.1:8080 (if START_EXPLORER=1)
Wallet GUI: ${RIGOR_QT} -regtest -rpcport=${LMS_NODE0_RPC} ...
Miner:      ${RIGOR_MINER} -o http://127.0.0.1:${LMS_NODE0_RPC} -u ${LMS_RPC_USER} -p ${LMS_RPC_PASS}

Validate:
  contrib/alpha/validate_miner_local.sh
  contrib/alpha/validate_pool_local.sh
  contrib/alpha/run_uat_checklist.sh
EOF

log "Stack ready."
cat "${RUNTIME}/STACK_INFO.txt"

# Shared paths for private testnet scripts. Sourced, not executed.
set -euo pipefail

TESTNET_TOOLS="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${TESTNET_TOOLS}/../.." && pwd)"
CONF_DIR="${ROOT}/testnet/bootstrap"
DATA_ROOT="${ROOT}/testnet/data"
RIGORD="${RIGORD:-${ROOT}/core/src/rigord}"
RIGOR_CLI="${RIGOR_CLI:-${ROOT}/core/src/rigor-cli}"

die() { echo "FAIL: $*" >&2; exit 1; }

require_binaries() {
  [[ -x "${RIGORD}" ]] || die "missing ${RIGORD}; build the node first (Autotools in core/)"
  [[ -x "${RIGOR_CLI}" ]] || die "missing ${RIGOR_CLI}"
}

node_datadir() { echo "${DATA_ROOT}/$1"; }
node_conf() { echo "${CONF_DIR}/$1.conf"; }
node_pidfile() { echo "${DATA_ROOT}/$1.pid"; }

rpc() {
  local id="$1"
  shift
  "${RIGOR_CLI}" -testnet \
    -datadir="$(node_datadir "${id}")" \
    -conf="$(node_conf "${id}")" \
    "$@"
}

wait_rpc() {
  local id="$1"
  local i
  for i in $(seq 1 60); do
    if rpc "${id}" getblockcount >/dev/null 2>&1; then
      return 0
    fi
    sleep 1
  done
  die "${id} RPC did not become ready"
}

is_running() {
  local pf
  pf="$(node_pidfile "$1")"
  [[ -f "${pf}" ]] && kill -0 "$(cat "${pf}")" 2>/dev/null
}

start_node() {
  local id="$1"
  require_binaries
  [[ -f "$(node_conf "${id}")" ]] || die "missing $(node_conf "${id}")"
  mkdir -p "$(node_datadir "${id}")" "${DATA_ROOT}"
  if is_running "${id}"; then
    echo "${id} already running pid=$(cat "$(node_pidfile "${id}")")"
    return 0
  fi
  "${RIGORD}" -testnet \
    -datadir="$(node_datadir "${id}")" \
    -conf="$(node_conf "${id}")" \
    -pid="$(node_pidfile "${id}")" \
    -printtoconsole=0 \
    >>"${DATA_ROOT}/${id}.stdout" 2>>"${DATA_ROOT}/${id}.stderr" &
  echo $! >"$(node_pidfile "${id}")"
  sleep 0.3
  if ! is_running "${id}"; then
    echo "----- ${id} stderr -----" >&2
    tail -n 40 "${DATA_ROOT}/${id}.stderr" >&2 || true
    die "${id} exited before RPC"
  fi
  wait_rpc "${id}"
  echo "STARTED=${id} pid=$(cat "$(node_pidfile "${id}")")"
}

stop_node() {
  local id="$1"
  local pf
  pf="$(node_pidfile "${id}")"
  if ! is_running "${id}"; then
    rm -f "${pf}"
    echo "STOPPED=${id} (was not running)"
    return 0
  fi
  rpc "${id}" stop >/dev/null 2>&1 || true
  local i
  for i in $(seq 1 20); do
    if ! is_running "${id}"; then
      rm -f "${pf}"
      echo "STOPPED=${id}"
      return 0
    fi
    sleep 1
  done
  kill "$(cat "${pf}")" 2>/dev/null || true
  rm -f "${pf}"
  echo "STOPPED=${id} (killed)"
}

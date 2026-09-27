#!/usr/bin/env bash
# Regtest golden vector fixture materialization harness.
#
# Usage:
#   golden_vector_materializer.sh [--datadir=PATH] [--datadir PATH] [--verify-fvr]
#       [--skip-mine] [--rematerialize-only] [vector_id]
#
# Prerequisites:
#   - rigord running on regtest with RPC enabled
#   - Chain caught up (not in initialblockdownload) before mining
#
# Examples:
#   ./contrib/tools/golden_vector_materializer.sh --datadir="${DATADIR}"
#   ./contrib/tools/golden_vector_materializer.sh --datadir="${DATADIR}" --rematerialize-only
#   ./contrib/tools/golden_vector_materializer.sh --datadir="${DATADIR}" GV-ORU-0001

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CORE_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILT_CLI="${CORE_DIR}/src/rigor-cli"
BUILT_D="${CORE_DIR}/src/rigord"

RIGOR_CLI="${RIGOR_CLI:-}"
RIGOR_D="${RIGOR_D:-}"
DATADIR="${DATADIR:-}"
VERIFY_FVR=false
TARGET="batch"
START_NODE=false
SKIP_MINE=false
REMATERIALIZE_ONLY=false
BATCH_SIZE=1000
MAX_VECTOR_HEIGHT=2100001
RPC_TIMEOUT=0
STABLE_POLLS=3
STABLE_INTERVAL=5

if [[ -x "${BUILT_CLI}" ]]; then
  RIGOR_CLI="${RIGOR_CLI:-${BUILT_CLI}}"
fi
if [[ -x "${BUILT_D}" ]]; then
  RIGOR_D="${RIGOR_D:-${BUILT_D}}"
fi

usage() {
  sed -n '2,16p' "$0"
  exit 1
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --datadir=*)
      DATADIR="${1#*=}"
      shift
      ;;
    --datadir)
      DATADIR="$2"
      shift 2
      ;;
    --verify-fvr)
      VERIFY_FVR=true
      shift
      ;;
    --start-node)
      START_NODE=true
      shift
      ;;
    --skip-mine)
      SKIP_MINE=true
      shift
      ;;
    --rematerialize-only)
      REMATERIALIZE_ONLY=true
      SKIP_MINE=true
      shift
      ;;
    -h|--help)
      usage
      ;;
    batch|all|GV-*)
      TARGET="$1"
      shift
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage
      ;;
  esac
done

if [[ -z "${RIGOR_CLI}" ]]; then
  if command -v rigor-cli >/dev/null 2>&1; then
    RIGOR_CLI="$(command -v rigor-cli)"
  else
    echo "rigor-cli not found. Build core or set RIGOR_CLI." >&2
    exit 127
  fi
fi

CLI=( "${RIGOR_CLI}" -regtest -rpcclienttimeout="${RPC_TIMEOUT}" )
if [[ -n "${DATADIR}" ]]; then
  mkdir -p "${DATADIR}"
  CLI+=( -datadir="${DATADIR}" )
fi

rpc() {
  "${CLI[@]}" "$@"
}

rpc_ping() {
  local -a ping_cli=( "${RIGOR_CLI}" -regtest -rpcclienttimeout=5 )
  if [[ -n "${DATADIR}" ]]; then
    ping_cli+=( -datadir="${DATADIR}" )
  fi
  ping_cli+=( getblockcount )
  "${ping_cli[@]}" >/dev/null 2>&1
}

rpc_endpoint_hint() {
  local port
  port="$("${RIGOR_CLI}" -regtest -help 2>/dev/null \
    | awk '/^-rpcport=/{print $2; exit}' \
    | grep -Eo '[0-9]+' || true)"
  if [[ -z "${port}" ]]; then
    port="28761"
  fi
  echo "127.0.0.1:${port} (regtest default)"
}

fail_no_rpc() {
  echo "golden_vector_materializer: cannot reach regtest RPC at $(rpc_endpoint_hint)" >&2
  echo "  datadir=${DATADIR:-<default>}" >&2
  echo "" >&2
  echo "Start rigord first (Terminal A), then re-run materializer:" >&2
  echo "  ./src/rigord -regtest -daemon -datadir=\"${DATADIR}\"" >&2
  echo "  ./src/rigor-cli -regtest -datadir=\"${DATADIR}\" -rpcwait getblockcount" >&2
  echo "" >&2
  echo "Or let the materializer start the daemon:" >&2
  echo "  ${SCRIPT_DIR}/golden_vector_materializer.sh --datadir=\"${DATADIR}\" --start-node batch" >&2
  echo "" >&2
  echo "Terminal B — progress monitor while mining:" >&2
  echo "  ${SCRIPT_DIR}/golden_vector_archive_monitor.sh --datadir=\"${DATADIR}\"" >&2
  exit 1
}

rpc_retry() {
  local attempts=5
  local delay=3
  local i
  for ((i = 1; i <= attempts; i++)); do
    if rpc "$@"; then
      return 0
    fi
    echo "golden_vector_materializer: RPC retry ${i}/${attempts} for: $*" >&2
    sleep "${delay}"
  done
  return 1
}

wait_for_rpc() {
  echo "golden_vector_materializer: waiting for RPC..."
  rpc -rpcwait -rpcwaittimeout=120 getblockcount >/dev/null
}

ensure_wallet() {
  # Ensure wallet for generatetoaddress ([] matches grep '[', so parse explicitly).
  local wallets_json
  wallets_json="$(rpc listwallets 2>/dev/null || echo '[]')"
  if [[ "${wallets_json}" == "[]" ]]; then
    rpc createwallet golden_vector_fixture >/dev/null 2>&1 \
      || rpc loadwallet golden_vector_fixture >/dev/null 2>&1 \
      || rpc createwallet gv_materialize >/dev/null 2>&1 \
      || rpc loadwallet gv_materialize >/dev/null 2>&1 \
      || true
  fi
  if ! rpc getnewaddress "" bech32 >/dev/null 2>&1; then
    echo "golden_vector_materializer: no wallet loaded — run createwallet or loadwallet first" >&2
    exit 1
  fi
}

chain_in_ibd() {
  rpc getblockchaininfo 2>/dev/null \
    | python3 -c 'import json,sys; print(json.load(sys.stdin).get("initialblockdownload", True))' \
    2>/dev/null || echo true
}

bootstrap_clear_stale_ibd() {
  # Fresh regtest datadirs often have a genesis tip older than -maxtipage (24h),
  # which keeps initialblockdownload=true forever until a new block is mined.
  local height ibd
  height="$(rpc getblockcount)"
  ibd="$(chain_in_ibd)"
  if [[ "${ibd}" == "False" || "${ibd}" == "false" ]]; then
    return 0
  fi
  # Archive chains already past genesis: IBD may stay true cosmetically on regtest
  # while generatetoaddress still works; skip slow bootstrap (wallet rescan stalls RPC).
  if [[ "${height}" -ge 1 ]]; then
    echo "golden_vector_materializer: height=${height}, initialblockdownload=${ibd} — skip bootstrap (regtest archive chain)"
    return 0
  fi
  echo "golden_vector_materializer: stale genesis tip keeps initialblockdownload=true; mining 1 bootstrap block..."
  local addr
  addr="$(rpc getnewaddress "" bech32)"
  rpc_retry generatetoaddress 1 "${addr}" >/dev/null
  ibd="$(chain_in_ibd)"
  if [[ "${ibd}" == "True" || "${ibd}" == "true" ]]; then
    echo "golden_vector_materializer: initialblockdownload still true after bootstrap block" >&2
    exit 1
  fi
  echo "golden_vector_materializer: chain ready (initialblockdownload=false)"
}

wait_for_height_stable() {
  local last=""
  local stable=0
  echo "golden_vector_materializer: waiting for stable block height..."
  while [[ "${stable}" -lt "${STABLE_POLLS}" ]]; do
    local h
    h="$(rpc getblockcount)"
    if [[ "${h}" == "${last}" ]]; then
      stable=$((stable + 1))
    else
      stable=0
      last="${h}"
    fi
    sleep "${STABLE_INTERVAL}"
  done
  echo "golden_vector_materializer: stable height=${last}"
}

if ! rpc_ping; then
  if [[ "${START_NODE}" == true ]]; then
    if [[ -z "${RIGOR_D}" ]]; then
      echo "--start-node requires rigord (set RIGOR_D or build src/rigord)." >&2
      exit 1
    fi
    echo "golden_vector_materializer: starting rigord (regtest)..."
    DAEMON=( "${RIGOR_D}" -regtest -daemon )
    if [[ -n "${DATADIR}" ]]; then
      DAEMON+=( -datadir="${DATADIR}" )
    fi
    "${DAEMON[@]}"
    sleep 2
  else
    fail_no_rpc
  fi
fi

wait_for_rpc
ensure_wallet
bootstrap_clear_stale_ibd
wait_for_height_stable

ADDR="$(rpc getnewaddress "" bech32)"
HEIGHT="$(rpc getblockcount)"

echo "golden_vector_materializer: current height=${HEIGHT}"

if [[ "${REMATERIALIZE_ONLY}" == true ]]; then
  echo "golden_vector_materializer: rematerialize-only (skip mining)"
elif [[ "${SKIP_MINE}" != true && "${HEIGHT}" -lt "${MAX_VECTOR_HEIGHT}" ]]; then
  NEED=$(( MAX_VECTOR_HEIGHT - HEIGHT ))
  echo "Mining ${NEED} blocks to height ${MAX_VECTOR_HEIGHT} (batch size ${BATCH_SIZE})..."
  while [[ "${HEIGHT}" -lt "${MAX_VECTOR_HEIGHT}" ]]; do
    N=$(( MAX_VECTOR_HEIGHT - HEIGHT ))
    if [[ "${N}" -gt "${BATCH_SIZE}" ]]; then
      N="${BATCH_SIZE}"
    fi
    rpc_retry generatetoaddress "${N}" "${ADDR}" >/dev/null
    HEIGHT="$(rpc getblockcount)"
    echo "  height=${HEIGHT}"
  done
fi

echo "Materializing golden vectors (target=${TARGET})..."
rpc_retry materializegoldenvector "${TARGET}"

VERIFY_ARGS=( getruntimevectorstatus "${TARGET}" )
if [[ "${VERIFY_FVR}" == true ]]; then
  VERIFY_ARGS+=( true )
fi

echo "Verification report:"
rpc "${VERIFY_ARGS[@]}"

if [[ "${TARGET}" == "batch" || "${TARGET}" == "all" ]]; then
  SUMMARY="$(rpc "${VERIFY_ARGS[@]}")"
  VERIFY_FVR="${VERIFY_FVR}" SUMMARY_JSON="${SUMMARY}" python3 - <<'PY'
import json, os, sys

data = json.loads(os.environ["SUMMARY_JSON"])
summary = data.get("summary", {})
pass_n = summary.get("pass", 0)
partial_n = summary.get("partial", 0)
fail_n = summary.get("fail", 0)
not_exec_n = summary.get("not_executed", 0)
print(
    "summary: pass=%d partial=%d fail=%d not_executed=%d"
    % (pass_n, partial_n, fail_n, not_exec_n)
)
if data.get("equivalence_claimed") is not False:
    raise SystemExit("equivalence_claimed must be false")
if fail_n != 0:
    raise SystemExit("harness fail count must be 0 after materialize")
PY
fi

echo "golden_vector_materializer: done."
echo "Run end-to-end verify:"
echo "  ${SCRIPT_DIR}/golden_vector_verify.sh --datadir=${DATADIR:-<datadir>} batch"
echo "  ${SCRIPT_DIR}/golden_vector_verify.sh --datadir=${DATADIR:-<datadir>} --require-full batch  # after full height"

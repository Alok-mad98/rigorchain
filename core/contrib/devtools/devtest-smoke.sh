#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DAEMON="${ROOT_DIR}/src/rigord"
CLI="${ROOT_DIR}/src/rigor-cli"
CONVERGENCE_TX_SCRIPT="${ROOT_DIR}/contrib/devtools/devtest-create-convergence-tx.py"

DATADIR="${DATADIR:-${ROOT_DIR}/.devtest-smoke}"
RPC_USER="${RPC_USER:-devtest}"
RPC_PASS="${RPC_PASS:-devtestpass}"
P2P_PORT="${P2P_PORT:-48760}"
RPC_PORT="${RPC_PORT:-48761}"
FALLBACK_FEE="${FALLBACK_FEE:-0.001}"
# On-chain OP_RETURN marker bytes (ASCII "convergence"); consensus / tests assert this literal.
MARKER_OP_RETURN_HEX="464f524745"
# Core no longer creates a default wallet; smoke test uses one descriptor wallet.
WALLET_NAME="${SMOKE_WALLET:-devtest-smoke}"
# Must match devtest consensus (kernel/chainparams.cpp CDevTestParams::nCoinbaseMaturity).
COINBASE_MATURITY_BLOCKS="${COINBASE_MATURITY_BLOCKS:-100}"
# Extra blocks beyond (maturity + genesis): wallet needs depth >= nCoinbaseMaturity+1 on the earliest
# post-genesis coinbase; mining maturity+5 (e.g. 105 when maturity=100) adds a small safety margin.
SMOKE_COINBASE_MINING_EXTRA="${SMOKE_COINBASE_MINING_EXTRA:-5}"

# ------------------------------------------------------------------
# Configuration
# ------------------------------------------------------------------
# MAX_START_RETRIES=3
#   Maximum number of retries if the service fails the health check
#   during startup.
#
# BACKOFF_SECONDS_1=1, BACKOFF_SECONDS_2=2, BACKOFF_SECONDS_3=4
#   Wait times between retries. Uses exponential backoff to prevent
#   system thrashing.
#
# RPC_READY_TIMEOUT_SECS=120
#   Seconds for each rigor-cli -rpcwait -rpcwaittimeout probe (connection + RPC warmup).
#
# RPC_READY_POLL_INTERVAL_SECS=0.5
#   Interval (in seconds) between health check polls.
#
# GRACEFUL_STOP_TIMEOUT_SECS=5
#   Time to wait for the process to exit after sending SIGTERM.
#   If the process is still running after this duration, SIGKILL
#   will be sent.
#
# ------------------------------------------------------------------

MAX_START_RETRIES="${MAX_START_RETRIES:-3}"
BACKOFF_SECONDS_1="${BACKOFF_SECONDS_1:-1}"
BACKOFF_SECONDS_2="${BACKOFF_SECONDS_2:-2}"
BACKOFF_SECONDS_3="${BACKOFF_SECONDS_3:-4}"
GRACEFUL_STOP_TIMEOUT_SECS="${GRACEFUL_STOP_TIMEOUT_SECS:-5}"
# Total seconds for each -rpcwait probe (connection refused + RPC warmup). Old default 25s was too low on WSL.
RPC_READY_TIMEOUT_SECS="${RPC_READY_TIMEOUT_SECS:-120}"
RPC_READY_POLL_INTERVAL_SECS="${RPC_READY_POLL_INTERVAL_SECS:-0.5}"

started_here=0

need_bin() {
    local path="$1"
    if [[ ! -x "$path" ]]; then
        echo "Missing executable: $path"
        echo "Please build first: make -C src -j\$(nproc) rigord rigor-cli"
        exit 1
    fi
}

rpc() {
    # Always pass -rpcport: datadir settings.json / bitcoin.conf may override the devtest default,
    # causing rigor-cli to hit the wrong port while rigord listens on RPC_PORT.
    "$CLI" -chain=devtest -datadir="$DATADIR" -rpcuser="$RPC_USER" -rpcpassword="$RPC_PASS" \
        -rpcport="$RPC_PORT" -rpcconnect=127.0.0.1 "$@"
}

rpc_w() {
    # Wallet RPCs (getnewaddress, generatetoaddress, …) must target SMOKE_WALLET.
    "$CLI" -chain=devtest -datadir="$DATADIR" -rpcuser="$RPC_USER" -rpcpassword="$RPC_PASS" \
        -rpcport="$RPC_PORT" -rpcconnect=127.0.0.1 -rpcwallet="$WALLET_NAME" "$@"
}

# Run an RPC that must return JSON on stdout. If rigor-cli fails or stdout is not JSON,
# print a clear message and exit 1 (avoids misleading tracebacks from python json.load under set -e).
# Usage: rpc_capture_json OUT_VAR_NAME <same args as rpc()>
rpc_capture_json() {
    local out_var="$1"
    shift
    local out rc
    set +e
    out="$(rpc "$@" 2>&1)"
    rc=$?
    set -e
    if [[ "$rc" -ne 0 ]]; then
        echo "[devtest-smoke] RPC failed (rigor-cli exit ${rc}): rpc $*" >&2
        echo "$out" >&2
        echo "[devtest-smoke] Hint: ensure only one rigord uses -rpcport=${RPC_PORT} and -datadir=${DATADIR}; for \"Block not found on disk\" try a clean datadir (stop node, rm -rf datadir, restart) or disable pruning." >&2
        exit 1
    fi
    if ! printf '%s\n' "$out" | python3 -c 'import json,sys; json.load(sys.stdin)' 2>/dev/null; then
        echo "[devtest-smoke] RPC exited 0 but response is not valid JSON: rpc $*" >&2
        echo "$out" >&2
        exit 1
    fi
    printf -v "$out_var" '%s' "$out"
}

create_convergence_raw_tx() {
    # Use --cli-extra=-opt=value so argparse does not treat the next token as a new flag
    # (values like -rpcuser=… start with '-' and trigger "expected one argument" otherwise).
    python3 "$CONVERGENCE_TX_SCRIPT" \
        --cli "$CLI" \
        --chain devtest \
        --cli-extra="-datadir=${DATADIR}" \
        --cli-extra="-rpcuser=${RPC_USER}" \
        --cli-extra="-rpcpassword=${RPC_PASS}" \
        --cli-extra="-rpcport=${RPC_PORT}" \
        --cli-extra="-rpcconnect=127.0.0.1" \
        --cli-extra="-rpcwallet=${WALLET_NAME}" \
        --convergence-orum 1.0 \
        --fee-sats 1000 \
        --json
}

cleanup() {
    if [[ "$started_here" -eq 1 ]]; then
        rpc stop >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

rigord_pids_for_datadir() {
    # Best-effort process discovery without assuming environment cleanliness.
    # We match both the executable name and the datadir path to reduce false positives.
    while read -r pid args; do
        [[ -z "$pid" ]] && continue
        if [[ "$args" == *"rigord"* && "$args" == *"$DATADIR"* && "$args" == *"-chain=devtest"* ]]; then
            echo "$pid"
        fi
    done < <(ps -eo pid=,args= 2>/dev/null || true)
}

listening_pids_on_port() {
    local port="$1"
    if command -v lsof >/dev/null 2>&1; then
        lsof -t -iTCP:"$port" -sTCP:LISTEN 2>/dev/null || true
        return 0
    fi

    if command -v ss >/dev/null 2>&1; then
        # Extract "pid=NNNN" from ss output.
        ss -ltnp "sport = :$port" 2>/dev/null | awk '
            {
                while (match($0, /pid=([0-9]+)/, a)) {
                    print a[1];
                    $0 = substr($0, RSTART + RLENGTH);
                }
            }
        ' || true
        return 0
    fi

    # Last resort: no reliable port->pid mapping.
    return 0
}

kill_pids_term_then_kill() {
    local timeout_secs="$1"
    shift
    local pids=("$@")
    if [[ "${#pids[@]}" -eq 0 ]]; then
        return 0
    fi

    kill -TERM "${pids[@]}" 2>/dev/null || true
    local start_ts
    start_ts="$(date +%s)"

    while true; do
        local alive=()
        for pid in "${pids[@]}"; do
            kill -0 "$pid" 2>/dev/null && alive+=("$pid")
        done

        if [[ "${#alive[@]}" -eq 0 ]]; then
            return 0
        fi

        local now_ts
        now_ts="$(date +%s)"
        if (( now_ts - start_ts >= timeout_secs )); then
            kill -KILL "${alive[@]}" 2>/dev/null || true
            return 0
        fi

        sleep 0.5
    done
}

release_lock_files_if_no_process() {
    # Common lock locations for sqlite/leveldb style datadirs in Bitcoin-like daemons.
    # Remove only when we believe no daemon is holding them.
    local any_running=0
    if rigord_pids_for_datadir | read -r _; then
        any_running=1
    fi

    if [[ "$any_running" -ne 0 ]]; then
        return 0
    fi

    local lock_candidates=(
        "${DATADIR}/.lock"
        "${DATADIR}/lock"
        "${DATADIR}/database/LOCK"
        "${DATADIR}/chainstate/LOCK"
        "${DATADIR}/blocks/LOCK"
    )

    for f in "${lock_candidates[@]}"; do
        if [[ -e "$f" ]]; then
            rm -f "$f" || true
        fi
    done
}

kill_switch() {
    # "Kill Switch" - defensive cleanup before starting a conflicting new instance.
    local rigord_pids
    rigord_pids="$(rigord_pids_for_datadir | tr '\n' ' ')"
    # shellcheck disable=SC2206
    local rigord_pid_array=($rigord_pids)

    if [[ "${#rigord_pid_array[@]}" -gt 0 && -n "${rigord_pid_array[0]}" ]]; then
        echo "Kill Switch: stopping existing rigord PIDs: ${rigord_pid_array[*]}"
        kill_pids_term_then_kill "$GRACEFUL_STOP_TIMEOUT_SECS" "${rigord_pid_array[@]}"
    fi

    # Ensure the RPC port isn't held by a zombie/foreign process.
    local port_pids
    port_pids="$(listening_pids_on_port "$RPC_PORT" | tr '\n' ' ')"
    # shellcheck disable=SC2206
    local port_pid_array=($port_pids)

    if [[ "${#port_pid_array[@]}" -gt 0 && -n "${port_pid_array[0]}" ]]; then
        echo "Kill Switch: releasing RPC port ${RPC_PORT} PIDs: ${port_pid_array[*]}"
        kill_pids_term_then_kill "$GRACEFUL_STOP_TIMEOUT_SECS" "${port_pid_array[@]}"
    fi

    # Remove lock files if no daemon appears to be running.
    release_lock_files_if_no_process

    # Best-effort verification that the port is free before returning.
    if listening_pids_on_port "$RPC_PORT" | read -r _; then
        echo "Kill Switch: warning - RPC port ${RPC_PORT} still appears occupied."
        return 1
    fi

    return 0
}

wait_for_rpc_ready() {
    # Single-shot: -rpcwait retries connection failure and RPC_IN_WARMUP (-28) until -rpcwaittimeout.
    # Plain getblockcount fails during warmup; the old poll loop could not wait long enough.
    # Second argument (poll interval) is ignored — kept for call-site compatibility.
    local timeout_secs="$1"
    set +e
    rpc -rpcwait -rpcwaittimeout="$timeout_secs" getblockcount >/dev/null 2>&1
    local rc=$?
    set -e
    return "$rc"
}

need_bin "$DAEMON"
need_bin "$CLI"
need_bin "$CONVERGENCE_TX_SCRIPT"

mkdir -p "$DATADIR"

echo "[1/8] Checking devtest RPC health..."
echo "    (hint: rm -rf the datadir does not stop rigord; this script uses a kill-switch when RPC is down.)"

existing_rigord_running=0
if rigord_pids_for_datadir | read -r _; then
    existing_rigord_running=1
fi

need_start=1
if wait_for_rpc_ready 5 "$RPC_READY_POLL_INTERVAL_SECS"; then
    need_start=0
fi

if (( need_start == 1 && existing_rigord_running == 1 )); then
    echo "rigord process detected but RPC not ready yet; waiting up to ${RPC_READY_TIMEOUT_SECS}s..."
    if wait_for_rpc_ready "$RPC_READY_TIMEOUT_SECS" "$RPC_READY_POLL_INTERVAL_SECS"; then
        need_start=0
    fi
fi

if (( need_start == 1 )); then
    echo "RPC is not ready; applying Kill Switch + exponential backoff retries..."
    attempt=1
    backoff_secs=0
    while (( attempt <= MAX_START_RETRIES )); do
        if (( attempt == 1 )); then
            backoff_secs="$BACKOFF_SECONDS_1"
        elif (( attempt == 2 )); then
            backoff_secs="$BACKOFF_SECONDS_2"
        else
            backoff_secs="$BACKOFF_SECONDS_3"
        fi

        echo "[2/8] Kill Switch (attempt ${attempt}/${MAX_START_RETRIES})..."
        kill_switch || true

        echo "[3/8] Starting devtest node (attempt ${attempt}/${MAX_START_RETRIES})..."
        # Bind RPC explicitly on loopback with matching rpcallowip (otherwise some builds refuse to
        # listen, or curl/CLI may not match the bound address). Logs go to datadir for debugging.
        "$DAEMON" \
            -chain=devtest \
            -daemon \
            -datadir="$DATADIR" \
            -rpcuser="$RPC_USER" \
            -rpcpassword="$RPC_PASS" \
            -rpcport="$RPC_PORT" \
            -rpcbind=127.0.0.1 \
            -rpcallowip=127.0.0.1 \
            -rpcbind=::1 \
            -rpcallowip=::1 \
            -port="$P2P_PORT" \
            -fallbackfee="$FALLBACK_FEE" \
            -txindex=1 \
            -server=1 \
            >>"${DATADIR}/rigord-devtest-smoke.log" 2>&1 || true

        if wait_for_rpc_ready "$RPC_READY_TIMEOUT_SECS" "$RPC_READY_POLL_INTERVAL_SECS"; then
            started_here=1
            echo "RPC is now responsive."
            break
        fi

        if (( attempt < MAX_START_RETRIES )); then
            echo "RPC still not ready; waiting ${backoff_secs}s before retry..."
            sleep "$backoff_secs"
        fi

        attempt=$((attempt + 1))
    done

    if [[ "$started_here" -ne 1 ]]; then
        echo "Failed to start a responsive devtest node after ${MAX_START_RETRIES} attempts."
        echo "Diagnostics (rigor-cli getblockcount, no -rpcwait):"
        rpc getblockcount 2>&1 || true
        echo "--- tail ${DATADIR}/rigord-devtest-smoke.log ---"
        tail -50 "${DATADIR}/rigord-devtest-smoke.log" 2>/dev/null || true
        echo "--- tail ${DATADIR}/devtest/debug.log ---"
        tail -40 "${DATADIR}/devtest/debug.log" 2>/dev/null || true
        exit 1
    fi
else
    echo "RPC is already responsive; leaving existing node untouched."
fi

echo "[2/8] Ensuring wallet ${WALLET_NAME} exists (descriptor)…"
if rpc_w getwalletinfo >/dev/null 2>&1; then
    echo "    (wallet already loaded)"
else
    set +e
    rpc createwallet "$WALLET_NAME" false false "" false true true >/dev/null 2>&1
    cw=$?
    if [[ "$cw" -ne 0 ]]; then
        rpc loadwallet "$WALLET_NAME" >/dev/null 2>&1
    fi
    set -e
fi

echo "[3/8] Creating test addresses..."
MINER_ADDR="$(rpc_w getnewaddress "devtest-miner")"
RECV_ADDR="$(rpc_w getnewaddress "devtest-recv")"
echo "Miner:    ${MINER_ADDR}"
echo "Receiver: ${RECV_ADDR}"

echo "[4/8] Mining blocks so coinbase is mature (devtest nCoinbaseMaturity=${COINBASE_MATURITY_BLOCKS}, extra=${SMOKE_COINBASE_MINING_EXTRA})..."
# Minimum spendable depth uses GetTxBlocksToMaturity (needs depth >= nCoinbaseMaturity+1);
# from height 0, mining (maturity + 1 + extra) blocks (default 105) leaves margin beyond genesis/first coinbase.
MINING_BLOCKS=$((COINBASE_MATURITY_BLOCKS + SMOKE_COINBASE_MINING_EXTRA))
rpc_w generatetoaddress "$MINING_BLOCKS" "$MINER_ADDR" >/dev/null
HEIGHT_NOW="$(rpc getblockcount)"
if [[ "$HEIGHT_NOW" -lt "$MINING_BLOCKS" ]]; then
    echo "Unexpected height after mining: ${HEIGHT_NOW} (expected at least ${MINING_BLOCKS} on a fresh chain)"
    exit 1
fi
echo "Height: ${HEIGHT_NOW} (mined ${MINING_BLOCKS} blocks to ${MINER_ADDR})"

echo "[5/8] Checking subsidy and coinbase maturity behavior..."
# Use height (integer) for getblockstats — passing a bare hex hash makes rigor-cli try to JSON-parse it and fail.
rpc_capture_json H1_JSON getblockstats 1 '["subsidy"]'
H1_SUBSIDY="$(printf '%s\n' "$H1_JSON" | python3 -c 'import json,sys; print(json.load(sys.stdin)["subsidy"])')"
echo "Block #1 subsidy (in sat): ${H1_SUBSIDY}"
echo "If initial policy target is 1000 ORUM, verify this value separately against your planned subsidy change."

TXID_SEND="$(rpc_w sendtoaddress "$RECV_ADDR" 1)"
echo "Spend from coinbase succeeded (maturity=${COINBASE_MATURITY_BLOCKS}): ${TXID_SEND}"
rpc_w generatetoaddress 1 "$MINER_ADDR" >/dev/null
RECV_CONF="$(rpc_w getreceivedbyaddress "$RECV_ADDR" 1)"
echo "Receiver confirmed amount: ${RECV_CONF}"

echo "[6/8] Mining to pass one retarget window (30 blocks)..."
rpc_w generatetoaddress 35 "$MINER_ADDR" >/dev/null
HEIGHT_POST="$(rpc getblockcount)"
echo "Height after retarget-window mining: ${HEIGHT_POST}"
TIP_HASH="$(rpc getbestblockhash)"
TIP_BITS="$(rpc getblockheader "$TIP_HASH" | python3 -c 'import json,sys; print(json.load(sys.stdin)["bits"])')"
echo "Tip nBits: ${TIP_BITS}"

echo "[7/8] Convergence visibility RPC checks..."
CV_STATUS="$(rpc getconvergencestatus)"
echo "getconvergencestatus: ${CV_STATUS}"
CV_BLOCK="$(rpc getblockconvergence "$HEIGHT_POST")"
echo "getblockconvergence(${HEIGHT_POST}): ${CV_BLOCK}"

echo "[8/8] Creating strict consensus-valid convergence raw tx..."
CONVERGENCE_TX_JSON="$(create_convergence_raw_tx)"
MARKER_TXID="$(python3 -c 'import json,sys; print(json.load(sys.stdin)["txid"])' <<<"$CONVERGENCE_TX_JSON")"
echo "convergence txid: ${MARKER_TXID}"
echo "mempool convergence metadata: $(python3 -c 'import json,sys; r=json.load(sys.stdin); print(json.dumps(r.get("convergence_meta", {}), ensure_ascii=False))' <<<"$CONVERGENCE_TX_JSON")"

RAW_TX_JSON="$(rpc getrawtransaction "$MARKER_TXID" true)"
ASM_OK="$(python3 -c 'import json,sys; tx=json.load(sys.stdin); ok=False
for v in tx.get("vout", []):
    asm=v.get("scriptPubKey", {}).get("asm", "")
    if "OP_RETURN" in asm and "464f524745" in asm:
        ok=True
        break
print("1" if ok else "0")' <<<"$RAW_TX_JSON")"
if [[ "$ASM_OK" != "1" ]]; then
    echo "Convergence marker assertion failed: OP_RETURN ${MARKER_OP_RETURN_HEX} not found in tx vout asm"
    exit 1
fi
echo "OP_RETURN marker assertion passed (contains OP_RETURN ${MARKER_OP_RETURN_HEX})."

echo
echo "devtest smoke test completed."
echo "Datadir: ${DATADIR}"
if [[ "$started_here" -eq 1 ]]; then
    echo "Node started by script; it will be stopped now."
else
    echo "Node was already running; left running."
fi

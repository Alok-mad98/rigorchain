#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CLI="${CLI:-${ROOT_DIR}/src/rigor-cli}"
MINE_BOOTSTRAP_TOOL="${ROOT_DIR}/utils/mine_bootstrap.py"
FORGE_TX_TOOL="${ROOT_DIR}/utils/convergence_tx_builder.py"

CHAIN="${CHAIN:-devtest}"
COUNT="${COUNT:-20}"
FORGE_AMOUNT="${FORGE_AMOUNT:-1.0}"
FORGE_DATA_HEX="${FORGE_DATA_HEX:-464f524745}"
RETRY_MAX="${RETRY_MAX:-2}"
MINE_EACH_ROUND="${MINE_EACH_ROUND:-1}"
MINE_BLOCKS_AFTER="${MINE_BLOCKS_AFTER:-0}"
MIN_PRIORITY="${MIN_PRIORITY:--1}"
DATADIR="${DATADIR:-}"
RPC_USER="${RPC_USER:-}"
RPC_PASS="${RPC_PASS:-}"
RPC_PORT="${RPC_PORT:-}"
RPC_WALLET="${RPC_WALLET:-}"
CSV_OUT="${CSV_OUT:-${ROOT_DIR}/contrib/devtools/devtest-convergence-batch.csv}"

need_bin() {
    local path="$1"
    if [[ ! -x "$path" ]]; then
        echo "Missing executable: $path" >&2
        exit 1
    fi
}

build_cli_extras() {
    local extras=()
    [[ -n "$DATADIR" ]] && extras+=("-datadir=${DATADIR}")
    [[ -n "$RPC_USER" ]] && extras+=("-rpcuser=${RPC_USER}")
    [[ -n "$RPC_PASS" ]] && extras+=("-rpcpassword=${RPC_PASS}")
    [[ -n "$RPC_PORT" ]] && extras+=("-rpcport=${RPC_PORT}")
    [[ -n "$RPC_WALLET" ]] && extras+=("-rpcwallet=${RPC_WALLET}")
    printf '%s\n' "${extras[@]}"
}

rpc() {
    local method="$1"; shift
    local extras=()
    while IFS= read -r item; do
        [[ -n "$item" ]] && extras+=("$item")
    done < <(build_cli_extras)
    "$CLI" "-chain=${CHAIN}" "${extras[@]}" "$method" "$@"
}

normalize_error_msg() {
    local raw_msg="$1"
    python3 - "$raw_msg" <<'PY'
import re
import sys

msg = (sys.argv[1] or "").strip()
lower = msg.lower()

patterns = [
    (r"min relay fee|mempool min fee|insufficient fee", "mempool policy: min relay fee not met"),
    (r"insufficient funds", "wallet: insufficient funds"),
    (r"convergence (priority|eviction_score).*below required minimum", "convergence: eviction score below required minimum"),
    (r"convergence (priority|eviction_score) not found", "convergence: eviction metadata missing"),
    (r"op_return|nulldata", "convergence: OP_RETURN output missing"),
    (r"convergence: must spend ORU output", "convergence: must spend ORU output"),
    (r"connection refused|could not connect", "rpc: connection failed"),
    (r"method not found", "rpc: method not found"),
    (r"timeout", "rpc: timeout"),
]

short = ""
for pat, desc in patterns:
    if re.search(pat, lower):
        short = desc
        break

if not short:
    # Fallback: prefer a meaningful non-empty line over generic headers.
    lines = [ln.strip() for ln in msg.splitlines() if ln.strip()]
    first = "unknown error"
    for ln in lines:
        if ln.lower() in {"error message:", "error:"}:
            continue
        first = ln
        if not ln.lower().startswith("error code:"):
            break
    short = re.sub(r"\s+", " ", first).strip()
    if len(short) > 120:
        short = short[:117] + "..."

print(short)
PY
}

need_bin "$CLI"
need_bin "$MINE_BOOTSTRAP_TOOL"
need_bin "$FORGE_TX_TOOL"

echo "Convergence batch: chain=${CHAIN} count=${COUNT} amount=${FORGE_AMOUNT}"
rpc -rpcwait getblockcount >/dev/null
start_height="$(rpc getblockcount)"

extras=()
while IFS= read -r item; do
    [[ -n "$item" ]] && extras+=("--cli-extra=${item}")
done < <(build_cli_extras)

python3 "$MINE_BOOTSTRAP_TOOL" --cli "$CLI" --chain "$CHAIN" "${extras[@]}"

mkdir -p "$(dirname "$CSV_OUT")"
echo "batch_id,timestamp,txid,convergence_amount,is_in_mempool,mempool_fee,convergence_priority,confirm_height,blocks_to_confirm,status,error_msg" >"$CSV_OUT"

success=0
failed=0
mined_confirmed=0

for ((i=1; i<=COUNT; i++)); do
    ts="$(date +%s)"
    attempt=0
    out=""
    err_msg=""
    tx_ok=0
    while (( attempt <= RETRY_MAX )); do
        if out="$(python3 "$FORGE_TX_TOOL" --cli "$CLI" --chain "$CHAIN" "${extras[@]}" --amount "$FORGE_AMOUNT" --convergence-data-hex "$FORGE_DATA_HEX" --min-priority "$MIN_PRIORITY" --json 2>/tmp/devtest-convergence-batch.err)"; then
            tx_ok=1
            break
        fi
        err_msg="$(< /tmp/devtest-convergence-batch.err)"
        attempt=$((attempt + 1))
        sleep 1
    done

    if [[ "$tx_ok" -eq 1 ]]; then
        txid="$(python3 -c 'import json,sys; print(json.load(sys.stdin)["txid"])' <<<"$out")"
        mempool_json="$(rpc getmempoolentry "$txid")"
        mempool_fee="$(python3 -c 'import json,sys; print(json.load(sys.stdin).get("fees",{}).get("base","0"))' <<<"$mempool_json")"
        convergence_priority="$(python3 -c 'import json,sys; e=json.load(sys.stdin); fm=e.get("convergence_meta",{}); fb=e.get("convergence",{}); print(fm.get("eviction_score",fb.get("eviction_score",fm.get("priority",fb.get("priority",0)))))' <<<"$mempool_json")"
        has_op_return="$(python3 -c 'import json,sys; print("1" if json.load(sys.stdin).get("has_op_return",False) else "0")' <<<"$out")"
        if [[ "$has_op_return" == "1" ]]; then
            status="SUCCESS"
        else
            status="NO_OP_RETURN"
        fi
        confirm_height="-1"
        blocks_to_confirm="-1"
        error_msg=""

        if [[ "$MINE_EACH_ROUND" -eq 1 ]]; then
            round_height_before="$(rpc getblockcount)"
            miner_addr="$(rpc getnewaddress "convergence-round-miner" "legacy")"
            rpc generatetoaddress "1" "$miner_addr" >/dev/null
            tx_json="$(rpc gettransaction "$txid")"
            confs="$(python3 -c 'import json,sys; print(int(json.load(sys.stdin).get("confirmations",0)))' <<<"$tx_json")"
            if [[ "$confs" -gt 0 ]]; then
                blockhash="$(python3 -c 'import json,sys; print(json.load(sys.stdin).get("blockhash",""))' <<<"$tx_json")"
                if [[ -n "$blockhash" ]]; then
                    confirm_height="$(rpc getblockheader "$blockhash" | python3 -c 'import json,sys; print(int(json.load(sys.stdin).get("height",-1)))')"
                    blocks_to_confirm=$((confirm_height - round_height_before))
                    mined_confirmed=$((mined_confirmed + 1))
                fi
            fi
        fi

        echo "${i},${ts},${txid},${FORGE_AMOUNT},true,${mempool_fee},${convergence_priority},${confirm_height},${blocks_to_confirm},${status},\"${error_msg}\"" >>"$CSV_OUT"
        success=$((success+1))
        echo "[$i/$COUNT] ok txid=${txid} fee=${mempool_fee} priority=${convergence_priority} retry=${attempt}"
    else
        short_err_msg="$(normalize_error_msg "$err_msg")"
        error_msg_csv="$(python3 -c 'import sys; print(sys.stdin.read().strip().replace("\"","\"\""))' <<<"$short_err_msg")"
        echo "${i},${ts},,${FORGE_AMOUNT},false,0,0,-1,-1,FAILED,\"${error_msg_csv}\"" >>"$CSV_OUT"
        failed=$((failed+1))
        echo "[$i/$COUNT] failed retry=${attempt}: ${short_err_msg}" >&2
    fi
done

if [[ "$MINE_BLOCKS_AFTER" -gt 0 ]]; then
    miner_addr="$(rpc getnewaddress "convergence-batch-miner" "legacy")"
    rpc generatetoaddress "$MINE_BLOCKS_AFTER" "$miner_addr" >/dev/null
fi

if [[ "$MINE_EACH_ROUND" -eq 0 ]]; then
    python3 - "$CSV_OUT" "$CLI" "$CHAIN" "$DATADIR" "$RPC_USER" "$RPC_PASS" "$RPC_PORT" "$start_height" <<'PY'
import csv
import json
import subprocess
import sys

csv_path, cli, chain, datadir, rpc_user, rpc_pass, rpc_port, start_height = sys.argv[1:]
start_height = int(start_height)

def rpc(method, *params):
    cmd = [cli, f"-chain={chain}"]
    if datadir:
        cmd.append(f"-datadir={datadir}")
    if rpc_user:
        cmd.append(f"-rpcuser={rpc_user}")
    if rpc_pass:
        cmd.append(f"-rpcpassword={rpc_pass}")
    if rpc_port:
        cmd.append(f"-rpcport={rpc_port}")
    cmd.extend([method, *params])
    out = subprocess.check_output(cmd, text=True)
    return out.strip()

rows = []
with open(csv_path, "r", encoding="utf-8", newline="") as f:
    reader = csv.DictReader(f)
    for row in reader:
        txid = row.get("txid", "")
        if txid:
            try:
                tx = json.loads(rpc("gettransaction", txid))
                confs = int(tx.get("confirmations", 0))
                if confs > 0 and tx.get("blockhash"):
                    header = json.loads(rpc("getblockheader", tx["blockhash"]))
                    confirm_height = int(header.get("height", -1))
                    row["confirm_height"] = str(confirm_height)
                    row["blocks_to_confirm"] = str(confirm_height - start_height)
                else:
                    row["confirm_height"] = "-1"
                    row["blocks_to_confirm"] = "-1"
            except Exception:
                row["confirm_height"] = "-1"
                row["blocks_to_confirm"] = "-1"
        rows.append(row)

with open(csv_path, "w", encoding="utf-8", newline="") as f:
    writer = csv.DictWriter(
        f,
        fieldnames=[
            "batch_id",
            "timestamp",
            "txid",
            "convergence_amount",
            "is_in_mempool",
            "mempool_fee",
            "convergence_priority",
            "confirm_height",
            "blocks_to_confirm",
            "status",
            "error_msg",
        ],
    )
    writer.writeheader()
    writer.writerows(rows)
PY
fi

echo
echo "=== Convergence batch summary ==="
echo "submitted=${COUNT}"
echo "broadcast_success=${success}"
echo "broadcast_failed=${failed}"
echo "mined_confirmed=${mined_confirmed}"
echo "end_height=$(rpc getblockcount)"
echo "csv_output=${CSV_OUT}"

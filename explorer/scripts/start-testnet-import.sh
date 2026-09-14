#!/usr/bin/env bash
# Start a testnet full import on :8081 using DB rigor_explorer_testnet.
# Does not touch the regtest explorer on :8080.
# Exits 2 if no testnet node is reachable — that is expected on this WSL today.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ENV_FILE="${EXPLORER_TESTNET_ENV:-$ROOT/.env.testnet}"
CLI="${RIGOR_CLI_BIN:-$ROOT/../core/src/rigor-cli}"
DATADIR="${RIGOR_DATADIR_TESTNET:-${RIGOR_DATADIR:-}}"

if [[ ! -f "$ENV_FILE" ]]; then
  echo "Missing $ENV_FILE — copy explorer/.env.testnet.example to .env.testnet and set RIGOR_DATADIR / RPC auth."
  echo "TESTNET_IMPORT=blocked reason=missing_env"
  exit 2
fi

set -a
# shellcheck disable=SC1090
source "$ENV_FILE"
set +a

CLI="${RIGOR_CLI_BIN:-$CLI}"
if [[ ! -x "$CLI" ]]; then
  echo "rigor-cli not executable: $CLI"
  echo "TESTNET_IMPORT=blocked reason=missing_cli"
  exit 2
fi

info="$("$CLI" -testnet ${RIGOR_DATADIR:+-datadir="$RIGOR_DATADIR"} ${RIGOR_RPC_PORT:+-rpcport="$RIGOR_RPC_PORT"} ${RIGOR_RPC_USER:+-rpcuser="$RIGOR_RPC_USER"} ${RIGOR_RPC_PASSWORD:+-rpcpassword="$RIGOR_RPC_PASSWORD"} getblockchaininfo 2>/dev/null || true)"
chain="$(printf '%s' "$info" | python3 -c 'import json,sys; s=sys.stdin.read().strip();
print(json.loads(s).get("chain","") if s.startswith("{") else "")' 2>/dev/null || true)"

if [[ "$chain" != "test" ]]; then
  echo "No testnet rigord on RPC ${RIGOR_RPC_PORT:-18761} (got chain='${chain:-none}')."
  echo "Start: $CLI -testnet -datadir=<testnet-datadir> -daemon"
  echo "Then: createdb rigor_explorer_testnet && $0"
  echo "TESTNET_IMPORT=blocked reason=no_testnet_node"
  exit 2
fi

if ! psql "${PG_URL}" -c 'SELECT 1' >/dev/null 2>&1; then
  dbname="$(python3 - <<'PY'
import os
from urllib.parse import urlparse
print(urlparse(os.environ["PG_URL"]).path.lstrip("/") or "rigor_explorer_testnet")
PY
)"
  echo "Creating database $dbname"
  createdb "$dbname"
fi

cd "$ROOT"
npm run build
echo "TESTNET_IMPORT=starting chain=test api=${API_HOST:-127.0.0.1}:${API_PORT:-8081}"
exec node dist/index.js

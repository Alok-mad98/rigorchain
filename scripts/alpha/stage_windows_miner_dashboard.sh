#!/usr/bin/env bash
# Copy start-miner-dashboard.cmd + dashboard/ next to rigor-miner.
# Wallet installers must not call this. No consensus changes.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DEST="${1:-}"
if [[ -z "${DEST}" ]]; then
  echo "Usage: $0 <dest-dir>" >&2
  exit 2
fi

DASH_SRC="${ROOT}/mining/windows-miner-dashboard"
LAUNCHER="${ROOT}/installer/windows/start-miner-dashboard.cmd"

if [[ ! -f "${DASH_SRC}/server.py" ]]; then
  echo "ERROR: missing ${DASH_SRC}/server.py" >&2
  exit 1
fi
if [[ ! -f "${LAUNCHER}" ]]; then
  echo "ERROR: missing ${LAUNCHER}" >&2
  exit 1
fi

mkdir -p "${DEST}/dashboard/public"

python3 - "${LAUNCHER}" "${DEST}/start-miner-dashboard.cmd" <<'PY'
import sys
from pathlib import Path
src, dst = Path(sys.argv[1]), Path(sys.argv[2])
text = src.read_text(encoding="utf-8").replace("\r\n", "\n").replace("\n", "\r\n")
dst.write_bytes(text.encode("utf-8"))
PY

cp -a "${DASH_SRC}/server.py" "${DEST}/dashboard/server.py"
cp -a "${DASH_SRC}/aggregator.py" "${DEST}/dashboard/aggregator.py"
cp -a "${DASH_SRC}/start.sh" "${DEST}/dashboard/start.sh"
cp -a "${DASH_SRC}/README.md" "${DEST}/dashboard/README.md"
cp -a "${DASH_SRC}/config.example.env" "${DEST}/dashboard/config.example.env"
cp -a "${DASH_SRC}/public/index.html" "${DEST}/dashboard/public/index.html"
cp -a "${DASH_SRC}/public/dashboard.css" "${DEST}/dashboard/public/dashboard.css"
cp -a "${DASH_SRC}/public/dashboard.js" "${DEST}/dashboard/public/dashboard.js"

echo "staged miner dashboard -> ${DEST}"

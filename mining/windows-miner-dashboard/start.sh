#!/usr/bin/env bash
# Start the RIGOR Miner Dashboard on 127.0.0.1:18083 (read-only).
set -euo pipefail
cd "$(dirname "$0")"
exec python3 server.py "$@"

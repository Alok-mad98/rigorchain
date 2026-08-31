#!/usr/bin/env bash
# P068 freeze gate wrapper (P069 still requires this name).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
exec python3 "$ROOT/tools/run_consensus_kernel_freeze_gate.py"

#!/usr/bin/env bash
# WSL substitute for GitHub Actions (quota exhausted). G2+G3 of EXPLORER_PRODUCTION_GATE.md.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
npm run test:gate
echo "GATE G2+G3 PASS (startup lock + amount accounting + block-list pagination unit tests)"

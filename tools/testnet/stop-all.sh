#!/usr/bin/env bash
set -euo pipefail
# shellcheck source=common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
stop_node node-003 || true
stop_node node-002 || true
stop_node node-001 || true
echo "STOP_ALL=PASS"

#!/usr/bin/env bash
# Back-compat wrapper. Canonical scanner is scripts/security_scan.py.
set -euo pipefail
ROOT="$(git rev-parse --show-toplevel)"
exec python3 "${ROOT}/scripts/security_scan.py" "$@"

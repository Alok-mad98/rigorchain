#!/usr/bin/env bash
# Wrapper when your cwd is ``core/`` (e.g. after ``make``). Delegates to repo-root script.
set -euo pipefail
_CORE_SCRIPTS="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
_RING_ROOT="$(cd "${_CORE_SCRIPTS}/../.." && pwd)"
exec "${_RING_ROOT}/scripts/run_fault_injection_suite.sh" "$@"

#!/usr/bin/env bash
# Poll until mingw depends finishes, then build PE + NSIS installer.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HOST=x86_64-w64-mingw32
SITE="${REPO_ROOT}/core/depends/${HOST}/share/config.site"
PIDFILE="${REPO_ROOT}/.runtime/alpha-local-stack/mingw-depends.pid"
LOG="${REPO_ROOT}/.runtime/alpha-local-stack/mingw-watch.log"

log() { printf '[%s] %s\n' "$(date -u +%H:%M:%S)" "$*" | tee -a "$LOG"; }

log "Waiting for ${SITE}…"
while [[ ! -f "${SITE}" ]]; do
  if [[ -f "${PIDFILE}" ]]; then
    pid="$(cat "${PIDFILE}")"
    if ! ps -p "$pid" >/dev/null 2>&1; then
      log "depends make exited but config.site missing — check mingw-depends.log"
      exit 1
    fi
  fi
  sleep 60
done
log "depends ready — building PE + NSIS"
chmod +x "${REPO_ROOT}/scripts/alpha/"*.sh
bash "${REPO_ROOT}/scripts/alpha/continue_windows_pe_after_depends.sh"
log "Windows alpha package complete"

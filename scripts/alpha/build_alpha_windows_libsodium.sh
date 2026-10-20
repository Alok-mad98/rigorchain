#!/usr/bin/env bash
# Cross-install the pinned static libsodium (tools/release/libsodium.lock) into
# core/depends/<HOST> for builds that skip the depends libsodium package.
# The tarball is SHA-256 verified; there is no unpinned or "-stable" fallback.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HOST="${HOST:-x86_64-w64-mingw32}"
PREFIX="${REPO_ROOT}/core/depends/${HOST}"

exec bash "${REPO_ROOT}/tools/release/fetch-libsodium.sh" --prefix "${PREFIX}" --host "${HOST}"

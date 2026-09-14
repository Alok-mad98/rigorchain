#!/usr/bin/env bash
# Compatibility wrapper. Canonical entry: tools/release/package-official.sh
exec "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/package-official.sh" "$@"

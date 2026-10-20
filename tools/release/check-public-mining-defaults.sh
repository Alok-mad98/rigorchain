#!/usr/bin/env bash
# Fail if a public mining template or Windows startup path forces solomining=1.
# Mentions that tell the operator not to set the flag are allowed.
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
files=(
  installer/windows/start-node.cmd
  installer/windows/rigor-miner.cmd
  installer/windows/fix-miningresfeeaddr.cmd
  tools/release/stage-official-windows.sh
  scripts/alpha/build_alpha_windows_nsis.sh
)
fail=0
for rel in "${files[@]}"; do
  path="$root/$rel"
  if [[ ! -f "$path" ]]; then
    echo "MISSING $rel" >&2
    fail=1
    continue
  fi
  # Ignore lines that tell the operator not to set the flag.
  hits="$(grep -nE '(^|[[:space:]])solomining=1([[:space:]]|$)|-solomining=1' "$path" | grep -viE 'do not|without' || true)"
  if [[ -n "$hits" ]]; then
    echo "FORCED solomining=1 in $rel" >&2
    echo "$hits" >&2
    fail=1
  fi
done
if [[ "$fail" -ne 0 ]]; then
  exit 1
fi
echo "public mining defaults: solomining=1 is not forced"

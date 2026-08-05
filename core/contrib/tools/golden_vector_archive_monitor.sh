#!/usr/bin/env bash
# PROOF-034: Poll regtest height and print percent complete + ETA while materializer mines.
#
# Usage:
#   golden_vector_archive_monitor.sh [--datadir=PATH] [--target-height=N] [--poll-sec=N]
#
# Run in Terminal B while golden_vector_materializer.sh runs in Terminal A.
# Ctrl+C stops the monitor only; mining continues in Terminal A.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CORE_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILT_CLI="${CORE_DIR}/src/rigor-cli"

DATADIR="${DATADIR:-${HOME}/.rigor/golden-vector-archive-regtest}"
TARGET_HEIGHT="${TARGET_HEIGHT:-2100001}"
POLL_SEC="${POLL_SEC:-30}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --datadir=*)
      DATADIR="${1#*=}"
      shift
      ;;
    --datadir)
      DATADIR="$2"
      shift 2
      ;;
    --target-height=*)
      TARGET_HEIGHT="${1#*=}"
      shift
      ;;
    --target-height)
      TARGET_HEIGHT="$2"
      shift 2
      ;;
    --poll-sec=*)
      POLL_SEC="${1#*=}"
      shift
      ;;
    --poll-sec)
      POLL_SEC="$2"
      shift 2
      ;;
    -h|--help)
      sed -n '2,8p' "$0"
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

if [[ -x "${BUILT_CLI}" ]]; then
  RIGOR_CLI="${BUILT_CLI}"
elif command -v rigor-cli >/dev/null 2>&1; then
  RIGOR_CLI="$(command -v rigor-cli)"
else
  echo "rigor-cli not found — build core (make) or set RIGOR_CLI." >&2
  exit 127
fi

export DATADIR TARGET_HEIGHT POLL_SEC RIGOR_CLI
exec python3 - <<'PY'
import os, subprocess, sys, time

datadir = os.environ["DATADIR"]
target = int(os.environ.get("TARGET_HEIGHT", "2100001"))
poll = float(os.environ.get("POLL_SEC", "30"))
cli_base = os.environ["RIGOR_CLI"]
cli = [cli_base, "-regtest", f"-datadir={datadir}", "getblockcount"]
milestones = [
    (630000, "GV-ORU-0003"),
    (2100000, "GV-RES-0003"),
    (2100001, "GV-DRAW-0003 / archive complete"),
]

def get_height():
    out = subprocess.check_output(cli, text=True, stderr=subprocess.STDOUT).strip()
    return int(out)

h_start = None
t_start = None
print(f"PROOF-034 monitor: datadir={datadir}")
print(f"PROOF-034 monitor: target height={target:,}, poll={poll}s")
print("Ctrl+C to stop monitor only (mining continues in Terminal A)\n")

while True:
    try:
        h = get_height()
    except subprocess.CalledProcessError as e:
        print(
            f"[{time.strftime('%H:%M:%S')}] RPC not ready — start rigord first "
            f"(see golden_vector_materializer.sh --start-node): {e.output.strip()}",
            flush=True,
        )
        time.sleep(poll)
        continue
    except FileNotFoundError:
        sys.exit("rigor-cli not found — run from core/ after make")

    now = time.time()
    if h_start is None:
        h_start, t_start = h, now

    done = max(0, h - h_start)
    remain = max(0, target - h)
    pct = min(100.0, 100.0 * h / target)
    elapsed = now - t_start

    if done > 0 and elapsed >= poll:
        rate = done / elapsed
        eta_sec = remain / rate if rate > 0 else float("inf")
        if eta_sec < 3600:
            eta = f"{eta_sec / 60:.1f} min"
        elif eta_sec < 86400:
            eta = f"{eta_sec / 3600:.1f} h"
        else:
            eta = f"{eta_sec / 86400:.1f} d"
    else:
        eta = "warming up…"

    next_ms = next(((ht, label) for ht, label in milestones if h < ht), None)
    ms = f"next: h={next_ms[0]:,} ({next_ms[1]})" if next_ms else "milestones: all reached"

    print(
        f"[{time.strftime('%H:%M:%S')}] "
        f"height={h:,}/{target:,}  {pct:6.2f}%  "
        f"remain={remain:,}  ETA≈{eta}  |  {ms}",
        flush=True,
    )

    if h >= target:
        print(
            f"\n✓ Target height {target:,} reached — "
            "materializer should finish vector setup shortly."
        )
        break

    time.sleep(poll)
PY

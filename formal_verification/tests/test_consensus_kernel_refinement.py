#!/usr/bin/env python3
"""P053 wrapper: delegates to tools/run_consensus_kernel_refinement.py."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
RUNNER = REPO / "tools" / "run_consensus_kernel_refinement.py"


def main() -> int:
    if not RUNNER.is_file():
        print(f"FAIL: missing {RUNNER}", file=sys.stderr)
        return 1
    return subprocess.run([sys.executable, str(RUNNER)], check=False).returncode


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""TASK-016: run the formal model internal-consistency gate."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHECKER = ROOT / "scripts" / "check_formal_model_consistency.py"


def main() -> int:
    r = subprocess.run([sys.executable, str(CHECKER)], cwd=str(ROOT))
    return r.returncode


if __name__ == "__main__":
    sys.exit(main())

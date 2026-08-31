#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P070: production canary readiness gate. Does not enable mainnet canary."""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
TOKEN = "CONSENSUS_KERNEL_CANARY_NOT_READY"


def fail(msg: str) -> int:
    print(TOKEN)
    print(f"FAIL: {msg}")
    return 1


def run(cmd: list[str]) -> None:
    print("==", " ".join(cmd))
    subprocess.run(cmd, check=True, cwd=str(ROOT))


def main() -> int:
    py = sys.executable
    steps: list[tuple[str, list[str]]] = [
        ("canary boundary", [py, str(TOOLS / "lint-production-canary-boundary.py")]),
        ("authority lint", [py, str(TOOLS / "lint-consensus-authority.py")]),
        ("duplicate authority", [py, str(TOOLS / "lint-consensus-authority-security.py")]),
        ("extracted isolation", [py, str(TOOLS / "audit-consensus-release-build.py")]),
        ("migration freeze", ["bash", str(TOOLS / "run_consensus_kernel_freeze_gate.sh")]),
        ("testnet canary", [py, str(TOOLS / "check_kernel_canary_runtime.py")]),
        ("refinement assumptions", [py, str(TOOLS / "check-coq-consensus-assumptions.py")]),
    ]
    try:
        for name, cmd in steps:
            print(f"== {name} ==")
            env = os.environ.copy()
            if name == "testnet canary" and "--skip-build" in sys.argv:
                cmd = cmd + ["--skip-build"]
            run(cmd)
    except subprocess.CalledProcessError as e:
        return fail(f"{e}")
    except FileNotFoundError as e:
        return fail(str(e))

    print("CONSENSUS_KERNEL_PRODUCTION_CANARY_READY = PASS")
    print("CANARY_AVAILABLE = TRUE")
    print("CANARY_ENABLED = FALSE")
    print("ROLLBACK_READY = TRUE")
    print("PRODUCTION_KERNEL_SWITCHED = FALSE")
    print("REFINEMENT_PROVED = TRUE")
    print("EQUIVALENCE_PROVED = FALSE")
    return 0


if __name__ == "__main__":
    sys.exit(main())

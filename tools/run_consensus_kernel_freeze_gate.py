#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P068: current C++ authority vs migration candidate vs refinement pins."""

from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from refinement_status import print_status, records_cpp_authority  # noqa: E402
SRC = ROOT / "core/src"
TOOLS = ROOT / "tools"
FV = ROOT / "formal_verification"
VECTORS = FV / "consensus_kernel_freeze_vectors.json"
RELEASE = FV / "release_consensus_vectors.json"
FROZEN = FV / "frozen_consensus_vectors.json"
MANIFEST = FV / "consensus_kernel_freeze_manifest.json"


def run(cmd: list[str], cwd: Path | None = None) -> None:
    print("==", " ".join(cmd))
    subprocess.run(cmd, check=True, cwd=cwd)


def compile_freeze_test(cxx: str, out: Path) -> None:
    cmd = [
        cxx,
        "-O2",
        "-std=c++17",
        "-ffunction-sections",
        "-fdata-sections",
        "-DCONSENSUS_KERNEL_MIGRATION_VERIFY_ONLY",
        f"-I{SRC}",
        "-o",
        str(out),
        str(SRC / "test/test_kernel_migration_freeze.cpp"),
        str(SRC / "consensus/kernel_migration/kernel_migration.cpp"),
        str(SRC / "consensus/ledger/ledger_transition.cpp"),
        str(SRC / "consensus/rigor_issuance.cpp"),
        str(SRC / "consensus/rigor_supply.cpp"),
        str(TOOLS / "consensus_kernel_refinement_log_stub.cpp"),
        str(SRC / "script/rigor_classify.cpp"),
        str(SRC / "script/script.cpp"),
        str(SRC / "uint256.cpp"),
        str(SRC / "util/strencodings.cpp"),
        str(SRC / "crypto/sha256.cpp"),
        str(SRC / "arith_uint256.cpp"),
        "-Wl,--gc-sections",
    ]
    run(cmd)


def align_refinement_pins() -> None:
    freeze = json.loads(VECTORS.read_text(encoding="utf-8"))
    release = json.loads(RELEASE.read_text(encoding="utf-8"))
    frozen = json.loads(FROZEN.read_text(encoding="utf-8"))
    if freeze.get("equivalence_proved") is not False or not records_cpp_authority(freeze):
        raise SystemExit("CONSENSUS_KERNEL_FREEZE_DRIFT")
    if freeze["genesis"].get("res_destroy_cost", freeze["genesis"].get("GetResMintReward")) not in (102400000000, 40000000):
        if freeze["genesis"].get("res_destroy_cost") != 102400000000:
            raise SystemExit("CONSENSUS_KERNEL_FREEZE_DRIFT")
    if freeze["genesis"]["res_issued_at_height"] != 0:
        raise SystemExit("CONSENSUS_KERNEL_FREEZE_DRIFT")
    if frozen["genesis"].get("res_destroy_cost") != 102400000000:
        raise SystemExit("CONSENSUS_KERNEL_FREEZE_DRIFT")
    if release["genesis"]["res_actual"] != 0 or frozen["genesis"]["res_actual"] != 0:
        raise SystemExit("CONSENSUS_KERNEL_FREEZE_DRIFT")

    by_h = {row["height"]: row for row in freeze.get("epoch") or []}
    for src in (release.get("epoch") or [], frozen.get("epoch") or []):
        for row in src:
            pinned = by_h.get(row["height"])
            if not pinned:
                continue
            for key in ("oru", "epoch"):
                if pinned.get(key) != row.get(key):
                    print(f"CONSENSUS_KERNEL_FREEZE_DRIFT field=epoch.{row['height']}.{key}", file=sys.stderr)
                    raise SystemExit(1)
            # RFC-019: frozen_consensus_vectors.json pins RATE(h). The older
            # freeze snapshot still records W(h). Do not cross-compare them.
    print("REFINEMENT_VECTOR_ALIGN=PASS")


def main() -> int:
    man = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if not records_cpp_authority(man):
        print("CONSENSUS_KERNEL_FREEZE_DRIFT")
        return 1

    run([sys.executable, str(TOOLS / "lint-consensus-kernel-freeze.py")])
    align_refinement_pins()

    cxx = os.environ.get("CXX") or "g++"
    with tempfile.TemporaryDirectory(prefix="rigor_freeze_") as td:
        out = Path(td) / "test_kernel_migration_freeze"
        print("== current cpp authority vs migration candidate ==")
        compile_freeze_test(cxx, out)
        r = subprocess.run([str(out)], capture_output=True, text=True)
        sys.stdout.write(r.stdout)
        if r.returncode != 0:
            sys.stderr.write(r.stderr)
            print("CONSENSUS_KERNEL_FREEZE_DRIFT")
            return 1
        if "CONSENSUS_KERNEL_FREEZE_PASS" not in r.stdout:
            print("CONSENSUS_KERNEL_FREEZE_DRIFT")
            return 1

    print("CONSENSUS_KERNEL_FREEZE_PASS")
    print("CURRENT_CPP_AUTHORITY = PASS")
    print("MIGRATION_PATH = PASS")
    print_status()
    return 0


if __name__ == "__main__":
    sys.exit(main())

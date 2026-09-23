#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P069: runtime canary monitor. Default production logs are absent (OFF)."""

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
LOGDIR = ROOT / "logs/consensus_kernel_canary"
STATUS = LOGDIR / "runtime_status.json"
PERF_BUDGET_US = 50_000.0


def fail(msg: str) -> int:
    print("CONSENSUS_KERNEL_CANARY_RUNTIME_DISCREPANCY")
    print(f"FAIL: {msg}")
    return 1


def compile_and_run_stream() -> None:
    cxx = os.environ.get("CXX") or "g++"
    LOGDIR.mkdir(parents=True, exist_ok=True)
    for stale in ("session.jsonl", "discrepancy.log", "runtime_status.json"):
        p = LOGDIR / stale
        if p.is_file():
            p.unlink()
    with tempfile.TemporaryDirectory(prefix="rigor_tn_canary_") as td:
        src = Path(td) / "run.cpp"
        src.write_text(
            """#include <test/kernel_runtime_canary.h>
#include <cstdio>
#include <string>
int main() {
    RigorKernelRuntimeCanaryResetStats();
    RigorKernelRuntimeCanarySetLogDir("logs/consensus_kernel_canary");
    std::string err;
    if (!RigorKernelRuntimeCanaryRunBlockStream(10000, err)) {
        std::fprintf(stderr, "%s\\n", err.c_str());
        return 1;
    }
    std::printf("BLOCKS_VALIDATED=%d\\n", RigorKernelRuntimeCanaryBlocksValidated());
    std::printf("DISCREPANCY_COUNT=%d\\n", RigorKernelRuntimeCanaryDiscrepancyCount());
    std::printf("CRASH_COUNT=%d\\n", RigorKernelRuntimeCanaryCrashCount());
    return 0;
}
""",
            encoding="utf-8",
        )
        out = Path(td) / "run_canary_stream"
        cmd = [
            cxx,
            "-O2",
            "-std=c++17",
            "-ffunction-sections",
            "-fdata-sections",
            "-DRIGOR_KERNEL_RUNTIME_CANARY_STANDALONE",
            "-DCONSENSUS_KERNEL_MIGRATION_VERIFY_ONLY",
            f"-I{SRC}",
            "-o",
            str(out),
            str(src),
            str(SRC / "test/kernel_runtime_canary.cpp"),
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
        print("==", " ".join(cmd))
        subprocess.run(cmd, check=True, cwd=str(ROOT))
        r = subprocess.run([str(out)], cwd=str(ROOT), check=True, capture_output=True, text=True)
        sys.stdout.write(r.stdout)
        if r.stderr:
            sys.stderr.write(r.stderr)


def main() -> int:
    if "--skip-build" not in sys.argv:
        try:
            compile_and_run_stream()
        except subprocess.CalledProcessError as e:
            return fail(f"canary stream failed: {e}")

    if not STATUS.is_file():
        return fail("missing logs/consensus_kernel_canary/runtime_status.json")
    st = json.loads(STATUS.read_text(encoding="utf-8"))
    if int(st.get("discrepancy_count", 1)) != 0:
        return fail("discrepancy_count != 0")
    if int(st.get("crash_count", 1)) != 0:
        return fail("crash_count != 0")
    if int(st.get("reject_divergence", 1)) != 0:
        return fail("reject_divergence != 0")
    blocks = int(st.get("blocks_validated", 0))
    if blocks < 10000:
        return fail(f"blocks_validated={blocks} < 10000")
    max_us = float(st.get("max_block_micros", 0))
    if max_us > PERF_BUDGET_US:
        return fail(f"performance regression max_block_micros={max_us}")
    if not records_cpp_authority(st):
        return fail("runtime status must record C++ production authority")
    print("CONSENSUS_KERNEL_TESTNET_CANARY = CLOSED/PASS")
    print("NETWORK = REGTEST/TESTNET")
    print(f"BLOCKS_VALIDATED = {blocks}")
    print("DISCREPANCY_COUNT = 0")
    print("CRASH_COUNT = 0")
    print_status()
    return 0


if __name__ == "__main__":
    sys.exit(main())

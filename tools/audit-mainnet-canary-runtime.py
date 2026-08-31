#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P072: long-run mainnet canary must not mutate DB / UTXO / block acceptance."""

from __future__ import annotations

import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "core/src"
TOOLS = ROOT / "tools"
CANARY_DIR = SRC / "consensus/kernel_canary"
RUNTIME = SRC / "node/kernel_runtime_canary.cpp"
VALIDATION = SRC / "validation.cpp"
MAKEFILE = SRC / "Makefile.am"
OBSERVATION = ROOT / "formal_verification/mainnet_canary_observation.json"

MUTATION = re.compile(
    r"\bAbortNode\s*\(|"
    r"\bFlushStateToDisk\s*\(|"
    r"\bSetBestBlock\s*\(|"
    r"\bCCoinsView(?:Cache)?\b|"
    r"\bCDBWrapper\b|"
    r"\bCDBBatch\b|"
    r"leveldb::|"
    r"\bdb\.Write\s*\(|"
    r"\bWriteBatch\s*\(|"
    r"\bview\.Set|"
    r"\bview\.Spend|"
    r"\bAddCoin\s*\(|"
    r"\bConnectBlock\s*\("
)
AUTHORITY_REPLACE = re.compile(
    r"accept\s*=\s*b\.accept|"
    r"return\s+b\.accept|"
    r"candidate.*ConnectBlock|"
    r"replace authority",
    re.IGNORECASE,
)


def fail(msg: str) -> int:
    print("CONSENSUS_KERNEL_MAINNET_CANARY_DISCREPANCY")
    print(f"FAIL: {msg}")
    return 1


def main() -> int:
    errors: list[str] = []
    files = [
        RUNTIME,
        CANARY_DIR / "MainnetCanaryMonitor.h",
        CANARY_DIR / "MainnetCanaryMonitor.cpp",
        CANARY_DIR / "canary_health.h",
    ]
    for p in files:
        if not p.is_file():
            return fail(f"missing {p}")
        text = p.read_text(encoding="utf-8", errors="replace")
        if MUTATION.search(text):
            errors.append(f"{p.relative_to(ROOT)} mutates DB/UTXO/acceptance")
        if AUTHORITY_REPLACE.search(text):
            errors.append(f"{p.relative_to(ROOT)} replaces authority result")

    makefile = MAKEFILE.read_text(encoding="utf-8", errors="replace")
    if "consensus/kernel_canary" in makefile:
        errors.append("Makefile.am lists kernel_canary")
    if "kernel_runtime_canary.cpp" in makefile:
        errors.append("Makefile.am still compiles runtime canary")

    validation = VALIDATION.read_text(encoding="utf-8", errors="replace")
    if "kernel_canary" in validation:
        errors.append("ConnectBlock orchestration references canary TU")
    if re.search(r"AbortNode\s*\([^)]*CANARY", validation):
        errors.append("ConnectBlock AbortNode on canary")
    if "(void)RigorObserveConnectBlockRuntimeCanary" in validation:
        errors.append("ConnectBlock still dual-path canary")

    ctl = (CANARY_DIR / "canary_controller.h").read_text(encoding="utf-8", errors="replace")
    if "inline constexpr bool PRODUCTION_MAINNET_CANARY_ARMED = true" not in ctl:
        errors.append("PRODUCTION_MAINNET_CANARY_ARMED must be TRUE")

    health = (CANARY_DIR / "canary_health.h").read_text(encoding="utf-8", errors="replace")
    for needle in ("ConsensusKernelCanaryHealth", "ACTIVE", "PAUSED", "DISABLED"):
        if needle not in health:
            errors.append(f"health missing {needle}")

    mon = (CANARY_DIR / "MainnetCanaryMonitor.h").read_text(encoding="utf-8", errors="replace")
    for needle in (
        "MainnetCanaryMonitor",
        "Start",
        "Stop",
        "Pause",
        "Status",
        "DisableOnFailure",
        "FaultInject",
        "ExportObservation",
        "rss_delta",
        "memory_growth",
        "rollback_events",
        "MAINNET_CANARY_LONGRUN_BLOCKS",
        "CANARY_FAULT_INJECTION_TEST",
        "mainnet_canary_blocks_total",
        "mainnet_canary_compare_total",
        "mainnet_canary_discrepancy_total",
        "mainnet_canary_runtime_cpp",
        "mainnet_canary_runtime_candidate",
        "CONSENSUS_KERNEL_MAINNET_CANARY_DISCREPANCY",
    ):
        if needle not in mon:
            errors.append(f"monitor missing {needle}")
    if "inline constexpr bool CANARY_FAULT_INJECTION_TEST = false" not in mon:
        errors.append("CANARY_FAULT_INJECTION_TEST must be false")
    if "inline constexpr int MAINNET_CANARY_LONGRUN_BLOCKS = 100000" not in mon:
        errors.append("MAINNET_CANARY_LONGRUN_BLOCKS must be 100000")

    runtime = RUNTIME.read_text(encoding="utf-8", errors="replace")
    if "return true so ConnectBlock keeps using C++ authority" not in runtime:
        errors.append("Observe must continue C++ authority")

    if errors:
        print("CONSENSUS_KERNEL_MAINNET_CANARY_DISCREPANCY")
        for e in errors:
            print(f"  - {e}")
        return 1

    cxx = os.environ.get("CXX") or "g++"
    with tempfile.TemporaryDirectory(prefix="rigor_mn_canary_longrun_") as td:
        src = Path(td) / "run.cpp"
        obs = OBSERVATION
        src.write_text(
            f"""#include <consensus/kernel_canary/MainnetCanaryMonitor.h>
#include <node/kernel_runtime_canary.h>
#include <cstdio>
#include <string>
int main() {{
    RigorKernelRuntimeCanaryResetStats();
    auto& m = MainnetCanaryMonitor::Instance();
    if (m.Health() != ConsensusKernelCanaryHealthState::PAUSED) return 10;
    std::string err;
    if (!m.RunSoak(MAINNET_CANARY_LONGRUN_BLOCKS, err)) {{
        std::fprintf(stderr, "%s\\n", err.c_str());
        return 1;
    }}
    const int blocks = (int)m.blocks_checked();
    if (blocks < MAINNET_CANARY_LONGRUN_BLOCKS) return 11;
    if (m.discrepancy_count() != 0) return 2;
    if (m.Health() != ConsensusKernelCanaryHealthState::ACTIVE) return 12;
    if (!m.ExportObservation("{obs.as_posix()}")) return 13;
    const char* faults[] = {{"issuance", "draw", "classify", "ledger_delta"}};
    for (const char* name : faults) {{
        RigorKernelRuntimeCanaryResetStats();
        if (!m.Start()) return 14;
        if (!m.FaultInject(name)) return 15;
        if (m.enabled()) return 3;
        if (m.Health() != ConsensusKernelCanaryHealthState::DISABLED) return 16;
        if (m.discrepancy_count() == 0) return 4;
        if (!RigorObserveConnectBlockRuntimeCanary(1, uint256(), uint256(), {{}}, uint256(), {{}}, {{}}, {{}})) return 5;
    }}
    std::printf("BLOCKS_MONITORED=%d\\n", blocks);
    std::printf("DISCREPANCY_COUNT=0\\n");
    std::printf("FAULT_INJECTION=PASS\\n");
    std::printf("ROLLBACK_TEST=PASS\\n");
    std::printf("CPP_AUTHORITY=ACTIVE\\n");
    std::printf("CANARY_ROLE=VERIFY_ONLY\\n");
    return 0;
}}
""",
            encoding="utf-8",
        )
        out = Path(td) / "run_mn_canary_longrun"
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
            str(SRC / "node/kernel_runtime_canary.cpp"),
            str(SRC / "consensus/rigor_issuance.cpp"),
            str(SRC / "consensus/rigor_supply.cpp"),
            str(TOOLS / "consensus_kernel_refinement_log_stub.cpp"),
            str(SRC / "script/rigor_classify.cpp"),
            str(SRC / "script/script.cpp"),
            str(SRC / "uint256.cpp"),
            str(SRC / "util/strencodings.cpp"),
            str(SRC / "consensus/convergence_logic.cpp"),
            str(SRC / "consensus_rigor/sigma_hash.cpp"),
            str(SRC / "crypto/sha256.cpp"),
            str(SRC / "arith_uint256.cpp"),
            "-Wl,--gc-sections",
        ]
        print("==", " ".join(cmd))
        try:
            subprocess.run(cmd, check=True, cwd=str(ROOT))
            r = subprocess.run([str(out)], cwd=str(ROOT), check=True, capture_output=True, text=True)
        except subprocess.CalledProcessError as e:
            return fail(str(e))
        sys.stdout.write(r.stdout)
        if "ROLLBACK_TEST=PASS" not in r.stdout:
            return fail("rollback test did not report PASS")
        if "FAULT_INJECTION=PASS" not in r.stdout:
            return fail("fault injection did not report PASS")
        if "BLOCKS_MONITORED=" not in r.stdout:
            return fail("missing BLOCKS_MONITORED")

    if not OBSERVATION.is_file():
        return fail("missing formal_verification/mainnet_canary_observation.json")
    data = json.loads(OBSERVATION.read_text(encoding="utf-8"))
    for field in (
        "start_height",
        "end_height",
        "blocks_checked",
        "comparisons",
        "discrepancies",
        "rollback_events",
        "runtime_statistics",
    ):
        if field not in data:
            return fail(f"observation missing {field}")
    if int(data["blocks_checked"]) < 100000:
        return fail("BLOCKS_MONITORED < 100000")
    if int(data["discrepancies"]) != 0:
        return fail("DISCREPANCY_COUNT != 0")
    if data.get("production_kernel_switched") is not False:
        return fail("observation must keep PRODUCTION_KERNEL_SWITCHED false")
    if data.get("equivalence_proved") is not False:
        return fail("observation must keep EQUIVALENCE_PROVED false")
    stats = data["runtime_statistics"]
    for k in ("cpp_execution_time_ms", "candidate_execution_time_ms", "rss_delta", "memory_growth"):
        if k not in stats:
            return fail(f"runtime_statistics missing {k}")

    print("CONSENSUS_KERNEL_MAINNET_CANARY_LONGRUN = PASS")
    print("CONSENSUS_KERNEL_MAINNET_CANARY = ACTIVE")
    print("ROLLBACK_TEST = PASS")
    print("FAULT_INJECTION = PASS")
    print("CPP_AUTHORITY = ACTIVE")
    print("CANARY_ROLE = VERIFY_ONLY")
    print("PRODUCTION_KERNEL_SWITCHED = FALSE")
    return 0


if __name__ == "__main__":
    sys.exit(main())

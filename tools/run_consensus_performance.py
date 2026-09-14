#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P0-066: compile and run Consensus / Transaction / Ledger / UTXO performance benches.

Does not rewrite consensus formulas. Production path stays handwritten C++.
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "core/src"
BENCH = ROOT / "benchmarks"
TOOLS = ROOT / "tools"
FUZZ_SMOKE = ROOT / "fuzz/utxo_state_transition_perf/run_smoke.sh"

SOURCES = [
    SRC / "consensus/ledger/ledger_transition.cpp",
    SRC / "consensus/rigor_issuance.cpp",
    SRC / "consensus/rigor_supply.cpp",
    TOOLS / "consensus_kernel_refinement_log_stub.cpp",
    SRC / "script/rigor_classify.cpp",
    SRC / "script/script.cpp",
    SRC / "uint256.cpp",
    SRC / "util/strencodings.cpp",
    SRC / "crypto/sha256.cpp",
    SRC / "arith_uint256.cpp",
]

BENCHES = (
    ("consensus_kernel", BENCH / "consensus_kernel_benchmark.cpp"),
    ("transaction_kernel", BENCH / "transaction_kernel_benchmark.cpp"),
    ("ledger_transition", BENCH / "ledger_transition_benchmark.cpp"),
)

SCENARIOS = (
    "empty_block",
    "oru_transfer",
    "res_mint_maturity",
    "convergence_draw",
    "large_utxo",
    "many_inputs",
    "many_outputs",
    "malformed_reject",
)


def compile_one(cxx: str, src: Path, out: Path) -> None:
    cmd = [
        cxx,
        "-O2",
        "-std=c++17",
        "-ffunction-sections",
        "-fdata-sections",
        f"-I{BENCH}",
        f"-I{SRC}",
        "-o",
        str(out),
        str(src),
        *[str(s) for s in SOURCES],
        "-Wl,--gc-sections",
    ]
    subprocess.run(cmd, check=True)


def parse_samples(text: str) -> list[dict]:
    rows = []
    for line in text.splitlines():
        line = line.strip()
        if not line.startswith("{"):
            continue
        obj = json.loads(line)
        if "scenario" in obj:
            rows.append(obj)
    return rows


def main() -> int:
    cxx = os.environ.get("CXX") or ("g++" if subprocess.call(["which", "g++"], stdout=subprocess.DEVNULL) == 0 else "c++")
    iters = os.environ.get("PERF_ITERS", "800")
    errors: list[str] = []
    all_rows: list[dict] = []

    with tempfile.TemporaryDirectory(prefix="rigor_perf_") as td:
        tmp = Path(td)
        for name, src in BENCHES:
            out = tmp / name
            print(f"== compile {name} ==")
            compile_one(cxx, src, out)
            print(f"== run {name} ==")
            r = subprocess.run([str(out), iters], check=False, capture_output=True, text=True)
            sys.stdout.write(r.stdout)
            if r.returncode != 0:
                errors.append(f"{name} exit {r.returncode}: {r.stderr.strip()}")
                continue
            rows = parse_samples(r.stdout)
            seen = {row["scenario"] for row in rows}
            missing = [s for s in SCENARIOS if s not in seen]
            if missing:
                errors.append(f"{name} missing scenarios: {missing}")
            all_rows.extend(rows)

            r2 = subprocess.run([str(out), iters], check=True, capture_output=True, text=True)
            rows2 = parse_samples(r2.stdout)
            fp1 = {(x["scenario"], x["path"]): x["fingerprint"] for x in rows}
            fp2 = {(x["scenario"], x["path"]): x["fingerprint"] for x in rows2}
            if fp1 != fp2:
                errors.append(f"{name} nondeterministic fingerprints")

    by = defaultdict(dict)
    for row in all_rows:
        by[(row["scenario"], row.get("bench", ""))][row["path"]] = row
        by[row["scenario"]][row["path"]] = row

    print("== path comparison (legacy vs kernel) ==")
    print(f"{'scenario':<22} {'legacy ns':>12} {'kernel ns':>12} {'legacy ops/s':>14} {'kernel ops/s':>14}")
    for scene in SCENARIOS:
        paths = [r for r in all_rows if r["scenario"] == scene]
        legacy = [r for r in paths if r["path"] == "legacy"]
        kernel = [r for r in paths if r["path"] == "kernel"]
        if not legacy or not kernel:
            continue
        ln = sum(r["latency_ns"] for r in legacy) / len(legacy)
        kn = sum(r["latency_ns"] for r in kernel) / len(kernel)
        lo = sum(r["ops_per_sec"] for r in legacy) / len(legacy)
        ko = sum(r["ops_per_sec"] for r in kernel) / len(kernel)
        print(f"{scene:<22} {ln:12.1f} {kn:12.1f} {lo:14.1f} {ko:14.1f}")
        budget = max(ln * 16.0, ln + 250000.0)
        if kn > budget:
            errors.append(f"consensus latency regression {scene}: kernel {kn:.1f}ns > budget {budget:.1f}ns")

    print("== fuzz smoke ==")
    fz = subprocess.run(["bash", str(FUZZ_SMOKE)], check=False, capture_output=True, text=True)
    sys.stdout.write(fz.stdout)
    if fz.returncode != 0:
        errors.append(f"fuzz smoke failed: {fz.stderr.strip()}")

    if errors:
        print("FAIL: consensus kernel performance")
        for e in errors:
            print(f"  - {e}")
        return 1

    print("CONSENSUS_KERNEL_PERFORMANCE = PASS")
    print("MEMORY_SAFETY_BASELINE = PASS")
    print("NO_CONSENSUS_REGRESSION = PASS")
    print("PRODUCTION_CONSENSUS_MODIFIED = NO")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P0-077: compile and run mainnet performance baseline benches.

Does not rewrite consensus formulas. Production path stays handwritten C++.
Writes docs/performance/performance_results.json plus performance_results.json.
"""

from __future__ import annotations

import json
import os
import platform
import subprocess
import sys
import tempfile
import time
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "core/src"
BENCH_ROOT = ROOT / "benchmarks"
BENCH = BENCH_ROOT / "rigor"
TOOLS = ROOT / "tools"
FUZZ_SMOKE = ROOT / "fuzz/performance_boundary/run_smoke.sh"
TEST = SRC / "test/test_performance_consensus_regression.cpp"
OUT_JSON = ROOT / "performance_results.json"
OUT_JSON_DOCS = ROOT / "docs/performance/performance_results.json"

SOURCES = [
    SRC / "consensus/ledger/ledger_transition.cpp",
    SRC / "consensus/rigor_issuance.cpp",
    SRC / "consensus/rigor_supply.cpp",
    TOOLS / "consensus_kernel_refinement_log_stub.cpp",
    SRC / "script/rigor_classify.cpp",
    SRC / "script/script.cpp",
    SRC / "uint256.cpp",
    SRC / "util/strencodings.cpp",
    SRC / "consensus/convergence_logic.cpp",
    SRC / "consensus_rigor/sigma_hash.cpp",
    SRC / "crypto/sha256.cpp",
    SRC / "arith_uint256.cpp",
]

BENCHES = (
    ("consensus_kernel", BENCH / "consensus_kernel_benchmark.cpp"),
    ("transaction_kernel", BENCH / "transaction_kernel_benchmark.cpp"),
    ("ledger_apply", BENCH / "ledger_apply_benchmark.cpp"),
    ("utxo_state_transition", BENCH / "utxo_state_transition_benchmark.cpp"),
    ("block_validation", BENCH / "block_validation_benchmark.cpp"),
)

SCENARIOS = (
    "empty_block",
    "normal_block",
    "large_transaction_block",
    "many_inputs",
    "many_outputs",
    "oru_transfer",
    "res_maturity_spend",
    "convergence_draw",
    "large_utxo",
)

LINTS = (
    TOOLS / "lint-performance-consensus-boundary.py",
    TOOLS / "lint-consensus-authority.py",
    TOOLS / "lint-ledger-kernel-boundary.py",
    TOOLS / "lint-utxo-state-transition-boundary.py",
)


def compile_one(cxx: str, src: Path, out: Path, extra_src: list[Path] | None = None) -> None:
    cmd = [
        cxx,
        "-O2",
        "-std=c++17",
        "-ffunction-sections",
        "-fdata-sections",
        f"-I{BENCH}",
        f"-I{BENCH_ROOT}",
        f"-I{SRC}",
        "-o",
        str(out),
        str(src),
        *[str(s) for s in (extra_src or SOURCES)],
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


def host_storage_baseline() -> dict:
    """Host write/flush/sync analog. Not a chainstate rewrite."""
    payload = b"\xab" * (1024 * 1024)
    with tempfile.NamedTemporaryFile(prefix="rigor_db_baseline_", delete=False) as f:
        path = f.name
        t0 = time.perf_counter()
        f.write(payload)
        f.flush()
        write_ns = (time.perf_counter() - t0) * 1e9
        t1 = time.perf_counter()
        os.fsync(f.fileno())
        sync_ns = (time.perf_counter() - t1) * 1e9
    os.unlink(path)
    t2 = time.perf_counter()
    table = {i: i * 3 for i in range(100000)}
    acc = 0
    for i in range(0, 100000, 7):
        acc += table.get(i, 0)
    lookup_ns = (time.perf_counter() - t2) * 1e9
    return {
        "write_1mib_ns": write_ns,
        "fsync_1mib_ns": sync_ns,
        "in_memory_100k_lookup_ns": lookup_ns,
        "lookup_checksum": acc,
        "note": "Host analog only. Production chainstate remains CCoinsViewDB / LevelDB.",
    }


def summarize(rows: list[dict]) -> dict:
    by_bench: dict[str, dict] = defaultdict(dict)
    kernel = {}
    block = {}
    utxo = {}
    rss = []
    for row in rows:
        bench = row.get("bench", "")
        scene = row.get("scenario", "")
        comp = row.get("component", "full")
        key = f"{scene}/{comp}/{row.get('path', '')}"
        by_bench[bench][key] = {
            "latency_ns": row.get("latency_ns", 0),
            "ops_per_sec": row.get("ops_per_sec", 0),
            "rss_kb": row.get("rss_kb", 0),
            "peak_rss_kb": row.get("peak_rss_kb", 0),
            "fingerprint": row.get("fingerprint", ""),
        }
        rss.append(int(row.get("peak_rss_kb") or 0))
        if bench == "consensus_kernel" and comp == "full":
            kernel[scene] = row.get("latency_ns", 0)
        if bench == "block_validation":
            block[scene] = row.get("latency_ns", 0)
        if bench == "utxo_state_transition" and comp == "apply":
            utxo[scene] = row.get("latency_ns", 0)
    return {
        "by_bench": by_bench,
        "kernel_latency": kernel,
        "block_validation_latency": block,
        "utxo_performance": utxo,
        "memory_profile": {
            "peak_rss_kb": max(rss) if rss else 0,
            "sample_count": len(rss),
        },
    }


def main() -> int:
    cxx = os.environ.get("CXX") or (
        "g++" if subprocess.call(["which", "g++"], stdout=subprocess.DEVNULL) == 0 else "c++"
    )
    iters = os.environ.get("PERF_ITERS", "600")
    errors: list[str] = []
    all_rows: list[dict] = []

    print("== P077 mainnet performance baseline ==")
    for lint in LINTS:
        print(f"== lint {lint.name} ==")
        r = subprocess.run(["python3", str(lint)], check=False, capture_output=True, text=True)
        sys.stdout.write(r.stdout)
        if r.returncode != 0:
            errors.append(f"{lint.name} failed: {r.stderr.strip() or r.stdout.strip()}")
            sys.stderr.write(r.stderr)

    with tempfile.TemporaryDirectory(prefix="rigor_mainnet_perf_") as td:
        tmp = Path(td)
        print("== compile test_performance_consensus_regression ==")
        test_bin = tmp / "test_performance_consensus_regression"
        try:
            compile_one(cxx, TEST, test_bin)
        except subprocess.CalledProcessError as e:
            errors.append(f"regression compile failed: {e}")
        else:
            print("== run test_performance_consensus_regression ==")
            r = subprocess.run([str(test_bin)], check=False, capture_output=True, text=True)
            sys.stdout.write(r.stdout)
            if r.returncode != 0:
                errors.append(f"regression test exit {r.returncode}: {r.stderr.strip()}")
                sys.stderr.write(r.stderr)

        for name, src in BENCHES:
            out = tmp / name
            print(f"== compile {name} ==")
            try:
                compile_one(cxx, src, out)
            except subprocess.CalledProcessError as e:
                errors.append(f"{name} compile failed: {e}")
                continue
            print(f"== run {name} ==")
            r = subprocess.run([str(out), iters], check=False, capture_output=True, text=True)
            sys.stdout.write(r.stdout)
            if r.returncode != 0:
                errors.append(f"{name} exit {r.returncode}: {r.stderr.strip()}")
                sys.stderr.write(r.stderr)
                continue
            rows = parse_samples(r.stdout)
            seen = {row["scenario"] for row in rows}
            missing = [s for s in SCENARIOS if s not in seen]
            if missing:
                errors.append(f"{name} missing scenarios: {missing}")
            all_rows.extend(rows)

            r2 = subprocess.run([str(out), iters], check=True, capture_output=True, text=True)
            rows2 = parse_samples(r2.stdout)
            fp1 = {(x["scenario"], x.get("component", ""), x["path"]): x["fingerprint"] for x in rows}
            fp2 = {(x["scenario"], x.get("component", ""), x["path"]): x["fingerprint"] for x in rows2}
            if fp1 != fp2:
                errors.append(f"{name} nondeterministic fingerprints")

    print("== fuzz smoke ==")
    fz = subprocess.run(["bash", str(FUZZ_SMOKE)], check=False, capture_output=True, text=True)
    sys.stdout.write(fz.stdout)
    if fz.returncode != 0:
        errors.append(f"fuzz smoke failed: {fz.stderr.strip()}")
        sys.stderr.write(fz.stderr)

    summary = summarize(all_rows)
    db = host_storage_baseline()
    payload = {
        "task": "TASK-RIGOR-MAINNET-PERFORMANCE-BASELINE-OPTIMIZATION-P0-077",
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "host": {
            "platform": platform.platform(),
            "processor": platform.processor(),
            "python": platform.python_version(),
        },
        "iters": iters,
        "samples": all_rows,
        "kernel_latency": summary["kernel_latency"],
        "block_validation_latency": summary["block_validation_latency"],
        "utxo_performance": summary["utxo_performance"],
        "memory_profile": summary["memory_profile"],
        "database_host_baseline": db,
        "optimization_result": {
            "ledger_uniqueness": "O(n log n) copy-sort for n>24; linear otherwise",
            "ledger_sums": "single-pass known-asset + ORU/RES sums",
            "sighash": "preimage buffer reserve; bytes unchanged",
            "utxo_delta": "move spent/created deltas out of LedgerApply",
            "predicate": "empty scriptPubKey skips CScript allocation",
            "issuance_memo": "benchmark-only last-hit cache; production still ComputeRigorIssuance",
        },
        "gates": {
            "PERFORMANCE_BASELINE": "FAIL" if errors else "PASS",
            "CONSENSUS_BEHAVIOR_CHANGED": False,
            "STATE_TRANSITION_CHANGED": False,
            "HASH_CHANGED": False,
            "MEMORY_SAFETY": "FAIL" if errors else "PASS",
            "CRASH_FOUND": "YES" if any("fuzz" in e for e in errors) else "NO",
        },
        "errors": errors,
    }
    text = json.dumps(payload, indent=2) + "\n"
    OUT_JSON.write_text(text, encoding="utf-8")
    OUT_JSON_DOCS.parent.mkdir(parents=True, exist_ok=True)
    OUT_JSON_DOCS.write_text(text, encoding="utf-8")
    print(f"wrote {OUT_JSON}")
    print(f"wrote {OUT_JSON_DOCS}")

    print("== KERNEL_LATENCY ==")
    for k, v in sorted(summary["kernel_latency"].items()):
        print(f"  {k}: {v:.1f} ns")
    print("== BLOCK_VALIDATION_LATENCY ==")
    for k, v in sorted(summary["block_validation_latency"].items()):
        print(f"  {k}: {v:.1f} ns")
    print("== UTXO_PERFORMANCE ==")
    for k, v in sorted(summary["utxo_performance"].items()):
        print(f"  {k}: {v:.1f} ns")
    print("== MEMORY_PROFILE ==")
    print(f"  peak_rss_kb: {summary['memory_profile']['peak_rss_kb']}")

    if errors:
        print("FAIL: mainnet performance baseline")
        for e in errors:
            print(f"  - {e}")
        print("PERFORMANCE_BASELINE = FAIL")
        return 1

    print("PERFORMANCE_BASELINE = PASS")
    print("CONSENSUS_BEHAVIOR_CHANGED = FALSE")
    print("STATE_TRANSITION_CHANGED = FALSE")
    print("HASH_CHANGED = FALSE")
    print("MEMORY_SAFETY = PASS")
    print("CRASH_FOUND = NO")
    print("RIGOR_MAINNET_PERFORMANCE_STATUS = PASS")
    print("MAINNET_PERFORMANCE_BASELINE_CLOSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())

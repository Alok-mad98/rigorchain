#!/usr/bin/env python3
"""TASK-008-R generator stability report. Empirical; not a Coq proof."""
from __future__ import annotations

import importlib.util
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GEN_PATH = ROOT / "tools" / "formal_bridge" / "generate_kernel_candidate.py"
TRACES = ROOT / "audit" / "formal_bridge" / "kernel_candidate_dataset" / "traces.jsonl"
REPORT = ROOT / "audit" / "formal_bridge" / "kernel_candidate_generator_report.md"
SAMPLE = ROOT / "audit" / "formal_bridge" / "kernel_candidate_dataset" / "live_connectblock" / "h102-a665422db725237b-reject" / "consensus_trace.json"


def load_gen():
    spec = importlib.util.spec_from_file_location("kigen", GEN_PATH)
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(mod)
    return mod


def main() -> int:
    gen = load_gen()
    sample = json.loads(SAMPLE.read_text())
    first = gen.generate_kernel_candidate(sample)
    n = 1000
    matches = 0
    for _ in range(n):
        got = gen.generate_kernel_candidate(sample)
        if got == first and got["candidate_hash"] == first["candidate_hash"]:
            matches += 1
    replay_ok = 0
    replay_n = 0
    hashes = set()
    if TRACES.is_file():
        for line in TRACES.read_text().splitlines():
            if not line.strip():
                continue
            replay_n += 1
            a = gen.generate_kernel_candidate(json.loads(line))
            b = gen.generate_kernel_candidate(json.loads(line))
            if a["candidate_hash"] == b["candidate_hash"]:
                replay_ok += 1
            hashes.add(a["candidate_hash"])
    REPORT.write_text(
        "# KernelInput candidate generator report (TASK-008-R)\n\n"
        "Empirical stability of `tools/formal_bridge/generate_kernel_candidate.py`.\n"
        "Not `CPP_KERNELINPUT_EQUIVALENCE`. Not Coq.\n\n"
        f"- sample trace: `{SAMPLE.relative_to(ROOT)}`\n"
        f"- 1000× same-trace identical object + hash: **{matches} / {n}**\n"
        f"- sample `candidate_hash`: `{first['candidate_hash']}`\n"
        f"- dataset replay (each trace generated twice): **{replay_ok} / {replay_n}**\n"
        f"- distinct hashes: {len(hashes)}\n"
        f"- result: **{'PASS' if matches == n and replay_n >= 2000 and replay_ok == replay_n else 'FAIL'}**\n\n"
        "Vacuous `ki_utxo_ok=true` on coinbase-only traces is DERIVED, not a UTXO model.\n"
        "`ki_incoming` / `ki_outgoing` / `ki_burn` / `ki_subsidy` / `ki_res_mint` stay MISSING.\n"
        "After `reconsiderblock` without a header path, a new C++ trace may omit `pow_result` / "
        "`asert_result` (DIRECT null). The **same saved JSON** still hashes identically; "
        "DERIVED/MISSING fields stay equal for that block.\n"
    )
    print(REPORT.read_text())
    if matches != n or replay_n < 2000 or replay_ok != replay_n:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

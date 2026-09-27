#!/usr/bin/env python3
"""Quality gate for the kernel candidate generator.

Not a refinement proof. Does not compile Coq. Does not change consensus.
"""
from __future__ import annotations

import importlib.util
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GEN_PATH = ROOT / "tools" / "formal_bridge" / "generate_kernel_candidate.py"
TRACES = ROOT / "audit" / "formal_bridge" / "kernel_candidate_dataset" / "traces.jsonl"
SPEC = ROOT / "formal_bridge" / "kernel_candidate_generator.md"
REPORT = ROOT / "audit" / "formal_bridge" / "kernel_candidate_generator_report.md"
FREEZE = ROOT / "audit" / "17_kernel_candidate_bridge_frozen.md"
PRIORITY = ROOT / "audit" / "formal_bridge" / "kernel_missing_object_priority.md"

KI_FIELDS = {
    "ki_utxo_ok",
    "ki_incoming",
    "ki_outgoing",
    "ki_fees",
    "ki_burn",
    "ki_subsidy",
    "ki_res_mint",
    "ki_script_ok",
    "ki_pow_ok",
    "ki_asert_ok",
}
ALLOWED_CLASS = {"DIRECT", "DERIVED", "MISSING"}
META = {"status", "note", "field_class", "candidate_hash"}
FORBIDDEN = (
    r"\bFORGE\b",
    r"\bMTH\b",
    r"script-as-asset",
    r"script_as_asset",
    r"dual-asset model",
    r"RNG-as-protocol",
)
errors: list[str] = []


def err(msg: str) -> None:
    errors.append(msg)


def load_gen():
    spec = importlib.util.spec_from_file_location("kigen", GEN_PATH)
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(mod)
    return mod


def check_files() -> None:
    for p in (GEN_PATH, SPEC, REPORT, FREEZE, PRIORITY, TRACES):
        if not p.is_file():
            err(f"missing {p.relative_to(ROOT)}")
    if FREEZE.is_file():
        text = FREEZE.read_text()
        if "CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED" not in text:
            err("audit/17 must state CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED")
        if "BRIDGE_GENERATOR_STABILITY = PROVED" not in text:
            err("audit/17 must state BRIDGE_GENERATOR_STABILITY = PROVED")
    if REPORT.is_file() and "PASS" not in REPORT.read_text():
        err("generator report is not PASS")


def check_code_forbidden() -> None:
    for p in (GEN_PATH, ROOT / "tools" / "formal_bridge" / "pack_traces.py"):
        if p.is_file():
            text = p.read_text()
            for pat in FORBIDDEN:
                if re.search(pat, text):
                    err(f"{p.relative_to(ROOT)} cites removed architecture /{pat}/")


def check_traces(gen) -> None:
    if not TRACES.is_file():
        return
    n = 0
    for i, line in enumerate(TRACES.read_text().splitlines(), 1):
        if not line.strip():
            continue
        n += 1
        try:
            trace = json.loads(line)
        except json.JSONDecodeError as exc:
            err(f"traces.jsonl:{i}: {exc}")
            continue
        a = gen.generate_kernel_candidate(trace)
        b = gen.generate_kernel_candidate(trace)
        if a != b or a.get("candidate_hash") != b.get("candidate_hash"):
            err(f"traces.jsonl:{i}: non-deterministic generator")
        unknown = set(a) - KI_FIELDS - META
        if unknown:
            err(f"traces.jsonl:{i}: unknown field {sorted(unknown)}")
        fc = a.get("field_class") or {}
        for k in KI_FIELDS:
            if k not in a:
                err(f"traces.jsonl:{i}: missing {k}")
            cls = fc.get(k)
            if cls not in ALLOWED_CLASS:
                err(f"traces.jsonl:{i}: {k} has no DIRECT/DERIVED/MISSING class")
            if cls == "MISSING" and a.get(k) is not None:
                err(f"traces.jsonl:{i}: MISSING {k} is not null")
        payload = {k: a[k] for k in sorted(KI_FIELDS)}
        digest = gen.candidate_hash(payload)
        if digest != a.get("candidate_hash"):
            err(f"traces.jsonl:{i}: candidate_hash mismatch")
        blob = json.dumps(a)
        for pat in FORBIDDEN:
            if re.search(pat, blob):
                err(f"traces.jsonl:{i}: forbidden architecture name")
    if n < 2000:
        err(f"traces.jsonl has {n} rows; need ≥2000")


def main() -> int:
    check_files()
    check_code_forbidden()
    if GEN_PATH.is_file():
        gen = load_gen()
        check_traces(gen)
    if errors:
        print("FAIL")
        for e in errors:
            print(" -", e)
        return 1
    print("PASS kernel candidate bridge")
    print("CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED")
    print("BRIDGE_GENERATOR_STABILITY = PROVED")
    return 0


if __name__ == "__main__":
    sys.exit(main())

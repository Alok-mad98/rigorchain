#!/usr/bin/env python3
""" collector quality gate. No node. Not a Coq proof."""
from __future__ import annotations

import importlib.util
import json
import re
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DESIGN = ROOT / "audit" / "formal_bridge" / "evidence_collector_design.md"
API = ROOT / "tools" / "formal_bridge" / "evidence_collector.py"
TESTS = ROOT / "tests" / "formal_bridge" / "test_evidence_collector.py"
BASE = "29893c7"

CONSENSUS_PATHS = (
    "core/src/validation.cpp",
    "core/src/consensus/kernel/genesis_authority.cpp",
    "core/src/primitives/block.h",
    "core/src/primitives/transaction.h",
    "core/src/serialize.h",
)
KERNEL_PATHS = ("formal_verification/rigor_kernel/Kernel/UnifiedTransition.v",)
COQ_GLOBS = ["formal_verification/**/*.v"]
FORBIDDEN = (r"\bFORGE\b", r"\bMTH\b", r"\bRNG\b", r"script-as-asset", r"dual-asset")
errors: list[str] = []


def err(msg: str) -> None:
    errors.append(msg)


def load_api():
    spec = importlib.util.spec_from_file_location("evidence_collector", API)
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(mod)
    return mod


def git_changed(paths: list[str]) -> bool:
    try:
        out = subprocess.check_output(
            ["git", "diff", "--name-only", BASE, "--", *paths],
            cwd=ROOT,
            text=True,
        )
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False
    names = [ln.strip() for ln in out.splitlines() if ln.strip()]
    # also unstaged vs HEAD for the same paths
    try:
        out2 = subprocess.check_output(
            ["git", "diff", "--name-only", "HEAD", "--", *paths],
            cwd=ROOT,
            text=True,
        )
        names.extend(ln.strip() for ln in out2.splitlines() if ln.strip())
    except subprocess.CalledProcessError:
        pass
    return bool(names)


def main() -> int:
    for p in (DESIGN, API, TESTS):
        if not p.is_file():
            err(f"missing {p.relative_to(ROOT)}")

    if DESIGN.is_file():
        text = DESIGN.read_text()
        for needle in (
            "schema_version",
            "evidence_id",
            "rpc",
            "consensus_trace",
            "test_harness",
            "node_observer",
            "validation_soundness",
            "connect_disconnect_inverse",
            "network assumption",
        ):
            if needle not in text:
                err(f"design missing {needle}")
        for pat in FORBIDDEN:
            if re.search(pat, text):
                err(f"design contains /{pat}/")
        if re.search(r"^CPP_KERNELINPUT_EQUIVALENCE = TRUE\s*$", text, re.M):
            err("design sets equivalence TRUE")

    consensus_changed = git_changed(list(CONSENSUS_PATHS))
    kernel_changed = git_changed(list(KERNEL_PATHS))
    coq_files = [str(p.relative_to(ROOT)) for p in ROOT.glob("formal_verification/**/*.v")]
    # Limit to kernel + extraction-ish trees the TASK forbids editing.
    coq_watch = [
        p
        for p in coq_files
        if p.startswith("formal_verification/rigor_kernel/") or "extract" in p.lower()
    ]
    coq_changed = git_changed(coq_watch) if coq_watch else False

    if API.is_file():
        text = API.read_text()
        for fn in (
            "create_block_evidence",
            "create_state_transition_evidence",
            "create_network_observation",
            "validate_evidence_record",
            "normalize_evidence",
        ):
            if f"def {fn}(" not in text:
                err(f"API missing {fn}")
        if "ConnectBlock(" in text or "EvaluateConnect(" in text:
            err("collector must not call ConnectBlock/EvaluateConnect")
        mod = load_api()
        rec = mod.create_block_evidence(
            block_hash="00",
            height=0,
            prev_hash=None,
            nBits=None,
            header_time=None,
            tx_count=0,
            accept=True,
            source="rpc",
            collected_at="2026-09-07T00:00:00Z",
        )
        for k in ("schema_version", "evidence_id", "timestamp", "source"):
            if k not in rec:
                err(f"envelope missing {k}")
        if rec.get("schema_version") != "v1":
            err("schema_version must be v1")
        again = mod.create_block_evidence(
            block_hash="00",
            height=0,
            prev_hash=None,
            nBits=None,
            header_time=None,
            tx_count=0,
            accept=True,
            source="rpc",
            collected_at="2099-01-01T00:00:00Z",
        )
        if rec["evidence_id"] != again["evidence_id"]:
            err("same observation produced different evidence_id")
        for e in mod.validate_evidence_record(rec):
            err(e)

        sample = (
            ROOT
            / "audit"
            / "formal_bridge"
            / "kernel_candidate_dataset"
            / "live_connectblock"
            / "h1-dc73e2e40d8ae06b-accept"
            / "consensus_trace.json"
        )
        if sample.is_file():
            tr = json.loads(sample.read_text())
            x = mod.create_block_evidence_from_trace(tr, collected_at="a")
            y = mod.create_block_evidence_from_trace(tr, collected_at="b")
            if x["evidence_id"] != y["evidence_id"]:
                err("same trace produced different evidence_id")
            for e in mod.validate_evidence_record(x):
                err(f"trace: {e}")

    if TESTS.is_file():
        suite = unittest.defaultTestLoader.discover(
            str(TESTS.parent),
            pattern="test_evidence_collector.py",
        )
        result = unittest.TextTestRunner(verbosity=1).run(suite)
        if not result.wasSuccessful():
            err(f"unit tests failed failures={len(result.failures)} errors={len(result.errors)}")

    status = "FAIL" if errors else "PASS"
    print(f"EVIDENCE_COLLECTOR_STATUS = {status}")
    print(f"CONSENSUS_CHANGED = {'YES' if consensus_changed else 'NO'}")
    print(f"KERNELINPUT_CHANGED = {'YES' if kernel_changed else 'NO'}")
    print(f"COQ_CHANGED = {'YES' if coq_changed else 'NO'}")
    if errors:
        for e in errors:
            print(" -", e)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

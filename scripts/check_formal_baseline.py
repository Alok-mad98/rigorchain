#!/usr/bin/env python3
"""formal baseline gate. Not a Coq proof. Not equivalence."""
from __future__ import annotations

import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BRIDGE = ROOT / "audit" / "formal_bridge"

BASELINE = BRIDGE / "formal_baseline_v1.md"
INVENTORY = BRIDGE / "dataset_inventory_v1.json"
CATALOG = BRIDGE / "counterexample_catalog_v1.md"
BOUNDARY = BRIDGE / "kernel_input_boundary_v1.md"
DECISION = BRIDGE / "formal_entry_decision_v1.md"
WF = ROOT / ".github" / "workflows" / "formal_baseline.yml"

KERNEL = ROOT / "formal_verification" / "rigor_kernel" / "Kernel" / "UnifiedTransition.v"
MAKE_KI = ROOT / "formal_verification" / "coq" / "MakeKernelInput.v"
EXTRACT = ROOT / "formal_verification" / "coq" / "ExtractMakeKernelInput.v"
VALIDATION = ROOT / "core" / "src" / "validation.cpp"
GENESIS = ROOT / "core" / "src" / "consensus" / "kernel" / "genesis_authority.cpp"
BLOCK_H = ROOT / "core" / "src" / "primitives" / "block.h"
TX_H = ROOT / "core" / "src" / "primitives" / "transaction.h"

FROZEN_SHA256 = {
    KERNEL: "5a6fd11e066a9f1cc0fdc887dcb018d1af1990e0c9ff950ab91b38aa53d5f444",
    MAKE_KI: "f2ea793e9706b3c2ab88c8eebca21df909bbc56e65d7199eab62602a423570d8",
    EXTRACT: "8ed4cb82d5017d61d639e0216a4dd50ce22640e39c18eeb7a697c0c2e968a317",
    VALIDATION: "3cf90c64860a3ac07bb869c114084a6763590d7cf58a9ec228bfe57649b66226",
    GENESIS: "2a7f7dae3b582c9345473e07e8e1328fedc9d8f491708981b3ecc7b49d3c4851",
    BLOCK_H: "6d756e2f29b68282cbd2a3eb17d9ff36b5339bfa5b803f7ca5256c41f2bbe509",
    TX_H: "b0862373d3dd760f27f26e1df9e9871b84b6255148390a3f8151970ade23380a",
}

REQUIRED_CE = [f"CE-{i:03d}" for i in range(1, 14)]
CE_SECTIONS = (
    "Input",
    "Observed C++ decision",
    "Reject layer",
    "Kernel candidate impact",
    "Future theorem target",
)
EVENT_FIELDS = (
    "event_id",
    "source",
    "scenario",
    "validation_layer",
    "kernel_candidate_hash",
    "future_theorem_candidate",
)
STATE_OPS = {"connect", "disconnect", "restart", "reorg"}
STATE_THEOREMS = {
    "connect_disconnect_inverse",
    "state_transition_correctness",
    "chain_selection",
}
SER_THEOREMS = {"canonical_decode", "object_identity"}

SCAN_DOCS = (BASELINE, CATALOG, BOUNDARY, DECISION)
FORBIDDEN_ARCH = (
    r"\bFORGE\b",
    r"\bMTH\b",
    r"\bRNG\b",
    r"script-as-asset",
    r"dual asset",
    r"dual-asset",
)

errors: list[str] = []


def err(msg: str) -> None:
    errors.append(msg)


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    h.update(path.read_bytes())
    return h.hexdigest()


def check_files() -> None:
    for p in (
        BASELINE,
        INVENTORY,
        CATALOG,
        BOUNDARY,
        DECISION,
        WF,
        KERNEL,
        MAKE_KI,
        EXTRACT,
        VALIDATION,
        GENESIS,
        BLOCK_H,
        TX_H,
        BRIDGE / "kernel_candidate_dataset" / "traces.jsonl",
        BRIDGE / "local_trial_dataset" / "index.json",
        ROOT / "tests" / "attack_vectors" / "traces" / "CE-001.json",
        ROOT / "tests" / "serialization_vectors" / "cases",
    ):
        if p.is_file() or p.is_dir():
            continue
        err(f"missing {p.relative_to(ROOT)}")


def check_claims() -> None:
    blob = "\n".join(p.read_text() for p in SCAN_DOCS if p.is_file())
    if "CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED" not in blob:
        err("docs must state CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED")
    if re.search(r"CPP_KERNELINPUT_EQUIVALENCE\s*=\s*(TRUE|PROVED)", blob):
        err("docs must not claim CPP_KERNELINPUT_EQUIVALENCE proved")
    if "KERNELINPUT_STATUS = ORACLE_ONLY" not in blob:
        err("docs must state KERNELINPUT_STATUS = ORACLE_ONLY")
    if "CPP_REFINEMENT = NOT_STARTED" not in blob:
        err("docs must state CPP_REFINEMENT = NOT_STARTED")
    if re.search(r"KernelInputCandidate\s*=\s*KernelInput", blob):
        err("docs must not equate KernelInputCandidate with KernelInput")
    lowered = blob.lower()
    if "evidence is a proof" in lowered or "evidence = proof" in lowered:
        err("docs must not claim evidence is a proof")
    for p in SCAN_DOCS:
        if not p.is_file():
            continue
        text = p.read_text()
        for pat in FORBIDDEN_ARCH:
            if re.search(pat, text):
                err(f"{p.name}: forbidden architecture token {pat}")


def check_baseline_diagram() -> None:
    text = BASELINE.read_text()
    for token in (
        "rigord",
        "AppInitMain",
        "ProcessNewBlock",
        "ConnectBlock",
        "EvaluateConnect",
        "LevelDB UTXO",
        "no runtime bridge",
        "FORMAL_BASELINE_STATUS = PASS",
        "DATASET_STATUS = FROZEN",
        "COUNTEREXAMPLE_STATUS = COMPLETE",
    ):
        if token not in text:
            err(f"formal_baseline_v1.md missing {token!r}")


def check_protected_hashes() -> None:
    for path, expected in FROZEN_SHA256.items():
        if not path.is_file():
            continue
        got = sha256_file(path)
        if got != expected:
            err(
                f"protected file changed: {path.relative_to(ROOT)} "
                f"(got {got}, freeze {expected})"
            )


def check_git_clean_protected() -> None:
    rels = [str(p.relative_to(ROOT)) for p in FROZEN_SHA256]
    try:
        out = subprocess.check_output(
            ["git", "diff", "--name-only", "HEAD", "--", *rels],
            cwd=ROOT,
            text=True,
        )
    except (subprocess.CalledProcessError, FileNotFoundError):
        return
    dirty = [line for line in out.splitlines() if line.strip()]
    if dirty:
        err("consensus/KernelInput dirty vs HEAD: " + ", ".join(dirty))


def check_kernelinput_record() -> None:
    text = KERNEL.read_text()
    if "Record KernelInput" not in text:
        err("UnifiedTransition.v missing Record KernelInput")
    for field in (
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
    ):
        if field not in text:
            err(f"KernelInput field missing: {field}")


def check_counterexamples() -> None:
    text = CATALOG.read_text()
    for ce in REQUIRED_CE:
        if f"## {ce}" not in text:
            err(f"catalog missing heading {ce}")
        block = text.split(f"## {ce}", 1)[-1]
        nxt = re.search(r"\n## CE-", block)
        chunk = block[: nxt.start()] if nxt else block
        for sec in CE_SECTIONS:
            if sec not in chunk:
                err(f"{ce} missing section {sec!r}")
        if "Future theorem target" in chunk and "`" not in chunk:
            err(f"{ce} future theorem target not named")
        src = BRIDGE / "counterexamples" / f"{ce}.json"
        if not src.is_file():
            err(f"missing {src.relative_to(ROOT)}")
        av = ROOT / "tests" / "attack_vectors" / "traces" / f"{ce}.json"
        if not av.is_file():
            err(f"missing {av.relative_to(ROOT)}")


def _check_event(ev: dict, where: str) -> None:
    for k in EVENT_FIELDS:
        if k not in ev:
            err(f"{where} event missing {k}: {ev.get('event_id')}")


def check_inventory() -> None:
    if not INVENTORY.is_file():
        return
    try:
        data = json.loads(INVENTORY.read_text())
    except json.JSONDecodeError as exc:
        err(f"inventory JSON invalid: {exc}")
        return
    if data.get("status") != "FROZEN":
        err("inventory status must be FROZEN")
    if "NOT PROVED" not in str(data.get("note", "")):
        err("inventory must record CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED")

    cons = data.get("consensus_dataset") or {}
    st = data.get("state_dataset") or {}
    ser = data.get("serialization_dataset") or {}
    if not cons.get("events"):
        err("consensus_dataset.events empty")
    if not st.get("events"):
        err("state_dataset.events empty")
    if not ser.get("events"):
        err("serialization_dataset.events empty")

    ce_ids = set()
    for ev in cons.get("events", []):
        _check_event(ev, "consensus")
        eid = ev.get("event_id")
        if isinstance(eid, str) and eid.startswith("CE-"):
            ce_ids.add(eid)
        src = ev.get("source") or ""
        if src not in {
            "kernel_candidate_dataset",
            "local_trial_dataset",
            "attack_vectors",
        }:
            err(f"consensus source not in freeze set: {src} ({eid})")
    for ce in REQUIRED_CE:
        if ce not in ce_ids:
            err(f"inventory missing consensus {ce}")

    seen_ops: set[str] = set()
    for ev in st.get("events", []):
        _check_event(ev, "state")
        op = ev.get("operation") or ev.get("scenario")
        if op in STATE_OPS:
            seen_ops.add(op)
        th = ev.get("future_theorem_candidate")
        if th not in STATE_THEOREMS:
            err(f"state event bad future theorem: {ev.get('event_id')} -> {th}")
    if seen_ops != STATE_OPS:
        err(f"state_dataset missing operations {STATE_OPS - seen_ops}")

    future_map = st.get("future_theorems") or {}
    for k, v in (
        ("connect", "connect_disconnect_inverse"),
        ("disconnect", "connect_disconnect_inverse"),
        ("restart", "state_transition_correctness"),
        ("reorg", "chain_selection"),
    ):
        if future_map.get(k) != v:
            err(f"state future_theorems[{k}] must be {v}")

    ser_ce = set()
    has_canon = False
    has_ident = False
    for ev in ser.get("events", []):
        _check_event(ev, "serialization")
        eid = ev.get("event_id")
        if isinstance(eid, str) and eid.startswith("CE-"):
            ser_ce.add(eid)
        th = ev.get("future_theorem_candidate")
        if th not in SER_THEOREMS:
            err(f"serialization bad future theorem: {eid} -> {th}")
        if th == "canonical_decode":
            has_canon = True
        if th == "object_identity":
            has_ident = True
    for ce in REQUIRED_CE:
        if ce not in ser_ce:
            err(f"serialization dataset missing {ce}")
    if not has_canon or not has_ident:
        err("serialization dataset must map canonical_decode and object_identity")

    sources = (cons.get("sources") or []) if isinstance(cons.get("sources"), list) else []
    joined = " ".join(str(s) for s in sources)
    for token in ("kernel_candidate_dataset", "local_trial_dataset", "attack_vectors"):
        if token not in joined:
            err(f"consensus sources missing {token}")

    cases_dir = ROOT / "tests" / "serialization_vectors" / "cases"
    n_sv = len(list(cases_dir.glob("SV-*.json"))) if cases_dir.is_dir() else 0
    if n_sv < 20:
        err(f"serialization_vectors too small: {n_sv}")


def check_boundary_and_decision() -> None:
    b = BOUNDARY.read_text()
    for token in (
        "KernelInputCandidate",
        "trace-derived observation",
        "UTXOState",
        "Transaction model",
        "Script model",
        "Amount arithmetic model",
        "Serialization model",
        "kernel_step",
    ):
        if token not in b:
            err(f"kernel_input_boundary_v1.md missing {token!r}")
    d = DECISION.read_text()
    for token in (
        "1.3 Differential Testing",
        "2.2 Byte Serialization",
        "2.4 ASERT Error Bound",
        "2.3 Reorg Safety",
        "C++ refinement",
        "CompCert",
        "CertiCoq",
        "Not ready for",
        "Ready for",
    ):
        if token not in d:
            err(f"formal_entry_decision_v1.md missing {token!r}")


def check_workflow() -> None:
    if not WF.is_file():
        return
    text = WF.read_text()
    if "check_formal_baseline.py" not in text:
        err("workflow must run check_formal_baseline.py")
    if "coqc" in text or "make kernel" in text.lower():
        err("formal_baseline.yml must not build Coq")


def main() -> int:
    check_files()
    check_claims()
    check_baseline_diagram()
    check_protected_hashes()
    check_git_clean_protected()
    check_kernelinput_record()
    check_counterexamples()
    check_inventory()
    check_boundary_and_decision()
    check_workflow()

    if errors:
        for e in errors:
            print(f"FAIL: {e}", file=sys.stderr)
        print("FORMAL_BASELINE_STATUS = FAIL")
        return 1

    print("FORMAL_BASELINE_STATUS = PASS")
    print("DATASET_STATUS = FROZEN")
    print("COUNTEREXAMPLE_STATUS = COMPLETE")
    print("KERNELINPUT_STATUS = ORACLE_ONLY")
    print("CPP_REFINEMENT = NOT_STARTED")
    return 0


if __name__ == "__main__":
    sys.exit(main())

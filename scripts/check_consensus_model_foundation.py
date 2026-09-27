#!/usr/bin/env python3
"""consensus input model foundation gate.

Not a Coq proof. Not a ConnectBlock change. Not KernelInput marshalling.
"""
from __future__ import annotations

import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BRIDGE = ROOT / "audit" / "formal_bridge"

OBJECT = BRIDGE / "consensus_object_model_v1.md"
TYPES = BRIDGE / "formal_consensus_types_v1.md"
PIPELINE = BRIDGE / "validation_pipeline_mapping_v1.md"
UTXO = BRIDGE / "utxo_model_gap_v1.md"
REG = BRIDGE / "formal_model_dataset_registry_v2.json"
CE_REG = BRIDGE / "counterexample_registry_v2.md"
FREEZE = ROOT / "audit" / "22_consensus_model_foundation_frozen.md"
WF = ROOT / ".github" / "workflows" / "consensus_model_foundation.yml"

KERNEL = ROOT / "formal_verification" / "rigor_kernel" / "Kernel" / "UnifiedTransition.v"
BLOCK_H = ROOT / "core" / "src" / "primitives" / "block.h"
TX_H = ROOT / "core" / "src" / "primitives" / "transaction.h"
SER_H = ROOT / "core" / "src" / "serialize.h"
CORE_READ = ROOT / "core" / "src" / "core_read.cpp"
VALIDATION = ROOT / "core" / "src" / "validation.cpp"

FROZEN_SHA256 = {
    KERNEL: "5a6fd11e066a9f1cc0fdc887dcb018d1af1990e0c9ff950ab91b38aa53d5f444",
    BLOCK_H: "6d756e2f29b68282cbd2a3eb17d9ff36b5339bfa5b803f7ca5256c41f2bbe509",
    TX_H: "b0862373d3dd760f27f26e1df9e9871b84b6255148390a3f8151970ade23380a",
    SER_H: "58d1e74a3e267ab1be20401fee8bb50a60514257d5274b7da87d474bf4fc4744",
    CORE_READ: "c09d50b73c6bb3a8d9507875fda474ad742f855c2c702c5e67b7751f64d4bb16",
    VALIDATION: "3cf90c64860a3ac07bb869c114084a6763590d7cf58a9ec228bfe57649b66226",
}

OBJECTS = (
    "BlockBytes",
    "DecodedBlock",
    "BlockHeader",
    "Transaction",
    "TxInput",
    "TxOutput",
    "Witness",
    "UTXOState",
    "ValidationResult",
    "KernelInput",
)
OBJECT_STATUS = (
    "C++ IMPLEMENTED",
    "MODEL TARGET",
    "MODEL MISSING",
    "EXISTING MODEL",
)
TYPE_FIELDS = {
    "Block": ("header", "transactions"),
    "Transaction": ("version", "inputs", "outputs", "locktime", "witness"),
    "Input": ("previous_outpoint", "script_data", "sequence"),
    "Output": ("amount", "script_pubkey"),
    "UTXO": ("outpoint", "output", "height"),
}
PIPELINE_STEPS = (
    "DecodedBlock",
    "CheckTransaction",
    "CheckTxInputs",
    "VerifyScript/Witness",
    "CheckProofOfWork",
    "ASERT",
    "ConnectBlock",
    "KernelInput",
)
PIPELINE_TAGS = ("C++ IMPLEMENTED", "MODEL TARGET", "PROOF FUTURE")
UTXO_NOW = ("CCoinsView", "Coin", "Undo", "LevelDB")
UTXO_FUTURE = ("UTXOState", "lookup", "insert", "remove", "restore")
UTXO_THEOREMS = (
    "input_lookup_sound",
    "spend_removes_utxo",
    "connect_disconnect_inverse",
)
CE_IDS = [f"CE-{i:03d}" for i in range(1, 14)]
SE_IDS = [f"SE-{i:03d}" for i in range(1, 6)]
CE_FIELDS = ("Input", "Observed C++ result", "Missing formal object", "Future theorem")
REG_FIELDS = ("id", "object", "source", "future_theorem")
REG_OBJECTS = {"block", "tx", "input", "output", "utxo"}
REG_SOURCES = {"runtime", "serialization", "attack"}
TASK_PREFIXES = ("/", "/", "/", "/")
SCAN = (OBJECT, TYPES, PIPELINE, UTXO, CE_REG, FREEZE)
FORBIDDEN = (
    r"\bFORGE\b",
    r"\bMTH\b",
    r"\bRNG\b",
    r"script-as-asset",
    r"dual-asset",
)

errors: list[str] = []


def err(msg: str) -> None:
    errors.append(msg)


def sha256_file(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check_files() -> None:
    for p in (OBJECT, TYPES, PIPELINE, UTXO, REG, CE_REG, FREEZE, WF, KERNEL):
        if not p.exists():
            err(f"missing {p.relative_to(ROOT)}")
    for path in BRIDGE.glob("**/*.v"):
        err(f"must not add Coq under formal_bridge: {path.relative_to(ROOT)}")


def check_claims() -> None:
    blob = "\n".join(p.read_text() for p in SCAN if p.is_file())
    for token in (
        "CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED",
        "KernelInputCandidate != KernelInput",
        "KernelCandidate = runtime observation",
        "CPP_REFINEMENT = NOT_STARTED",
        "KERNELINPUT_CHANGED = NO",
    ):
        if token not in blob:
            err(f"docs missing {token!r}")
    if re.search(r"CPP_KERNELINPUT_EQUIVALENCE\s*=\s*(TRUE|PROVED)", blob):
        err("must not claim CPP_KERNELINPUT_EQUIVALENCE proved")
    if re.search(r"KernelInputCandidate\s*=\s*KernelInput", blob):
        err("must not equate candidate with KernelInput")
    if re.search(r"KERNELINPUT_CHANGED\s*=\s*YES", blob):
        err("must not claim KernelInput changed")
    for p in SCAN:
        if not p.is_file():
            continue
        text = p.read_text()
        for pat in FORBIDDEN:
            if re.search(pat, text):
                err(f"{p.name}: forbidden token {pat}")
    types_text = TYPES.read_text() if TYPES.is_file() else ""
    if re.search(r"\b(Qed|Proof\.|Theorem )\b", types_text):
        err("formal_consensus_types_v1.md must not contain proofs")


def check_object_boundary() -> None:
    if not OBJECT.is_file():
        return
    text = OBJECT.read_text()
    if "OBJECT_BOUNDARY = COMPLETE" not in text:
        err("object model missing OBJECT_BOUNDARY = COMPLETE")
    for name in OBJECTS:
        if name not in text:
            err(f"object model missing {name}")
    for tag in OBJECT_STATUS:
        if tag not in text:
            err(f"object model missing {tag}")
    if "Bitcoin wire" not in text:
        err("object model missing Bitcoin wire origin")
    if "consensus validation" not in text.lower():
        err("object model missing consensus validation origin")
    if "future refinement" not in text.lower():
        err("object model missing future refinement origin")


def check_types() -> None:
    if not TYPES.is_file():
        return
    text = TYPES.read_text()
    for rec, fields in TYPE_FIELDS.items():
        if rec not in text:
            err(f"types spec missing {rec}")
        for field in fields:
            if field not in text:
                err(f"types spec {rec} missing field {field}")


def check_pipeline() -> None:
    if not PIPELINE.is_file():
        return
    text = PIPELINE.read_text()
    if "PIPELINE_MAPPING = COMPLETE" not in text:
        err("pipeline missing PIPELINE_MAPPING = COMPLETE")
    for step in PIPELINE_STEPS:
        if step not in text:
            err(f"pipeline missing {step}")
    for tag in PIPELINE_TAGS:
        if tag not in text:
            err(f"pipeline missing {tag}")


def check_utxo_gap() -> None:
    if not UTXO.is_file():
        return
    text = UTXO.read_text()
    if "UTXO_MODEL_GAP = IDENTIFIED" not in text:
        err("utxo gap missing IDENTIFIED")
    for name in UTXO_NOW + UTXO_FUTURE + UTXO_THEOREMS:
        if name not in text:
            err(f"utxo gap missing {name}")
    if "utxo_wf_flag" not in text:
        err("utxo gap must record current Coq flag shape")


def check_counterexamples() -> None:
    if not CE_REG.is_file():
        return
    text = CE_REG.read_text()
    for cid in CE_IDS + SE_IDS:
        if f"## {cid}" not in text:
            err(f"counterexample registry missing {cid}")
            continue
        block = text.split(f"## {cid}", 1)[-1]
        nxt = re.search(r"\n## (?:CE|SE)-", block)
        chunk = block[: nxt.start()] if nxt else block
        for sec in CE_FIELDS:
            if sec not in chunk:
                err(f"{cid} missing {sec}")
        if cid.startswith("CE-"):
            src = BRIDGE / "counterexamples" / f"{cid}.json"
            if not src.is_file():
                err(f"missing {src.relative_to(ROOT)}")
        else:
            src = BRIDGE / "serialization_counterexamples_v1" / f"{cid}.json"
            if not src.is_file():
                err(f"missing {src.relative_to(ROOT)}")
    if re.search(r"^## CE-014\b", text, re.M) or re.search(r"^## SE-006\b", text, re.M):
        err("must not add new CE/SE in this TASK")


def check_registry() -> None:
    if not REG.is_file():
        return
    try:
        data = json.loads(REG.read_text())
    except json.JSONDecodeError as exc:
        err(f"registry JSON invalid: {exc}")
        return
    if data.get("status") != "REUSED":
        err("registry status must be REUSED")
    entries = data.get("entries") or []
    meta = data.get("meta") or {}
    if meta.get("n_entries") != len(entries):
        err("registry n_entries mismatch")
    if len(entries) < 18:
        err(f"registry too small to cover CE+SE: {len(entries)}")
    if len(entries) > 200:
        err(f"registry must reuse compactly, not harvest: {len(entries)}")
    seen: set[str] = set()
    objects: set[str] = set()
    sources: set[str] = set()
    theorems: set[str] = set()
    prefixes: set[str] = set()
    for ev in entries:
        for k in REG_FIELDS:
            if k not in ev:
                err(f"registry entry missing {k}: {ev.get('id')}")
        oid = str(ev.get("id") or "")
        obj = ev.get("object")
        src = ev.get("source")
        th = ev.get("future_theorem")
        if obj not in REG_OBJECTS:
            err(f"{oid}: object must be block|tx|input|output|utxo")
        if src not in REG_SOURCES:
            err(f"{oid}: source must be runtime|serialization|attack")
        if not th:
            err(f"{oid}: empty future_theorem")
        if oid in seen:
            err(f"duplicate registry id {oid}")
        seen.add(oid)
        objects.add(obj)
        sources.add(src)
        theorems.add(th)
        prefixes.add(oid.split("/")[0] + "/")
    if objects != REG_OBJECTS:
        err(f"registry objects incomplete: {objects}")
    if sources != REG_SOURCES:
        err(f"registry sources incomplete: {sources}")
    for prefix in TASK_PREFIXES:
        if not any(i.startswith(prefix) for i in seen):
            err(f"registry missing reused prefix {prefix}")
        tag = prefix.rstrip("/")
        if tag not in prefixes and not any(i.startswith(prefix) for i in seen):
            err(f"registry missing {tag}")
    for cid in CE_IDS:
        if f"/{cid}" not in seen:
            err(f"registry missing /{cid}")
        if f"/{cid}" not in seen:
            err(f"registry missing /{cid}")
    for sid in SE_IDS:
        if f"/{sid}" not in seen:
            err(f"registry missing /{sid}")
    for th in UTXO_THEOREMS:
        if th not in theorems:
            err(f"registry has no row for {th}")
    attack = ROOT / "tests" / "attack_vectors" / "traces"
    kcd = BRIDGE / "kernel_candidate_dataset"
    se_dir = BRIDGE / "serialization_counterexamples_v1"
    sv_dir = ROOT / "tests" / "serialization_vectors" / "cases"
    diff = json.loads((BRIDGE / "differential_dataset" / "index.json").read_text())
    scenarios = set((diff.get("by_scenario") or {}).keys())
    for oid in seen:
        if oid.startswith("/CE-"):
            cid = oid.split("/", 1)[1]
            if not (attack / f"{cid}.json").is_file():
                err(f"reused id not in attack dataset: {oid}")
        elif oid.startswith("/CE-"):
            cid = oid.split("/", 1)[1]
            exact = kcd / cid / "kernel_candidate.json"
            variants = list(kcd.glob(f"{cid}*/kernel_candidate.json"))
            if not exact.is_file() and not variants:
                err(f"reused id not in dataset: {oid}")
        elif oid.startswith("/"):
            scen = oid.split("/", 1)[1]
            if scen not in scenarios:
                err(f"reused id not in scenarios: {oid}")
        elif oid.startswith("/SE-"):
            sid = oid.split("/", 1)[1]
            if not (se_dir / f"{sid}.json").is_file():
                err(f"reused id not in SE: {oid}")
        elif oid.startswith("/SV-"):
            name = oid.split("/", 1)[1]
            if not (sv_dir / f"{name}.json").is_file():
                err(f"reused id not in SV: {oid}")
        else:
            err(f"registry id not a reused /008/012/014 row: {oid}")


def check_protected() -> None:
    for path, expected in FROZEN_SHA256.items():
        if not path.is_file():
            err(f"missing protected {path.relative_to(ROOT)}")
            continue
        got = sha256_file(path)
        if got != expected:
            err(f"protected file changed: {path.relative_to(ROOT)}")
    text = KERNEL.read_text()
    if "Record KernelInput" not in text:
        err("Record KernelInput missing")
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


def check_workflow_freeze() -> None:
    if WF.is_file():
        wf = WF.read_text()
        if "check_consensus_model_foundation.py" not in wf:
            err("workflow must run check_consensus_model_foundation.py")
        if "coqc" in wf:
            err("consensus_model_foundation.yml must not build Coq")
    if FREEZE.is_file():
        text = FREEZE.read_text()
        for token in (
            "CONSENSUS_MODEL_FOUNDATION = PASS",
            "OBJECT_BOUNDARY = COMPLETE",
            "PIPELINE_MAPPING = COMPLETE",
            "UTXO_MODEL_GAP = IDENTIFIED",
            "DATASET_REUSED = PASS",
            "KERNELINPUT_CHANGED = NO",
            "CPP_REFINEMENT = NOT_STARTED",
            "CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED",
        ):
            if token not in text:
                err(f"freeze missing {token}")


def main() -> int:
    check_files()
    check_claims()
    check_object_boundary()
    check_types()
    check_pipeline()
    check_utxo_gap()
    check_counterexamples()
    check_registry()
    check_protected()
    check_workflow_freeze()

    if errors:
        for e in errors:
            print(f"FAIL: {e}", file=sys.stderr)
        print("CONSENSUS_MODEL_FOUNDATION = FAIL")
        return 1

    print("CONSENSUS_MODEL_FOUNDATION = PASS")
    print("OBJECT_BOUNDARY = COMPLETE")
    print("PIPELINE_MAPPING = COMPLETE")
    print("UTXO_MODEL_GAP = IDENTIFIED")
    print("DATASET_REUSED = PASS")
    print("KERNELINPUT_CHANGED = NO")
    print("CPP_REFINEMENT = NOT_STARTED")
    return 0


if __name__ == "__main__":
    sys.exit(main())

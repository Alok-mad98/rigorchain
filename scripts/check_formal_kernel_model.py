#!/usr/bin/env python3
""" formal consensus kernel input model gate.

Not a C++ change. Not extraction. Not CPP_KERNELINPUT_EQUIVALENCE.
"""
from __future__ import annotations

import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BRIDGE = ROOT / "audit" / "formal_bridge"
FV = ROOT / "formal_verification"
CM = FV / "consensus_model"
REF = FV / "refinement"

BLOCK_V = CM / "BlockModel.v"
TX_V = CM / "TransactionModel.v"
UTXO_V = CM / "UTXOState.v"
VAL_V = CM / "ValidationResult.v"
KIV2_V = CM / "KernelInputV2.v"
IFACE_V = REF / "interfaces.v"
COQPROJECT = CM / "_CoqProject"

MODEL = BRIDGE / "formal_kernel_model_v1.md"
DESIGN = BRIDGE / "kernel_input_v2_design.md"
REG = BRIDGE / "formal_model_dataset_registry_v3.json"
SKEL = BRIDGE / "formal_theorem_skeleton_v1.md"
FREEZE = ROOT / "audit" / "22_formal_kernel_model_frozen.md"
WF = ROOT / ".github" / "workflows" / "formal_kernel_model.yml"

KERNEL = FV / "rigor_kernel" / "Kernel" / "UnifiedTransition.v"
BLOCK_H = ROOT / "core" / "src" / "primitives" / "block.h"
TX_H = ROOT / "core" / "src" / "primitives" / "transaction.h"
SER_H = ROOT / "core" / "src" / "serialize.h"
CORE_READ = ROOT / "core" / "src" / "core_read.cpp"
VALIDATION = ROOT / "core" / "src" / "validation.cpp"
KERNEL_CRIT = FV / "rigor_kernel" / "_CoqProject.kernel-critical"
COQ_FULL = FV / "coq" / "_CoqProject"

FROZEN_SHA256 = {
    KERNEL: "5a6fd11e066a9f1cc0fdc887dcb018d1af1990e0c9ff950ab91b38aa53d5f444",
    BLOCK_H: "6d756e2f29b68282cbd2a3eb17d9ff36b5339bfa5b803f7ca5256c41f2bbe509",
    TX_H: "b0862373d3dd760f27f26e1df9e9871b84b6255148390a3f8151970ade23380a",
    SER_H: "58d1e74a3e267ab1be20401fee8bb50a60514257d5274b7da87d474bf4fc4744",
    CORE_READ: "c09d50b73c6bb3a8d9507875fda474ad742f855c2c702c5e67b7751f64d4bb16",
    VALIDATION: "3cf90c64860a3ac07bb869c114084a6763590d7cf58a9ec228bfe57649b66226",
}

CHAIN = (
    "BlockBytes",
    "DecodeModel",
    "Block",
    "Transaction",
    "UTXOState",
    "ValidationResult",
    "KernelInput",
)
TX_FIELDS = (
    "version",
    "inputs",
    "outputs",
    "locktime",
    "witness",
    "previous_outpoint",
    "script_data",
    "sequence",
    "amount",
    "script_pubkey",
    "stack",
)
VAL_FIELDS = ("tx_valid", "inputs_valid", "script_valid", "amount_valid")
UTXO_OPS = ("lookup", "insert", "remove")
IFACE_FUNS = ("decode_block", "validate_block", "make_kernel_input")
THEOREMS = (
    "decode_block_deterministic",
    "validate_inputs_sound",
    "utxo_lookup_correct",
    "kernel_input_complete",
)
REG_FIELDS = ("id", "object_type", "validation_stage", "future_theorem")
REG_OBJECTS = {"block", "transaction", "input", "output", "utxo", "witness"}
TASK_PREFIXES = ("/", "/", "/", "/")
SCAN = (MODEL, DESIGN, SKEL, FREEZE)
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
    for p in (
        BLOCK_V,
        TX_V,
        UTXO_V,
        VAL_V,
        KIV2_V,
        IFACE_V,
        COQPROJECT,
        MODEL,
        DESIGN,
        REG,
        SKEL,
        FREEZE,
        WF,
        KERNEL,
    ):
        if not p.exists():
            err(f"missing {p.relative_to(ROOT)}")


def check_claims() -> None:
    blob = "\n".join(p.read_text() for p in SCAN if p.is_file())
    for token in (
        "CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED",
        "KernelInputCandidate != KernelInput",
        "KERNELINPUT_CHANGED = NO",
        "CONSENSUS_CHANGED = NO",
        "CPP_REFINEMENT = NOT_STARTED",
    ):
        if token not in blob:
            err(f"docs missing {token!r}")
    if re.search(r"CPP_KERNELINPUT_EQUIVALENCE\s*=\s*(TRUE|PROVED)", blob):
        err("must not claim CPP_KERNELINPUT_EQUIVALENCE proved")
    if re.search(r"KernelInputCandidate\s*=\s*KernelInput", blob):
        err("must not equate candidate with KernelInput")
    for p in SCAN:
        if not p.is_file():
            continue
        text = p.read_text()
        for pat in FORBIDDEN:
            if re.search(pat, text):
                err(f"{p.name}: forbidden token {pat}")


def check_block_tx_models() -> None:
    if not BLOCK_V.is_file() or not TX_V.is_file():
        return
    b = BLOCK_V.read_text()
    t = TX_V.read_text()
    for name in ("BlockBytes", "DecodeModel", "Record Block", "header", "transactions"):
        if name not in b:
            err(f"BlockModel.v missing {name}")
    if "Record BlockHeader" not in b:
        err("BlockModel.v missing Record BlockHeader")
    for rec in ("Record TxInput", "Record TxOutput", "Record Witness", "Record Transaction"):
        if rec not in t:
            err(f"TransactionModel.v missing {rec}")
    for field in TX_FIELDS:
        if field not in t:
            err(f"TransactionModel.v missing field {field}")
    if re.search(r"Record Transaction\s*:=\s*\{[^}]*bool", t, re.S):
        err("Transaction must not be abstracted to bool")


def check_utxo_model() -> None:
    if not UTXO_V.is_file():
        return
    text = UTXO_V.read_text()
    if "utxo_wf_flag" in text:
        err("consensus_model/UTXOState.v must not reuse utxo_wf_flag")
    if "Definition UTXOState" not in text:
        err("UTXOState.v missing Definition UTXOState")
    for op in UTXO_OPS:
        if f" {op} " not in f" {text} " and f" {op}(" not in text and f"Fixpoint {op}" not in text and f"Definition {op}" not in text:
            err(f"UTXOState.v missing {op}")
    if "OutPoint" not in text or "TxOutput" not in text:
        err("UTXOState must map OutPoint to TxOutput")
    for name in ("spend_removes_coin", "restore_coin", "input_lookup_sound"):
        if name not in text:
            err(f"UTXOState.v missing {name}")


def check_validation_kernel() -> None:
    if VAL_V.is_file():
        text = VAL_V.read_text()
        if "Record ValidationResult" not in text:
            err("ValidationResult.v missing record")
        for f in VAL_FIELDS:
            if f not in text:
                err(f"ValidationResult.v missing {f}")
        for cpp in ("CheckTransaction", "CheckTxInputs", "VerifyWitnessProgram"):
            if cpp not in text:
                err(f"ValidationResult.v missing C++ map {cpp}")
    if KIV2_V.is_file():
        text = KIV2_V.read_text()
        if "Record KernelInputV2" not in text:
            err("KernelInputV2.v missing record")
        for tok in ("ki_pow_ok", "ki_asert_ok", "Block", "ValidationResult", "UTXOState"):
            if tok not in text:
                err(f"KernelInputV2.v missing {tok}")
        if "kernel_input_of_validation" not in text:
            err("KernelInputV2.v missing kernel_input_of_validation")
        if "Record KernelInput :=" in text and "ExistingKernelInput" not in text:
            err("must not replace UnifiedTransition KernelInput in KernelInputV2.v")


def check_interface_theorems() -> None:
    if not IFACE_V.is_file():
        return
    text = IFACE_V.read_text()
    for fn in IFACE_FUNS:
        if fn not in text:
            err(f"interfaces.v missing {fn}")
    if "Parameter decode_block" not in text and "Parameter decode_block" not in text:
        err("interfaces.v must Parameter decode_block")
    if "Parameter validate_block" not in text:
        err("interfaces.v must Parameter validate_block")
    if "Parameter make_kernel_input" not in text:
        err("interfaces.v must Parameter make_kernel_input")
    for th in THEOREMS:
        if f"Theorem {th}" not in text:
            err(f"interfaces.v missing Theorem {th}")
        # body must be Admitted, not Qed
        chunk = text.split(f"Theorem {th}", 1)[-1]
        nxt = re.search(r"\nTheorem ", chunk)
        body = chunk[: nxt.start()] if nxt else chunk
        if "Admitted" not in body:
            err(f"{th} must be Admitted")
        if re.search(r"\bQed\.", body):
            err(f"{th} must not be proved (Qed)")
    for name in CHAIN:
        blob = "\n".join(
            p.read_text()
            for p in (BLOCK_V, TX_V, UTXO_V, VAL_V, KIV2_V, IFACE_V, MODEL)
            if p.is_file()
        )
        if name not in blob:
            err(f"model chain missing {name}")
            break


def check_not_in_load_bearing_cones() -> None:
    for proj, label in ((KERNEL_CRIT, "kernel-critical"), (COQ_FULL, "coq/_CoqProject")):
        if not proj.is_file():
            continue
        text = proj.read_text()
        for name in (
            "consensus_model/BlockModel.v",
            "consensus_model/UTXOState.v",
            "refinement/interfaces.v",
        ):
            if name in text:
                err(f"{label} must not include {name}")
    for p in (BLOCK_V, TX_V, UTXO_V, VAL_V, KIV2_V, IFACE_V):
        if not p.is_file():
            continue
        text = p.read_text()
        if re.search(r"^\s*Extract\b", text, re.M):
            err(f"{p.name} must not extract")


def check_registry() -> None:
    if not REG.is_file():
        return
    try:
        data = json.loads(REG.read_text())
    except json.JSONDecodeError as exc:
        err(f"registry JSON invalid: {exc}")
        return
    if data.get("status") != "ALIGNED":
        err("v3 registry status must be ALIGNED")
    entries = data.get("entries") or []
    meta = data.get("meta") or {}
    if meta.get("n_entries") != len(entries):
        err("registry n_entries mismatch")
    if len(entries) < 18:
        err(f"registry too small: {len(entries)}")
    if len(entries) > 200:
        err(f"registry must reuse compactly: {len(entries)}")
    seen: set[str] = set()
    objects: set[str] = set()
    stages: set[str] = set()
    theorems: set[str] = set()
    for ev in entries:
        for k in REG_FIELDS:
            if k not in ev:
                err(f"registry entry missing {k}: {ev.get('id')}")
        oid = str(ev.get("id") or "")
        ot = ev.get("object_type")
        st = ev.get("validation_stage")
        th = ev.get("future_theorem")
        if ot not in REG_OBJECTS:
            err(f"{oid}: object_type must be one of {sorted(REG_OBJECTS)}")
        if not st:
            err(f"{oid}: empty validation_stage")
        if not th:
            err(f"{oid}: empty future_theorem")
        if oid in seen:
            err(f"duplicate registry id {oid}")
        seen.add(oid)
        objects.add(ot)
        stages.add(st)
        theorems.add(th)
 for prefix in TASK_PREFIXES:
        if not any(i.startswith(prefix) for i in seen):
            err(f"registry missing reused prefix {prefix}")
    for th in THEOREMS:
        if th not in theorems:
            err(f"registry has no row for {th}")
    if "utxo" not in objects or "block" not in objects:
        err(f"registry object_types incomplete: {objects}")
    v2 = BRIDGE / "formal_model_dataset_registry_v2.json"
    if v2.is_file():
        ids2 = {e.get("id") for e in json.loads(v2.read_text()).get("entries") or []}
        extra = seen - ids2
        if extra:
            err(f"v3 introduced new ids (must reuse v2): {sorted(extra)[:5]}")


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
    if "utxo_wf_flag" not in (FV / "rigor_kernel" / "Kernel" / "UnifiedState.v").read_text():
        err("must not edit UnifiedState UTXO flag in this TASK")


def check_coqc() -> None:
    if not shutil.which("coqc"):
        return
    cmd = [
        "coqc",
        "-q",
        "-R",
        str(CM),
        "ConsensusModel",
        "-R",
        str(REF),
        "ConsensusRefinement",
        str(IFACE_V),
    ]
    # compile deps first in order
    order = [TX_V, BLOCK_V, UTXO_V, VAL_V, KIV2_V, IFACE_V]
    for src in order:
        r = subprocess.run(
            [
                "coqc",
                "-q",
                "-R",
                str(CM),
                "ConsensusModel",
                "-R",
                str(REF),
                "ConsensusRefinement",
                str(src),
            ],
            cwd=str(FV),
            capture_output=True,
            text=True,
        )
        if r.returncode != 0:
            err(f"coqc failed on {src.name}: {r.stderr.strip()[:400]}")
            return
    del cmd


def check_workflow_freeze() -> None:
    if WF.is_file():
        wf = WF.read_text()
        if "check_formal_kernel_model.py" not in wf:
            err("workflow must run check_formal_kernel_model.py")
        if "coqc" in wf:
            err("formal_kernel_model.yml must not require coqc in CI")
    if FREEZE.is_file():
        text = FREEZE.read_text()
        for token in (
            "FORMAL_KERNEL_MODEL = PASS",
            "BLOCK_MODEL = EXISTS",
            "TX_MODEL = EXISTS",
            "UTXO_STATE_MODEL = EXISTS",
            "VALIDATION_MODEL = EXISTS",
            "KERNEL_INPUT_BRIDGE_INTERFACE = EXISTS",
            "DATASET_ALIGNMENT = PASS",
            "CONSENSUS_CHANGED = NO",
            "CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED",
        ):
            if token not in text:
                err(f"freeze missing {token}")


def main() -> int:
    check_files()
    check_claims()
    check_block_tx_models()
    check_utxo_model()
    check_validation_kernel()
    check_interface_theorems()
    check_not_in_load_bearing_cones()
    check_registry()
    check_protected()
    check_coqc()
    check_workflow_freeze()

    if errors:
        for e in errors:
            print(f"FAIL: {e}", file=sys.stderr)
        print("FORMAL_KERNEL_MODEL = FAIL")
        return 1

    print("FORMAL_KERNEL_MODEL = PASS")
    print("BLOCK_MODEL = EXISTS")
    print("TX_MODEL = EXISTS")
    print("UTXO_STATE_MODEL = EXISTS")
    print("VALIDATION_MODEL = EXISTS")
    print("KERNEL_INPUT_BRIDGE_INTERFACE = EXISTS")
    print("DATASET_ALIGNMENT = PASS")
    print("CONSENSUS_CHANGED = NO")
    return 0


if __name__ == "__main__":
    sys.exit(main())

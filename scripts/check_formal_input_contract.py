#!/usr/bin/env python3
"""formal input contract gate. Not a Coq proof. Not equivalence."""
from __future__ import annotations

import importlib.util
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formal_bridge"))
from formal_contract_hashes import dataset_hash, schema_hash, sha256_file  # noqa: E402

BRIDGE = ROOT / "audit" / "formal_bridge"
CONTRACT = BRIDGE / "formal_input_contract_v1.md"
GAP = BRIDGE / "formal_object_gap_v1.md"
REG = BRIDGE / "formal_dataset_registry_v1.json"
MATRIX = BRIDGE / "theorem_candidate_matrix_v2.md"
FREEZE = ROOT / "audit" / "20_formal_input_contract_frozen.md"
WF = ROOT / ".github" / "workflows" / "formal_input_contract.yml"
CATALOG = BRIDGE / "counterexample_catalog_v1.md"
BOUNDARY = BRIDGE / "kernel_input_boundary_v1.md"
COLLECT = ROOT / "tools" / "formal_bridge" / "evidence_collector.py"
KERNEL = ROOT / "formal_verification" / "rigor_kernel" / "Kernel" / "UnifiedTransition.v"

KERNEL_SHA256 = "5a6fd11e066a9f1cc0fdc887dcb018d1af1990e0c9ff950ab91b38aa53d5f444"

TIER1 = (
    "canonical_decode",
    "compactsize_unique",
    "pow_validation_sound",
    "asert_boundary",
)
TIER2 = (
    "utxo_lookup_sound",
    "script_validation_sound",
    "amount_arithmetic_sound",
    "connect_disconnect_inverse",
)
TIER3 = (
    "chain_selection",
    "cpp_kernelinput_refinement",
    "c++ implementation refinement",
)
REQUIRED_CE = [f"CE-{i:03d}" for i in range(1, 14)]
CE_SECTIONS = (
    "Input",
    "Observed C++ decision",
    "Reject layer",
    "Kernel candidate impact",
    "Future theorem target",
)
MISSING_OBJECTS = (
    "Block",
    "Transaction",
    "TxInput",
    "TxOutput",
    "Script",
    "Witness",
    "UTXOState",
    "MerkleTree",
    "CompactSize codec",
    "Amount arithmetic",
    "UndoState",
    "ChainSelection",
)
EXISTING_OBJECTS = ("KernelInput", "KernelState", "kernel_step")
PIPELINE = (
    "BlockBytes",
    "DecodedBlock",
    "ValidatedTransactionSet",
    "UTXOState",
    "ValidationResult",
    "KernelInput",
)
ENTRY_FIELDS = (
    "id",
    "source",
    "class",
    "input",
    "expected_layer",
    "future_theorem",
    "status",
)
SOURCES = (
    "",
    "",
    "",
    "",
    "",
)
FORBIDDEN = (
    r"\bFORGE\b",
    r"\bMTH\b",
    r"\bRNG\b",
    r"script-as-asset",
    r"dual asset",
    r"dual-asset",
)
SCAN = (CONTRACT, GAP, MATRIX, FREEZE)

errors: list[str] = []


def err(msg: str) -> None:
    errors.append(msg)


def load_col():
    spec = importlib.util.spec_from_file_location("evidence_collector", COLLECT)
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(mod)
    return mod


def check_files() -> None:
    for p in (CONTRACT, GAP, REG, MATRIX, FREEZE, WF, CATALOG, KERNEL):
        if not p.exists():
            err(f"missing {p.relative_to(ROOT)}")


def check_claims() -> None:
    blob = "\n".join(p.read_text() for p in SCAN if p.is_file())
    for token in (
        "CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED",
        "KernelInputCandidate != KernelInput",
        "Evidence != Proof",
        "Dataset != Theorem",
        "KERNELINPUT_BOUNDARY = CLEAN",
    ):
        if token not in blob:
            err(f"docs missing {token!r}")
    if re.search(r"CPP_KERNELINPUT_EQUIVALENCE\s*=\s*(TRUE|PROVED)", blob):
        err("docs must not claim CPP_KERNELINPUT_EQUIVALENCE proved")
    if re.search(r"KernelInputCandidate\s*=\s*KernelInput", blob):
        err("docs must not equate KernelInputCandidate with KernelInput")
    for p in SCAN:
        if not p.is_file():
            continue
        text = p.read_text()
        for pat in FORBIDDEN:
            if re.search(pat, text):
                err(f"{p.name}: forbidden architecture token {pat}")


def check_contract() -> None:
    text = CONTRACT.read_text()
    for name in PIPELINE:
        if name not in text:
            err(f"contract missing layer {name}")
    for tag in ("IMPLEMENTED", "OBSERVED", "TARGET", "MISSING MODEL"):
        if tag not in text:
            err(f"contract missing status tag {tag}")
    if "C++ Runtime Observation" not in text or "Future Coq Proof Entry" not in text:
        err("contract missing observation → proof-entry diagram")


def check_gap() -> None:
    text = GAP.read_text()
    if "## Present" not in text or "## Missing" not in text:
        err("gap doc missing ## Present / ## Missing")
    for name in EXISTING_OBJECTS:
        if name not in text:
            err(f"gap doc missing existing {name}")
    for name in MISSING_OBJECTS:
        if name not in text:
            err(f"gap doc missing object {name}")


def check_matrix() -> None:
    text = MATRIX.read_text()
    for name in ("Tier 1", "Tier 2", "Tier 3"):
        if name not in text:
            err(f"matrix missing {name}")
    for name in TIER1 + TIER2 + TIER3:
        if name not in text:
            err(f"matrix missing theorem {name}")


def check_counterexamples() -> None:
    text = CATALOG.read_text()
    for ce in REQUIRED_CE:
        if f"## {ce}" not in text:
            err(f"catalog missing {ce}")
        block = text.split(f"## {ce}", 1)[-1]
        nxt = re.search(r"\n## CE-", block)
        chunk = block[: nxt.start()] if nxt else block
        for sec in CE_SECTIONS:
            if sec not in chunk:
                err(f"{ce} missing {sec}")
        src = BRIDGE / "counterexamples" / f"{ce}.json"
        if not src.is_file():
            err(f"missing {src.relative_to(ROOT)}")


def check_kernelinput_boundary() -> None:
    if not KERNEL.is_file():
        return
    got = sha256_file(KERNEL)
    if got != KERNEL_SHA256:
        err(f"UnifiedTransition.v changed (got {got})")
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
    if BOUNDARY.is_file():
        b = BOUNDARY.read_text()
        if "ORACLE_ONLY" not in b or "NOT PROVED" not in b:
            err("kernel_input_boundary_v1.md no longer ORACLE_ONLY / NOT PROVED")


def check_registry() -> dict | None:
    if not REG.is_file():
        return None
    try:
        data = json.loads(REG.read_text())
    except json.JSONDecodeError as exc:
        err(f"registry JSON invalid: {exc}")
        return None
    meta = data.get("meta") or {}
    entries = data.get("entries") or []
    if data.get("status") != "OBSERVED":
        err("registry status must be OBSERVED")
    if "NOT PROVED" not in str(data.get("note", "")):
        err("registry must record CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED")
    if not entries:
        err("registry entries empty")
    got_ds = dataset_hash()
    got_sc = schema_hash()
    if meta.get("dataset_hash") != got_ds:
        err("dataset_hash mismatch (sources changed or registry stale)")
    if meta.get("schema_hash") != got_sc:
        err("schema_hash mismatch")
    if meta.get("n_entries") != len(entries):
        err("registry n_entries mismatch")
    ids = []
    classes = set()
    theorems = set()
    prefixes = set()
    for ev in entries:
        for k in ENTRY_FIELDS:
            if k not in ev:
                err(f"registry entry missing {k}: {ev.get('id')}")
        if ev.get("status") != "OBSERVED":
            err(f"{ev.get('id')}: status must be OBSERVED")
        if ev.get("class") not in {"A", "B", "C"}:
            err(f"{ev.get('id')}: class must be A|B|C")
        if not ev.get("future_theorem"):
            err(f"{ev.get('id')}: empty future_theorem")
        if not ev.get("input"):
            err(f"{ev.get('id')}: empty input")
        ids.append(ev.get("id"))
        classes.add(ev.get("class"))
        theorems.add(ev.get("future_theorem"))
        eid = str(ev.get("id") or "")
        prefixes.add(eid.split("/")[0])
    if len(ids) != len(set(ids)):
        err("duplicate registry id")
    if classes != {"A", "B", "C"}:
        err(f"registry must include A, B, and C (have {classes})")
    for src in SOURCES:
        if src not in prefixes:
            err(f"registry missing source {src}")
    for ce in REQUIRED_CE:
        if not any(ce in str(i) for i in ids):
            err(f"registry missing {ce}")
    for th in TIER1 + TIER2:
        if th not in theorems:
            err(f"registry has no row for {th}")
    if "chain_selection" not in theorems:
        err("registry has no chain_selection observation")
    freeze = FREEZE.read_text() if FREEZE.is_file() else ""
    if got_ds not in freeze or got_sc not in freeze:
        err("audit/20 freeze missing current dataset_hash / schema_hash")
    return data


def check_evidence_ids() -> None:
    col = load_col()
    trial = BRIDGE / "local_trial_dataset"
    if not trial.is_dir():
        err("missing local_trial_dataset")
        return
    n = 0
    for path in sorted(trial.glob("*.json")):
        if path.name == "index.json":
            continue
        rec = json.loads(path.read_text())
        stored = rec.get("evidence_id")
        recomputed = col.compute_evidence_id(rec)
        if stored != recomputed:
            err(f"{path.name}: evidence_id unstable ({stored} != {recomputed})")
        n += 1
    if n < 13:
        err(f"expected 13 local trial evidence files, got {n}")


def check_workflow() -> None:
    if not WF.is_file():
        return
    text = WF.read_text()
    if "check_formal_input_contract.py" not in text:
        err("workflow must run check_formal_input_contract.py")
    if "coqc" in text:
        err("formal_input_contract.yml must not build Coq")


def check_freeze() -> None:
    if not FREEZE.is_file():
        return
    text = FREEZE.read_text()
    for token in (
        "FORMAL_INPUT_CONTRACT = PASS",
        "DATASET_REGISTRY = PASS",
        "THEOREM_MATRIX = PASS",
        "KERNELINPUT_BOUNDARY = CLEAN",
        "CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED",
    ):
        if token not in text:
            err(f"freeze missing {token}")


def main() -> int:
    check_files()
    check_claims()
    check_contract()
    check_gap()
    check_matrix()
    check_counterexamples()
    check_kernelinput_boundary()
    check_registry()
    check_evidence_ids()
    check_workflow()
    check_freeze()

    if errors:
        for e in errors:
            print(f"FAIL: {e}", file=sys.stderr)
        print("FORMAL_INPUT_CONTRACT = FAIL")
        return 1

    print("FORMAL_INPUT_CONTRACT = PASS")
    print("DATASET_REGISTRY = PASS")
    print("THEOREM_MATRIX = PASS")
    print("KERNELINPUT_BOUNDARY = CLEAN")
    print("CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED")
    return 0


if __name__ == "__main__":
    sys.exit(main())

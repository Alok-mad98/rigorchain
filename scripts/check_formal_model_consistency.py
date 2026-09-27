#!/usr/bin/env python3
"""internal Coq model consistency gate.

Qed lemmas only. Not C++ refinement. Not KernelInputCandidate equivalence.
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
TESTS = ROOT / "tests" / "formal_model"

CONS = BRIDGE / "formal_kernel_model_consistency_v1.md"
STATUS = BRIDGE / "formal_model_proof_status.md"
EXPECTED = TESTS / "expected_theorems.json"
TEST_PY = TESTS / "test_internal_consistency.py"
WF = ROOT / ".github" / "workflows" / "formal_model_consistency.yml"

IFACE = REF / "interfaces.v"
KERNEL = FV / "rigor_kernel" / "Kernel" / "UnifiedTransition.v"
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

PROOF_FILES = (
    CM / "UTXOStateProperties.v",
    CM / "TransactionProperties.v",
    CM / "ValidationProperties.v",
    CM / "KernelInputV2Properties.v",
)
REQUIRED = (
    ("insert_lookup", CM / "UTXOStateProperties.v"),
    ("remove_lookup", CM / "UTXOStateProperties.v"),
    ("update_remove_consistency", CM / "UTXOStateProperties.v"),
    ("well_formed_transaction", CM / "TransactionProperties.v"),
    ("accepted_validation", CM / "ValidationProperties.v"),
    ("make_kernel_input_field_consistency", CM / "KernelInputV2Properties.v"),
)
COMPILE = (
    CM / "TransactionModel.v",
    CM / "BlockModel.v",
    CM / "UTXOState.v",
    CM / "ValidationResult.v",
    CM / "KernelInputV2.v",
    CM / "UTXOStateProperties.v",
    CM / "ValidationProperties.v",
    CM / "TransactionProperties.v",
    CM / "KernelInputV2Properties.v",
)
ASSUME_RE = re.compile(r"^\s*(Admitted|Axiom|Parameter)\b")
SCAN = (CONS, STATUS)
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
    for p in (CONS, STATUS, EXPECTED, TEST_PY, IFACE, KERNEL, *PROOF_FILES):
        if not p.exists():
            err(f"missing {p.relative_to(ROOT)}")


def check_claims() -> None:
    blob = "\n".join(p.read_text() for p in SCAN if p.is_file())
    for token in (
        "FORMAL_MODEL_INTERNAL_CONSISTENCY = PASS",
        "CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED",
        "KernelInputCandidate != KernelInput",
        "FUTURE_REFINEMENT_BOUNDARY",
    ):
        if token not in blob:
            err(f"docs missing {token!r}")
    if re.search(r"CPP_KERNELINPUT_EQUIVALENCE\s*=\s*(TRUE|PROVED)", blob):
        err("must not claim CPP_KERNELINPUT_EQUIVALENCE proved")
    if re.search(r"C\+\+\s*->\s*Coq", blob) and "not proved" not in blob.lower():
        err("must not claim C++ -> Coq")
    for p in SCAN:
        if not p.is_file():
            continue
        text = p.read_text()
        for pat in FORBIDDEN:
            if re.search(pat, text):
                err(f"{p.name}: forbidden token {pat}")


def theorem_qed(path: Path, name: str) -> None:
    if not path.is_file():
        return
    text = path.read_text()
    if f"Theorem {name}" not in text:
        err(f"{path.name} missing Theorem {name}")
        return
    chunk = text.split(f"Theorem {name}", 1)[-1]
    nxt = re.search(r"\n(?:Theorem|Lemma) ", chunk)
    body = chunk[: nxt.start()] if nxt else chunk
    if not re.search(r"\bQed\.", body):
        err(f"{name} must close with Qed")
    if re.search(r"\bAdmitted\b", body):
        err(f"{name} must not be Admitted")


def check_proofs() -> None:
    for name, path in REQUIRED:
        theorem_qed(path, name)
    kiv2 = CM / "KernelInputV2Properties.v"
    if kiv2.is_file():
        text = kiv2.read_text()
        if "Definition make_kernel_input" not in text:
            err("KernelInputV2Properties.v missing model make_kernel_input")
        for tok in ("ki_pow_ok", "ki_asert_ok", "inputs_valid", "script_valid"):
            if tok not in text:
                err(f"make_kernel_input consistency missing {tok}")
    txp = CM / "TransactionProperties.v"
    if txp.is_file():
        text = txp.read_text()
        if "invalid_input" not in text or "accepted_validation" not in text:
            err("well_formed_transaction must mention invalid_input and accepted_validation")
    val = CM / "ValidationProperties.v"
    if val.is_file():
        text = val.read_text()
        for tok in ("inputs_valid", "script_valid", "amount_valid"):
            if tok not in text:
                err(f"accepted_validation missing {tok}")


def check_no_admits_in_internal() -> None:
    for path in PROOF_FILES:
        if not path.is_file():
            continue
        for i, line in enumerate(path.read_text().splitlines(), 1):
            if ASSUME_RE.match(line):
                err(f"{path.name}:{i} internal proof must not use {line.strip().split()[0]}")
    if IFACE.is_file():
        text = IFACE.read_text()
        if "FUTURE_REFINEMENT_BOUNDARY" not in text:
            err("interfaces.v must mark FUTURE_REFINEMENT_BOUNDARY")
        if "Parameter make_kernel_input" not in text:
            err("refinement Parameter make_kernel_input missing")


def check_expected_json() -> None:
    if not EXPECTED.is_file():
        return
    data = json.loads(EXPECTED.read_text())
    names = {row.get("name") for row in data.get("theorems") or []}
    for name, _path in REQUIRED:
        if name not in names:
            err(f"expected_theorems.json missing {name}")


def check_protected() -> None:
    for path, expected in FROZEN_SHA256.items():
        if not path.is_file():
            err(f"missing protected {path.relative_to(ROOT)}")
            continue
        if sha256_file(path) != expected:
            err(f"protected file changed: {path.relative_to(ROOT)}")
    if "Record KernelInput" not in KERNEL.read_text():
        err("Record KernelInput missing")


def check_coqc() -> None:
    if not shutil.which("coqc"):
        return
    for src in COMPILE:
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


def check_workflow() -> None:
    if WF.is_file() and "check_formal_model_consistency.py" not in WF.read_text():
        err("workflow must run check_formal_model_consistency.py")
    if WF.is_file() and "coqc" in WF.read_text():
        err("formal_model_consistency.yml must not require coqc in CI")


def main() -> int:
    check_files()
    check_claims()
    check_proofs()
    check_no_admits_in_internal()
    check_expected_json()
    check_protected()
    check_coqc()
    check_workflow()

    if errors:
        for e in errors:
            print(f"FAIL: {e}", file=sys.stderr)
        print("FORMAL_MODEL_INTERNAL_CONSISTENCY = FAIL")
        return 1

    print("FORMAL_MODEL_INTERNAL_CONSISTENCY = PASS")
    print("CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""TASK-020 formal validation closure gate.

Qed-only model pipeline. Not C++ refinement. Not extraction.
"""
from __future__ import annotations

import hashlib
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

MATRIX = BRIDGE / "formal_validation_closure_matrix_v1.md"
WF = ROOT / ".github" / "workflows" / "formal_validation_closure.yml"

KIC_V = CM / "KernelInputConstruction.v"
VP_V = CM / "ValidationPipeline.v"

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

CORE_THEOREMS = (
    ("decoded_block_validation_deterministic", VP_V),
    ("validation_failure_no_transition", VP_V),
    ("accepted_block_kernel_input_consistent", KIC_V),
    ("kernel_input_then_step_deterministic", VP_V),
    ("serialization_validation_composition", VP_V),
)

NEW_FILES = (KIC_V, VP_V)

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
    CM / "ValidationModel.v",
    CM / "KernelStepModel.v",
    CM / "ChainState.v",
    CM / "ByteModel.v",
    CM / "CompactSizeModel.v",
    CM / "TransactionCodec.v",
    CM / "BlockCodec.v",
    KIC_V,
    VP_V,
)

ASSUME_RE = re.compile(r"^\s*(Admitted|Axiom|Parameter)\b")
MATRIX_COLS = ("Theorem", "Proof status", "Dataset citation")
DATASETS = ("TASK-010", "TASK-012", "TASK-014")
FORBIDDEN = (
    r"\bFORGE\b",
    r"\bMTH\b",
    r"\bARDA\b",
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
    for p in (MATRIX, *NEW_FILES, KERNEL):
        if not p.exists():
            err(f"missing {p.relative_to(ROOT)}")


def check_claims() -> None:
    if not MATRIX.is_file():
        return
    text = MATRIX.read_text()
    for token in (
        "FORMAL_VALIDATION_CLOSURE = PASS",
        "CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED",
        "KernelInputCandidate != KernelInput",
    ):
        if token not in text:
            err(f"matrix missing {token!r}")
    if re.search(r"CPP_KERNELINPUT_EQUIVALENCE\s*=\s*(TRUE|PROVED)", text):
        err("must not claim CPP_KERNELINPUT_EQUIVALENCE proved")
    for col in MATRIX_COLS:
        if col not in text:
            err(f"matrix missing column {col}")
    for ds in DATASETS:
        if ds not in text:
            err(f"matrix missing dataset citation {ds}")
    for pat in FORBIDDEN:
        if re.search(pat, text):
            err(f"matrix forbidden token {pat}")


def check_model_api() -> None:
    if VP_V.is_file():
        text = VP_V.read_text()
        if "Definition validate_decoded_block" not in text:
            err("ValidationPipeline.v missing validate_decoded_block")
        for tok in (
            "TransactionCodec",
            "BlockCodec",
            "ValidationModel",
            "UTXOState",
        ):
            if tok not in text:
                err(f"ValidationPipeline.v must connect {tok}")
        if "From ConsensusRefinement" in text or "Require Import interfaces" in text:
            err("ValidationPipeline.v must not import refinement/interfaces.v")
        if "kernel_step" not in text or "ChainState" not in text:
            err("ValidationPipeline.v must reach kernel_step and ChainState")
    if KIC_V.is_file():
        text = KIC_V.read_text()
        if "Definition make_kernel_input_v2" not in text:
            err("KernelInputConstruction.v missing make_kernel_input_v2")
        for tok in ("pow", "asert", "fees", "amount"):
            if tok not in text.lower() and tok not in text:
                err(f"KernelInputConstruction.v missing field source {tok}")
        if "kiv2_pow_ok" not in text or "kiv2_asert_ok" not in text:
            err("KernelInputConstruction.v must source pow/asert")
        if "block_fees" not in text:
            err("KernelInputConstruction.v missing fees source")
        if "From ConsensusRefinement" in text:
            err("KernelInputConstruction.v must not import refinement")


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


def check_theorems() -> None:
    for name, path in CORE_THEOREMS:
        theorem_qed(path, name)
        if MATRIX.is_file() and f"`{name}`" not in MATRIX.read_text():
            err(f"matrix missing `{name}`")


def check_no_assumes() -> None:
    for path in NEW_FILES:
        if not path.is_file():
            continue
        for i, line in enumerate(path.read_text().splitlines(), 1):
            if ASSUME_RE.match(line):
                err(f"{path.name}:{i} {line.strip().split()[0]} forbidden")


def check_protected() -> None:
    for path, expected in FROZEN_SHA256.items():
        if not path.is_file():
            err(f"missing protected {path.relative_to(ROOT)}")
            continue
        if sha256_file(path) != expected:
            err(f"protected file changed: {path.relative_to(ROOT)}")
    if "Record KernelInput" not in KERNEL.read_text():
        err("Record KernelInput missing")


def check_coq_build() -> None:
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
    if WF.is_file() and "check_formal_validation_closure.py" not in WF.read_text():
        err("workflow must run check_formal_validation_closure.py")


def main() -> int:
    check_files()
    check_claims()
    check_model_api()
    check_theorems()
    check_no_assumes()
    check_protected()
    check_coq_build()
    check_workflow()

    if errors:
        for e in errors:
            print(f"FAIL: {e}", file=sys.stderr)
        print("FORMAL_VALIDATION_CLOSURE = FAIL")
        print("CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED")
        return 1

    print("FORMAL_VALIDATION_CLOSURE = PASS")
    print("CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED")
    return 0


if __name__ == "__main__":
    sys.exit(main())

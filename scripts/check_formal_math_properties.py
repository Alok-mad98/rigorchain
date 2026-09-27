#!/usr/bin/env python3
""" formal consensus mathematical properties gate.

Qed-only ASERT / reorg-safety model. Not C++ refinement. Not extraction.
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

MATRIX = BRIDGE / "math_property_dataset_mapping_v1.md"
WF = ROOT / ".github" / "workflows" / "formal_math_properties.yml"

ASERT_V = CM / "ASERTModel.v"
SAFE_V = CM / "ReorgSafetyModel.v"

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
    ("asert_deterministic", ASERT_V),
    ("asert_monotonic_time", ASERT_V),
    ("asert_bounded_adjustment", ASERT_V),
    ("asert_same_history_same_target", ASERT_V),
    ("common_ancestor_preserved", SAFE_V),
    ("valid_reorg_preserves_state", SAFE_V),
    ("heavier_chain_selected", SAFE_V),
    ("invalid_branch_cannot_replace", SAFE_V),
)

NEW_FILES = (ASERT_V, SAFE_V)

COMPILE = (
    CM / "TransactionModel.v",
    CM / "BlockModel.v",
    CM / "UTXOState.v",
    CM / "ValidationResult.v",
    CM / "TransactionProperties.v",
    CM / "ValidationModel.v",
    CM / "ChainState.v",
    CM / "ReorgModel.v",
    ASERT_V,
    SAFE_V,
)

ASSUME_RE = re.compile(r"^\s*(Admitted|Axiom|Parameter)\b")
MATRIX_COLS = ("Theorem", "Proof status", "Dataset citation")
DATASETS = ("", "", "", "")
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
    for p in (MATRIX, *NEW_FILES, KERNEL):
        if not p.exists():
            err(f"missing {p.relative_to(ROOT)}")


def check_claims() -> None:
    if not MATRIX.is_file():
        return
    text = MATRIX.read_text()
    for token in (
        "FORMAL_MATH_PROPERTIES = PASS",
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
    if ASERT_V.is_file():
        text = ASERT_V.read_text()
        if "Definition get_next_target_model" not in text:
            err("ASERTModel.v missing get_next_target_model")
        for tok in ("timestamp", "height", "target"):
            if tok not in text.lower() and tok not in text:
                err(f"ASERTModel.v missing {tok}")
        if "From ConsensusRefinement" in text:
            err("ASERTModel.v must not import refinement")
        if "From RigorKernel" in text:
            err("ASERTModel.v must not import RigorKernel")
    if SAFE_V.is_file():
        text = SAFE_V.read_text()
        for tok in ("Definition CommonAncestor", "Fork", "ChainWeight"):
            if tok not in text:
                err(f"ReorgSafetyModel.v missing {tok}")
        if "From ConsensusRefinement" in text or "Require Import interfaces" in text:
            err("ReorgSafetyModel.v must not import refinement/interfaces.v")


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
    if WF.is_file() and "check_formal_math_properties.py" not in WF.read_text():
        err("workflow must run check_formal_math_properties.py")


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
        print("FORMAL_MATH_PROPERTIES = FAIL")
        print("CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED")
        return 1

    print("FORMAL_MATH_PROPERTIES = PASS")
    print("CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED")
    return 0


if __name__ == "__main__":
    sys.exit(main())

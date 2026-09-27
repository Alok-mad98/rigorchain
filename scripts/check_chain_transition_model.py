#!/usr/bin/env python3
""" chain-state transition model gate.

Qed-only on ChainState / ReorgModel. Not C++ refinement. Not extraction.
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

MATRIX = BRIDGE / "chain_transition_theorem_matrix_v1.md"
IFACE = REF / "interfaces.v"
WF = ROOT / ".github" / "workflows" / "chain_transition_model.yml"
CHAIN_V = CM / "ChainState.v"
REORG_V = CM / "ReorgModel.v"

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
    ("connect_then_disconnect_inverse", CHAIN_V),
    ("disconnect_then_connect_inverse", CHAIN_V),
    ("accepted_block_preserves_state_wf", CHAIN_V),
    ("rejected_block_no_state_change", CHAIN_V),
    ("deterministic_chain_transition", CHAIN_V),
    ("reorg_preserves_validity", REORG_V),
    ("common_ancestor_exists", REORG_V),
    ("reorg_disconnect_connect_consistent", REORG_V),
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
    CM / "ValidationModel.v",
    CM / "KernelStepModel.v",
    CHAIN_V,
    REORG_V,
)

ASSUME_RE = re.compile(r"^\s*(Admitted|Axiom|Parameter)\b")
MATRIX_COLS = ("Theorem", "Status", "Dataset reference", "Future refinement dependency")
DATASETS = ("", "", "")
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
    for p in (MATRIX, CHAIN_V, REORG_V, KERNEL):
        if not p.exists():
            err(f"missing {p.relative_to(ROOT)}")


def check_claims() -> None:
    if not MATRIX.is_file():
        return
    text = MATRIX.read_text()
    for token in (
        "CHAIN_TRANSITION_MODEL_STATUS = PASS",
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
    if CHAIN_V.is_file():
        text = CHAIN_V.read_text()
        for tok in (
            "Record ChainState",
            "cs_height",
            "current tip",
            "Definition connect_block_model",
            "Definition disconnect_block_model",
            "Block validation",
            "Chain selection",
        ):
            if tok not in text and tok != "Record ChainState":
                pass
        if "Definition ChainState" not in text and "Record ChainState" not in text:
            err("ChainState.v missing ChainState")
        if "chain_height" not in text:
            err("ChainState.v missing height")
        if "chain_tip" not in text:
            err("ChainState.v missing tip")
        if "chain_utxo" not in text:
            err("ChainState.v missing UTXOState")
        if "ChainMeta" not in text:
            err("ChainState.v missing chain metadata")
        if "Definition connect_block_model" not in text:
            err("missing connect_block_model")
        if "Definition disconnect_block_model" not in text:
            err("missing disconnect_block_model")
        if "Block validation" not in text or "Chain selection" not in text:
            err("ChainState.v must distinguish validation vs selection")
    if REORG_V.is_file():
        text = REORG_V.read_text()
        for tok in ("ForkA", "ForkB", "ChainWeight", "Definition reorg_model"):
            if tok not in text:
                err(f"ReorgModel.v missing {tok}")


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
    for path in (CHAIN_V, REORG_V):
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
    if WF.is_file() and "check_chain_transition_model.py" not in WF.read_text():
        err("workflow must run check_chain_transition_model.py")


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
        print("CHAIN_TRANSITION_MODEL_STATUS = FAIL")
        return 1

    print("CHAIN_TRANSITION_MODEL_STATUS = PASS")
    print("CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED")
    return 0


if __name__ == "__main__":
    sys.exit(main())

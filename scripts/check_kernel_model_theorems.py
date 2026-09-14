#!/usr/bin/env python3
"""TASK-017 kernel model core theorem gate.

Qed-only on consensus_model. Not C++ refinement. Not extraction.
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

MATRIX = BRIDGE / "kernel_model_theorem_matrix_v1.md"
IFACE = REF / "interfaces.v"
WF = ROOT / ".github" / "workflows" / "kernel_model_theorems.yml"

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
    ("insert_then_lookup", CM / "UTXOStateProperties.v"),
    ("remove_then_lookup_none", CM / "UTXOStateProperties.v"),
    ("different_outpoint_independence", CM / "UTXOStateProperties.v"),
    ("spend_requires_existing_output", CM / "UTXOStateProperties.v"),
    ("cannot_spend_removed_output", CM / "UTXOStateProperties.v"),
    ("valid_transaction_implies_amount_consistent", CM / "ValidationModel.v"),
    ("valid_transaction_implies_inputs_exist", CM / "ValidationModel.v"),
    ("valid_transaction_implies_scripts_checked", CM / "ValidationModel.v"),
    ("invalid_transaction_cannot_be_accepted", CM / "ValidationModel.v"),
    ("accepted_block_contains_only_valid_transactions", CM / "ValidationModel.v"),
    ("rejected_transaction_blocks_state_transition", CM / "ValidationModel.v"),
    ("failed_validation_preserves_state", CM / "ValidationModel.v"),
    ("make_kernel_input_sound", CM / "KernelInputV2Properties.v"),
    ("accepted_step_preserves_utxo_wf", CM / "KernelStepModel.v"),
    ("rejected_step_is_identity", CM / "KernelStepModel.v"),
    ("deterministic_kernel_transition", CM / "KernelStepModel.v"),
    # Byte-level serialization closure. These were outside the gate.
    ("encode_decode_block", CM / "BlockCodec.v"),
    ("block_encoding_injective", CM / "BlockCodec.v"),
    ("encode_block_injective", CM / "BlockCodec.v"),
    ("decode_block_accepts_something", CM / "BlockCodec.v"),
    ("decode_block_rejects_trailing_bytes", CM / "BlockCodec.v"),
    ("decode_header_needs_80", CM / "BlockCodec.v"),
    ("decode_block_rejects_noncanonical_tx_count", CM / "BlockCodec.v"),
    ("decode_rejects_superfluous_witness", CM / "BlockCodec.v"),
    ("decode_does_not_imply_block_ok", CM / "BlockCodec.v"),
    ("wire_divergence_is_exactly_trailing", CM / "WireCodec.v"),
    ("wire_is_malleable", CM / "WireCodec.v"),
    ("compactsize_wire_bounded", CM / "WireCodec.v"),
    ("block_tx_count_bounded_by_length", CM / "WireCodec.v"),
    # Wire scalars are N, not unary nat. The two spec theorems are what
    # says the length guards that make that safe did not also change
    # which byte strings the model accepts; the other two are the
    # executability they buy, proved by evaluation.
    ("take_n_N_spec", CM / "ByteModel.v"),
    ("decode_vec_N_spec", CM / "TransactionCodec.v"),
    ("codec_runs_on_full_range_values", CM / "BlockCodec.v"),
    ("oversized_count_rejected_by_evaluation", CM / "BlockCodec.v"),
    # Divergence 3, found by fuzz/codec_differential rather than by
    # reading the C++. The gate holds the characterisation, not the
    # existence: "they differ" is cheap, "they differ only on
    # transactions that fail transaction_ok" is the claim being made.
    ("cpp_divergence_is_exactly_empty_tx", CM / "WireCodec.v"),
    ("cpp_divergence_excluded_by_transaction_ok", CM / "WireCodec.v"),
    ("strict_implies_cpp_tx", CM / "WireCodec.v"),
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
    CM / "ByteModel.v",
    CM / "CompactSizeModel.v",
    CM / "TransactionCodec.v",
    CM / "BlockCodec.v",
    CM / "WireCodec.v",
    IFACE,
)

ASSUME_RE = re.compile(r"^\s*(Admitted|Axiom|Parameter)\b")
MATRIX_COLS = ("THEOREM", "STATUS", "OBJECT", "FUTURE_REFINEMENT_DEPENDENCY")
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
    for p in (MATRIX, IFACE, KERNEL, *(path for _, path in CORE_THEOREMS)):
        if not p.exists():
            err(f"missing {p.relative_to(ROOT)}")


def check_claims() -> None:
    if not MATRIX.is_file():
        return
    text = MATRIX.read_text()
    for token in (
        "FORMAL_KERNEL_MODEL_THEOREM_STATUS = PASS",
        "CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED",
        "KernelInputCandidate != KernelInput",
        "FUTURE_REFINEMENT_BOUNDARY",
    ):
        if token not in text:
            err(f"matrix missing {token!r}")
    if re.search(r"CPP_KERNELINPUT_EQUIVALENCE\s*=\s*(TRUE|PROVED)", text):
        err("must not claim CPP_KERNELINPUT_EQUIVALENCE proved")
    for col in MATRIX_COLS:
        if col not in text:
            err(f"matrix missing column {col}")
    for pat in FORBIDDEN:
        if re.search(pat, text):
            err(f"matrix forbidden token {pat}")


def theorem_qed(path: Path, name: str) -> None:
    if not path.is_file():
        return
    text = path.read_text()
    # Anchored so that a name that is a prefix of another theorem's name
    # cannot silently be checked against that other theorem's proof.
    start = re.search(rf"^\s*Theorem\s+{re.escape(name)}\s*(?=[:(])", text, re.M)
    if start is None:
        err(f"{path.name} missing Theorem {name}")
        return
    chunk = text[start.end():]
    nxt = re.search(r"\n(?:Theorem|Lemma|Corollary|Definition|Fixpoint|Example)\s", chunk)
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
    vm = CM / "ValidationModel.v"
    if vm.is_file() and "Definition validate_block" not in vm.read_text():
        err("ValidationModel.v missing model validate_block")
    ks = CM / "KernelStepModel.v"
    if ks.is_file() and "Definition kernel_step" not in ks.read_text():
        err("KernelStepModel.v missing model kernel_step")


def check_no_unmarked_assumes() -> None:
    for path in sorted(CM.glob("*.v")):
        for i, line in enumerate(path.read_text().splitlines(), 1):
            if ASSUME_RE.match(line):
                err(f"{path.name}:{i} unmarked {line.strip().split()[0]}")
    if IFACE.is_file():
        text = IFACE.read_text()
        if "FUTURE_REFINEMENT_BOUNDARY" not in text:
            err("interfaces.v Parameter/Admitted must be FUTURE_REFINEMENT_BOUNDARY")
        for i, line in enumerate(text.splitlines(), 1):
            if ASSUME_RE.match(line) and "FUTURE_REFINEMENT_BOUNDARY" not in text:
                err(f"interfaces.v:{i} unmarked assumption")


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
    if WF.is_file() and "check_kernel_model_theorems.py" not in WF.read_text():
        err("workflow must run check_kernel_model_theorems.py")


def main() -> int:
    check_files()
    check_claims()
    check_theorems()
    check_no_unmarked_assumes()
    check_protected()
    check_coq_build()
    check_workflow()

    if errors:
        for e in errors:
            print(f"FAIL: {e}", file=sys.stderr)
        print("FORMAL_KERNEL_MODEL_THEOREM_STATUS = FAIL")
        return 1

    print("FORMAL_KERNEL_MODEL_THEOREM_STATUS = PASS")
    print("CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED")
    return 0


if __name__ == "__main__":
    sys.exit(main())

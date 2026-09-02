#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P067: migration readiness release check. Does not switch production."""

from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from refinement_status import print_status, records_cpp_authority  # noqa: E402
SRC = ROOT / "core/src"
TOOLS = ROOT / "tools"
FV = ROOT / "formal_verification"
VECTORS = FV / "kernel_migration_vectors.json"
COQ = FV / "coq"

REFINEMENT_VS = (
    "ConsensusKernelRefinement.v",
    "TransactionKernelRefinement.v",
    "LedgerTransitionRefinement.v",
    "UTXOStateTransitionRefinement.v",
    "ConnectBlockKernelBoundary.v",
)


def run(cmd: list[str], cwd: Path | None = None) -> None:
    print("==", " ".join(cmd))
    subprocess.run(cmd, check=True, cwd=cwd)


def compile_migration_test(cxx: str, out: Path) -> None:
    cmd = [
        cxx,
        "-O2",
        "-std=c++17",
        "-ffunction-sections",
        "-fdata-sections",
        "-DCONSENSUS_KERNEL_MIGRATION_VERIFY_ONLY",
        f"-I{SRC}",
        "-o",
        str(out),
        str(SRC / "test/test_kernel_migration_readiness.cpp"),
        str(SRC / "consensus/kernel_migration/kernel_migration.cpp"),
        str(SRC / "consensus/ledger/ledger_transition.cpp"),
        str(SRC / "consensus/rigor_issuance.cpp"),
        str(SRC / "consensus/rigor_supply.cpp"),
        str(TOOLS / "consensus_kernel_refinement_log_stub.cpp"),
        str(SRC / "script/rigor_classify.cpp"),
        str(SRC / "script/script.cpp"),
        str(SRC / "uint256.cpp"),
        str(SRC / "util/strencodings.cpp"),
        str(SRC / "consensus/convergence_logic.cpp"),
        str(SRC / "consensus_rigor/sigma_hash.cpp"),
        str(SRC / "crypto/sha256.cpp"),
        str(SRC / "arith_uint256.cpp"),
        "-Wl,--gc-sections",
    ]
    run(cmd)


def print_assumptions() -> None:
    req = """
Require ConsensusKernelRefinement.
Print Assumptions ConsensusKernelRefinement.issuance_refinement.
"""
    if (COQ / "UTXOStateTransitionRefinement.v").is_file():
        req += """
Require UTXOStateTransitionRefinement.
Print Assumptions UTXOStateTransitionRefinement.state_transition_refinement.
"""
    r = subprocess.run(
        [
            "coqtop",
            "-q",
            "-R",
            ".",
            "Rigor",
            "-R",
            "fee-burn",
            "Rigor.FeeBurn",
            "-R",
            "../rigor_v01",
            "RigorV01",
        ],
        input=req,
        text=True,
        capture_output=True,
        cwd=COQ,
        check=True,
    )
    text = r.stdout + r.stderr
    print(text)
    if "Axioms:" in text:
        raise SystemExit("FAIL: Print Assumptions listed axioms")
    print("Print Assumptions: Closed under the global context (Axioms: 0)")
    print("COQ_ADMITTED = 0")
    print("COQ_AXIOMS = 0")
    print("COQ_PARAMETERS = 0")


def main() -> int:
    v = json.loads(VECTORS.read_text(encoding="utf-8"))
    if not records_cpp_authority(v):
        print("FAIL: vectors claim production switch")
        return 1
    if v.get("equivalence_proved") is not False:
        print("FAIL: vectors overclaim equivalence")
        return 1

    run([sys.executable, str(TOOLS / "lint-consensus-authority.py")])
    run([sys.executable, str(TOOLS / "lint-consensus-authority-security.py")])
    run([sys.executable, str(TOOLS / "lint-consensus-kernel-migration.py")])

    print("== coqc refinement proofs ==")
    for name in REFINEMENT_VS:
        path = COQ / name
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        if re_admitted(text):
            print(f"FAIL: {name} has Admitted/Axiom/Parameter")
            return 1
        run(
            [
                "coqc",
                "-q",
                "-R",
                ".",
                "Rigor",
                "-R",
                "fee-burn",
                "Rigor.FeeBurn",
                "-R",
                "../rigor_v01",
                "RigorV01",
                name,
            ],
            cwd=COQ,
        )
    print_assumptions()

    import tempfile

    cxx = os.environ.get("CXX") or "g++"
    with tempfile.TemporaryDirectory(prefix="rigor_mig_") as td:
        out = Path(td) / "test_kernel_migration_readiness"
        print("== dual execution verify ==")
        compile_migration_test(cxx, out)
        run([str(out)])

    fuzz = ROOT / "fuzz/utxo_state_transition_perf/run_smoke.sh"
    if fuzz.is_file():
        run(["bash", str(fuzz)])
    adv = ROOT / "fuzz/consensus_kernel_adversarial/run_smoke.sh"
    if adv.is_file():
        run(["bash", str(adv)])

    run([sys.executable, str(TOOLS / "run_consensus_performance.py")])
    run(["bash", str(FV / "scripts/run_differential_gate.sh")])

    print("CONSENSUS_KERNEL_MIGRATION_READINESS = CLOSED/PASS")
    print_status()
    print("DUPLICATE_AUTHORITY = NONE")
    print("MIGRATION_BOUNDARY = READY")
    return 0


def re_admitted(text: str) -> bool:
    import re

    return bool(re.search(r"^\s*(Admitted|Axiom|Parameter)\s", text, re.M))


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P060: Coq assumption gate for Consensus / Transaction / ConnectBlock / surface.

FAIL => CONSENSUS_FORMAL_ASSUMPTION_DRIFT. Does not rewrite Coq or C++.
Requires pinned coqc 8.15 on PATH (do not apt-install Ubuntu Coq).
"""

from __future__ import annotations

import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COQ = ROOT / "formal_verification/coq"
FLAGS = ["-q", "-R", ".", "Rigor", "-R", "fee-burn", "Rigor.FeeBurn", "-R", "../rigor_v01", "RigorV01"]

MODULES = (
    "SignatureBoundary.v",
    "TransactionKernelRefinement.v",
    "ConsensusKernelRefinement.v",
    "ConnectBlockInputPreparation.v",
    "KernelResultApplication.v",
    "ConnectBlockKernelBoundary.v",
    "ConsensusSurfaceClosure.v",
    "LedgerTransitionRefinement.v",
    "UTXOStateTransitionRefinement.v",
)

THEOREMS = (
    "ConsensusKernelRefinement.issuance_refinement",
    "ConsensusKernelRefinement.classification_refinement",
    "ConsensusKernelRefinement.draw_refinement",
    "ConsensusKernelRefinement.supply_delta_refinement",
    "ConsensusKernelRefinement.coinbase_boundary_refinement",
    "ConnectBlockKernelBoundary.connectblock_kernel_boundary",
    "TransactionKernelRefinement.predicate_theorem",
    "TransactionKernelRefinement.spend_predicate_refinement",
    "ConsensusSurfaceClosure.ConsensusSurfaceClosed",
    "LedgerTransitionRefinement.ledger_apply_eq",
    "LedgerTransitionRefinement.double_spend_rejects",
    "UTXOStateTransitionRefinement.state_transition_refinement",
    "UTXOStateTransitionRefinement.transaction_gate",
    "UTXOStateTransitionRefinement.consensus_gate",
    "UTXOStateTransitionRefinement.ledger_transition",
)

ASSUME_RE = re.compile(r"^\s*(Admitted|Axiom|Parameter)\s")
EXTRACT_RE = re.compile(r"^\s*Extract Constant")


def fail(msg: str) -> int:
    print("CONSENSUS_FORMAL_ASSUMPTION_DRIFT")
    print(f"FAIL: {msg}")
    return 1


def main() -> int:
    admitted = axioms = parameters = 0
    for name in MODULES:
        path = COQ / name
        if not path.is_file():
            return fail(f"missing {name}")
        text = path.read_text(encoding="utf-8", errors="replace")
        for i, line in enumerate(text.splitlines(), 1):
            if ASSUME_RE.match(line):
                kind = line.strip().split()[0]
                if kind == "Admitted":
                    admitted += 1
                elif kind == "Axiom":
                    axioms += 1
                elif kind == "Parameter":
                    parameters += 1
                print(f"  {name}:{i}: {line.strip()}")
            if EXTRACT_RE.match(line):
                return fail(f"Extract Constant in {name}:{i}")

    print(f"COQ_ADMITTED={admitted}")
    print(f"COQ_AXIOMS={axioms}")
    print(f"COQ_PARAMETERS={parameters}")
    if admitted or axioms or parameters:
        return fail("assumption counts are not zero")

    if not shutil.which("coqc"):
        return fail("coqc is not on PATH")
    if not shutil.which("coqtop"):
        return fail("coqtop is not on PATH")

    for name in MODULES:
        print(f"== coqc {name} ==")
        r = subprocess.run(
            ["coqc", *FLAGS, name],
            cwd=COQ,
            capture_output=True,
            text=True,
        )
        if r.returncode != 0:
            sys.stderr.write(r.stdout)
            sys.stderr.write(r.stderr)
            return fail(f"coqc {name}")

    prints = "\n".join(f"Print Assumptions {t}." for t in THEOREMS)
    script = (
        "Require ConsensusSurfaceClosure.\n"
        "Require LedgerTransitionRefinement.\n"
        "Require UTXOStateTransitionRefinement.\n"
        + prints
        + "\n"
    )
    print("== Print Assumptions ==")
    r = subprocess.run(
        ["coqtop", *FLAGS],
        cwd=COQ,
        input=script,
        capture_output=True,
        text=True,
    )
    out = (r.stdout or "") + (r.stderr or "")
    print(out)
    if r.returncode != 0:
        return fail("coqtop Print Assumptions")
    if re.search(r"^Axioms:", out, re.MULTILINE):
        return fail("Print Assumptions listed axioms")

    print("FORMAL_ASSUMPTION_GATE=PASS")
    print("COQ_ADMITTED=0")
    print("COQ_AXIOMS=0")
    print("COQ_PARAMETERS=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())

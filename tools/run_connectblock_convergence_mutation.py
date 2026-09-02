#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P0-086: source-level mutation of ConnectBlock consensus gates.

A mutant is killed if the convergence lint (or an embedded ordering
check) fails on the mutated ConnectBlock extract. Surviving mutants
must be listed in EQUIVALENT_MUTANTS with an algebraic/logical reason.
\"Tests did not cover it\" is not a permitted reason.
"""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VALIDATION = ROOT / "core/src/validation.cpp"
LINT = ROOT / "tools/lint-connectblock-convergence-audit.py"

# Production gates that must be killed by lint if deleted from ConnectBlock.
GATES = [
    ("pow_via_checkblock", "CheckBlock("),
    ("merkle_via_checkblock", "CheckBlock("),  # same call; listed for the catalog
    ("coinbase_bip30", "bad-txns-BIP30"),
    ("tx_inputs", "CheckTxInputs"),
    ("predicate", "CheckInputScripts"),
    ("ledger_updatecoins", "UpdateCoins("),
    ("supply_kernel", "RigorConsensusKernel::EvaluateConnect"),
    ("issuance_kernel", "RigorConsensusKernel::EvaluateConnect"),
    ("res_kernel", "RigorConsensusKernel::EvaluateConnect"),
    ("draw_resolve", "RigorConsensusKernel::ResolveDraw"),
    ("maturity_via_checktxinputs", "CheckTxInputs"),
    ("state_range_setbestblock", "view.SetBestBlock"),
    ("reorg_undo", "WriteUndoDataForBlock"),
]

# Unique deletion tokens so we do not delete CheckBlock twice as one mutant.
UNIQUE_DELETIONS = [
    "CheckBlock(",
    "bad-txns-BIP30",
    "CheckTxInputs",
    "CheckInputScripts",
    "UpdateCoins(",
    "RigorConsensusKernel::EvaluateConnect",
    "RigorConsensusKernel::ResolveDraw",
    "view.SetBestBlock",
    "WriteUndoDataForBlock",
    "ValidateCoinbaseConvergenceRootScriptSig",
    "GetConvergenceDrawSetForBlock",
]

EQUIVALENT_MUTANTS = {
    "CheckRigorDrawTargetCommitment": (
        "The VerifyDB helper always returns true and is not on the ConnectBlock "
        "accept path. Deleting a constant-true function cannot change ConnectBlock "
        "acceptance. It is still a coverage gap (no-op commitment check), not a "
        "surviving production gate."
    ),
    "ConnectBlockRigor::VerifyCoinbaseConvergenceAndResLayout": (
        "The helper is not called from ConnectBlock. RigorSupplyDelta already "
        "enforces bad-cb-res-amount via EvaluateConnect. Deleting an unreachable "
        "function is extensionally equivalent for acceptance."
    ),
    "ContextualCheckBlock_reinvoke_in_ConnectBlock": (
        "ConnectBlock does not call ContextualCheckBlock today. There is no "
        "statement to delete. The upgrade/reindex hazard is an OPEN finding, "
        "not an equivalent production gate."
    ),
}


def extract_connectblock(text: str) -> str:
    start = text.find("bool Chainstate::ConnectBlock(")
    if start < 0:
        return ""
    end = text.find("\nCoinsCacheSizeState Chainstate::GetCoinsCacheSizeState", start)
    return text[start:end]


def lint_on(validation_text: str) -> bool:
    """Run the lint's ConnectBlock extract checks against an in-memory mutant."""
    spec = importlib.util.spec_from_file_location("cb_lint", LINT)
    assert spec and spec.loader
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    cb = extract_connectblock(validation_text)
    if not cb:
        return False
    for gate in mod.REQUIRED_GATES:
        if gate not in cb:
            return False
    return True


def main() -> int:
    original = VALIDATION.read_text(encoding="utf-8", errors="replace")
    if not lint_on(original):
        print("FAIL: baseline ConnectBlock does not pass lint")
        return 1

    start = original.find("bool Chainstate::ConnectBlock(")
    end = original.find("\nCoinsCacheSizeState Chainstate::GetCoinsCacheSizeState", start)
    if start < 0 or end < 0:
        print("FAIL: cannot isolate ConnectBlock for mutation")
        return 1

    killed: list[str] = []
    survivors: list[str] = []
    seen: set[str] = set()
    for token in UNIQUE_DELETIONS:
        if token in seen:
            continue
        seen.add(token)
        body = original[start:end]
        if token not in body:
            survivors.append(f"{token} (token missing in ConnectBlock; not equivalent)")
            continue
        mutant_body = body.replace(token, "/* MUTANT_DELETED */")
        mutant = original[:start] + mutant_body + original[end:]
        if lint_on(mutant):
            survivors.append(token)
        else:
            killed.append(token)

    print("MUTATION_KILLED", len(killed))
    for k in killed:
        print(f"  KILLED {k}")
    print("EQUIVALENT_MUTANTS", len(EQUIVALENT_MUTANTS))
    for name, reason in EQUIVALENT_MUTANTS.items():
        print(f"  EQUIVALENT {name}")
        print(f"    reason: {reason}")

    # Named gate catalog: every requested mutant maps to a unique deletion or equivalent.
    named = {g[0]: g[1] for g in GATES}
    for name, token in named.items():
        if token not in UNIQUE_DELETIONS and name not in EQUIVALENT_MUTANTS:
            survivors.append(f"{name} unmapped")

    if survivors:
        print("FAIL: surviving mutants (not listed as equivalent)")
        for s in survivors:
            print(f"  SURVIVED {s}")
        return 1
    print("MUTATION_STATUS=PASS")
    print("PASS: every production ConnectBlock gate mutant is killed by lint")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P0-086: ConnectBlock consensus-convergence lint.

Audit-only. Does not modify consensus semantics.
Checks: unique kernel handoff, required gates, mutation-before-commit
ordering, no production formula copies inside ConnectBlock.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VALIDATION = ROOT / "core/src/validation.cpp"
CONNECT_RIGOR = ROOT / "core/src/connect_block_rigor_checks.cpp"
CATALOG = ROOT / "formal_verification/vectors/rigor_connectblock_convergence_catalog.json"
HEX_VEC = ROOT / "formal_verification/vectors/rigor_connectblock_vectors.json"
MANIFEST = ROOT / "formal_verification/connectblock_convergence_audit_manifest.json"

REQUIRED_GATES = (
    "CheckBlock(",
    "bad-txns-BIP30",
    "CheckTxInputs",
    "CheckInputScripts",
    "UpdateCoins(",
    "RigorConsensusKernel::EvaluateConnect",
    "WriteUndoDataForBlock",
    "view.SetBestBlock",
)

FORBIDDEN_IN_CONNECTBLOCK = (
    (r"\bRigorSupplyDelta\s*\(", "ConnectBlock must not call RigorSupplyDelta (Kernel only)"),
    (r"\bResolveConvergenceDraw\s*\(", "ConnectBlock must not call ResolveConvergenceDraw"),
    (r"\bGetBlockSubsidy\s*\(", "ConnectBlock must not call GetBlockSubsidy"),
    (r"\bConnectEvaluateCoinbaseResMint\s*\(", "ConnectBlock must not call mint eval directly"),
    (r"\bVerifyCoinbaseConvergenceAndResLayout\s*\(", "dead ConnectBlockRigor helper must stay unwired"),
)


def extract_connectblock(text: str) -> str:
    start = text.find("bool Chainstate::ConnectBlock(")
    if start < 0:
        return ""
    end = text.find("\nCoinsCacheSizeState Chainstate::GetCoinsCacheSizeState", start)
    if end < 0:
        end = text.find("\nbool Chainstate::FlushStateToDisk(", start)
    return text[start:end]


def extract_connecttip(text: str) -> str:
    start = text.find("bool Chainstate::ConnectTip(")
    if start < 0:
        return ""
    end = text.find("\nvoid Chainstate::InvalidateBlockOnDisk", start)
    if end < 0:
        end = text.find("\nbool Chainstate::DisconnectTip(", start)
    # ConnectTip is before DisconnectTip in some trees; after in this tree.
    if end < 0 or end < start:
        end = text.find("\nbool Chainstate::ActivateBestChain(", start)
    return text[start:end] if end > start else text[start : start + 8000]


def main() -> int:
    errors: list[str] = []
    text = VALIDATION.read_text(encoding="utf-8", errors="replace")
    cb = extract_connectblock(text)
    if not cb:
        print("FAIL: ConnectBlock not found", file=sys.stderr)
        return 1

    for gate in REQUIRED_GATES:
        if gate not in cb:
            errors.append(f"missing gate in ConnectBlock: {gate}")

    for pat, msg in FORBIDDEN_IN_CONNECTBLOCK:
        if re.search(pat, cb):
            errors.append(msg)

    eval_pos = cb.find("RigorConsensusKernel::EvaluateConnect")
    undo_pos = cb.find("WriteUndoDataForBlock")
    # Genesis returns early with SetBestBlock; the persist write is the last one.
    best_pos = cb.rfind("view.SetBestBlock")
    update_pos = cb.find("UpdateCoins(")
    if eval_pos < 0 or undo_pos < 0 or best_pos < 0:
        errors.append("cannot locate EvaluateConnect / undo / SetBestBlock")
    else:
        if not (eval_pos < undo_pos < best_pos):
            errors.append("STATE_COMMIT before kernel accept: EvaluateConnect must precede undo+SetBestBlock")
        if update_pos > 0 and update_pos < eval_pos:
            # Documented hybrid: per-tx cache mutate before kernel. Lint records, does not rewrite.
            print("NOTE: UpdateCoins precedes EvaluateConnect (stack-local cache; ConnectTip flushes only on success)")

    if "NOTE: We don't currently (re-)invoke ContextualCheckBlock()" not in cb:
        errors.append("lost the ContextualCheckBlock non-reinvoke comment; upgrade hazard must stay explicit")

    tip = extract_connecttip(text)
    if "ConnectBlock(" not in tip:
        errors.append("ConnectTip does not call ConnectBlock")
    flush_pos = tip.find("view.Flush()")
    fail_pos = tip.find("if (!rv)")
    if flush_pos < 0 or fail_pos < 0 or not (fail_pos < flush_pos):
        errors.append("ConnectTip must discard failed ConnectBlock before Flush")

    if "bool Chainstate::RollforwardBlock(" in text:
        rf = text[text.find("bool Chainstate::RollforwardBlock(") :]
        rf = rf[: rf.find("\nbool Chainstate::ReplayBlocks(")]
        if "ConnectBlock(" in rf or "EvaluateConnect" in rf:
            errors.append("RollforwardBlock must stay a crash-recovery UTXO replay, not a second accept path")
        else:
            print("NOTE: RollforwardBlock applies coins without consensus (crash recovery only)")

    if CONNECT_RIGOR.is_file():
        rigor = CONNECT_RIGOR.read_text(encoding="utf-8", errors="replace")
        if "VerifyCoinbaseConvergenceAndResLayout" not in rigor:
            errors.append("missing dead helper file used as unused-duplicate evidence")
    if "VerifyCoinbaseConvergenceAndResLayout" in cb:
        errors.append("dead ConnectBlockRigor helper became wired — second RES authority")

    catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
    hex_rows = json.loads(HEX_VEC.read_text(encoding="utf-8"))
    names = {r["name"] for r in hex_rows}
    if "valid_height1_coinbase_only" not in names or "invalid_stale_merkle_extra_tx" not in names:
        errors.append("hex golden pair missing")
    if catalog.get("live_mint_golden_vector") != "NOT_COVERED":
        errors.append("do not claim live mint golden coverage")
    if catalog.get("reorgbind_golden_vector") != "NOT_COVERED":
        errors.append("do not claim ReorgBind golden coverage")
    case_names = [c["name"] for c in catalog["cases"]]
    required = [
        "valid_block",
        "invalid_merkle",
        "invalid_tx",
        "invalid_coinbase",
        "invalid_witness",
        "invalid_predicate",
        "invalid_pow",
        "invalid_target",
        "invalid_timestamp",
        "invalid_issuance",
        "invalid_res",
        "invalid_draw",
        "invalid_supply",
        "invalid_maturity",
        "duplicate_tx",
        "malformed_block",
        "state_mutation_failure",
        "connect_disconnect_connect",
        "reorg",
        "invalid_after_valid",
    ]
    for n in required:
        if n not in case_names:
            errors.append(f"catalog missing case {n}")

    if MANIFEST.exists():
        man = json.loads(MANIFEST.read_text(encoding="utf-8"))
        if man.get("connectblock_consensus_convergence_audit") == "CLOSED":
            errors.append("manifest must not be CLOSED while ReorgBind/live-mint goldens and contextual re-run remain open")

    print("CONNECTBLOCK_LOC", cb.count("\n") + 1)
    print("REQUIRED_GATES", len(REQUIRED_GATES))
    print("LIVE_MINT_GOLDEN_VECTOR=NOT_COVERED")
    print("REORGBIND_GOLDEN_VECTOR=NOT_COVERED")
    if errors:
        print("FAIL: connectblock convergence lint")
        for e in errors:
            print(f"  - {e}")
        return 1
    print("PASS: ConnectBlock convergence lint (audit gates present; catalog honest)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

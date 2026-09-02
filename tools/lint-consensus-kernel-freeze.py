#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P068: freeze unique authorities. No formula in ConnectBlock. No OCaml leak."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from refinement_status import cpp_authority_reason, print_status  # noqa: E402
SRC = ROOT / "core/src"
MAKEFILE = SRC / "Makefile.am"
VALIDATION = SRC / "validation.cpp"
TEST_INC = SRC / "Makefile.test.include"
MANIFEST = ROOT / "formal_verification/consensus_kernel_freeze_manifest.json"
VECTORS = ROOT / "formal_verification/consensus_kernel_freeze_vectors.json"
RELEASE = ROOT / "formal_verification/release_consensus_vectors.json"

FROZEN = {
    "issuance": "GetBlockSubsidy",
    "res_schedule": "GetResMintReward",
    "draw": "ResolveConvergenceDraw",
    "classification": "ClassifyRigorTxout",
    "supply": "RigorSupplyDelta",
    "spend": "VerifyRigorPredicate",
    "ledger": "LedgerApply",
    "pow_boundary": "PowBoundary",
}

FORMULA = re.compile(
    r"nHeight\s*/\s*nSubsidyHalvingInterval|"
    r"base\s*>>\s*halvings|"
    r"Mul256x256Full|"
    r"oru_circulating_delta\s*="
)
CLASSIFY_TRIPLE = ("IsWitnessProgram", "ParseResMintScript", "ParseConvergeScript")
OCAML = re.compile(r"consensus_kernel\.ml|caml_startup|\bcaml_|libasmrun")


def fail(errors: list[str]) -> int:
    print("CONSENSUS_AUTHORITY_FREEZE_VIOLATION")
    print("CONSENSUS_KERNEL_FREEZE_DRIFT")
    print("FAIL: consensus kernel freeze lint")
    for e in errors:
        print(f"  - {e}")
    return 1


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//.*?$", "", text, flags=re.MULTILINE)
    return text


def extract_connectblock(text: str) -> str:
    start = text.find("bool Chainstate::ConnectBlock(")
    if start < 0:
        return ""
    end = text.find("\nbool Chainstate::FlushStateToDisk(", start)
    if end < 0:
        end = text.find("\nCoinsCacheSizeState Chainstate::GetCoinsCacheSizeState", start)
    return text[start:end]


def main() -> int:
    errors: list[str] = []
    man = json.loads(MANIFEST.read_text(encoding="utf-8"))
    vec = json.loads(VECTORS.read_text(encoding="utf-8"))

    authorities = man.get("authorities")
    if not isinstance(authorities, dict):
        return fail(["freeze manifest missing authorities"])
    if set(authorities) != set(FROZEN):
        extras = set(authorities) - set(FROZEN)
        missing = set(FROZEN) - set(authorities)
        if extras:
            errors.append(f"new authority domain(s): {sorted(extras)}")
        if missing:
            errors.append(f"missing frozen domain(s): {sorted(missing)}")
    for domain, name in FROZEN.items():
        if authorities.get(domain) != name:
            errors.append(f"{domain} must remain {name}")

    def contains_retired_key(obj: object) -> bool:
        if isinstance(obj, dict):
            for k, v in obj.items():
                if str(k).lower() == "production_kernel_switched":
                    return True
                if contains_retired_key(v):
                    return True
        elif isinstance(obj, list):
            return any(contains_retired_key(x) for x in obj)
        return False
    if contains_retired_key(man):
        errors.append(
            "retired PRODUCTION_KERNEL_SWITCHED must not appear; "
            "C++ production is PRODUCTION_CONSENSUS_AUTHORITY, "
            "extracted cutover is EXTRACTED_KERNEL_PRODUCTION"
        )
    ok, reason = cpp_authority_reason(man)
    if not ok:
        errors.append(f"manifest must record C++ production authority ({reason})")
    if man.get("equivalence_proved") is not False:
        errors.append("manifest overclaims equivalence")
    if man.get("candidate_equals_production") is not False:
        errors.append("manifest must keep candidate != production")

    if vec.get("genesis", {}).get("GetResMintReward") != 40000000:
        errors.append("vectors GetResMintReward(0) pin drifted")
    if vec.get("genesis", {}).get("res_issued_at_height") != 0:
        errors.append("vectors res_issued_at_height(0) pin drifted")
    for h in (0, 1, 99, 100, 101, 209999, 210000, 419999, 420000):
        if h not in (vec.get("issuance_heights") or []):
            errors.append(f"vectors missing issuance height {h}")
    for name in ("empty_F", "single_candidate", "equal_weight", "different_txid", "T_eq_0",
                 "overflow_weight", "duplicate_tx"):
        if name not in (vec.get("draw") or []):
            errors.append(f"vectors missing draw {name}")
    for name in ("valid_ORU", "valid_RES", "unknown_version", "malformed_bytes",
                 "oversized_payload", "empty_script"):
        if name not in (vec.get("classify") or []):
            errors.append(f"vectors missing classify {name}")
    for name in ("bad_signature", "wrong_pubkey", "mutated_witness", "wrong_digest", "replay_domain"):
        if name not in (vec.get("transaction") or []):
            errors.append(f"vectors missing transaction {name}")
    for name in ("double_spend", "negative_value", "overflow", "duplicate_input", "unknown_asset"):
        if name not in (vec.get("ledger") or []):
            errors.append(f"vectors missing ledger {name}")

    if RELEASE.is_file():
        rel = json.loads(RELEASE.read_text(encoding="utf-8"))
        freeze_epoch = {row["height"]: row for row in vec.get("epoch") or [] if "res_schedule" in row}
        for row in rel.get("epoch") or []:
            pinned = freeze_epoch.get(row["height"])
            if not pinned:
                continue
            for key in ("oru", "res_schedule", "epoch"):
                if pinned.get(key) != row.get(key):
                    errors.append(f"freeze epoch {row['height']} {key} drifted from refinement pin")

    validation = VALIDATION.read_text(encoding="utf-8", errors="replace")
    cb = strip_comments(extract_connectblock(validation))
    if not cb:
        errors.append("ConnectBlock not found")
    else:
        if FORMULA.search(cb):
            errors.append("validation.cpp ConnectBlock contains a subsidy/draw/supply formula")
        if all(tok in cb for tok in CLASSIFY_TRIPLE):
            errors.append("validation.cpp ConnectBlock contains a classify matcher")
        if "SelectWinner" in cb or "RunSelection" in cb:
            errors.append("validation.cpp ConnectBlock contains a draw algorithm")
        if OCAML.search(cb):
            errors.append("ConnectBlock references extracted OCaml")

    makefile = MAKEFILE.read_text(encoding="utf-8", errors="replace")
    if OCAML.search(makefile) or "consensus_kernel.ml" in makefile:
        errors.append("Makefile.am links extracted OCaml")
    if "consensus/kernel_migration" in makefile or "consensus/kernel_canary" in makefile:
        errors.append("Makefile.am compiles a migration/canary candidate into production")
    if "ENABLE_CONSENSUS_KERNEL_CANARY" in makefile:
        errors.append("Makefile.am enables the canary")

    if OCAML.search(validation):
        errors.append("validation.cpp references extracted OCaml")

    test_inc = TEST_INC.read_text(encoding="utf-8", errors="replace") if TEST_INC.is_file() else ""
    if "test_consensus_kernel_freeze.cpp" not in test_inc:
        errors.append("Boost freeze test is not registered")
    if "test_kernel_migration_freeze.cpp" in test_inc:
        errors.append("verify-only freeze dual-path must not join test_rigor")

    if errors:
        return fail(errors)
    print("AUTHORITY_FREEZE=PASS")
    print_status()
    return 0


if __name__ == "__main__":
    sys.exit(main())

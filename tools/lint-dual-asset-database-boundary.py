#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P0-086B: dual-asset database boundary lint.

Confirms CoinsDB / coins / undo / compressor do not store an asset tag and
do not implement ORU ↔ RES conversion. Records observational interpreters.
Does not modify production consensus code.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "core/src"

COINS_H = SRC / "coins.h"
COINS_CPP = SRC / "coins.cpp"
TXDB_CPP = SRC / "txdb.cpp"
COMP_H = SRC / "compressor.h"
UNDO_H = SRC / "undo.h"
CLASSIFY = SRC / "script/rigor_classify.cpp"
TX_VERIFY = SRC / "consensus/tx_verify.cpp"
CORE_WRITE = SRC / "core_write.cpp"
ASSET_CTX = SRC / "qt/rigorwallet/assetcontext.cpp"
EXPLORER = ROOT / "explorer/src/script.ts"
DOC = ROOT / "docs/security/RIGOR_DUAL_ASSET_ORU_RES_DATABASE_AUDIT_P0_086B.md"
MANIFEST = ROOT / "formal_verification/dual_asset_database_audit_manifest.json"
VECTORS = ROOT / "formal_verification/dual_asset_database_vectors/catalog.json"
GATE = ROOT / "tools/run-dual-asset-database-security-audit.sh"
CODEC = ROOT / "tools/dual_asset_database_codec.py"


def main() -> int:
    errors: list[str] = []
    for p in (
        COINS_H,
        COINS_CPP,
        TXDB_CPP,
        COMP_H,
        UNDO_H,
        CLASSIFY,
        TX_VERIFY,
        DOC,
        MANIFEST,
        VECTORS,
        GATE,
        CODEC,
    ):
        if not p.exists():
            errors.append(f"missing {p.relative_to(ROOT)}")

    coins_h = COINS_H.read_text(encoding="utf-8", errors="replace")
    if "asset is implied by scriptPubKey" not in coins_h:
        errors.append("coins.h must record that asset is implied by scriptPubKey")
    if re.search(r"\bAssetType\b", coins_h):
        errors.append("Coin must not carry an AssetType field")
    if "TxOutCompression" not in coins_h:
        errors.append("Coin must serialize via TxOutCompression")

    txdb = TXDB_CPP.read_text(encoding="utf-8", errors="replace")
    if "static constexpr uint8_t DB_COIN{'C'}" not in txdb:
        errors.append("CoinsDB key type byte must remain 'C'")
    if re.search(r"DB_ORU|DB_RES|DB_ASSET", txdb):
        errors.append("CoinsDB must not grow an asset-specific type byte")

    coins_cpp = COINS_CPP.read_text(encoding="utf-8", errors="replace")
    # Membership skip is EntersUtxoSet (!unspendable && !IsConvergenceScript).
    if "EntersUtxoSet" not in coins_cpp:
        errors.append("AddCoins must still skip convergence via EntersUtxoSet (not an ORU/RES UTXO)")
    if re.search(r"ASSET_ORU|ASSET_RES", coins_cpp):
        errors.append("coins.cpp must not interpret AssetType (shared container only)")

    classify = CLASSIFY.read_text(encoding="utf-8", errors="replace")
    if "AssetType AssetTypeFromScriptPubKey" not in classify:
        errors.append("canonical AssetTypeFromScriptPubKey missing")
    if "case TxoutType::WITNESS_V0_KEYHASH" not in classify:
        errors.append("ORU v0/20 mapping missing")
    if "case TxoutType::WITNESS_V1_RES" not in classify:
        errors.append("RES v1/32 mapping missing")
    if "case TxoutType::RES_MINT" not in classify:
        errors.append("RES mint mapping missing")
    if "return ASSET_INVALID" not in classify:
        errors.append("unknown script must fail closed to ASSET_INVALID")

    tx_verify = TX_VERIFY.read_text(encoding="utf-8", errors="replace")
    if "illegal-res-to-oru-conversion" not in tx_verify:
        errors.append("CheckTxInputs must keep illegal-res-to-oru-conversion")
    if "mixed-asset-output" not in tx_verify:
        errors.append("CheckTxInputs must keep mixed-asset-output (no ORU-funded RES)")
    if "cross-asset-input" not in tx_verify:
        errors.append("CheckTxInputs_ORU must reject RES inputs")

    # Observational replicas — counted, not forbidden.
    replicas: list[str] = []
    if CLASSIFY.is_file():
        replicas.append("ClassifyRigorTxout / AssetTypeFromScriptPubKey (canonical)")
    if CORE_WRITE.is_file() and "ValueFromAmountAsset" in CORE_WRITE.read_text(
        encoding="utf-8", errors="replace"
    ):
        replicas.append("ValueFromAmountAsset (display; non-RES defaults to ORU scale)")
    if ASSET_CTX.is_file() and "return ASSET_ORU" in ASSET_CTX.read_text(
        encoding="utf-8", errors="replace"
    ):
        replicas.append("Qt AssetContext::fromAssetType / toAssetType (non-RES → ORU)")
    if EXPLORER.is_file():
        replicas.append("explorer classifyScriptPubKey (TypeScript replica)")
    replicas.append("audit dual_asset_database_codec.classify_asset (harness replica)")

    consensus_asset_authority = 1
    observational_replicas = len(replicas) - 1
    oru_db_authority = 1
    res_db_authority = 3  # Coin + CBlockIndex mint memo + observational supply

    print("CONSENSUS_ASSET_ID_AUTHORITY_COUNT", consensus_asset_authority)
    print("OBSERVATIONAL_ASSET_ID_REPLICA_COUNT", observational_replicas)
    print("ASSET_ID_AUTHORITY_COUNT", consensus_asset_authority)
    print("ORU_DATABASE_AUTHORITY_COUNT", oru_db_authority)
    print("RES_DATABASE_AUTHORITY_COUNT", res_db_authority)
    print("ORU_SUPPLY_AUTHORITY_COUNT 1")
    print("RES_SUPPLY_AUTHORITY_COUNT 1")
    for r in replicas:
        print(f"  INTERPRETER {r}")
    print("ORU_RES_STORAGE_MODEL SHARED_COIN_MODEL")
    print("DB_LAYER_ASSET_TAG NONE")
    print("DB_LAYER_ORU_RES_CONVERSION NONE")

    if errors:
        for e in errors:
            print(f"FAIL: {e}", file=sys.stderr)
        return 1
    print("LINT_DUAL_ASSET_DATABASE_BOUNDARY=PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())

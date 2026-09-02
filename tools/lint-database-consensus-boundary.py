#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P0-086: database layer must not become a second Consensus Kernel.

Scans CoinsDB / coins view / chainstate persistence / undo / dbwrapper.
Records reconstruction paths and hidden interpretation sites.
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
TXDB_H = SRC / "txdb.h"
TXDB_CPP = SRC / "txdb.cpp"
DBWRAP_H = SRC / "dbwrapper.h"
DBWRAP_CPP = SRC / "dbwrapper.cpp"
UNDO_H = SRC / "undo.h"
COMP_H = SRC / "compressor.h"
COMP_CPP = SRC / "compressor.cpp"
VALIDATION = SRC / "validation.cpp"
DOC = ROOT / "docs/security/RIGOR_DATABASE_STATE_PERSISTENCE_SECURITY_AUDIT_P0_086.md"
MANIFEST = ROOT / "formal_verification/database_state_persistence_audit_manifest.json"
VECTORS = ROOT / "formal_verification/database_state_golden_vectors.json"
GATE = ROOT / "tools/run-database-state-persistence-security-audit.sh"
LINT = ROOT / "tools/lint-database-consensus-boundary.py"
CODEC = ROOT / "tools/database_state_codec.py"
TEST = SRC / "test/test_database_state_persistence_security.cpp"
PY_TEST = ROOT / "tests/database/test_database_state_persistence.py"
FUZZ = ROOT / "fuzz/database/database_codec_fuzz.cpp"
FUZZ_RUN = ROOT / "fuzz/database/run_smoke.sh"

KERNEL_DEFS = re.compile(
    r"^bool\s+RigorConsensusKernel::EvaluateConnect|"
    r"^bool\s+ConnectBlock\s*\(|"
    r"^DisconnectResult\s+Chainstate::DisconnectBlock|"
    r"^CAmount\s+GetBlockSubsidy\s*\(|"
    r"^RigorIssuance\s+ComputeRigorIssuance|"
    r"^ConvergenceDrawOutcome\s+ResolveConvergenceDraw|"
    r"^bool\s+VerifyRigorPredicate\s*\(|"
    r"^AssetType\s+ClassifyRigorAsset\s*\(",
    re.MULTILINE,
)

HIDDEN_VALIDATION = [
    (
        COINS_CPP,
        r"if\s*\(\s*!EntersUtxoSet\(",
        "AddCoin/AddCoins skip via EntersUtxoSet (UTXO membership)",
    ),
]

FAIL_CLOSED = [
    (
        DBWRAP_H,
        r"DESERIALIZATION_FAILURE / DATABASE_CORRUPTION",
        "CDBWrapper::Read throws on deserialize failure",
    ),
    (
        DBWRAP_H,
        r"trailing bytes",
        "CDBWrapper consume-all on key/value",
    ),
    (
        DBWRAP_H,
        r"HasKeyType",
        "Iterator prefix end is HasKeyType, not GetKey false",
    ),
    (
        COINS_H,
        r"CoinDiskFormatter",
        "Disk/undo checksum lives in CoinDiskFormatter",
    ),
    (
        COINS_H,
        r"CoinIntegrityChecksum",
        "Coin disk integrity checksum",
    ),
    (
        COMP_H,
        r"ScriptPlainFormatter: script exceeds MAX_SCRIPT_SIZE \(DATABASE_CORRUPTION\)",
        "ScriptPlainFormatter oversized fail-closed",
    ),
    (
        VALIDATION,
        r"height-0 non-coinbase undo",
        "ApplyTxInUndo height-0 non-coinbase throws",
    ),
    (
        COINS_CPP,
        r"std::abort\(\)",
        "CCoinsViewErrorCatcher abort on DB read errors",
    ),
]

RECONSTRUCTION = [
    (VALIDATION, r"bool\s+Chainstate::ReplayBlocks\s*\(", "ReplayBlocks interrupted BatchWrite"),
    (VALIDATION, r"-reindex-chainstate", "reindex-chainstate path"),
    (TXDB_CPP, r"DB_HEAD_BLOCKS", "partial-flush head-blocks marker"),
    (SRC / "node/blockstorage.cpp", r"fReindex|reindex", "block file reindex"),
    (SRC / "validation.cpp", r"PopulateAndValidateSnapshot|ActivateSnapshot", "UTXO snapshot load"),
]

# Residual sites: recorded, not a second Kernel. Must remain present so the
# audit does not silently lose them.
RESIDUAL = [
    (
        TXDB_CPP,
        r"return m_db->Exists\(CoinEntry",
        "CCoinsViewDB::HaveCoin uses Exists (no deserialize)",
    ),
    (
        COINS_CPP,
        r"bool CCoinsViewErrorCatcher::GetCoin",
        "ErrorCatcher overrides GetCoin only",
    ),
    (
        VALIDATION,
        r"CCoinsViewCache cache\(&db\)",
        "ReplayBlocks cache wraps CoinsDB, not ErrorCatcher",
    ),
]

FORBIDDEN_IN_DB_LAYER = re.compile(
    r"EvaluateConnect|GetBlockSubsidy|ComputeRigorIssuance|"
    r"ResolveConvergenceDraw|VerifyRigorPredicate|ClassifyRigorTxout|"
    r"IsValidResConsensusAmount|nCoinbaseMaturity"
)

SKIP_DIR = {
    "test",
    "bench",
    "fuzz",
    "qt",
    "leveldb",
    "secp256k1",
    "minisketch",
    "crc32c",
    "univalue",
}


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//.*?$", "", text, flags=re.MULTILINE)
    return text


def db_layer_files() -> list[Path]:
    files = [COINS_H, COINS_CPP, TXDB_H, TXDB_CPP, DBWRAP_H, DBWRAP_CPP, UNDO_H, COMP_H, COMP_CPP]
    return [p for p in files if p.is_file()]


def main() -> int:
    errors: list[str] = []
    required = (
        COINS_H,
        COINS_CPP,
        TXDB_H,
        TXDB_CPP,
        DBWRAP_H,
        UNDO_H,
        COMP_H,
        DOC,
        MANIFEST,
        VECTORS,
        GATE,
        LINT,
        CODEC,
        TEST,
        PY_TEST,
        FUZZ,
        FUZZ_RUN,
    )
    for p in required:
        if not p.exists():
            errors.append(f"missing {p.relative_to(ROOT)}")

    kernel_hits: list[str] = []
    for path in db_layer_files():
        text = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
        for m in KERNEL_DEFS.finditer(text):
            kernel_hits.append(f"{path.relative_to(ROOT)}: {m.group(0).splitlines()[0]}")

    validation_hits: list[str] = []
    for path, pat, label in HIDDEN_VALIDATION:
        if not path.is_file():
            errors.append(f"missing scan target {path.relative_to(ROOT)}")
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        if re.search(pat, text, re.DOTALL):
            validation_hits.append(label)
        else:
            errors.append(f"expected hidden-interpretation site missing: {label}")

    fail_closed_hits: list[str] = []
    for path, pat, label in FAIL_CLOSED:
        if not path.is_file():
            errors.append(f"missing scan target {path.relative_to(ROOT)}")
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        if re.search(pat, text, re.DOTALL):
            fail_closed_hits.append(label)
        else:
            errors.append(f"expected fail-closed site missing: {label}")

    recon_hits: list[str] = []
    for path, pat, label in RECONSTRUCTION:
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        if re.search(pat, text):
            recon_hits.append(label)

    residual_hits: list[str] = []
    for path, pat, label in RESIDUAL:
        if not path.is_file():
            errors.append(f"missing residual scan target {path.relative_to(ROOT)}")
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        if re.search(pat, text):
            residual_hits.append(label)
        else:
            errors.append(f"expected residual site missing: {label}")

    for path in db_layer_files():
        text = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
        for m in FORBIDDEN_IN_DB_LAYER.finditer(text):
            errors.append(
                f"database layer contains consensus symbol {m.group(0)} in {path.relative_to(ROOT)}"
            )

    # Production coins.h must keep Bitcoin layout (no trailing asset tag).
    coins_h = COINS_H.read_text(encoding="utf-8", errors="replace")
    if "VARINT((coinbase ? 1 : 0) | (height << 1))" not in coins_h:
        errors.append("Coin canonical layout comment missing from coins.h")
    if "TxOutCompression" not in coins_h:
        errors.append("Coin must serialize via TxOutCompression")
    if "CoinDiskFormatter" not in coins_h:
        errors.append("CoinDiskFormatter missing")
    if "CoinIntegrityChecksum" not in coins_h:
        errors.append("Coin integrity checksum missing")
    ser_fn = re.search(r"void Serialize\(Stream &s\) const \{.*?^    \}", coins_h, re.DOTALL | re.MULTILINE)
    if ser_fn and "CoinIntegrityChecksum" in ser_fn.group(0):
        errors.append("checksum must not live inside Coin::Serialize")

    coins_cpp = COINS_CPP.read_text(encoding="utf-8", errors="replace")
    if "std::abort()" not in coins_cpp:
        errors.append("CCoinsViewErrorCatcher must abort on LevelDB read errors")
    if "EntersUtxoSet" not in coins_cpp:
        errors.append("AddCoin/AddCoins must use EntersUtxoSet")
    if "crc32c::Crc32c" not in coins_cpp:
        errors.append("CoinIntegrityChecksum must use crc32c::Crc32c")

    dbwrap = DBWRAP_H.read_text(encoding="utf-8", errors="replace")
    if "DESERIALIZATION_FAILURE / DATABASE_CORRUPTION" not in dbwrap:
        errors.append("CDBWrapper::Read must throw DESERIALIZATION_FAILURE")
    if "HasKeyType" not in dbwrap:
        errors.append("CDBIterator::HasKeyType missing")
    if "trailing bytes" not in dbwrap:
        errors.append("CDBWrapper consume-all missing")

    comp = COMP_H.read_text(encoding="utf-8", errors="replace")
    if "script << RIGOR_MARKER_RETURN" in comp:
        errors.append("ScriptCompression still rewrites oversized scripts")
    if "ScriptPlainFormatter" not in comp:
        errors.append("TxOutCompression must use ScriptPlainFormatter")

    undo = UNDO_H.read_text(encoding="utf-8", errors="replace")
    if "using TxInUndoFormatter = CoinDiskFormatter" not in undo:
        errors.append("undo must equal disk Coin encoding")
    if "nVersionDummy" in undo:
        errors.append("Bitcoin dummy version byte must be removed from undo")

    txdb = TXDB_CPP.read_text(encoding="utf-8", errors="replace")
    if "DB_COIN_INTEGRITY" in txdb:
        errors.append("DB_COIN_INTEGRITY format marker must be removed")
    if "NeedsUpgrade" not in txdb:
        errors.append("NeedsUpgrade missing")
    if "CoinDiskFormatter" not in txdb:
        errors.append("CoinsDB GetCoin/BatchWrite must use CoinDiskFormatter")
    if "HasKeyType" not in txdb:
        errors.append("Coins cursor prefix end must use HasKeyType")

    validation = VALIDATION.read_text(encoding="utf-8", errors="replace")
    if "IsBIP30Unspendable" in validation or "IsBIP30Repeat" in validation:
        errors.append("Bitcoin BIP30 exception helpers must be removed")
    if "EntersUtxoSet" not in validation:
        errors.append("DisconnectBlock must use EntersUtxoSet")

    consensus_authority_count = len(kernel_hits)
    validation_authority_count = len(validation_hits)
    reconstruction_path_count = len(recon_hits)

    print("DATABASE_LAYER_SCAN_FILES", len(db_layer_files()))
    print("DATABASE_CONSENSUS_AUTHORITY_COUNT", consensus_authority_count)
    print("DATABASE_VALIDATION_AUTHORITY_COUNT", validation_authority_count)
    print("DATABASE_STATE_RECONSTRUCTION_PATH_COUNT", reconstruction_path_count)
    print("DATABASE_FAIL_CLOSED_SITE_COUNT", len(fail_closed_hits))
    print("DATABASE_RESIDUAL_SITE_COUNT", len(residual_hits))
    for h in kernel_hits:
        print(f"  KERNEL_DEF {h}")
    for h in validation_hits:
        print(f"  VALIDATION {h}")
    for h in fail_closed_hits:
        print(f"  FAIL_CLOSED {h}")
    for h in residual_hits:
        print(f"  RESIDUAL {h}")
    for h in recon_hits:
        print(f"  RECONSTRUCT {h}")

    if consensus_authority_count != 0:
        errors.append("database layer defines a Consensus Kernel symbol")

    if errors:
        for e in errors:
            print(f"FAIL: {e}", file=sys.stderr)
        return 1

    print("DATABASE_CONSENSUS_KERNEL_IN_DB_LAYER=NONE")
    print("DATABASE_CONSENSUS_LOGIC_DUPLICATION=RECORDED")
    print("DATABASE_FAIL_CLOSED=TRUE")
    print("LINT_DATABASE_CONSENSUS_BOUNDARY=PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())

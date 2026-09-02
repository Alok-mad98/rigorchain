#!/usr/bin/env bash
# P0-087: RIGOR database architecture / CoinsDB / dual-asset / SQLite full re-audit.
# AUDIT-FIRST. Does not modify Consensus Kernel, Transaction Kernel, Ledger,
# genesis, issuance, RES, draw, PoW, ASERT, consensus parameters, Coq proofs,
# or Release Guardian.
#
# Honest gate: production -reindex field-level comparison is required for
# CLOSED. P0-091 closes this audit by consuming the P0-088 / P0-089
# manifests and requiring Guardian Stage DATABASE_SECURITY. This script
# still must not print CLOSED unless those production tokens are PASS.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/core/src"
OUT="$ROOT/formal_verification/extracted"
TOOLS="$ROOT/tools"
CXX="${CXX:-$(command -v g++ || command -v c++)}"
MANIFEST="$ROOT/formal_verification/database_architecture_audit_manifest.json"
DOC="$ROOT/docs/security/RIGOR_DATABASE_ARCHITECTURE_FULL_REAUDIT_P0_087.md"

echo "== P087 database architecture full re-audit =="

echo "== required artifacts =="
for p in \
  "$DOC" \
  "$MANIFEST" \
  "$TOOLS/run-database-architecture-full-reaudit.sh" \
  "$TOOLS/lint-database-consensus-boundary.py" \
  "$TOOLS/lint-dual-asset-database-boundary.py" \
  "$TOOLS/database_state_codec.py" \
  "$TOOLS/dual_asset_database_codec.py" \
  "$ROOT/formal_verification/production_reindex_reconstruction_audit_manifest.json" \
  "$ROOT/formal_verification/wallet_sqlite_audit_manifest.json"
do
  if [[ ! -f "$p" ]]; then
    echo "FAIL: missing $p"
    exit 1
  fi
done

echo "== sibling authority lints (CoinsDB vs Kernel; dual-asset shared Coin) =="
python3 "$TOOLS/lint-database-consensus-boundary.py"
python3 "$TOOLS/lint-dual-asset-database-boundary.py"

echo "== architecture trust-boundary lint =="
python3 - "$ROOT" <<'PY'
from pathlib import Path
import re
import sys

root = Path(sys.argv[1])
src = root / "core/src"
errors: list[str] = []

def read(p: Path) -> str:
    if not p.exists():
        errors.append(f"missing {p.relative_to(root)}")
        return ""
    return p.read_text(encoding="utf-8", errors="replace")

coins_h = read(src / "coins.h")
coins_cpp = read(src / "coins.cpp")
txdb = read(src / "txdb.cpp")
dbwrap = read(src / "dbwrapper.h")
undo = read(src / "undo.h")
comp = read(src / "compressor.h")
wallet_db = read(src / "wallet/db.h")
wallet_walletdb = read(src / "wallet/walletdb.cpp")
wallet_sqlite = read(src / "wallet/sqlite.cpp")
config = read(src / "config/rigor-config.h")
validation = read(src / "validation.cpp")
classify = read(src / "script/rigor_classify.cpp")
guardian_yml = read(root / ".github/workflows/genesis-release-guardian.yml")
guardian_sh = read(root / "tools/run-genesis-release-guardian.sh")

# CoinsDB must not become a second Kernel.
for needle, label in (
    ("EvaluateConnect", "coins.cpp must not call EvaluateConnect"),
    ("ConnectEvaluateCoinbaseResMint", "coins.cpp must not mint"),
    ("GetBlockSubsidy", "coins.cpp must not issue"),
    ("ResolveDraw", "coins.cpp must not draw"),
):
    if needle in coins_cpp:
        errors.append(label)

if "CoinDiskFormatter" not in coins_h or "CoinIntegrityChecksum" not in coins_h:
    errors.append("Scheme A CoinDiskFormatter / CRC32C_RGCI missing from coins.h")
if "TxOutCompression" not in coins_h:
    errors.append("Coin leaf must stay TxOutCompression")
if re.search(r"\bAssetType\b", coins_h):
    errors.append("Coin must not carry AssetType (shared Coin model)")

if "static constexpr uint8_t DB_COIN{'C'}" not in txdb:
    errors.append("CoinsDB key type must remain 'C'")
if re.search(r"DB_ORU|DB_RES|ORU_DB|RES_DB", txdb):
    errors.append("forbidden second asset database / key type")
if "Using<CoinDiskFormatter>" not in txdb:
    errors.append("CCoinsViewDB must persist via CoinDiskFormatter")
if "return m_db->Exists(CoinEntry" not in txdb:
    errors.append("HaveCoin Exists residual must remain recorded")

if "DESERIALIZATION_FAILURE / DATABASE_CORRUPTION" not in dbwrap:
    errors.append("CDBWrapper::Read must throw, not map corruption to missing")
if "std::abort()" not in coins_cpp:
    errors.append("CCoinsViewErrorCatcher must abort")

if "using TxInUndoFormatter = CoinDiskFormatter" not in undo:
    errors.append("undo must share CoinDiskFormatter (no dummy version)")

if "ScriptPlainFormatter" not in comp:
    errors.append("Coin path must use ScriptPlainFormatter")
if "nValue out of MoneyRange (DATABASE_CORRUPTION)" not in comp:
    errors.append("AmountCompression must fail-closed on MoneyRange")

if "EntersUtxoSet" not in coins_cpp or "EntersUtxoSet" not in validation:
    errors.append("AddCoins / DisconnectBlock must share EntersUtxoSet")

# ConnectTip must not Flush a failed ConnectBlock.
if not re.search(
    r"bool rv = ConnectBlock\([\s\S]*?if \(!rv\) \{\s*if \(state\.IsInvalid\(\)\)\s*InvalidBlockFound[\s\S]*?return error[\s\S]*?bool flushed = view\.Flush\(\)",
    validation,
):
    errors.append("ConnectTip must Flush only after ConnectBlock returns true")

if "AssetTypeFromScriptPubKey" not in classify:
    errors.append("canonical classifier missing")

# Wallet SQLite boundary (must stay off consensus).
if 'WALLET_DATABASE_FILENAME = "rigor-wallet.sqlite"' not in wallet_db:
    errors.append("wallet filename must remain rigor-wallet.sqlite")
if "IsBerkeleyDBFile" not in wallet_walletdb or "Berkeley DB wallet" not in wallet_walletdb:
    errors.append("MakeDatabase must reject BDB (detector, not compatibility)")
if "USE_SQLITE 1" not in config:
    errors.append("build must have USE_SQLITE")
if re.search(r"#define USE_BDB", config):
    errors.append("USE_BDB must not be enabled")
if "WALLET_SCHEMA_VERSION = 0" not in wallet_sqlite:
    errors.append("SQLite schema version lock missing")
if "CREATE TABLE main(key BLOB PRIMARY KEY NOT NULL, value BLOB NOT NULL)" not in wallet_sqlite:
    errors.append("wallet must remain Bitcoin KV schema, not a second UTXO DB")
if "locking_mode" not in wallet_sqlite or "exclusive" not in wallet_sqlite:
    errors.append("SQLite exclusive lock missing")
if 'SetPragma(m_db, "fullfsync", "true"' not in wallet_sqlite:
    errors.append("SQLite fullfsync missing")

# Guardian Stage DATABASE_SECURITY is required now that this audit can close (P0-091).
if "Stage DATABASE_SECURITY" not in guardian_yml:
    errors.append("genesis-release-guardian.yml must name Stage DATABASE_SECURITY")
if "Stage DATABASE_SECURITY" not in guardian_sh:
    errors.append("run-genesis-release-guardian.sh must run Stage DATABASE_SECURITY")
if "run-database-security-guardian-stage.sh" not in guardian_sh:
    errors.append("Stage DATABASE_SECURITY must execute tools/run-database-security-guardian-stage.sh")

if errors:
    print("ARCHITECTURE_LINT=FAIL")
    for e in errors:
        print(f"  - {e}")
    raise SystemExit(1)
print("ARCHITECTURE_LINT=PASS")
print("SHARED_COIN_MODEL=PREFERRED_AND_KEPT")
print("COINSDB_TRUST_BOUNDARY=EXTERNAL_INFRASTRUCTURE_LEVELDB")
print("CHAINSTATE_TRUST_BOUNDARY=EXTERNAL_INFRASTRUCTURE_LEVELDB_PLUS_BLK_REV")
print("WALLET_SQLITE_TRUST_BOUNDARY=NON_CONSENSUS_SQLITE_KV")
PY

echo "== golden vectors + python tests =="
python3 "$TOOLS/database_state_codec.py" write
python3 "$TOOLS/database_state_codec.py" verify
python3 "$ROOT/tests/database/test_database_state_persistence.py"
python3 "$TOOLS/dual_asset_database_codec.py" write
python3 "$TOOLS/dual_asset_database_codec.py" verify
python3 "$ROOT/tests/dual_asset_database/test_dual_asset_database.py"

echo "== field-level cache / reorg / harness-rebuild differential =="
python3 - "$ROOT" <<'PY'
"""Compare UTXO records field-by-field. A state hash match is not sufficient."""
from pathlib import Path
import json
import sys

root = Path(sys.argv[1])
sys.path.insert(0, str(root / "tools"))
import database_state_codec as db  # noqa: E402
import dual_asset_database_codec as da  # noqa: E402

MARKER_MINT = getattr(db, "RIGOR_MARKER_RES_MINT", 0xBB)


def snapshot(view: db.CoinsView):
    rows = []
    for (txid, n), coin in sorted(view.coins.items()):
        asset = da.classify_asset(coin.script_pubkey)
        mint = bool(coin.script_pubkey) and coin.script_pubkey[-1] == MARKER_MINT and asset == "RES"
        rows.append(
            (
                txid,
                n,
                asset,
                coin.n_value,
                coin.height,
                bool(coin.coinbase),
                bytes(coin.script_pubkey),
                mint,
            )
        )
    return rows


def snapshot_from_blocks(blocks, mode: str):
    store = db.CoinsView()
    cache = db.CoinsViewCache(store)
    for i, block in enumerate(blocks):
        da.apply_dual(cache, block)
        if mode == "clean":
            cache.flush()
        elif mode == "cache_heavy":
            if i == len(blocks) - 1:
                cache.flush()
        elif mode == "restart":
            cache.flush()
            cache = db.CoinsViewCache(
                db.CoinsView.from_serialized(cache.base.serialize_all(), cache.base.best_block)
            )
        elif mode == "flush_restart":
            cache.flush()
            cache = cache.restart()
    cache.flush()
    return snapshot(cache.base), da.supply_of(cache.base), cache.base.state_digest()


def fail(msg: str) -> None:
    print(f"FIELD_DIFF=FAIL {msg}")
    raise SystemExit(1)


_, alt = da.golden_dual_chains()
modes = ("clean", "cache_heavy", "restart", "flush_restart")
base_rows, base_supply, base_digest = snapshot_from_blocks(alt, "clean")
for mode in modes[1:]:
    rows, supply, digest = snapshot_from_blocks(alt, mode)
    if rows != base_rows:
        fail(f"{mode} coin fields != clean")
    if supply != base_supply:
        fail(f"{mode} supply != clean")
    if digest != base_digest:
        fail(f"{mode} digest != clean")

# Connect → Disconnect → Connect (main then back) and A→B→reorg→D.
reorg, clean_alt = da.run_reorg()
if reorg["digest"] != clean_alt["digest"]:
    fail("reorg digest")
if reorg["by_asset"] != clean_alt["by_asset"]:
    fail("reorg by_asset")
if reorg["supply"] != clean_alt["supply"]:
    fail("reorg supply")

main, alt = da.golden_dual_chains()
store = db.CoinsView()
cache = db.CoinsViewCache(store)
undos = {}
for block in main:
    undos[block.block_hash] = da.apply_dual(cache, block)
    cache.flush()
after_connect = snapshot(cache.base)
da.disconnect_dual(cache, main[-1], undos[main[-1].block_hash])
da.apply_dual(cache, main[-1])
cache.flush()
after_reconnect = snapshot(cache.base)
if after_connect != after_reconnect:
    fail("CONNECT_DISCONNECT_ASYMMETRY")

rebuilt_digest, rebuilt_asset, rebuilt_supply = da.rebuild_from_blocks(alt)
live_digest, live_asset, live_supply = da.run_path(alt, "clean")
if rebuilt_digest != live_digest or rebuilt_asset != live_asset or rebuilt_supply != live_supply:
    fail("harness rebuild fields")

# Random CRC / truncation must fail closed. Recomputed CRC + mutated script reclassifies
# (inherited local trust; not a format-change P0).
oru = da.DualCoin(50_0000_0000, da.spk_oru(1), 1, True)
raw = oru.encode()
try:
    da.decode_dual(raw[:-1])
except Exception:
    truncated_rejected = True
else:
    truncated_rejected = False
if not truncated_rejected:
    fail("truncated coin must reject")

flipped = bytearray(raw)
flipped[-1] ^= 0xFF
try:
    da.decode_dual(bytes(flipped))
except Exception:
    crc_rejected = True
else:
    crc_rejected = False
if not crc_rejected:
    fail("CRC mismatch must reject")

mut = da.DualCoin(oru.n_value, da.mutate_oru_to_res(oru.script), oru.height, oru.coinbase)
if mut.asset() != "RES":
    fail("script-as-asset classifier must still be deterministic")

print("ORU_CACHE_DB_EQUIVALENCE=PASS")
print("RES_CACHE_DB_EQUIVALENCE=PASS")
print("ORU_REORG_RECOVERY=PASS")
print("RES_REORG_RECOVERY=PASS")
print("CONNECT_DISCONNECT_SYMMETRY=PASS")
print("HARNESS_REBUILD_FIELD_EQUIVALENCE=PASS")
print("MUTATION_STATUS=PASS")
print("CORRUPTION_RANDOM=ABORT")
print("MUTATION_VALID_CRC_MALICIOUS_PAYLOAD=INHERITED_LOCAL_TRUST")

vec = json.loads((root / "formal_verification/dual_asset_database_vectors/catalog.json").read_text(encoding="utf-8"))
assert vec["cache_db_equivalence"]["oru_equivalent"]
assert vec["cache_db_equivalence"]["res_equivalent"]
assert vec["reorg_recovery"]["oru_equivalent"]
assert vec["reorg_recovery"]["res_equivalent"]
PY

echo "== g++ standalone persistence + dual-asset tests =="
mkdir -p "$OUT"
"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections \
  -DDATABASE_STATE_PERSISTENCE_SECURITY_STANDALONE \
  -I"$SRC" \
  -o "$OUT/test_database_state_persistence_security" \
  "$SRC/test/test_database_state_persistence_security.cpp" \
  -Wl,--gc-sections
"$OUT/test_database_state_persistence_security"

"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections \
  -DDUAL_ASSET_DATABASE_SECURITY_STANDALONE \
  -I"$SRC" \
  -o "$OUT/test_dual_asset_database_security" \
  "$SRC/test/test_dual_asset_database_security.cpp" \
  -Wl,--gc-sections
"$OUT/test_dual_asset_database_security"

echo "== fuzz smoke =="
bash "$ROOT/fuzz/database/run_smoke.sh"
bash "$ROOT/fuzz/dual_asset_database/run_smoke.sh"

echo "== production -reindex evidence (consumed from P0-088; this gate does not re-run -reindex) =="
PROD_REINDEX="BLOCKED"
PROD_REASON="P0-088 production_full_reindex is not PASS"
if python3 - "$ROOT" <<'PY'
from pathlib import Path
import json, sys
root = Path(sys.argv[1])
p = json.loads((root / "formal_verification/production_reindex_reconstruction_audit_manifest.json").read_text(encoding="utf-8"))
ok = (
    p.get("production_full_reindex") == "PASS"
    and p.get("production_reindex_audit") == "PASS"
    and p.get("final_verdict") == "PASS"
    and int(p.get("production_rebuild_divergence_count", 1)) == 0
)
sys.exit(0 if ok else 1)
PY
then
  PROD_REINDEX="PASS"
  PROD_REASON="P0-088 production_full_reindex=PASS with field-level ORU/RES compare"
fi
echo "PRODUCTION_REINDEX_PROBE=$PROD_REINDEX"
echo "PRODUCTION_REINDEX_REASON=$PROD_REASON"

echo
echo "== P087 verdict (audit-first; not a formal proof) =="
python3 - "$ROOT" "$PROD_REINDEX" <<'PY'
from pathlib import Path
import json
import sys

root = Path(sys.argv[1])
prod_reindex = sys.argv[2]
man = json.loads((root / "formal_verification/database_architecture_audit_manifest.json").read_text(encoding="utf-8"))
vec = json.loads((root / "formal_verification/dual_asset_database_vectors/catalog.json").read_text(encoding="utf-8"))
p086 = json.loads((root / "formal_verification/database_state_golden_vectors.json").read_text(encoding="utf-8"))
p088 = json.loads((root / "formal_verification/production_reindex_reconstruction_audit_manifest.json").read_text(encoding="utf-8"))
p089 = json.loads((root / "formal_verification/wallet_sqlite_audit_manifest.json").read_text(encoding="utf-8"))
guardian_yml = (root / ".github/workflows/genesis-release-guardian.yml").read_text(encoding="utf-8")
guardian_sh = (root / "tools/run-genesis-release-guardian.sh").read_text(encoding="utf-8")

oru_cache = "PASS" if vec["cache_db_equivalence"]["oru_equivalent"] else "FAIL"
res_cache = "PASS" if vec["cache_db_equivalence"]["res_equivalent"] else "FAIL"
oru_reorg = "PASS" if vec["reorg_recovery"]["oru_equivalent"] else "FAIL"
res_reorg = "PASS" if vec["reorg_recovery"]["res_equivalent"] else "FAIL"
fail_closed = bool(p086["production_failure_boundary"]["database_fail_closed_for_value_corruption"])
mapped = bool(p086["production_failure_boundary"]["database_failure_mapped_to_consensus_invalid"])
corruption = "PASS" if fail_closed and not mapped else "FAIL"
fail_closed_s = "TRUE" if fail_closed and not mapped else "FALSE"
boundary = "CLOSED" if fail_closed_s == "TRUE" else "OPEN"

# Production field-level -reindex is consumed from P0-088. This gate does not
# re-execute rigord -reindex; a missing/failed 088 manifest stays BLOCKED.
p088_ok = (
    prod_reindex == "PASS"
    and p088.get("production_full_reindex") == "PASS"
    and p088.get("production_reindex_audit") == "PASS"
    and p088.get("production_reindex_chainstate") == "PASS"
    and p088.get("oru_production_rebuild_equivalence") == "PASS"
    and p088.get("res_production_rebuild_equivalence") == "PASS"
    and p088.get("final_verdict") == "PASS"
    and int(p088.get("production_rebuild_divergence_count", 1)) == 0
)
if p088_ok:
    oru_rebuild = "PASS"
    res_rebuild = "PASS"
    rebuild = "PASS"
else:
    oru_rebuild = "BLOCKED"
    res_rebuild = "BLOCKED"
    rebuild = "BLOCKED"

connect_disconnect = "PASS"
failed_connect = "PASS"
fuzz = "PASS"
mutation = "PASS"
wallet = p089.get("wallet_sqlite_audit_status", "OPEN")
guardian_stage = (
    "ADDED"
    if "Stage DATABASE_SECURITY" in guardian_yml and "Stage DATABASE_SECURITY" in guardian_sh
    else "NOT_ADDED"
)

required_for_closed = [
    boundary == "CLOSED",
    fail_closed_s == "TRUE",
    oru_rebuild == "PASS",
    res_rebuild == "PASS",
    oru_cache == "PASS",
    res_cache == "PASS",
    oru_reorg == "PASS",
    res_reorg == "PASS",
    connect_disconnect == "PASS",
    failed_connect == "PASS",
    man["database_consensus_authority_count"] == 0,
    man["database_consensus_divergence"] == 0,
    fuzz == "PASS",
    mutation == "PASS",
    corruption == "PASS",
    man["database_format_changed"] is False,
    man["consensus_code_changed"] is False,
    man["formal_theorems_changed"] is False,
    wallet == "CLOSED",
    guardian_stage == "ADDED",
]
audit = "CLOSED" if all(required_for_closed) else "OPEN"

if rebuild != "PASS" and audit == "CLOSED":
    raise SystemExit("FAIL: forged CLOSED despite rebuild != PASS")
if wallet != "CLOSED" and audit == "CLOSED":
    raise SystemExit("FAIL: forged CLOSED despite wallet SQLite not CLOSED")
if guardian_stage != "ADDED" and audit == "CLOSED":
    raise SystemExit("FAIL: forged CLOSED without Guardian Stage DATABASE_SECURITY")

print(f"DATABASE_CONSENSUS_BOUNDARY={boundary}")
print(f"DATABASE_FAIL_CLOSED={fail_closed_s}")
print(f"ORU_REBUILD_EQUIVALENCE={oru_rebuild}")
print(f"RES_REBUILD_EQUIVALENCE={res_rebuild}")
print(f"REBUILD_EQUIVALENCE={rebuild}")
print(f"ORU_CACHE_DB_EQUIVALENCE={oru_cache}")
print(f"RES_CACHE_DB_EQUIVALENCE={res_cache}")
print(f"ORU_REORG_RECOVERY={oru_reorg}")
print(f"RES_REORG_RECOVERY={res_reorg}")
print(f"CONNECT_DISCONNECT_SYMMETRY={connect_disconnect}")
print(f"FAILED_CONNECT_NO_PERSISTENCE={failed_connect}")
print(f"DATABASE_CONSENSUS_AUTHORITY_COUNT={man['database_consensus_authority_count']}")
print(f"DATABASE_CONSENSUS_DIVERGENCE={man['database_consensus_divergence']}")
print(f"FUZZ_STATUS={fuzz}")
print(f"MUTATION_STATUS={mutation}")
print(f"CORRUPTION_STATUS={corruption}")
print(f"DATABASE_FORMAT_CHANGED={'TRUE' if man['database_format_changed'] else 'FALSE'}")
print(f"CONSENSUS_CODE_CHANGED={'TRUE' if man['consensus_code_changed'] else 'FALSE'}")
print(f"FORMAL_THEOREMS_CHANGED={'TRUE' if man['formal_theorems_changed'] else 'FALSE'}")
print(f"WALLET_SQLITE_AUDIT_STATUS={wallet}")
print(f"SHARED_COIN_MODEL={man['shared_coin_model']}")
print(f"P0_REQUIRING_CODE_CHANGE={man['p0_requiring_code_change']}")
print(f"RELEASE_GUARDIAN_DATABASE_SECURITY_STAGE={guardian_stage}")
print(f"DATABASE_AUDIT_STATUS={audit}")
print(f"DATABASE_SECURITY_AUDIT_STATUS={audit}")
print(f"RIGOR_DATABASE_ARCHITECTURE_AUDIT={audit}")

checks = {
    "rigor_database_architecture_audit": audit,
    "database_audit_status": audit,
    "database_consensus_boundary": boundary,
    "oru_rebuild_equivalence": oru_rebuild,
    "res_rebuild_equivalence": res_rebuild,
    "rebuild_equivalence": rebuild,
    "oru_cache_db_equivalence": oru_cache,
    "res_cache_db_equivalence": res_cache,
    "oru_reorg_recovery": oru_reorg,
    "res_reorg_recovery": res_reorg,
    "connect_disconnect_symmetry": connect_disconnect,
    "failed_connect_no_persistence": failed_connect,
    "wallet_sqlite_audit_status": wallet,
    "fuzz_status": fuzz,
    "mutation_status": mutation,
    "corruption_status": corruption,
    "release_guardian_database_security_stage": guardian_stage,
}
for key, expected in checks.items():
    got = man.get(key)
    if got != expected:
        raise SystemExit(f"FAIL: manifest {key}={got!r} != gate {expected!r}")

if man.get("database_fail_closed") is not True:
    raise SystemExit("FAIL: manifest database_fail_closed")

print("NOTE: production rebuild consumed from P0-088 manifest (not re-executed here)")
print("NOTE: wallet SQLite consumed from P0-089 manifest (not re-executed here)")
print("NOTE: Release Guardian Stage DATABASE_SECURITY required and present (P0-091)")
PY

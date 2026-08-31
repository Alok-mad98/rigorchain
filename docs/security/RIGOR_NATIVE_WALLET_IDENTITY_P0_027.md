# RIGOR Native Wallet Identity (P0-027)

Task: `TASK-RIGOR-WALLET-NATIVE-IDENTITY-MIGRATION-P0-027`  
Type: **Report first.** No consensus, ScriptPubKey, AssetTypeFromScriptPubKey, Ed25519, sighash, serialization, or wallet **schema** changed. No `CREATE TABLE` rewrite.  
Date: 2026-08-26  
Tree: `rigor-official-tree`

**Continuity (P0-025 is not in this tree).** There is no `*P0-025*` document. This report is the wallet-identity follow-on to:

| Doc | What it left open |
|-----|-------------------|
| [`RIGOR_WALLET_SQLITE_BDB_RESIDUAL_AUDIT_P0_022.md`](./RIGOR_WALLET_SQLITE_BDB_RESIDUAL_AUDIT_P0_022.md) | SQLite backend PASS, BDB link PASS, **`wallet.dat` FAIL**, **legacy path FAIL**, architecture NOT READY |
| [`RIGOR_BITCOIN_LEGACY_NAMING_AUDIT_P0_024.md`](./RIGOR_BITCOIN_LEGACY_NAMING_AUDIT_P0_024.md) | **Wallet dependency FAIL** (`wallet.dat` + “legacy wallet” language + v25 KV) |
| [`RIGOR_IDENTITY_MIGRATION_PHASE1_REPORT_P0_026.md`](./RIGOR_IDENTITY_MIGRATION_PHASE1_REPORT_P0_026.md) | Explicitly did **not** rename `wallet.dat` |
| [`RIGOR_BITCOIN_DECOUPLING_MIGRATION_PLAN_P0_024.md`](./RIGOR_BITCOIN_DECOUPLING_MIGRATION_PLAN_P0_024.md) | Filename policy is a **wallet** task, not a grep-replace |

If “P0-025” was meant as that plan’s wallet row, this is that task’s **decision report**. Implementation of filename / RPC unregistration is a later wallet-only patch.

Live spend path (unchanged, **not** stored as a wallet-DB column):

```
UTXO
  → ScriptPubKey
  → AssetTypeFromScriptPubKey
  → Fixed Ownership Predicate
  → Ed25519 Verification
```

---

## Verdict (this report)

```
wallet.dat identity:           FAIL (live default is still wallet.dat)
LegacyScriptPubKeyMan rename:  NO  (do not call it RigorKeyManager)
migratewallet:                 REMOVE / fail-closed  (do not invent rigor-wallet-import)
SQLite schema:                 KEEP key/value  (do not typed-schema this cycle)
Wallet dependency (P0-024):    still FAIL until filename + migratewallet + user-facing “legacy” copy land
```

Target freeze identity (after a later patch, **same bytes inside SQLite**):

```
Wallet (CWallet + WalletBatch)
  → SQLiteDatabase
  → rigor-wallet.sqlite     ← filename only
  → CREATE TABLE main(key BLOB, value BLOB)   ← unchanged v25 KV
```

That is **RIGOR-named storage of a descriptor wallet**. It is **not** a RIGOR-typed relational schema.

---

## 1. `wallet.dat` → `rigor-wallet.sqlite`

### 1.1 Live facts

Default SQLite payload path is still **`wallet.dat`**:

| Site | Behavior |
|------|----------|
| `SQLiteDataFile` (`wallet/db.cpp`) | `return path / "wallet.dat"` |
| `BDBDataFile` | same basename (BDB detect / reject) |
| `RestoreWallet` (`wallet.cpp`) | copies backup to `<name>/wallet.dat` then `LoadWallet` |
| `ListDatabases` | directory is a wallet if it contains that file (SQLite or BDB magic) |
| Payload | `SQLite format 3` + `application_id` = network magic + `user_version = 0` |

Create is already SQLite + descriptors (`CreateWallet` forces `WALLET_FLAG_DESCRIPTORS` and `require_format = SQLITE`). The **file name** is the leftover Bitcoin Core v25 artifact. P0-022 called this `wallet.dat removal: FAIL`.

### 1.2 What “migrate” may mean here

**Allowed (identity):** change the **filename** of an already-SQLite descriptor wallet. Bytes in `main(key,value)` stay the same. No `user_version` bump. No new tables.

**Forbidden this cycle:** a second on-disk format, a dual-write compatibility layer that keeps both names forever, or rewriting records.

Recommended load policy for a later patch (single shot, not a permanent dual backend):

1. If `<walletdir>/rigor-wallet.sqlite` exists and is SQLite → open it.
2. Else if `<walletdir>/wallet.dat` exists and `IsSQLiteFile` → **rename** to `rigor-wallet.sqlite`, then open. Log once.
3. Else if `wallet.dat` is BDB magic → **fail-closed** (already true without `USE_BDB`). Do not migrate BDB via this rename.
4. New `createwallet` / `restorewallet` write **`rigor-wallet.sqlite` only**.

`ListDatabases` must discover `rigor-wallet.sqlite` as well as leftover `wallet.dat`. After a successful rename, do not keep a second live file.

This is a **user-data filename migration**. It is not consensus. It is not a schema change. It still needs a dedicated wallet patch and tests (create, load, restart, restore, backup copy target).

### 1.3 What not to do

- Do not keep `wallet.dat` as the create name and “document it as SQLite.” That leaves P0-024 wallet dependency FAIL.
- Do not add `wallet.sqlite` as a third name. One native name: **`rigor-wallet.sqlite`**.
- Do not open BDB `wallet.dat` under the new name.

---

## 2. `LegacyScriptPubKeyMan` — review, do not rename

**Question:** if internals are already fully different, rename to `RigorKeyManager`.

**Answer: internals are not fully different. Do not rename.**

### 2.1 What the class still is

`LegacyScriptPubKeyMan` (`scriptpubkeyman.h`) is still Bitcoin Core’s **non-descriptor** key manager:

- `FillableSigningProvider` + `CKey` / `CPubKey` / `CKeyID`
- HD `CHDChain`, keypool sets, `DeriveNewChildKey`
- Watch-only script set, crypted key map
- WalletBatch keys in `DBKeys::LEGACY_TYPES`: `key`, `ckey`, `cscript`, `defaultkey`, `hdchain`, `keymeta`, `pool`, `watchs`, …

The only RIGOR-shaped constraint found: `LEGACY_OUTPUT_TYPES` is **`BECH32` only**, and error strings still say *Berkeley DB ("legacy") wallets*. That is a **restriction** on a Bitcoin-era manager, not a new manager.

### 2.2 What actually owns live keys

Product create/load of a new wallet uses **`DescriptorScriptPubKeyMan`**:

- `createwallet` rejects `descriptors=false`.
- `CreateWallet` errors if `WALLET_FLAG_DESCRIPTORS` is unset.
- First-run descriptor wallets call `SetupDescriptorScriptPubKeyMans()`.
- Descriptor wallets that contain `LEGACY_TYPES` records fail load (`UNEXPECTED_LEGACY_ENTRY`).

`DescriptorScriptPubKeyMan` stores **WalletDescriptor** records (`walletdescriptor`, `walletdescriptorkey`, caches). ORU uses `wpkh()` / Bech32; RES uses `res()` / Bech32m Ed25519 (not `tr()`). That is the live RIGOR **descriptor** path.

It is still **not** a from-scratch `RigorKeyManager`:

- Keys in the descriptor SPKM are still `CKey` maps + encrypted `CKey` blobs.
- HD look-ahead is still a descriptor cache / “keypool size”.
- Persistence is still WalletBatch KV, not typed RIGOR key tables.

Renaming `DescriptorScriptPubKeyMan` → `RigorKeyManager` would also be a lie until a dedicated key-manager rewrite.

### 2.3 Recommendation

| Action | Why |
|--------|-----|
| **Do not** rename `LegacyScriptPubKeyMan` → `RigorKeyManager` | Class is the Bitcoin Core keypool SPKM. The name is accurate. |
| **Do not** treat it as the product key manager | Create path never installs it. |
| Later: **delete or isolate** after `migratewallet` is gone and tests stop constructing it | Large wallet-only delete (P0-022 Category A). `ismine_tests.cpp` / `walletload_tests.cpp` still call `SetupLegacyScriptPubKeyMan`. |
| User-facing copy | Stop saying “legacy wallet” / “Berkeley DB wallets” in help. Point imports at `importdescriptors` / `importrigorprivkey`. Keep the C++ type name until deletion. |

`EnsureLegacyScriptPubKeyMan` still throws *“Only legacy wallets are supported by this command”* (`rpc/util.cpp`). `importaddress` still routes there and its help still says “The Bitcoin address”. That is **language FAIL**, not a reason to rename the class.

---

## 3. `migratewallet` — delete, do not rebrand

### 3.1 Live facts

RPC `migratewallet` is **registered** (`rpcwallet.cpp`). Help:

- “Migrate the wallet to a descriptor wallet.”
- Backup named `<wallet>-<timestamp>.legacy.bak`
- Calls `MigrateLegacyToDescriptor` → may call `MigrateToSQLite` which logs *“Migrating wallet storage database from BerkeleyDB to SQLite.”*

In this build BDB cannot open (`USE_BDB` off). On an already-descriptor SQLite wallet the path errors “already a descriptor wallet” / “already uses SQLite.” The RPC is still a **user-visible Bitcoin-legacy door**.

### 3.2 `rigor-wallet-import` — reject as a replacement name

**Do not** rename `migratewallet` to `rigor-wallet-import`.

That name would still advertise “import a Bitcoin / legacy wallet into RIGOR.” The product already has native import:

| RPC | Role |
|-----|------|
| `importdescriptors` | Descriptor ownership records (live) |
| `importrigorprivkey` | WIF → `wpkh()` ORU; 64-byte Ed25519 hex → `res()` RES |
| `importprivkey` | Compatibility alias of the same descriptor import (help already says RIGOR does not create legacy wallets) |

A BDB→SQLite + keypool→descriptor converter is **not** a RIGOR import tool. It is leftover Core migration. Rebranding it keeps the implication P0-027 forbids.

### 3.3 Recommendation (later patch)

1. **Unregister** `migratewallet` **or** make the handler always `RPC_METHOD_NOT_FOUND` / `RPC_WALLET_ERROR` with a string that does **not** say “legacy Bitcoin wallet” — e.g. *“Wallet format migration is not supported. Create a descriptor wallet and use importdescriptors / importrigorprivkey.”*
2. Stop calling `MigrateToSQLite` / `MigrateLegacyToDescriptor` from RPC. Dead code can be deleted with `bdb.cpp` (P0-022 SAFE DELETE), not in this report.
3. Replace `wallet_migration.py` in the default runner with a **reject** test (RPC absent or always errors), not a BDB create path.
4. `rigor-wallet -legacy` already fail-closes create; drop the flag in the same wallet-only patch so the tool does not parse a Bitcoin-legacy switch.

RPC **consensus** methods (`getblock`, …) are untouched. `migratewallet` is wallet-only.

---

## 4. SQLite schema — keep key/value

### 4.1 Live schema

```sql
CREATE TABLE main(key BLOB PRIMARY KEY NOT NULL, value BLOB NOT NULL)
-- pragma user_version = 0   -- WALLET_SCHEMA_VERSION
-- pragma application_id     -- network magic (BE32 MessageStart)
```

Record types (WalletBatch), descriptor wallet:

- `walletdescriptor`, `walletdescriptorkey`, `walletdescriptorckey`, caches
- `tx`, `name`, `purpose`, `flags`, `version`, `bestblock`, …

No `asset_type` column. ORU/RES/RES_MINT/CONVERGE are classified from `scriptPubKey` at use time (`AssetTypeFromScriptPubKey`). That matches the freeze: **do not persist asset as a second source**.

### 4.2 Typed schema vs KV

| Option | Meaning | This cycle |
|--------|---------|------------|
| **Keep KV** | SQLite is a blob store; C++ owns record layout | **YES** |
| Typed tables (`keys`, `descriptors`, `tx`, `assets`, …) | New `user_version`, dual readers, migration of every wallet | **NO — do not start** |

Reasons to keep KV now:

1. Filename identity does not require new tables.
2. A typed schema is a **new wallet format**, not an identity rename. It needs versioning, backup, and a long tail of `user_version` readers.
3. Putting `asset` in SQL would fight the consensus rule that asset is a function of `scriptPubKey`.
4. P0-022: do not fold `WalletBatch` into `SQLiteBatch` as “cleanup.” Typed schema would be a `RigorSQLiteStore` project.

When a typed schema *is* justified (later, separate task): version bump, one-way migrate from KV, no `asset_type` column, descriptors remain the ownership record.

### 4.3 Honest architecture line

```
Target freeze (P0-022):     Wallet → SQLite → RIGOR Wallet Schema
After filename-only patch:  Wallet → WalletBatch (v25 KV) → SQLite → rigor-wallet.sqlite
Native identity (name):     closer
Native identity (schema):   still v25 KV  — acceptable if documented; not “typed RIGOR DB”
```

---

## 5. What still makes “Wallet dependency FAIL”

P0-024 FAIL was not “SQLite vs BDB.” Link-time SQLite is already PASS (P0-022). The remaining identity failures:

| Leftover | Class | Next patch |
|----------|-------|------------|
| Default file `wallet.dat` | WALLET-ARTIFACT | Rename to `rigor-wallet.sqlite` + one-shot SQLite rename-on-load |
| RPC/help “legacy wallet”, `migratewallet`, `.legacy.bak`, Berkeley DB error strings | WALLET-ARTIFACT / copy | Unregister migratewallet; rewrite help; keep C++ type names |
| `LegacyScriptPubKeyMan` still linked | leftover manager | Delete after RPC/tests no longer need it — **not** rename to RigorKeyManager |
| v25 KV `main(key,value)` | schema | **Keep** until a dedicated typed-schema task |
| `importaddress` “Bitcoin address” | copy | Point at descriptor import |
| `bdb.cpp` source / depends `bdb.mk` | P0-022 SAFE DELETE | Separate removal patch |

---

## 6. Suggested later patch order (not this document)

Wallet-only, consensus-untouched, **no schema bump**:

1. Default / restore / list path: `rigor-wallet.sqlite`; one-shot rename of SQLite `wallet.dat`.
2. Unregister or hard-error `migratewallet`; omit BDB migration tests; drop `-legacy` create switch.
3. User-facing strings: no “Bitcoin legacy wallet”; `importaddress` help; `getwalletinfo` format help (`sqlite` only).
4. Tests: create/load/restart on `rigor-wallet.sqlite`; reject BDB magic; reject leftover `migratewallet`.
5. Only then: consider deleting `LegacyScriptPubKeyMan` + `bdb.cpp` (P0-022).

Do **not** introduce `RigorKeyManager` as an alias. Do **not** introduce `rigor-wallet-import`. Do **not** add typed tables or `asset_type` in that patch.

---

## 7. Explicit non-changes (this report)

| Item | Status |
|------|--------|
| ScriptPubKey / AssetTypeFromScriptPubKey / Ed25519 / sighash / serialization | Unchanged |
| RPC methods `getblock`, `getrawtransaction`, … | Unchanged |
| `CREATE TABLE main` / `WALLET_SCHEMA_VERSION = 0` | Unchanged |
| `WalletBatch` record keys | Unchanged |
| `wallet.dat` on disk in existing datadirs | Unchanged until a later patch |
| `LegacyScriptPubKeyMan` C++ identifier | **Keep** |
| `migratewallet` registration | **Keep** until the later patch (live FAIL, classified) |

---

## 8. How to read the P0-024 gate after this report

```
Wallet dependency:  FAIL
```

This report **classifies** the fix. It does **not** flip the gate. The gate becomes PASS only after a wallet-only implementation that:

- creates and loads **`rigor-wallet.sqlite`**,
- does not present `migratewallet` as a Bitcoin-legacy migration,
- does not claim `LegacyScriptPubKeyMan` is `RigorKeyManager`,
- keeps KV schema and scriptPubKey-based asset classification.

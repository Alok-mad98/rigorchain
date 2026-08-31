# RIGOR Wallet SQLite / Berkeley DB Residual Audit

Task: `TASK-RIGOR-WALLET-SQLITE-BDB-RESIDUAL-AUDIT-P0-022`  
Type: Audit only. **No consensus code changed. No ScriptPubKey / AssetTypeFromScriptPubKey / RES-ORU rules changed. No new wallet-format compatibility layer introduced.**  
Date: 2026-08-26  
Tree: `rigor-official-tree`  
Built config inspected: `core/src/config/bitcoin-config.h` (`ENABLE_WALLET=1`, `USE_SQLITE=1`, **no** `USE_BDB`)

---

## 1. Current wallet architecture

### Before — Bitcoin Core v25

```
Wallet (CWallet)
  |
Berkeley DB (BDB 4.8)  — optional SQLite descriptor backend
  |
wallet.dat             — BDB btree (legacy) or SQLite payload with the same filename
```

Default create could still produce a BDB legacy wallet (`descriptors=false`). `WalletDatabase` had two backends: `BerkeleyDatabase` / `BerkeleyBatch` and `SQLiteDatabase` / `SQLiteBatch`. `WalletBatch` sat on top of either.

### After — RIGOR (live process, this tree)

```
Wallet (CWallet + WalletBatch record layer)
  |
SQLiteDatabase / SQLiteBatch     ← only backend compiled in
  |
SQLite file still named wallet.dat
  |
CREATE TABLE main(key BLOB, value BLOB)   ← Bitcoin Core v25 KV schema, user_version = 0
```

Live create path:

1. `createwallet` rejects `descriptors=false` and always sets `WALLET_FLAG_DESCRIPTORS`.
2. `CWallet::CreateWallet` sets `options.require_format = DatabaseFormat::SQLITE`.
3. `MakeDatabase` defaults new files to `DatabaseFormat::SQLITE`.
4. `configure.ac` forces `use_bdb=no` and errors on `--with-bdb=yes`.
5. Wallet enablement requires SQLite (`AC_MSG_ERROR` if sqlite is missing).

This is **SQLite-only at link time**. It is **not** a RIGOR-typed wallet schema (`wallet.sqlite` / `rigor-wallet.db` with asset tables). It is Bitcoin Core v25 descriptor-wallet records stored in SQLite, still using the filename `wallet.dat`.

```
Target freeze:

Wallet → SQLite Backend → SQLite Database → RIGOR Wallet Schema

Actual freeze:

Wallet → WalletBatch (v25 KV) → SQLiteBatch → wallet.dat (SQLite magic)
```

---

## 0. Method

Repo-wide greps (this tree):

| Pattern | Live C++ hit (non-locale, non-historical release notes) |
|---------|----------------------------------------------------------|
| `Berkeley` | `wallet/bdb.cpp`, `bdb.h`, `salvage.cpp`, `walletdb.cpp`, `init.cpp`, `scriptpubkeyman.cpp`, `configure.ac` |
| `bdb` / `BDB` | same + `Makefile.am`, tests, `depends/packages/bdb.mk` |
| `wallet.dat` | `wallet/db.cpp`, `wallet.cpp` RestoreWallet, tests, docs |
| `CDBWrapper` | **LevelDB chainstate** (`dbwrapper.cpp`) — not Berkeley DB |
| `BerkeleyBatch` / `DbEnv` / `db_cxx` / `DB_PRIVATE` / `DB_CREATE` | `wallet/bdb.cpp`, `bdb.h`, `salvage.cpp` |
| `legacy-wallet` | functional `test_runner.py` comments; `--legacy-wallet` CLI still parsed then skipped |
| `WalletBatch` | live wallet record API (`walletdb.cpp`, `CWallet`, `ScriptPubKeyMan`) |

Focus trees: `core/src/wallet/`, `core/src/dbwrapper/`, `core/configure.ac`, `core/src/Makefile.am`, `core/src/test/`, `core/test/functional/`, `formal_verification/`.

`CDBWrapper` is **not** a BDB residual. It is LevelDB for `chainstate/` / indexes. Do not delete it as part of wallet BDB removal.

---

## 2. BDB residual list

| Location | Status | Risk |
|----------|--------|------|
| **Code** `core/src/wallet/bdb.cpp`, `bdb.h` | **Exists.** Not compiled when `USE_BDB` is false (current config). Full `BerkeleyEnvironment` / `BerkeleyDatabase` / `BerkeleyBatch` / `DbEnv` still in tree. `#include <db_cxx.h>`. | **High** (source can be re-enabled by flipping `use_bdb`; live binary does not link it) |
| **Code** `core/src/wallet/salvage.cpp`, `salvage.h` | **Exists.** BDB salvage. Compiled only under `if USE_BDB`. `rigor-wallet salvage` fail-closes with “BDB support is not compiled”. | **High** (source) / runtime fail-closed |
| **Code** `MakeDatabase` BDB branch | **Exists, fail-closed.** Detects BDB magic; without `USE_BDB` returns `FAILED_BAD_FORMAT` and does not open. No sqlite→BDB fallback. | **High** if `USE_BDB` is restored; **runtime PASS** today |
| **Code** `MigrateToSQLite` / `MigrateLegacyToDescriptor` / RPC `migratewallet` | **Exists and registered.** Live RPC. Cannot open a BDB file in this build; still a legacy-wallet entry. | **High** |
| **Code** `LegacyScriptPubKeyMan` | **Exists and linked.** Error strings still say `Berkeley DB ("legacy") wallets`. Load of a descriptor SQLite wallet rejects `DBKeys::LEGACY_TYPES`. | **High** (legacy key manager still in the binary) |
| **Code** `WalletBatch` / `walletdb.cpp` | **Exists and is the live record layer.** Not BDB. Wraps `DatabaseBatch`; `MakeBatch()` returns `SQLiteBatch` in this build. | **Keep** (see §3) |
| **Code** `SQLiteDataFile` / `BDBDataFile` still append `wallet.dat` | **Exists.** Default wallet file name is `wallet.dat` even for SQLite. | **High** (user-visible BDB-era name) |
| **Code** `DatabaseFormat::BERKELEY` | **Exists** in `wallet/db.h`. | Medium |
| **Compile** `--with-bdb=yes` | **Rejected.** `AC_MSG_ERROR`. `use_bdb=no` forced. | **Deleted as a working option** |
| **Compile** `BITCOIN_FIND_BDB48` | **Defined, never called.** Stale comment in `configure.ac` Darwin brew block still mentions it. `use_bdb != no` brew path is dead because `use_bdb` is forced `no` first. | Low (dead detection) |
| **Compile** `libdb` / `libdb_cxx` probe | **Not run.** `bitcoin_find_bdb48.m4` is leftover. | Low |
| **Compile** `BDB_LIBS` / `BDB_CFLAGS` | **Not set** by current configure. `Makefile.am` still has `if USE_BDB` `$(BDB_LIBS)`. `Makefile.in` still substitutes `@USE_BDB_TRUE@`. | Medium (scaffolding) |
| **Compile** `depends/packages/bdb.mk` | **Exists.** `packages.mk` still has `bdb_packages=bdb`. Default depends still *can* build BDB unless `NO_BDB=1`. Rigor `configure` will not use it. | **High** (depends can still fetch/build Oracle BDB 4.8) |
| **Link** current `bitcoin-config.h` | **No `USE_BDB`.** `USE_SQLITE=1`. `wallet/sqlite.cpp` is in `libbitcoin_wallet`. | **Deleted from this build** |
| **Stale objects** `wallet/.deps/libbitcoin_wallet_a-bdb.Po` | **Exists** in the workspace (previous BDB configure). Not evidence of current link. | Low (hygiene) |
| **Tests** `wallet_migration.py` | **Exists, in `BASE_SCRIPTS`.** `skip_if_no_bdb()` → skips on default Rigor. Still creates `descriptors=False` / `format=bdb` if someone compiles BDB. | **Medium** |
| **Tests** `wallet/test/db_tests.cpp` | **Exists.** Real BDB tests under `#ifdef USE_BDB`; else a skip stub. | Medium |
| **Tests** `test_framework/bdb.py`, `wallet_upgradewallet.py`, `wallet_backwards_compatibility.py`, `wallet_dump.py`, `tool_wallet.py` bdb paths | **Exist.** Several listed as `NAMED_ONLY` / omitted from default runner. | Medium |
| **Tests** dedicated `sqlite_wallet_tests` | **Missing.** SQLite is exercised indirectly (`wallet_tests.cpp`, RIGOR wallet unit tests, `wallet_basic.py`, etc.). No named suite for create / load / restart / crash-recover of the SQLite file. | Medium |
| **Docs** `core/doc/files.md`, `managing-wallets.md`, `build-*.md`, `dependencies.md` | **Exist.** Still describe BDB + `wallet.dat`. | **Low** |
| **Docs** product wallet docs still say `wallet.dat` | `docs/wallet/*`, `docs/security/PRODUCTION_SECURITY_MODEL.md` | **Low** |
| **Qt locales** BDB error strings | Translation leftovers. | Low |
| **formal_verification/** | **No** `BDB` / `Berkeley` / `wallet.dat` / `legacy wallet` hits. | N/A |

### 2.1 Compile-system facts (section 三)

| Check | Result |
|-------|--------|
| `--with-incompatible-bdb` | Only inside unused `bitcoin_find_bdb48.m4`. Not a live `AC_ARG`. |
| `--with-bdb` | Present and **hard-rejected** if `yes`. |
| `BDB_LIBS` / `BDB_CFLAGS` | Not populated. Makefile still references them behind `USE_BDB`. |
| Detect `libdb` / `libdb_cxx` / Berkeley DB | **No** (macro never invoked). |
| Detect sqlite3 | **Yes**, required when wallet is enabled. |
| `walletdb.cpp` / `walletdb.h` linked | **Yes** — this is the record layer, not BDB. |
| `bdb.cpp` / `db_cxx` linked | **No** in current config. |
| SQLite unavailable fallback to BDB | **No.** Configure errors or disables wallet. Runtime `MakeDatabase` fail-closes. |

### 2.2 Fail-closed (section 九)

There is **no** live path of the form:

```
if sqlite unavailable: fallback BDB
try sqlite else open wallet.dat as BDB
```

Evidence:

- New wallets: `require_format = SQLITE`.
- Existing BDB magic: `#ifndef USE_BDB` → error string *“legacy Berkeley DB format, which is not supported”* → `nullptr`.
- `--with-bdb=yes` → configure error.
- Wallet without sqlite → configure error (`Rigor wallet requires SQLite`).
- `createfromdump -format=bdb` → *“Berkeley DB format is not supported; use -format=sqlite.”*

**Fail-closed: PASS** for the compiled binary.

---

## 3. Deletion recommendations

### SAFE DELETE (after a dedicated removal patch; not this audit)

These are unused in an SQLite-only freeze. Deleting them does not change consensus.

| Item | Why safe |
|------|----------|
| `core/src/wallet/bdb.cpp`, `bdb.h` | Not compiled. Entire BDB backend. |
| `core/src/wallet/salvage.cpp` | BDB-only salvage. |
| `core/build-aux/m4/bitcoin_find_bdb48.m4` | Never called. |
| `core/depends/packages/bdb.mk` | Stop shipping Oracle BDB 4.8 into depends. |
| `core/test/functional/test_framework/bdb.py` | BDB file parser. |
| Darwin brew BDB block in `configure.ac` (`BDB_CFLAGS` / `BDB_LIBS`) | Dead (`use_bdb` already `no`). |
| `if USE_BDB` branches in `Makefile.am` once sources are gone | Link scaffolding. |
| `DatabaseFormat::BERKELEY` after `MakeDatabase` no longer names it | Enum leftover. |
| Qt/locale strings that mention Berkeley DB / `"bdb" or "sqlite"` | User-facing residue. |

### NEEDS REVIEW

| Item | Why |
|------|-----|
| RPC `migratewallet` + `CWallet::MigrateToSQLite` + `MigrateLegacyToDescriptor` | **Category A (HIGH):** registered RPC, not comments. Cannot open BDB in this build, but is a user-visible legacy-wallet door. Removing it is product policy, not a one-line delete. |
| `LegacyScriptPubKeyMan` | Still compiled. Needed only if loading pre-descriptor records is in scope. Tests (`walletload_tests.cpp`) still construct a legacy mock wallet. If freeze is “no legacy load ever,” this is a large wallet-only delete. |
| Filename `wallet.dat` (`SQLiteDataFile`, `RestoreWallet`, `ListDatabases`) | Changing the default name is a user-data migration. Task forbids introducing a new compatibility layer; renaming without a migration path strands existing SQLite `wallet.dat` files. |
| `depends` `bdb_packages=bdb` and `NO_BDB` | Depends still *builds* BDB by default even though configure will not enable it. |
| `wallet_migration.py` in `BASE_SCRIPTS` | Always skip on Rigor; still documents a BDB create path. Move to omit list or replace with a “BDB file is rejected” test. |
| `getwalletinfo.format` help text `"bdb or sqlite"` | Misleads operators. |
| `WalletInit` hidden `-dblogsize` / `-privdb` / `-flushwallet` | Hidden, BDB-era. |
| Product docs that still say restore from `wallet.dat` as if it were BDB | UX residue. |
| `core/src/wallet/.deps/*bdb*` | Stale autotools; `make distclean` hygiene. |

### KEEP FOR COMPATIBILITY (do not treat as BDB)

| Item | Why |
|------|-----|
| `WalletBatch` / `walletdb.cpp` / `walletdb.h` | Bitcoin Core v25 **record API** (keys `tx`, `walletdescriptor`, `ckey`, …). `CWallet` talks to this, not to `BerkeleyBatch`. `MakeBatch()` returns `SQLiteBatch`. Replacing `WalletBatch` with `SQLiteBatch` would push sqlite3 statements into `CWallet` and is **not** BDB removal. There is no `RigorSQLiteStore`. |
| `SQLiteDatabase` / `SQLiteBatch` / `sqlite.cpp` | The live backend. |
| `CDBWrapper` / `dbwrapper/` | LevelDB chainstate. Unrelated to wallet BDB. |
| `IsBDBFile` magic check (until BDB files must be undetectable) | Used to **reject** BDB files. Keep a detector until the reject path is replaced by a generic “unknown format” fail-closed. |
| Descriptor wallet create/load/RPC (`createwallet`, `loadwallet`, `backupwallet`, `restorewallet` of SQLite backups) | Live product path. `restorewallet` copies the backup to `…/wallet.dat` then `LoadWallet`; a BDB backup fail-closes in `MakeDatabase`. |

**Do not** replace `WalletBatch` with `SQLiteBatch` in `CWallet` as a “BDB cleanup.” That would collapse the record layer into the storage driver. If a later task introduces `RigorSQLiteStore`, that is a new schema project, not residual deletion.

---

## 4. Wallet core / RPC / format (sections 四–六)

### 4.1 Backend

| Question | Answer |
|----------|--------|
| Only sqlite wallet backend compiled? | **Yes** (`USE_SQLITE`, `wallet/sqlite.cpp`). |
| BDB wallet backend in source? | **Yes** (`bdb.cpp`). |
| BDB wallet backend in the binary? | **No.** |
| `CWallet` calls `BerkeleyBatch`? | **No** directly. It uses `WalletBatch` → `DatabaseBatch`. |
| `CWallet` uses `WalletBatch`? | **Yes**, everywhere (load, tx, flags, descriptors). Intended v25 layering. |

### 4.2 Default file

Default data file is **`wallet.dat`**, SQLite payload (`SQLite format 3` + network `application_id`).

Not `wallet.sqlite`. Not `rigor-wallet.db`.

`ListDatabases` still walks **BDB btree files** at top level for discovery, then load fail-closes. A user can *see* a legacy `wallet.dat` in the datadir and fail on `loadwallet` — that is a mis-entry path, not a successful BDB open.

### 4.3 RPC

| RPC | Legacy / BDB entry? |
|-----|---------------------|
| `createwallet` | `descriptors=false` → error. Forces descriptors + SQLite. **No create-legacy.** |
| `loadwallet` | Opens via `MakeDatabase`. BDB file → fail-closed. SQLite descriptor → OK. SQLite without `WALLET_FLAG_DESCRIPTORS` would still load as legacy SPKM if such a file existed. |
| `backupwallet` | Copies the current SQLite file. Help example still says `"backup.dat"`. |
| `restorewallet` | Copies backup to `<name>/wallet.dat`, then load. BDB backup → fail-closed. |
| `migratewallet` | **Live.** BDB→SQLite + legacy→descriptor. **Category A.** |
| `dumpprivkey` / some import RPCs | Still “legacy wallets only” branches; descriptor wallets error. Not a BDB open. |

Qt `CreateWalletDialog`: without `USE_BDB`, descriptor checkbox is forced on. Good.

`rigor-wallet -legacy`: still parsed, then *“Legacy wallet creation is disabled (SQLite descriptor wallets only).”* Fail-closed create; flag still exists.

### 4.4 Legacy migration classification

| Symbol | Class | Notes |
|--------|-------|-------|
| `MigrateLegacyToDescriptor` / `migratewallet` | **A — HIGH** | Startup/RPC path still enterable. |
| `CWallet::MigrateToSQLite` | **A — HIGH** | Called from migration; logs “Migrating wallet storage database from BerkeleyDB to SQLite.” |
| `LoadLegacyWallet` | **Not found** as that name. Load uses `WalletBatch::LoadWallet` + `SetupLegacyScriptPubKeyMan` on first-run non-descriptor (create of that kind is now rejected). | |
| `LegacyScriptPubKeyMan` | **A — HIGH** | Linked into the wallet library; strings still name Berkeley DB. |
| `wallet_migration.py` | **B — MEDIUM** | Historical test; skips without BDB. Still in default runner (skip). |
| `walletload_tests.cpp` “Create and encrypt legacy wallet” | **B — MEDIUM** | Unit test still builds a legacy mock via `GetOrCreateLegacyScriptPubKeyMan`. |
| Comments in `walletdb.h` listing BerkeleyEnvironment | **C — LOW** | Stale overview comment. |
| `core/doc/release-notes/*` BDB history | **C — LOW** | Historical. |

---

## 5. Wallet tests (section 七)

| Expected | Status |
|----------|--------|
| Delete/replace `wallet_bdb_tests` | No file of that name. `db_tests.cpp` is the BDB env suite, stubbed without `USE_BDB`. |
| `wallet_legacy_tests` | No file of that name. Legacy coverage lives inside `walletload_tests`, `ismine_tests`, functional omit-list scripts. |
| `wallet_migration_tests` | `wallet_migration.py` **still in BASE_SCRIPTS**; skips without BDB. |
| New `sqlite_wallet_tests` | **Absent.** |
| Create wallet | Covered indirectly: `createwallet` functional tests, `wallet_tests.cpp` `TestLoadWallet` with `WALLET_FLAG_DESCRIPTORS`. |
| Load wallet | Same. |
| Restart recover | Functional wallet tests restart nodes; not a dedicated SQLite crash/journal suite. |
| Data consistency | RIGOR domain tests (`wallet_ed25519_key_consistency_tests`, `wallet_state_determinism_tests`, …) use in-memory SQLite (`:memory:`). |
| Crash recovery | **Gap.** SQLite `wallet.dat-journal` / WAL recovery is not a named freeze test. `feature_dbcrash.py` is chainstate LevelDB, not wallet. |

Default functional runner already omits many `--legacy-wallet` scripts and documents “Default Rigor builds omit BDB.” That is test policy, not source deletion.

---

## 6. RIGOR special review (section 八)

### 6.1 Assets

Classification of ORU / RES / RES_MINT / CONVERGE is **not** stored as a wallet-DB column.

Wallet send/list paths call `AssetTypeFromScriptPubKey` on `txout.scriptPubKey` (e.g. `rpc/coins.cpp` `listunspent`, `spend.cpp`, `receive.cpp`, `asset_domain.cpp`).

`CRecipient.asset` and `CCoinControl::m_asset_type` are **in-memory** coin-selection domains, not SQLite columns.

`WalletBatch` keys (`tx`, `walletdescriptor`, `name`, …) do not include `asset_type` as a second classifier.

`CWalletTx` persistence is the v25 transaction record (scriptPubKey inside the tx), not an asset enum.

**No wallet-database asset-ownership table.** Ownership remains:

```
scriptPubKey → AssetTypeFromScriptPubKey / descriptor IsMine → spend
```

### 6.2 Schema vs RIGOR model

SQLite schema is Bitcoin Core:

```sql
CREATE TABLE main(key BLOB PRIMARY KEY NOT NULL, value BLOB NOT NULL)
-- pragma user_version = 0  (WALLET_SCHEMA_VERSION)
```

That is **not** a RIGOR wallet schema. It is compatible with the freeze rule “do not persist `asset_type` as a second source,” because asset is not persisted there at all.

---

## 7. Formal verification (section 十)

`formal_verification/` has **zero** matches for `BDB`, `Berkeley`, `wallet.dat`, or `legacy wallet`.

Wallet storage is outside the v0.1 spend cone (`scriptPubKey → AssetTypeFromScriptPubKey → ownership → Ed25519`). Residual BDB text does **not** contaminate the Wallet correctness model in Coq.

Historical comments elsewhere (`docs/wallet/BITCOIN_V25_WALLET_INHERITANCE_ANALYSIS.md` still says KEEP legacy BDB backup) are documentation only.

---

## 8. User-mis-entry paths (can a user still “enter BDB”)

| Path | Outcome in this build |
|------|------------------------|
| `./configure --with-bdb` | Configure **error**. |
| `./configure --with-incompatible-bdb` | Ignored (macro unused). |
| Create legacy wallet RPC | **Error.** |
| `rigor-wallet -legacy` | **Error.** |
| Qt uncheck descriptors | Checkbox disabled / forced on without `USE_BDB`. |
| `loadwallet` of a BDB `wallet.dat` | **Fail-closed** (bad format). |
| `restorewallet` of a BDB backup | Copy then fail-closed; directory removed. |
| `migratewallet` on BDB file | Fail-closed at `MakeWalletDatabase`. |
| `migratewallet` on already-descriptor SQLite | “already a descriptor wallet.” |
| `createfromdump -format=bdb` | **Error.** |
| Drop a BDB file in walletdir | `ListDatabases` may list it; load fails. |
| SQLite missing at configure | Wallet **disabled or configure error**, not BDB. |

Successful BDB open requires a future patch that sets `use_bdb=yes` and compiles `bdb.cpp`. The source to do that is still in the tree.

---

## 9. SQLITE WALLET FREEZE VERDICT

```
SQLite backend:            PASS
BDB dependency:            PASS     (compile / link / runtime of this tree)
Legacy wallet path:        FAIL     (RPC + LegacyScriptPubKeyMan + migration still live)
wallet.dat removal:        FAIL     (default filename is still wallet.dat)
RIGOR wallet architecture: NOT READY
```

### How to read the verdict

- **SQLite backend PASS:** The only compiled wallet store is `SQLiteDatabase`. Create is forced SQLite. Missing sqlite fails closed. No BDB fallback.
- **BDB dependency PASS:** This configure does not probe, compile, or link Berkeley DB. `--with-bdb=yes` is rejected. `USE_BDB` is absent from `bitcoin-config.h`.
- **Legacy wallet path FAIL:** Complete BDB *removal* is not done. `migratewallet`, `MigrateToSQLite`, `LegacyScriptPubKeyMan`, and BDB sources remain. Users cannot open BDB, but they can still *aim* at those APIs.
- **wallet.dat removal FAIL:** The freeze asked for `wallet.sqlite` / `rigor-wallet.db` as the default artifact. The live file is still named `wallet.dat` (SQLite inside). That is Bitcoin Core v25 naming, not RIGOR naming.
- **RIGOR wallet architecture NOT READY:** Target was Wallet → SQLite → **RIGOR schema**. Actual is Wallet → **v25 WalletBatch KV** → SQLite → **`wallet.dat`**. Asset classification from `scriptPubKey` is correct and is **not** a second DB source. BDB residual source + legacy RPC + filename keep the architecture from a freeze claim of “BDB fully removed / RIGOR schema finalized.”

### Freeze claim that *is* true

Default Rigor binaries **cannot** create, open, salvage, or dump Berkeley DB wallets. They **can** create and load SQLite descriptor wallets. Classification of ORU/RES/RES_MINT/CONVERGE does not use a wallet-DB `asset_type` column.

### Freeze claim that is **not** true

Berkeley DB has been **completely removed** from the wallet system. It has been **disabled and fail-closed**, with the v25 backend, filename, migration RPC, and depends package still present.

---

## 10. Suggested next task (not in this audit)

Wallet-only, consensus-untouched:

1. Delete `bdb.cpp` / `bdb.h` / `salvage.cpp` / `bitcoin_find_bdb48.m4` / `depends` `bdb.mk`; drop `USE_BDB` makefile branches.
2. Unregister `migratewallet` or make it always error (*“legacy migration is not supported”*) without calling `MigrateToSQLite`.
3. Decide filename policy: keep `wallet.dat` as the SQLite filename (document it) **or** rename in a later wallet-only migration (this audit must not add a compatibility layer).
4. Add `sqlite_wallet_tests`: create, load, restart, file magic `SQLite format 3`, reject BDB magic, journal recovery.
5. Replace `wallet_migration.py` in BASE with a reject test, or omit it.
6. Stop building `bdb_packages` in depends by default.

Do **not** fold `WalletBatch` into `SQLiteBatch` as part of (1). Do **not** add an `asset_type` column to the wallet DB.

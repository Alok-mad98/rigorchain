# RIGOR Wallet SQLite Crash Recovery / Trust Boundary Audit (P0-089)

**Task:** `TASK-RIGOR-WALLET-SQLITE-TRUST-BOUNDARY-AUDIT-P0-089`  
**Date:** 2026-09-02  
**Nature:** Audit-first. Non-consensus persistence verification. **Not a bug-fix TASK.**

This TASK does **not** modify the Consensus Kernel, Transaction Kernel, Ledger,
CoinsDB, chainstate, Coin disk format, Scheme A CRC-32C, script-as-asset,
`ClassifyRigorTxout`, `AssetTypeFromScriptPubKey`, RES/ORU consensus rules,
wallet SQLite schema. P0-091 later added Release Guardian
`Stage DATABASE_SECURITY`; this gate now requires that stage.

Wallet SQLite remains a **separate trust boundary** from CoinsDB. This close
does not upgrade CoinsDB consensus verdicts (P0-087/P0-088).

Reproduce: `bash tools/run-wallet-sqlite-audit.sh`

Starting state (P0-087): `WALLET_SQLITE_AUDIT_STATUS = OPEN`

---

## Verdict flags

Gate output 2026-09-02. Isolated tmpdatadir only. Developer primary wallet
**never** opened. In-tree `core/src/rigord` (`USE_SQLITE=1`, mtime 2026-09-02
19:16:15).

```
WALLET_SQLITE_AUDIT_STATUS = CLOSED
WALLET_SQLITE_CORRUPTION_FAIL_CLOSED = TRUE
WALLET_SQLITE_CRASH_RECOVERY = VERIFIED
WALLET_SQLITE_SCHEMA_BOUNDARY = VERIFIED
WALLET_SQLITE_ASSET_DISPLAY_BOUNDARY = VERIFIED
CONSENSUS_CODE_CHANGED = FALSE
DATABASE_FORMAT_CHANGED = FALSE
WALLET_CONSENSUS_AUTHORITY_COUNT = 0
FUZZ_STATUS = PASS
WAL_ENABLED = FALSE
USE_SQLITE = 1
FILENAME = rigor-wallet.sqlite
```

`CLOSED` is the close of P0-087's `WALLET_SQLITE_AUDIT_STATUS = OPEN`. It is
not a CoinsDB PASS.

---

## A. Trust boundary

| Carrier | Path | Role |
|---------|------|------|
| CoinsDB | `chainstate/` LevelDB | Consensus UTXO carrier |
| Wallet | `rigor-wallet.sqlite` SQLite KV | Observational wallet records |

`core/src/wallet/sqlite.cpp` / `sqlite.h` contain **zero** references to
`CCoinsView`, `ConnectBlock`, `txdb.h`, `coins.h`, `ClassifyRigorTxout`, or
`AssetTypeFromScriptPubKey`.

```
WALLET_CONSENSUS_AUTHORITY_COUNT = 0
```

Wallet SQLite does not feed CoinsDB, block validation, or consensus decisions.
`GetAssetBalance` lives in wallet receive/display code. Display mistakes are
`WALLET_SECURITY`, not `CONSENSUS`.

---

## B. SQLite crash campaign

Executed twice:

1. Synthetic production schema (`CREATE TABLE main(key BLOB PRIMARY KEY NOT NULL, value BLOB NOT NULL)`, `user_version=0`, DELETE journal).
2. Copy of a real `rigor-wallet.sqlite` from the isolated functional datadir (node stopped; exclusive lock released).

WAL is **not** enabled (`journal_mode=delete` on the production file).

| Case | Result |
|------|--------|
| Normal start → records → clean shutdown → restart | balances match pre/post (pre-mint and post-mint) |
| A. SIGKILL during open `BEGIN`+`INSERT` (WalletBatch-shaped write) | partial key **not** visible; prior KV intact; integrity ok |
| B. SIGKILL before `COMMIT` | no partial transaction visible |
| C. `COMMIT` then SIGKILL before process exit | committed row survived |
| D. Corrupt btree page | `PRAGMA integrity_check` / open fails; **not** a silent empty wallet |

Production: `TxnBegin` / `COMMIT` / `ROLLBACK`; batch close aborts an open txn;
`fullfsync=true`; `synchronous=OFF` only with `-unsafesqlitesync`. Functional
tests use unsafe sync; crash A–C used `synchronous=FULL`.

---

## C. Schema verification

| Check | Evidence |
|-------|----------|
| Filename | `WALLET_DATABASE_FILENAME = "rigor-wallet.sqlite"`; live file basename matches |
| Backend | `USE_SQLITE=1`; `USE_BDB` is not defined |
| Table | `main` present; `user_version=0` |
| BDB `wallet.dat` | `loadwallet` refuse: *This is a Berkeley DB wallet. RIGOR wallets are SQLite descriptor wallets only; there is no migration path.* |
| Unknown `user_version=99` | *Unknown sqlite wallet schema version 99. Only version 0 is supported* |
| Asset column | **Not present.** Script-as-asset remains classifier-side |

---

## D. Asset display boundary

Wallet is not asset authority. `GetAssetBalance(asset)` filters
`AssetTypeFromScriptPubKey`. Immature coinbase (including coinbase RES mint)
is excluded from trusted via `IsTxImmatureCoinBase`.

Isolated `devtest` node with `-reswalletbootstrap` (wallet-owned bech32m fee
address) and a saturating RES mint:

| Bucket | Observed |
|--------|----------|
| After 1 block | `oru.trusted=0`, `res.trusted=0`; both immature > 0 and **not equal** |
| After maturity | `oru.trusted=1000.00000000`, `res.trusted=1.000000` (separated) |
| Send 10 ORU | RES trusted did **not** absorb the ORU amount |
| RES mint `4d310c8a…:2` | `res_mint=true`, `coinbase=true`; **not** added to `res.trusted` while immature |

No ORU→RES or RES→ORU conversion in display buckets.

---

## E. Corruption matrix

All cases used copies under the isolated tmpdatadir. Node stopped or wallet
unloaded before SQLite mutation (production `locking_mode=exclusive`).

| # | Case | Result |
|---|------|--------|
| 1 | Truncated file (128 bytes) | refuse: *Data is not a RIGOR SQLite wallet* (`IsSQLiteFile` size/magic) |
| 2 | Malformed KV (`name` blob) | `NEED_RESCAN` / noncritical warning path — **not** a silent healthy load |
| 3 | Unknown schema | refuse: schema version 99 |
| 4 | Random btree page corruption | integrity failure: *database disk image is malformed*; `loadwallet` refuses |
| 5 | Missing TX + bestblock records | wallet loaded; `getbalances` recovered via chain rescan |

No silent empty wallet on page corruption.

**Residual (WALLET_SECURITY, not CONSENSUS, not a CoinsDB finding):** a garbage
**TX** KV is detected (`DataStream::read(): end of data: iostream error`) and
sets `NEED_RESCAN`. A harness `loadwallet` of that wallet against an already
long chain started a from-height-0 rescan and dropped the RPC before
completion. That is not silent-empty and was **not** patched in this TASK
(no schema change, no CoinsDB change). E.2 in the passing gate uses a `name`
record so the fail-closed check does not depend on that rescan.

Inherited Core: `DatabaseBatch::Read` deserialize catch returns false (missing)
for point reads. Cursor `LoadWallet` still walks every KV. Key-type corruption
is `CORRUPT`. Documented in P0-087 as P2.

---

## F. Fuzz

`fuzz/wallet_sqlite/run_smoke.sh`: header / unknown `user_version` / truncated /
mutated-page refuse; corpus seeds do not crash.

```
FUZZ_STATUS = PASS
CRASH_FOUND = NO
```

---

## G. Do not fix (this TASK)

Did not:

- add an asset column
- store consensus data in SQLite
- change wallet schema
- enable WAL
- modify CoinsDB
- add Release Guardian `DATABASE_SECURITY` (P0-091 did; this gate now requires it)

---

## H. Deliverables

- `docs/security/RIGOR_WALLET_SQLITE_AUDIT_P0_089.md` (this file)
- `formal_verification/wallet_sqlite_audit_manifest.json`
- `tools/run-wallet-sqlite-audit.sh`
- `tools/wallet_sqlite_campaign.py`
- `core/test/functional/feature_wallet_sqlite_audit_p0_089.py` (gate-only; not in `test_runner.py`)
- `fuzz/wallet_sqlite/run_smoke.sh`

Campaign dumps stay in gitignored `formal_verification/wallet_sqlite_audit/`.

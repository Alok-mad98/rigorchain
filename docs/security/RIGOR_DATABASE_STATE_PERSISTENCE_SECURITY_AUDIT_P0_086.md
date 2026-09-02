# RIGOR Database / CoinsDB / State Persistence Security Audit (P0-086)

**Task:** `TASK-RIGOR-DATABASE-STATE-PERSISTENCE-SECURITY-AUDIT-P0-086`  
**Date:** 2026-09-02  
**Follow-up:** fail-closed fix for P0-086-01 / P0-086-02 (Scheme A) plus P1
items P0-086-03 / P0-086-04 / P0-086-06. Persistence is a carrier of
consensus state. It is not a second Consensus Kernel. This document does
**not** modify the Consensus Kernel, Transaction Kernel, Ledger, genesis
authority, issuance, RES, draw, PoW, ASERT, consensus parameters, P2P
consensus rules, wallet consensus behavior, formal theorems, or existing
Coq proofs.

This is not a LevelDB formalization and not a claim that
`DATABASE_TEST_PASS` implies `DATABASE_FORMALLY_PROVED`.

```
DATABASE_SECURITY_AUDIT                    = PASS
DATABASE_CONSENSUS_BOUNDARY                = CLOSED
DATABASE_FAIL_CLOSED                       = TRUE
CACHE_DB_EQUIVALENCE                       = PASS
REORG_RECOVERY                             = PASS
REBUILD_EQUIVALENCE                        = PARTIAL
CONSENSUS_STATE_EQUIVALENCE                = TRUE
CORRUPTION_TEST                            = PASS
DATABASE_CONSENSUS_DIVERGENCE              = 0
FUZZ_STATUS                                = PASS
CRASH_FOUND                                = NO
UNDEFINED_BEHAVIOR                         = NO
DATABASE_CONSENSUS_AUTHORITY_COUNT         = 0
DATABASE_VALIDATION_AUTHORITY_COUNT        = 1
DATABASE_STATE_RECONSTRUCTION_PATH_COUNT   = 5
INHERITED_BITCOIN_DB_SURFACE               = AUDITED
DATABASE_FORMAL_PROOF                      = FALSE
DATABASE_TRUST_BOUNDARY                    = EXTERNAL_INFRASTRUCTURE
RELEASE_GUARDIAN_DATABASE_COVERAGE         = FULL
DATABASE_RELEASE_GATE                      = PARTIAL
PRODUCTION_CONSENSUS_MODIFIED              = NO
COIN_DISK_FORMAT                           = LEAF+CRC32C_RGCI
PRE_CHECKSUM_CHAINSTATE                    = WIPE_OR_REINDEX

FINAL_VERDICT:
RIGOR_DATABASE_STATE_PERSISTENCE_SECURITY_AUDIT = PASS
DATABASE_RELEASE_GATE                           = PARTIAL
```

Reproduce: `bash tools/run-database-state-persistence-security-audit.sh`

**Scheme A (chosen):** CRC-32C (Castagnoli) over
`nHeight || fCoinBase || nValue || scriptPubKey` (domain tag `RGCI`) is
appended by `CoinDiskFormatter` on CoinsDB values and undo records. It is
**not** part of `Coin::Serialize` (MuHash / `RIGOR/STATE_HASH/v4` leaves).
Deserialize / checksum / oversized-script / trailing-byte failure throws
`DESERIALIZATION_FAILURE / DATABASE_CORRUPTION`. `CDBWrapper::Read` of an
**existing** key must not return false. `CCoinsViewErrorCatcher` still
`std::abort()`s. Height-0 non-coinbase undo throws; `DisconnectBlock`
`AbortNode`s. Genesis coinbase undos (`nHeight=0`, `fCoinBase=1`) keep
stored metadata.

Checksum is never applied to `DataStream` leaves. Do not add `GetType()`
to `DataStream` to reuse disk encoding.

**Ops:** no in-place migration. Wipe `chainstate/` and run `-reindex`, or
`-reindex-chainstate` from `blk*.dat`. Pre-v4 / pre-CRC32C / Bitcoin-script
compressed records are unreadable. AssumeUTXO dumps use leaf `Coin`
encoding (no checksum); regenerate dumps after a leaf-format change.
Pre-mainnet: wipe and reindex is the supported path. Do not silently
repair corrupt records.

Remaining conditions (not a fail-closed regression):
- `REBUILD_EQUIVALENCE=PARTIAL` — production `rigord -reindex` was not
  executed by this gate (harness consensus-state digest only).
- `DATABASE_RELEASE_GATE=PARTIAL` — do not treat a green harness as a
  mainnet release certificate while production reindex is unproven.
- `RELEASE_GUARDIAN_DATABASE_COVERAGE=FULL` — Guardian Stage 5b runs
  `tools/run-database-state-persistence-security-audit.sh` as an
  independent stage. Guardian consensus Stages 1–5 were not rewritten.
- `DATABASE_VALIDATION_AUTHORITY_COUNT=1` — `EntersUtxoSet` in
  `AddCoin` / `AddCoins` (P0-086-05); not a second Kernel.

P0-086B (dual-asset) still says decode-failure maps to missing coin. That
sibling report is **stale** relative to Scheme A in this document. Script
mutation with a *recomputed* CRC remains a LevelDB trust-boundary issue
(P0-086-11), not a random-corruption accept path.

---


## Proof scope

### PASS (limited)

| Claim | Meaning |
|-------|---------|
| No second Consensus Kernel in the DB layer | `coins.cpp` / `txdb.cpp` / `dbwrapper.*` / `undo.h` / `compressor.*` do not define `EvaluateConnect`, `ConnectBlock`, issuance, draw, or predicate verify. Lint: `tools/lint-database-consensus-boundary.py`. |
| LevelDB I/O errors abort | `HandleError` throws `dbwrapper_error`. `CCoinsViewErrorCatcher::GetCoin` calls `std::abort()` so an I/O error is not “coin absent”. |
| Existing-key deserialize / checksum failure aborts | `CDBWrapper::Read` throws; ErrorCatcher aborts. Not `TX_CONSENSUS`. |
| Height / coinbase / amount / script integrity | Scheme A CRC-32C on disk/undo via `CoinDiskFormatter`. |
| Cache overlay is deterministic on clean data | Same golden chain under clean / cache-heavy / restart / flush+restart yields one consensus-state digest. |
| Reorg + undo reconstructs RIGOR fields in the harness | Spent / created / coinbase / RES / mint undo round-trips. Convergence is not stored and is not restored as a UTXO. |
| Wallet / pool / explorer DBs are not accept/reject authorities | SQLite wallet and pool SQL are off the ConnectBlock path. |
| Block-index `GetValue` failure is fail-closed | `LoadBlockIndexGuts` returns error if a `CDiskBlockIndex` value cannot be read. Pre-H-1 truncated paid-winner records throw. |

### NOT CLAIM

| Out of scope | Why |
|--------------|-----|
| LevelDB correctness proof | LSM, WAL, checksums, and compaction are external infrastructure. |
| Coq model of CoinsDB | This TASK does not add a theorem. |
| Byte-identical LevelDB files after rebuild | Not required. `CONSENSUS_STATE_EQUIVALENCE` is the target. |
| Production `-reindex` of a live datadir | The gate runs a replica harness, not `rigord -reindex`. |
| “Inherited from Bitcoin Core, therefore safe” | Forbidden. The inherited surface was re-audited because Coin, undo, ConnectBlock, and block-index fields changed. |

---

## 1. DATABASE_AUTHORITY_MAP

```
blk*.dat / rev*.dat          (block + undo bytes)
        │
        ▼
ConnectBlock / DisconnectBlock / ReplayBlocks / -reindex
        │
        ▼
LedgerApply / Transaction Kernel / Consensus Kernel     ← accept / reject
        │
        ▼
CCoinsViewCache  (dirty / fresh overlay)
        │
        ▼
CCoinsViewErrorCatcher  (I/O and deserialize → abort)
        │
        ▼
CCoinsViewDB / chainstate/   (LevelDB Coin records)
```

| Database | Writer | Reader | Affects consensus | Rebuildable | Consensus authority |
|----------|--------|--------|-------------------|-------------|---------------------|
| CoinsDB `chainstate/` | `CCoinsViewCache::Flush/Sync` → `CCoinsViewDB::BatchWrite` | `GetCoin` / `HaveCoin` / cursor via cache | Yes (UTXO presence, amount, script, height, coinbase) | Yes, from `blk*.dat` | **No** |
| Block index `blocks/index/` | `CBlockTreeDB::WriteBatchSync` | `LoadBlockIndexGuts` | Yes (header location, nStatus, RIGOR memos) | Yes, from `blk*.dat` | **No** |
| Undo `rev*.dat` | `UndoWriteToDisk` | `DisconnectBlock` / `ReplayBlocks` | Yes (restore spent coins) | Yes, if blocks remain | **No** |
| Block files `blk*.dat` | `BlockManager` | Connect / reindex | Yes | No (they *are* the source) | **No** |
| Wallet `rigor-wallet.sqlite` | wallet | wallet / RPC | No | Yes | **No** |
| txindex / blockfilter / coinstats | optional indexes | RPC / P2P filters | No | Yes | **No** |
| Pool / explorer SQL | pool / explorer | pool / explorer | No | Yes | **No** |
| Observational / runtime supply mirror | `ConnectBlock` after validation | RPC / explorer | No (memory + optional index) | Yes | **No** |

`CBlockIndex` fields that **are** persisted and **are not** Coin fields:

| Field | Generated by | Read by | Consensus input? | Rebuildable |
|-------|--------------|---------|------------------|-------------|
| `m_draw_candidate_root` | ConnectBlock | draw window | Memo; draw is re-derived | Yes |
| `has_res` / `last_res_mint_block_hash` | ConnectBlock | RPC / window | Cached | Yes |
| `m_block_res_mint_sum` / `m_block_converge_sum` | ConnectBlock | index / RPC | Not a draw input | Yes |
| `m_paid_winner_txid` | ConnectBlock | window assembly | **Not a consensus input.** `ValidatePaidWinnerRecord` re-checks against the block. Corrupt record fails the read. | Yes |
| `m_draw_set` | ConnectBlock | child connect after prune | Same memo discipline | Yes |
| Observational cumulative convergence | ConnectBlock `Update` | RPC | Isolated; not script/mint | Yes |
| `m_runtime_supply_mirror` | post-validation | observational | Memory-only | Yes |

Asset, issuance, RES, draw, maturity, and supply are **not** extra Coin columns.
They are inferred from `scriptPubKey` + height + kernel rules after the record
is loaded.

---

## 2. Core questions

### A. Can bad DB data make a node accept an invalid block?

**Not via deserialized Coin metadata after Scheme A.** Height and
`fCoinBase` are covered by `CoinIntegrityChecksum`. A flipped coinbase bit
or reduced height fails checksum verification and aborts (or refuses to
load the chainstate). Oversized scripts and failed `DecompressScript` throw
instead of rewriting the script.

BIP30 overwrite rejection uses `HaveCoin` / `Exists` (key presence). A
value that fails to deserialize no longer looks absent: `Read` throws and
ErrorCatcher aborts. RIGOR sets `BIP34Height = 1`.

### B. Can bad DB data make a node reject a valid block?

**Not by mapping deserialize failure to `TX_CONSENSUS`.** An existing key
with a malformed value throws `DESERIALIZATION_FAILURE / DATABASE_CORRUPTION`.
`CCoinsViewErrorCatcher::GetCoin` calls `std::abort()`. ConnectBlock does
not label that as `bad-txns-inputs-missingorspent`.

A **missing** key is still absence (correct). A **well-formed** coin that
fails Kernel rules is still `CONSENSUS_INVALID` (correct).

### C. After damage, is reindex / rebuild unique?

**For a complete `blk*.dat` history, yes in principle:** ConnectBlock is
deterministic, so a clean replay of the same headers and blocks yields one
UTXO set. The harness confirms `CONSENSUS_STATE_EQUIVALENCE` on the golden
chain. Production `-reindex` was **not** executed by this gate
(`REBUILD_EQUIVALENCE = PARTIAL`).

### D. Clean / existing / reconstructed / reindexed / interrupted write?

| Path | Result in this audit |
|------|----------------------|
| Clean DB → ConnectBlock | Harness digest D |
| Cache-heavy → ConnectBlock | Same D |
| Restart → ConnectBlock | Same D |
| Flush → restart → ConnectBlock | Same D |
| Reorg then continue | Same D as the clean alternate branch |
| Interrupted `BatchWrite` | Production writes `DB_HEAD_BLOCKS` then coins then `DB_BEST_BLOCK`. `ReplayBlocks` rolls back / forward. Inherited; not re-run against LevelDB here. |
| Production `-reindex` | Not executed. Flag stays PARTIAL. |

### E. Is there a second copy of consensus logic?

**Not a second Kernel.** Recorded as
`DATABASE_CONSENSUS_LOGIC_DUPLICATION` (membership only; P0-086-05):

1. `EntersUtxoSet` (`AddCoins` / `AddCoin` / `DisconnectBlock`) decides UTXO
   membership (skip convergence and unspendable scripts).
2. Amount / asset / maturity are interpreted **after** a successful load by
   `CheckTxInputs` / `ClassifyRigorTxout` — Kernel/tx-verify, not CoinsDB.

Closed by this fix (were hidden transforms; now fail-closed):

- `ScriptPlainFormatter::Unser` oversized → throw `DATABASE_CORRUPTION`
- `CDBWrapper::Read` / `GetKey` / `GetValue` existing-key deserialize or
  trailing bytes → throw (not missing)
- `ApplyTxInUndo` height-0 non-coinbase → throw; `DisconnectBlock` aborts
- Bitcoin BIP30 exception heights, dummy undo version, `DB_COINS='c'`,
  P2PKH/P2SH/P2PK Coin compression, and `RequiresMaturity()` removed

---

## 3. CoinsDB / UTXO encoding

Canonical Coin **leaf** (`Coin::Serialize`, MuHash / `RIGOR/STATE_HASH/v4`):

```
VARINT( (coinbase ? 1 : 0) | (height << 1) )
VARINT( CompressAmount(nValue) )
VARINT( scriptLen ) || script
```

Canonical Coin **disk / undo** (`CoinDiskFormatter`):

```
leaf bytes
uint32 LE CRC-32C Castagnoli   # domain "RGCI"; not a MAC
```

`nHeight` and `fCoinBase` are a real `uint32_t` and `bool` (not a 31+1
bitfield). Script bytes are plain; Bitcoin P2PKH/P2SH/P2PK specials are
not used on the Coin path (`ScriptCompression` remains for compress tests
only).

Checksum is never part of `Coin::Serialize`. `DataStream` leaves omit it.

Canonical key:

```
0x43 ('C') || txid[32] || VARINT(vout)
```

Undo equals disk Coin. No dummy version byte. No `DB_COIN_INTEGRITY`
format marker. `NeedsUpgrade()` is always false; recovery is wipe /
`-reindex`.

Spent coins are **erased**, not stored as a spent record.

```
DB serialization  ↔  C++ Coin  ↔  Consensus Kernel input
```

Serialization is storage **plus** three implicit interpretations:

- `IsSpent` ⇔ `CTxOut::IsNull` (`nValue == -1`).
- Asset ⇔ `scriptPubKey` (ORU v0/20, RES v1/32, `RIGOR_MARKER_RES_MINT`,
  converge, return).
- Oversized script ⇔ throw `DATABASE_CORRUPTION`.

Golden vectors:
`formal_verification/database_state_golden_vectors.json`.

---

## 4. Database ↔ Consensus Kernel boundary

```
ConnectBlock
    → CheckTxInputs / LedgerApply / Transaction Kernel
        → CCoinsViewCache::AccessCoin / SpendCoin / AddCoins
            → CCoinsViewErrorCatcher
                → CCoinsViewDB::GetCoin
                    → CDBWrapper::Read
```

Hidden transformations found:

| Kind | Site | Risk |
|------|------|------|
| Deserialize exception → throw | `CDBWrapper::Read` | Fail-closed (P0-086-01) |
| I/O error → throw → abort | `HandleError` / ErrorCatcher | Fail-closed |
| Missing key → false | `status.IsNotFound` | Correct empty |
| Oversized script → throw | `ScriptPlainFormatter` | Fail-closed |
| Trailing bytes → throw | `CDBWrapper` consume-all | Fail-closed |
| Amount decompressed `> MAX_MONEY` | `AmountCompression` | Fail-closed |
| Endian | Integers little-endian / VARINT | Deterministic |
| Stale cache | DIRTY/FRESH rules | Inherited; wrong FRESH is a consensus bug if mis-set |
| Cursor prefix end | `HasKeyType`; `'C'` parse fail is corruption | Fail-closed |

**High risk closed:** a DB read failure of an **existing** key is
`DATABASE_CORRUPTION`, not “coin does not exist”.

---

## 5. FAIL-CLOSED

| Injection | Production result | Class |
|-----------|-------------------|-------|
| Missing key | `GetCoin` false | empty (correct) |
| Malformed / truncated value | `Read` throw → ErrorCatcher abort | `DATABASE_CORRUPTION` |
| Wrong type byte on key | not found / cursor stop | missing |
| Wrong amount / height / coinbase | checksum mismatch → abort | `DATABASE_CORRUPTION` |
| Invalid compressed script | throw | `DATABASE_CORRUPTION` |
| Duplicate entry | last write wins (LevelDB) | store |
| Iterator GetValue fail (block index) | throw / `LoadBlockIndexGuts` error | fail-closed |
| Iterator GetValue fail (coins cursor) | throw (no longer skipped) | fail-closed |
| LevelDB read I/O | throw → abort | fail-closed |
| LevelDB write I/O | throw | caller abort / flush fail |
| Batch mid-flush crash | `DB_HEAD_BLOCKS` + `ReplayBlocks` | recovery required |
| Checksum (record-level) | CRC-32C `RGCI` on `CoinDiskFormatter` | fail-closed |
| Height-0 non-coinbase undo | throw → `AbortNode` | fail-closed |

`DATABASE_FAILURE_MUST_NOT_CREATE_CONSENSUS_ACCEPTANCE` holds for Coin
value corruption and height / coinbase mutation.

`DATABASE_CORRUPTION_FAIL_CLOSED` is **TRUE**.

---

## 6. CACHE_DB_EQUIVALENCE

`CCoinsViewCache` is an overlay with DIRTY / FRESH. Production comments
state that a wrong FRESH/DIRTY combination is a consensus failure.

Harness paths on the golden chain (Genesis → 1 → 2′ → 3′):

```
Path A  clean flush after every block
Path B  cache-heavy (flush only at tip)
Path C  restart (serialize all coins, new cache)
Path D  flush → restart → continue
```

All four produce one SHA-256 consensus-state digest. Inherited
`coins_tests.cpp` covers random cache/flush/undo sequences for the Bitcoin
Coin shape. RIGOR-specific skip of convergence is included in the harness.

Stale cache after a **failed** `Flush` leaves the cache undefined
(documented). That is a node-abort / recovery case, not an accept path.

---

## 7. Reorg / undo / crash recovery

Production:

1. `UpdateCoins` spends inputs into `CTxUndo` and `AddCoins` creates outputs.
2. `DisconnectBlock` spends created outputs (except unspendable and
   convergence) and `ApplyTxInUndo` restores prevouts.
3. `BatchWrite` marks `DB_HEAD_BLOCKS = {new, old}`, writes dirty coins in
   chunks, then `DB_BEST_BLOCK`.
4. `ReplayBlocks` disconnects the old branch and roll-forwards the new one
   when heads are a 2-vector.
5. `feature_dbcrash.py` is the inherited crash-injection functional test
   (`simulate_crash_ratio`). It does not lock RIGOR mint / converge /
   RES undo.

Harness: disconnect block 2, apply 2′ and 3′, compare to a clean alternate
branch. Digests match. Undo bytes restore amount, script, height, coinbase
for ORU / RES / mint. Convergence never entered the UTXO set, so undo does
not restore it (symmetric with `AddCoins`).

`ApplyTxInUndo` height-0 **non-coinbase** is P0-086-06 (fixed): no sibling
copy; `DISCONNECT_FAILED`. Genesis coinbase undos keep height 0 + coinbase.

---

## 8. DATABASE REBUILD EQUIVALENCE

```
REBUILD_EQUIVALENCE            = PARTIAL
CONSENSUS_STATE_EQUIVALENCE    = TRUE   (harness golden chain)
LevelDB file bytes identical   = not required
```

A. Live apply of the alternate branch  
B. Serialize every Coin, deserialize, compare digest  
C. Cache drop / restart  
D. Replay blocks from the in-memory “block storage” of the harness  

A–D agree. Production `-reindex` / delete-`chainstate` was not run against
`rigord`. That is why the flag is PARTIAL, not a forced PASS.

---

## 9. DATABASE CORRUPTION TEST

The harness and the C++ standalone inject empty, truncated, flipped,
wrong-type, oversized, checksum-tampered, and height/coinbase-tampered
payloads.

```
corrupted / truncated Coin
    → CDBWrapper::Read throws DESERIALIZATION_FAILURE
    → CCoinsViewErrorCatcher std::abort()
    → not TX_CONSENSUS / not inputs-missingorspent
```

```
oversized script / DecompressScript false
    → ios_base::failure DATABASE_CORRUPTION
```

```
height / fCoinBase bit flip without matching checksum
    → Coin integrity checksum mismatch → abort
```

`CORRUPTION_TEST = PASS`.

---

## 10. Fuzz

`fuzz/database/` covers:

- Coin value decoder
- undo decoder
- key decoder (type byte `C`)
- truncated / oversize / random records

Smoke: `fuzz/database/run_smoke.sh`.  
`FUZZ_STATUS=PASS`, `CRASH_FOUND=NO`, `UNDEFINED_BEHAVIOR=NO`.

This is not a long-running campaign and not a proof of the absence of UB
in LevelDB itself.

---

## 11. RIGOR-specific fields

| Field | Persisted? | Generated | Read | Consensus? | Rebuild? | Canonical ser? | Overflow guard? | Corrupt fail-closed? | Second path? |
|-------|------------|-----------|------|------------|----------|----------------|-----------------|----------------------|--------------|
| `nValue` | Yes (compressed) | tx | CheckTxInputs | Yes | Yes | Yes | `MoneyRange` after load | Yes (checksum) | No |
| Asset | No (script) | ClassifyRigorTxout | CheckTxInputs | Yes | Yes | script bytes | fail-closed classify | No | One classifier |
| Issuance / RES mint | Script + coinbase mint output | Kernel | Kernel | Yes | Yes | script | mint kernel | No in Coin | No |
| Draw | Block index memo `m_draw_set` / paid winner | ConnectBlock | window | Memo only | Yes | CDiskBlockIndex | ValidatePaidWinnerRecord | Yes for paid winner | No |
| Maturity | `nHeight` + `fCoinBase` | AddCoins | CheckTxInputs | Yes | Yes | VARINT code + checksum | height subtraction | **Yes** (checksum) | `nCoinbaseMaturity` in tx_verify |
| Convergence | **Not a UTXO** | AddCoins skip | in-block only | Yes (destroyed) | N/A | — | — | — | Membership rule duplicated |
| Supply-visible | Observational / RPC | post-validation | RPC | No | Yes | not Coin | — | — | No |
| Witness | In the block, not Coin | tx | predicate | Yes | Yes | tx | — | — | No |
| Predicate | Not persisted | verify | verify | Yes | N/A | — | — | — | No |

---

## 12. Error-class matrix

| Class | Meaning | Allowed mapping |
|-------|---------|-----------------|
| `CONSENSUS_INVALID` | Well-read state fails Kernel / tx rules | Must not be used for I/O or decode failure |
| `DATABASE_FAILURE` | LevelDB I/O | Abort / stop |
| `NODE_RECOVERY_REQUIRED` | `DB_HEAD_BLOCKS`, `NeedsUpgrade`, reindex flag | Restart with `-reindex` |
| `CORRUPTED_PERSISTENT_STATE` | Value exists but will not deserialize, or checksum fails | Abort / `-reindex-chainstate`. Must not become `CONSENSUS_INVALID`. |

Production mapping after Scheme A:

```
CORRUPTED_PERSISTENT_STATE  →  abort / FAILURE_INCOMPATIBLE_DB   (P0-086-01, P0-086-02)
```

---

## 13. Authority lock

`tools/lint-database-consensus-boundary.py` scans coins / txdb / dbwrapper /
undo / compressor.

```
DATABASE_CONSENSUS_AUTHORITY_COUNT           = 0
DATABASE_VALIDATION_AUTHORITY_COUNT          = 1
DATABASE_STATE_RECONSTRUCTION_PATH_COUNT     = 5
```

Reconstruction paths counted: `ReplayBlocks`, `-reindex-chainstate` notes,
`DB_HEAD_BLOCKS`, block-file reindex, UTXO snapshot load.

The database is **not** a second Consensus Kernel. Remaining validation
site is UTXO-membership via `EntersUtxoSet` (P0-086-05).
Value corruption is fail-closed, so the consensus boundary is **CLOSED**
for the P0 deserialize / metadata findings.

---

## 14. Golden vectors

`formal_verification/database_state_golden_vectors.json` locks:

1. clean chainstate Coin  
2. spent (erased, not stored)  
3. created Coin  
4. coinbase  
5. RIGOR RES witness v1  
6. RES mint script  
7. reorg digest  
8. disconnect / undo  
9. restart  
10. rebuild-from-serialize  
11. corrupted record outcomes  
12. missing / empty record  

They lock serialization, reconstruction, and the failure boundary.
They are **not** a formal proof.

---

## 15. Formal verification boundary

| Layer | Status |
|-------|--------|
| A. Coq Consensus Kernel | Unchanged; not a DB theorem |
| B. C++ / production differential | Not claimed |
| C. Database serialization | Locked by vectors + standalone codec |
| D. Recovery equivalence | Harness only |
| E. Database implementation | Inherited wrapper; fail-closed gaps recorded |
| F. LevelDB | `DATABASE_TRUST_BOUNDARY = EXTERNAL_INFRASTRUCTURE` |

```
DATABASE_FORMAL_PROOF     = FALSE
DATABASE_FORMALLY_PROVED  = FALSE
```

---

## 16. Inherited Bitcoin Core tests

### INHERITED_TESTS (still useful)

- `coins_tests.cpp` — cache, flush, FRESH/DIRTY, random apply/undo  
- `validation_flush_tests.cpp` — cache size / flush triggers  
- `validation_chainstate_tests.cpp` — chainstate init  
- `fuzz/coins_view.cpp`, `fuzz/coinscache_sim.cpp`  
- `feature_dbcrash.py` — LevelDB crash injection  

### RIGOR_REQUIRED_RETESTS

Inherited tests do **not** lock:

- witness v0 ORU / v1 RES / `RIGOR_MARKER_RES_MINT` Coin bytes  
- convergence **not** entering the UTXO set  
- undo of those RIGOR outputs  
- `CDiskBlockIndex` paid-winner / draw-set fail-closed (covered separately
  by `paid_winner_index_tests.cpp`)  
- deserialize failure **not** becoming `TX_CONSENSUS`  
- height / coinbase corruption  

This TASK added the audit harness, Scheme A checksum, and fail-closed
deserialize path. Consensus Kernel / `EvaluateConnect` /
`ValidatePaidWinnerRecord` / `ClassifyRigorTxout` were not changed.

---

## 17. CI / Release Guardian

Genesis Release Guardian consensus Stages 1–5 were **not rewritten**.
Stage 5b is an independent database audit stage:

```
bash tools/run-database-state-persistence-security-audit.sh
```

```
RELEASE_GUARDIAN_DATABASE_COVERAGE = FULL
DATABASE_RELEASE_GATE              = PARTIAL
```

Fail-closed P0 items are closed. The release gate stays **PARTIAL**
because production `-reindex` was not executed and LevelDB remains
external infrastructure. Do not promote `DATABASE_TEST_PASS` to a
mainnet certificate.

---

## 18. Findings and remediation

| ID | Sev | Status | Notes |
|----|-----|--------|-------|
| P0-086-01 | P0 | **Fixed** | `CDBWrapper::Read` / `GetValue` throw `DESERIALIZATION_FAILURE / DATABASE_CORRUPTION`. ErrorCatcher aborts. Missing key still returns false. |
| P0-086-02 | P0 | **Fixed (Scheme A)** | CRC-32C Castagnoli over height, coinbase, amount, scriptPubKey via `CoinDiskFormatter`. Not in `Coin::Serialize`. Wipe / `-reindex`. |
| P0-086-03 | P1 | **Fixed** | `ScriptPlainFormatter` oversized throws; no rewrite to `RIGOR_MARKER_RETURN`. |
| P0-086-04 | P1 | **Fixed** | Coin path has no Bitcoin compressed-script types 4/5. `ScriptCompression` retained for compress tests only. |
| P0-086-05 | P1 | Recorded | UTXO-membership is `EntersUtxoSet` in `coins.cpp` / `DisconnectBlock`. Not a second accept/reject kernel. |
| P0-086-06 | P1 | **Fixed** | Height-0 non-coinbase undo throws; `DisconnectBlock` `AbortNode`s. Genesis coinbase undos kept. |
| P0-086-07 | P2 | Partial | Coins cursor `GetValue` deserialize now throws. Prefix end is `HasKeyType`. `CCoinsViewCache::Cursor` still skips a `GetKey`/`GetValue` false (not on ConnectBlock). |
| P0-086-08 | P2 | **Fixed** | Bitcoin BIP30 exception heights removed. Duplicate outpoints always rejected. |
| P0-086-09 | P2 | Recorded | `CCoinsViewDB::HaveCoin` uses `Exists` (no deserialize). ConnectBlock cache path uses `GetCoin` and aborts. |
| P0-086-10 | P2 | Recorded | `ReplayBlocks` wraps `CoinsDB()` directly, not ErrorCatcher. Deserialize still throws, not `TX_CONSENSUS`. |
| P0-086-11 | P2 | Recorded | CRC-32C is integrity, not a MAC. Recomputed-trailer tampering is the LevelDB trust boundary. |
| P0-086-12 | P2 | Recorded | Gate replica is not `test_bitcoin`. Production codec is locked by `coins_tests.cpp`. |
| P0-086-13 | coverage | Recorded | Production `-reindex` not executed. `REBUILD_EQUIVALENCE=PARTIAL`. |
| P0-086-14 | P2 | Recorded | Fuzz is smoke + overlay ops, not a long campaign. |

---

## 19. Audit principle

```
database  =  persistence of consensus state
          ≠  a second consensus referee
```

What holds:

```
DATABASE_REBUILD  ≡  CONSENSUS_STATE     (harness; production reindex PARTIAL)
DATABASE_CONSENSUS_AUTHORITY_COUNT = 0
DATABASE_CORRUPTION  ≠  CONSENSUS_ACCEPTANCE
DATABASE_FAILURE     →  FAIL CLOSED
DATABASE_RELEASE_GATE = PARTIAL
```

CoinsDB is a persistent carrier, not a second Consensus Kernel. Any
damaged record must fail closed. Do not silently repair it.

`INHERITED_BITCOIN_DB_SURFACE = AUDITED`. Inheritance was not used as a
waiver.

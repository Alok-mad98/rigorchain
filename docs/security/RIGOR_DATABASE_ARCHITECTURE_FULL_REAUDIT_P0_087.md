# RIGOR Database Architecture / CoinsDB / Dual-Asset / SQLite Full Re-Audit (P0-087)

**Task:** `TASK-RIGOR-DATABASE-ARCHITECTURE-FULL-REAUDIT-P0-087`  
**Date:** 2026-09-02  
**Nature:** Audit-first. Architecture review of persistence, not a scatter of bugfixes.

This TASK does **not** modify the Consensus Kernel, Transaction Kernel, Ledger,
genesis, issuance, RES, draw, PoW, ASERT, consensus parameters, P2P consensus
rules, wallet consensus behavior, formal theorems, Coq proofs, or Coin disk
format. P0-091 later closed this audit and added Guardian `Stage DATABASE_SECURITY`.

Pre-mainnet: there is no historical mainnet compatibility burden. The format
that already exists is the intended final format unless a P0 proves it cannot
safely represent dual-asset state.

```
RIGOR_DATABASE_ARCHITECTURE_AUDIT          = CLOSED
DATABASE_AUDIT_STATUS                      = CLOSED
DATABASE_CONSENSUS_BOUNDARY                = CLOSED
DATABASE_FAIL_CLOSED                       = TRUE
ORU_REBUILD_EQUIVALENCE                    = PASS
RES_REBUILD_EQUIVALENCE                    = PASS
REBUILD_EQUIVALENCE                        = PASS
ORU_CACHE_DB_EQUIVALENCE                   = PASS
RES_CACHE_DB_EQUIVALENCE                   = PASS
ORU_REORG_RECOVERY                         = PASS
RES_REORG_RECOVERY                         = PASS
CONNECT_DISCONNECT_SYMMETRY                = PASS
FAILED_CONNECT_NO_PERSISTENCE              = PASS
DATABASE_CONSENSUS_AUTHORITY_COUNT         = 0
DATABASE_VALIDATION_AUTHORITY_COUNT        = 1
DATABASE_CONSENSUS_DIVERGENCE              = 0
FUZZ_STATUS                                = PASS
MUTATION_STATUS                            = PASS
CORRUPTION_STATUS                          = PASS
DATABASE_FORMAT_CHANGED                    = FALSE
CONSENSUS_CODE_CHANGED                     = FALSE
FORMAL_THEOREMS_CHANGED                    = FALSE
WALLET_SQLITE_AUDIT_STATUS                 = CLOSED
SHARED_COIN_MODEL                          = PREFERRED_AND_KEPT
P0_REQUIRING_CODE_CHANGE                   = 0
RELEASE_GUARDIAN_DATABASE_SECURITY_STAGE   = ADDED
CURRENT_FORMAT_CANNOT_SAFELY_REPRESENT_RIGOR_DUAL_ASSET_STATE = FALSE

FINAL_VERDICT:
RIGOR_DATABASE_ARCHITECTURE_AUDIT = CLOSED
```

Reproduce: `bash tools/run-database-architecture-full-reaudit.sh`

**Closure (P0-091).** This architecture TASK originally stayed OPEN because
(1) production field-level `-reindex` was not executed here and (2) wallet
SQLite remained OPEN. Those two gates were proven by P0-088 and P0-089.
This document does not close them by prose: the architecture gate now
consumes those manifests and requires Guardian `Stage DATABASE_SECURITY`.

P0-086 (Scheme A CRC-32C fail-closed) and P0-086B (dual-asset) are inputs.
P0-086B's `ORU_CORRUPTION_FAIL_CLOSED=FALSE` is **stale** relative to Scheme A
for *random* value corruption. Script mutation with a *recomputed* CRC remains
an inherited local LevelDB trust-boundary fact, not a reason to split CoinsDB
or add an asset column.

---

## Method

| Rule | Applied |
|------|---------|
| AUDIT-FIRST | No production consensus / CoinsDB / SQLite format change in this TASK |
| Bitcoin Core DB foundation | Inherited Flush / undo / `DB_HEAD_BLOCKS` / ReplayBlocks / ErrorCatcher |
| Minimal RIGOR surface | Script-as-asset + Scheme A CRC + `EntersUtxoSet` membership |
| No second CoinsDB | `SHARED_COIN_MODEL` kept |
| No ORU_DB / RES_DB | Confirmed absent |
| No BDB wallet | SQLite-only; BDB files rejected |
| CoinsDB ≠ Wallet | Separate trust boundaries throughout |
| No forged CLOSED | Rebuild stays BLOCKED unless P0-088 production field compare is PASS |
| P0 stop-and-report | No P0 requiring code change was found |

NOT CLAIM: LevelDB correctness proof, Coq model of CoinsDB, byte-identical
LevelDB files after rebuild, `DATABASE_FORMALLY_PROVED`.

---

## A. Trust boundaries

```
COINSDB_TRUST_BOUNDARY       = EXTERNAL_INFRASTRUCTURE_LEVELDB
CHAINSTATE_TRUST_BOUNDARY    = EXTERNAL_INFRASTRUCTURE_LEVELDB_PLUS_BLK_REV
WALLET_SQLITE_TRUST_BOUNDARY = NON_CONSENSUS_SQLITE_KV
```

These are not one security domain.

```
blk*.dat / rev*.dat                 (block + undo bytes; rebuild source)
        │
        ▼
ConnectBlock / DisconnectBlock / ReplayBlocks / -reindex
        │
        ▼
LedgerApply / Transaction Kernel / Consensus Kernel     ← accept / reject
        │
        ▼
CCoinsViewCache          (dirty / fresh overlay; discarded on failed Connect)
        │
        ▼
CCoinsViewErrorCatcher   (I/O + deserialize → abort, never "missing coin")
        │
        ▼
CCoinsViewDB / chainstate/   (LevelDB Coin records, key 0x43)
```

Wallet SQLite is **not** on this path.

| Store | Engine | Writer | Reader | Affects consensus | Rebuildable | Consensus authority |
|-------|--------|--------|--------|-------------------|-------------|---------------------|
| CoinsDB `chainstate/` | LevelDB `CDBWrapper` | `Flush/Sync` → `BatchWrite` | cache → ErrorCatcher → `GetCoin` | Yes (UTXO carrier) | Yes, from `blk*.dat` | **No** |
| Block index `blocks/index/` | LevelDB | `WriteBatchSync` | `LoadBlockIndexGuts` | Yes (location + RIGOR memos) | Yes | **No** |
| `blk*.dat` / `rev*.dat` | flat files | `BlockManager` | Connect / reindex | Yes | Blocks *are* the source | **No** |
| Wallet `rigor-wallet.sqlite` | SQLite KV | wallet | wallet / RPC | **No** | Yes (rescan) | **No** |
| txindex / blockfilter / coinstats | optional | indexes | RPC | No | Yes | **No** |
| Pool / explorer SQL | external | apps | apps | No | Yes | **No** |

### COINSDB_TRUST_BOUNDARY

LevelDB holds one Coin per outpoint:

```
key   = 0x43 || txid32 || VARINT(vout)
value = Coin leaf || CRC32C_RGCI
leaf  = VARINT((coinbase?1:0)|(height<<1)) || VARINT(CompressAmount) || VARINT(scriptLen) || script
```

Asset, issuance, draw, and supply are **not** Coin columns. After a successful
load, `ClassifyRigorTxout` / `AssetTypeFromScriptPubKey` interpret
`scriptPubKey`. A checksum mismatch or malformed value is
`DATABASE_CORRUPTION` (throw → ErrorCatcher `std::abort()`), not
`TX_CONSENSUS`.

Partial flush uses inherited `DB_HEAD_BLOCKS` then `DB_BEST_BLOCK`.
`ReplayBlocks` is crash recovery, not a second Kernel.

### CHAINSTATE_TRUST_BOUNDARY

Chainstate = CoinsDB + block index + `blk*.dat`/`rev*.dat`.

RIGOR fields on `CDiskBlockIndex` (draw set, paid winner, has_res, mint/converge
sums, observational cumulative) are memos. `GetConvergenceDrawSetForBlock`
prefers the memo, else rebuilds from block+undo. Truncated pre-H-1 index records
throw in `LoadBlockIndexGuts` (fail-closed). They are not a second UTXO set.

### WALLET_SQLITE_TRUST_BOUNDARY

```
CWallet → WalletBatch (v25 KV records) → SQLiteBatch
       → rigor-wallet.sqlite
       → CREATE TABLE main(key BLOB PRIMARY KEY, value BLOB)
       → PRAGMA user_version = 0
```

Compiled `USE_SQLITE=1`, no `USE_BDB`. `MakeDatabase` rejects Berkeley DB files
with an explicit error. This is a detector, not a compatibility layer.

Wallet SQLite cannot accept or reject blocks. A wallet display bug is
`WALLET_SECURITY` or `OBSERVATIONAL`, never automatically `CONSENSUS`.

---

## B. Bitcoin Core foundation vs RIGOR modifications

Principle: if Core already guarantees it, do not reimplement.

| Surface | INHERITED_BEHAVIOR | RIGOR_MODIFICATION | SECURITY_EFFECT |
|---------|--------------------|--------------------|-----------------|
| 1. Coin serialization | VARINT code + compressed txout | `ScriptPlainFormatter` (no P2PKH/P2SH/P2PK special cases); CRC is **not** in `Coin::Serialize` (MuHash / STATE_HASH v4 leaves stay leaf-only) | Dual-asset scripts round-trip as opaque bytes. Leaves and disk encoding stay split. |
| 2. Key layout | `0x43 \|\| txid \|\| VARINT(vout)` | Unchanged. No `DB_ORU` / `DB_RES` | Shared outpoint identity. Collision only if same outpoint. |
| 3. BatchWrite | Erase `DB_BEST_BLOCK`, write `DB_HEAD_BLOCKS`, coins, then `DB_BEST_BLOCK` | Diagnostic `partial_flush_count` log only | Crash window unchanged. |
| 4. Cache flush | `CCoinsViewCache::Flush/Sync` DIRTY/FRESH | `AddCoin` returns early if `!EntersUtxoSet` | Convergence never occupies UTXO. Membership, not accept/reject. |
| 5. Undo | Per-input Coin restore | `TxInUndoFormatter = CoinDiskFormatter`; no dummy version byte; height-0 non-coinbase throws | Random undo corruption aborts. Genesis coinbase undos keep height 0 + coinbase. |
| 6. Block index / `DB_HEAD_BLOCKS` | Core two-hash marker | Extra RIGOR memos appended fail-closed (paid winner, draw set) | Truncation throws. Memos rebuildable from blocks when present. |
| 7. Replay | `DisconnectBlock` old branch, `RollforwardBlock` new branch | Unchanged Rollforward (SpendCoin+AddCoins, no Kernel) | Inherited: interrupted flush reconstructs UTXO from already-stored blocks. Asset identity comes from those scripts. |
| 8. Corruption | LevelDB I/O → `HandleError` throw; ErrorCatcher abort | Existing-key deserialize / trailing bytes / CRC mismatch throw (P0-086 Scheme A) | Corruption is not `bad-txns-inputs-missingorspent`. |
| 9. Restart | Load index, ReplayBlocks, ActivateBestChain | Same | Dirty ConnectBlock cache is not a datadir. |
| 10. Reorg rollback | `DisconnectTip` → `DisconnectBlock` → Flush | Same `EntersUtxoSet` skip on disconnect | UTXO membership symmetric. |
| 11. DisconnectBlock symmetry | Restore prevouts, erase created outputs | Skip non-`EntersUtxoSet` outputs; compare amount/script/height/coinbase | Asset identity is the script; it is compared. Index memos are not cleared (P1, rebuildable). |
| 12. Reindex | Wipe coins, replay `blk*.dat` via ConnectBlock | Same Kernel on every block | Unique reconstruction **in principle**. Production field compare **not executed** this TASK. |

Scheme A CRC is the one RIGOR disk-format addition already shipped in P0-086.
It is a corruption detector, not a MAC, and not a second Kernel.
`DATABASE_FORMAT_CHANGED=FALSE` for *this* TASK.

Do not add an asset tag. A tag plus `scriptPubKey` would be two authorities.

---

## C. Dual-asset ORU / RES

```
ORU_RES_STORAGE_MODEL = SHARED_COIN_MODEL
SHARED_COIN_MODEL     = PREFERRED_AND_KEPT
```

No `ORU_DB`, no `RES_DB`. One `Coin` carrier.

| Kind | How the database distinguishes it |
|------|-----------------------------------|
| ORU Coin | After load: `AssetTypeFromScriptPubKey` → `ASSET_ORU` (witness v0/20 or converge lock). Converge does **not** enter UTXO. |
| Ordinary RES Coin | `WITNESS_V1_RES` (v1/32, not retired CVG2/RES2 prefixes) |
| RES mint Coin | `RIGOR_MARKER_RES_MINT` script (`IsResMintScript`). Same Coin row. Usually coinbase. Script encodes `maturity_height` + payout hash. Spend maturity is still coinbase height vs `nCoinbaseMaturity`. At mint, Kernel requires `maturity_height == nHeight + nCoinbaseMaturity`. |

### Invariants

| Invariant | Result |
|-----------|--------|
| ASSET_IDENTITY_PERSISTENCE | **PASS_VIA_SCRIPT_AS_ASSET.** Identity is the persisted script, not a mutable column. |
| ASSET_CLASSIFICATION_DETERMINISTIC | **PASS.** Single production classifier: `ClassifyRigorTxout` / `AssetTypeFromScriptPubKey`. Unknown / empty / P2PKH / retired v1 prefixes → `ASSET_INVALID`, never default ORU. |
| ORU_RES_NO_IMPLICIT_CONVERSION | **PASS_AT_CONSENSUS.** `illegal-res-to-oru-conversion`, `mixed-asset-output`, `cross-asset-input`. Dual-pool RES spend with ORU fee is explicit. |
| MINT_CLASSIFICATION_DETERMINISTIC | **PASS.** `IsResMintScript` exact template. Ordinary v1 RES is not a mint. |
| MATURITY_PERSISTENCE | **PASS_VIA_HEIGHT_COINBASE_CRC.** Height and `fCoinBase` are in the CRC domain. Random flip aborts. |
| REORG_ASSET_IDENTITY_PRESERVED | **PASS_HARNESS.** Field-level reorg == clean alternate branch. |
| REINDEX_ASSET_IDENTITY_PRESERVED | **BLOCKED_PRODUCTION.** Harness rebuild field match is not `rigord -reindex`. |

### Task C checklist

1. **Classification vs attacker-controlled script bytes.** Yes: script *is* the asset. That is the Bitcoin UTXO model plus RIGOR's script-as-asset rule. A local attacker who can rewrite CoinsDB can also rewrite amounts. CRC stops *random* flips. A recomputed CRC is local disk compromise (P0-086-11). **Do not add a parallel asset field.**
2. **Persistence enough after reload.** Yes: leaf stores height, coinbase, amount, full script. Classifier is a pure function of the script.
3. **ORU → RES via DB mutation.** Random: CRC abort. Malicious valid CRC: local reclassify. Reindex from `blk*.dat` restores truth. Not remotely triggerable.
4. **RES → ORU.** Same.
5. **Mint vs ordinary RES.** Distinct scripts. Mutation of mint → v1 still `ASSET_RES` but loses mint lock (local). Kernel will not treat a v1 output as a mint.
6. **Convergence / draw in UTXO.** `AddCoins` / `AddCoin` skip `!EntersUtxoSet`. Disconnect skips the same. Draw is index memo + rebuild from undo.
7. **Immature mint spendable.** Spend path uses `coin.IsCoinBase()` + `nHeight` vs `nCoinbaseMaturity`. Mint script maturity is enforced at create. Random coinbase-bit flip aborts (CRC). Local valid-CRC tamper is inherited UTXO trust.
8. **ORU / RES maturity saved.** Same height/coinbase fields for both assets.
9. **cache → DB → restart → reindex identity.** Harness field tuples match. Production reindex BLOCKED.
10. **Connect / Disconnect / reorg identity.** Harness `CONNECT → DISCONNECT → CONNECT` tuples match. Reorg matches clean alt.
11. **GetValueOut / GetBalance / GetCoin / HaveCoin mix.** `GetCoin`/`HaveCoin` are untyped carriers (correct). `CheckTxInputs` splits pools. `GetValueOut()` without asset still sums all outputs — **OBSERVATIONAL / test** (P0-086B-04). Wallet `GetAssetBalance` is per-asset. Legacy `GetBalance()` still mixes atoms but is tests/bench-only; Qt/RPC `getBalance` uses ORU `GetAssetBalance`.

```
CURRENT_FORMAT_CANNOT_SAFELY_REPRESENT_RIGOR_DUAL_ASSET_STATE = FALSE
```

Do not change the disk format.

---

## D. Corruption / attack model

CRC-32C is not a MAC. Fail-closed means: **random or malformed records abort or refuse to load**, not "treat as missing and keep validating." Deliberate rewrite with a valid trailer is **local infrastructure compromise**. Recovery is wipe + `-reindex` from `blk*.dat`.

| # | Case | Disposition | Remote? | Consensus split? | Funds lock? | Wrong balance? |
|---|------|-------------|---------|------------------|-------------|----------------|
| 1 | Truncated record | **ABORT** (`Read` throw / ErrorCatcher) | No | No (node dies) | Local until reindex | No |
| 2 | Malformed serialization | **ABORT** | No | No | Local until reindex | No |
| 3 | Invalid VARINT | **ABORT** | No | No | Local | No |
| 4 | Invalid script length | **ABORT** (`ScriptPlainFormatter`) | No | No | Local | No |
| 5 | Invalid amount | **ABORT** (`MoneyRange`) | No | No | Local | No |
| 6 | Invalid height | **ABORT** (uint32 overflow or CRC) | No | No | Local | No |
| 7 | Invalid coinbase flag | **ABORT** if CRC leftover; **ACCEPT as local lie** if CRC recomputed | No | Local only | Local | Local |
| 8 | Asset-reclassification mutation | **ABORT** if CRC leftover; **ACCEPT as local lie** if CRC recomputed | No | Local only | Local; honest chain unaffected | Local until reindex |
| 9 | CRC mismatch | **ABORT** | No | No | Local | No |
| 10 | Valid CRC, malicious payload | **ACCEPT** as inherited LevelDB trust; **REBUILD** from blocks | No | Local only | Local | Local |
| 11 | Missing key | **REJECT** spend (`TX_CONSENSUS` missing/spent) — correct absence | No* | If DB uniquely wrong, this node diverges | Local | Local |
| 12 | Duplicate key | LevelDB last-write; Core map semantics | No | Same as Core | — | — |
| 13 | Stale key | `DB_HEAD_BLOCKS` + ReplayBlocks / reindex | No | Inherited recovery | — | — |
| 14 | Partial batch | `DB_HEAD_BLOCKS` set; ReplayBlocks **REBUILD** coins | No | Inherited | — | — |
| 15 | Interrupted flush | Same as 14 | No | Inherited | — | — |
| 16 | Crash before commit | Cache lost; disk old tip | No | Inherited | — | — |
| 17 | Crash after commit | New tip persisted | No | Inherited | — | — |
| 18 | Stale `DB_HEAD_BLOCKS` | ReplayBlocks error if heads unknown → start fail (**ABORT** / recovery) | No | No | Need reindex | No |
| 19 | Inconsistent undo | `DisconnectBlock` **ABORT** / `DISCONNECT_FAILED` | No | No | Reindex | No |
| 20 | Reorg interruption | DisconnectTip Flush then chain rewrite; crash uses ReplayBlocks | No | Inherited | — | — |

`*` Missing key is not remote CoinsDB write. A remote invalid spend is Kernel `TX_CONSENSUS` against an honest UTXO set.

```
CORRUPTED_DB → INVALID_BLOCK_ACCEPTED
```

**Not via deserialize / CRC failure.** Those abort. They do not become
`TX_CONSENSUS`.

**Possible only as local well-formed tampering** (case 10): the node treats the
tampered Coin as truth until rebuild. That is Bitcoin's CoinsDB trust model.
It is not remotely triggerable. It is not a network-wide invalid-block accept.
Fail-closed policy for *random* corruption is **ABORT**, which matches the
design. Abort is not automatically "safe" against an operator who rewrites
LevelDB with valid trailers; the designed recovery is `-reindex`.

P0-086-09: `CCoinsViewDB::HaveCoin` is `Exists` (key presence). ConnectBlock
cache `HaveCoin` goes through `FetchCoin`/`GetCoin` and aborts on bad values.
BIP30 overwrite uses cache `HaveCoin`. Residual P2, not a silent accept of
invalid blocks.

---

## E. Reindex / rebuild (highest priority)

Previous: `ORU_REBUILD_EQUIVALENCE=PARTIAL`, `RES_REBUILD_EQUIVALENCE=PARTIAL`.

This TASK requires production:

1. clean chainstate  
2. normal sync  
3. shutdown  
4. restart  
5. flush + restart  
6. `-reindex-chainstate`  
7. full `-reindex`  
8–12. compare chainstate, ORU, RES, mint, draw — **fields**, not only a hash  

Harness replica (this gate):

- Modes `clean` / `cache_heavy` / `restart` / `flush_restart` compared as
  sorted tuples `(outpoint, asset, amount, height, coinbase, script, mint)`.
- Reorg vs clean alternate: same tuples / per-asset digest / supply.
- Serialize-rebuild vs live: same.

```
HARNESS_REBUILD_FIELD_EQUIVALENCE = PASS
ORU_REBUILD_EQUIVALENCE           = BLOCKED
RES_REBUILD_EQUIVALENCE           = BLOCKED
REBUILD_EQUIVALENCE               = BLOCKED
```

`feature_reindex.py` only checks `getblockcount`. That is not field-level ORU/RES
equivalence and is not used as PASS.

This gate found an in-tree `core/src/rigord` binary. It still reports
**BLOCKED**, because a block-count restart is not a per-UTXO
`(asset, amount, height, coinbase, script, mint)` diff. Do not upgrade
PARTIAL/harness-PASS to production PASS.

**Tooling only (not consensus):** `lint-dual-asset-database-boundary.py` now
matches production `EntersUtxoSet` and the existing `coins.h` comment. Dual-asset
`catalog.json` was regenerated so it matches Scheme A CRC disk encoding (the
pre-CRC catalog no longer decoded).

Do not upgrade PARTIAL to PASS because a replica digest matched.

---

## F. ConnectBlock database atomicity

`STATE_MUTATION_BEFORE_ACCEPTANCE=TRUE` (P0-086-CB-06) remains a **code-order**
fact: per-tx `UpdateCoins` mutates the **stack-local** `CCoinsViewCache` before
`RigorConsensusKernel::EvaluateConnect`.

This TASK does not reorder ConnectBlock.

Persisted state:

| On ConnectBlock failure | What happens |
|-------------------------|--------------|
| Cache | Stack view discarded. Parent `CoinsTip()` unchanged. |
| DB coins | `ConnectTip` calls `view.Flush()` **only if** `ConnectBlock` returned true. |
| Undo | `WriteUndoDataForBlock` runs **after** Kernel success and after `fJustCheck`. Failure before that writes no undo. |
| Best block | `view.SetBestBlock` at end of successful ConnectBlock; not flushed on failure. |
| Chainstate tip | `m_chain.SetTip` after Flush + `FlushStateToDisk`. |
| Restart residue | Failed connect leaves no new Coin batch. |
| Reindex | Replays blocks through ConnectBlock; a block that failed is not the source of UTXO. |

Index memos (`m_draw_set`, `has_res`, mint sums, …) are written **after**
`EvaluateConnect` returns true, then `MarkBlockIndexDirty`. They are not written
on Kernel reject. `fJustCheck` (GBT / `TestBlockValidity`) does not mark dirty.

```
FAILED_CONNECT ⇒ NO_PERSISTED_CONSENSUS_STATE_CHANGE
FAILED_CONNECT_NO_PERSISTENCE = PASS
```

Covered failure classes (invalid block, tx, predicate, mint, convergence, RES
maturity, ORU/RES conversion): all return false from ConnectBlock / CheckTxInputs
**before** Flush. No P0 persistence leak.

`STATE_MUTATION_BEFORE_ACCEPTANCE` is still TRUE in-function. It does not persist.
Do not rewrite ConnectBlock in this TASK to make the flag FALSE.

WriteUndo success then process crash before coins Flush: undo may exist for a
block whose coins are not yet the tip. Inherited Core. Replay / next connect
handles it. Not a RIGOR dual-asset hole.

---

## G. Disconnect / reorg / rollforward

```
CONNECT → DISCONNECT → CONNECT     harness field tuples match
A → B → C vs A → B → reorg → D     harness match
```

| Object | Connect | Disconnect | Reorg |
|--------|---------|------------|-------|
| ORU Coin | AddCoin | Spend created / ApplyTxInUndo | Restored from undo |
| RES Coin | Same | Same | Same |
| RES mint Coin | Same (script identity) | Erased with coinbase outputs | Restored if spent inputs; mint output removed with block |
| Convergence | Not a Coin | Not spent | Not restored as UTXO |
| Draw | Index memo + rebuild | Observational caches invalidated; memo left on abandoned index | Child uses parent memo or rebuilds from blk/rev |
| Maturity | height + coinbase persisted | Compared on disconnect (`nHeight`, `fCoinBase`, `out`) | Preserved in undo CRC |

```
CONNECT_DISCONNECT_SYMMETRY = PASS          (UTXO / asset / amount / height / coinbase / script)
CONNECT_DISCONNECT_INDEX_MEMO_SYMMETRY = PARTIAL
```

P1: `DisconnectBlock` does not clear `m_draw_set` / `has_res` / paid-winner on
the abandoned index. Reconnect overwrites. `GetConvergenceDrawSetForBlock` can
rebuild from disk if the memo is unset. Do not invent a second memo eraser.

`RollforwardBlock` still skips ConnectBlock (inherited crash recovery). Asset
bytes come from the block. It is not a second Kernel. P2 residual P0-087-07.

No `CONNECT_DISCONNECT_ASYMMETRY` or `REORG_ASSET_DIVERGENCE` on the UTXO carrier
in the harness.

---

## H. Wallet SQLite (separate)

```
WALLET_SQLITE_AUDIT_STATUS = CLOSED   # closed by P0-089; still a separate trust boundary
wallet backend             = SQLITE_ONLY
BDB target                 = FALSE
filename                   = rigor-wallet.sqlite
schema                     = Bitcoin Core v25 KV, user_version = 0
migrations                 = none (unknown user_version → Verify fail)
```

| Topic | Finding |
|-------|---------|
| Atomic transaction | `TxnBegin` / `COMMIT` / `ROLLBACK` |
| WAL | **Not enabled.** SQLite default rollback journal (DELETE). Inherited Core. |
| Journal / fsync | `fullfsync=true`; `synchronous=OFF` only if `use_unsafe_sync` |
| Crash recovery | SQLite rollback journal + exclusive `locking_mode`. No RIGOR-specific WAL campaign. |
| Corruption | `PRAGMA integrity_check` in `Verify`. Unknown app_id / schema → refuse load. |
| Partial write | SQLite transaction; batch close aborts open txn |
| Concurrent access | Exclusive lock; second process fails open |
| Restart / rescan | Reload KV; chain rescan is observational vs CoinsDB |
| Keys / txs / addresses | Standard wallet records, not CoinsDB |
| ORU / RES / mint balances | `GetAssetBalance(asset)` filters `AssetTypeFromScriptPubKey`. Immature mint excluded for RES. |
| Asset in SQLite | **Not stored as a column.** Same script-as-asset as CoinsDB. |

Wallet cannot implement ORU→RES conversion. Display mistakes:

| Issue | Class |
|-------|-------|
| `GetAssetBalance(ORU)` vs `GetAssetBalance(RES)` | Correct split |
| `getreceivedbyaddress` non-RES domain → ORU scale (INVALID/NONE as ORU) | **WALLET_SECURITY / OBSERVATIONAL** (P0-086B-08) |
| `GetValueOut()` mixed sum | **OBSERVATIONAL** (not CheckTxInputs) |
| `GetBalance()` mixed atoms | tests/bench only; production RPC uses per-asset |
| RPC `gettxout` ORU decimals for RES | **OBSERVATIONAL** (P0-086B-06) |
| SQLite `DatabaseBatch::Read` deserialize catch → false (missing) | **WALLET_SECURITY P2** — not consensus; rescan can rebuild from chain |

Do not "fix" these by touching CoinsDB. Do not restore BDB.

OPEN because: no production SQLite crash/fuzz campaign in this TASK, and
observational display findings remain. Not a Consensus OPEN.

---

## I. Database vs Consensus Kernel

```
DATABASE_CONSENSUS_AUTHORITY_COUNT  = 0
DATABASE_VALIDATION_AUTHORITY_COUNT = 1
```

CoinsDB: **PERSIST / LOAD / RECOVER / REBUILD**.

The one validation *boundary* (not a Kernel):

| Site | What it decides | Allowed? |
|------|-----------------|----------|
| `EntersUtxoSet` in `AddCoin` / `AddCoins` / `DisconnectBlock` | UTXO membership (skip unspendable + converge) | Yes — membership only. Must stay symmetric. |
| `CoinIntegrityChecksum` / `CoinDiskFormatter` | Record integrity | Corruption abort, not amount/asset validity |
| `AmountCompression` MoneyRange | Deserialize bound | Fail-closed load, not Kernel amount policy |
| `ClassifyRigorTxout` | Asset after load | Consensus/script, **not** in `coins.cpp` |

`coins.cpp` / `txdb.cpp` / `dbwrapper.*` / `undo.h` do not call
`EvaluateConnect`, issuance, draw, or predicate verify.

PASS: database does not decide amount / asset / maturity / mint / draw
**validity**. Kernel / `CheckTxInputs` do that on well-read Coins.

---

## J. Mutation / differential / fuzz

Gate mutations: Coin serialization, classifier, maturity (height/coinbase),
mint marker, CRC, key layout, batch ordering (inherited), reorg rollback,
disconnect cleanup.

Differential (field tuples, not bool):

| Path | ORU | RES |
|------|-----|-----|
| CLEAN DB | match | match |
| CACHE DB | match | match |
| RESTART DB | match | match |
| REINDEX DB (harness serialize-rebuild) | match | match |
| REBUILT DB (production `-reindex`) | **BLOCKED** | **BLOCKED** |

Compared fields: asset, amount, height, coinbase, mint flag, UTXO membership,
script bytes, recovery tuples.

Random CRC / truncation: **ABORT**. Classifier remains deterministic.
Valid-CRC script mutation: **INHERITED_LOCAL_TRUST** (not `BOUNDARY_DIVERGENCE`
of two honest reconstructions).

Fuzz: `fuzz/database/run_smoke.sh` and `fuzz/dual_asset_database/run_smoke.sh`.
`FUZZ_STATUS=PASS` is smoke, not a campaign (same limit as P0-086-14).

---

## K. Fix / do-not-fix

No production code was changed. No P0 required a format change.

| ID | SEVERITY | LOCATION | REMOTE | CONSENSUS | FUNDS | INHERITED | RIGOR | MINIMAL_FIX | DO_NOT_FIX_REASON |
|----|----------|----------|--------|-----------|-------|-----------|-------|-------------|-------------------|
| P0-087-01 | coverage | production `-reindex` | n/a | n/a | n/a | n/a | verification gap | Run `rigord -reindex` and diff UTXO fields | Not a code bug; blocks CLOSED |
| P0-087-02 | P2 | Coin script + CRC | No | Local only | Local until reindex | Yes (UTXO trust) | Dual-asset makes reclassify visible | None | Format already represents assets. Tag would be a second authority. CRC is not a MAC on Bitcoin either. |
| P0-087-03 | P1 | `EntersUtxoSet` | No | Membership only | No | RIGOR membership copy | Convergence skip | Keep one helper, keep Disconnect symmetric | Do not build a second Kernel |
| P0-087-04 | P2 | `CCoinsViewDB::HaveCoin` Exists | No | Cache path deserializes | No | Yes | No | Optional deserialize | Inherited; ConnectBlock uses GetCoin |
| P0-087-05 | P2 | ReplayBlocks no ErrorCatcher | No | Throw, not TX_CONSENSUS | No | Yes | No | Wrap catcher | Inherited; still fail-closed |
| P0-087-06 | P2 | Cache `Cursor` skips `GetValue` false | No | RPC/snapshot | No | Partial | Deserialize now throws | Abort on cursor error | Not ConnectBlock |
| P0-087-07 | P2 | `RollforwardBlock` | No | Crash recovery | No | Yes | Scripts still carry asset | Do not merge with ConnectBlock this TASK | Mature Core recovery |
| P0-087-08 | P1 | Disconnect index memos | No | Memo hygiene | No | RIGOR memos | Rebuildable | Clear on disconnect | Reconnect overwrites; not CoinsDB |
| P0-087-09 | P1 | UpdateCoins before Kernel | No | Cache only | No | Order | Persist is fail-closed | Reorder later if desired | Do not rewrite ConnectBlock to close a flag |
| P0-087-10 | P1 | `GetValueOut()` | No | No | Display | Mixed sum | Dual-asset | Prefer `GetValueOut(asset)` | Not CoinsDB |
| P0-087-11 | P1 | RPC/Qt decimals | No | No | Display | — | Dual-asset | RPC presentation | Not CoinsDB |
| P0-087-12 | P2 | `GetBalance()` mix | No | No | tests/bench | Yes | Dual-asset | Leave or delete later | Production uses `GetAssetBalance` |
| P0-087-13 | P2 | `getreceivedbyaddress` | No | No | Display | — | Domain default ORU | Wallet RPC | Not CoinsDB |
| P0-087-14 | P2 | Wallet Read deserialize→false | No | No | Hide until rescan | Yes | No | Fail-closed wallet read | Separate boundary; not CoinsDB |
| P0-087-15 | info | SQLite journal DELETE | No | No | Inherited durability | Yes | No | Do not force WAL | Core default; exclusive lock + fullfsync |
| P0-087-16 | P2 | `ApplyBlockUndoCoinsOnly_SCRATCH` | No | Duplicate helper | No | Copy | Same skip | Do not grow a second undo authority | Do not delete in an architecture audit |
| P0-087-17 | P2 | CRC not MAC | No | Local tamper | Local | Yes | Scheme A | Do not add MAC to Coin | Would be a format/protocol change without P0 |
| P0-087-18 | coverage | fuzz smoke | n/a | n/a | n/a | — | — | Campaign later | Do not claim a campaign |
| P0-087-19 | P2 | observational non-res bucket | No | No | Explorer | — | Dual-asset | Observational RPC | Not CoinsDB |
| P0-087-20 | docs | P0-086B fail-closed flags | n/a | n/a | n/a | — | Stale vs Scheme A | This document supersedes random-corruption claims | Do not rewrite 086B in this TASK |

P0-086-01 / P0-086-02 (deserialize→missing, unchecked height/coinbase): **fixed
in production by Scheme A.** Not re-opened as P0.

---

## L. Acceptance

CLOSED requires all of:

| Gate | This audit |
|------|------------|
| DATABASE_CONSENSUS_BOUNDARY=CLOSED | **CLOSED** |
| DATABASE_FAIL_CLOSED=TRUE | **TRUE** |
| ORU_REBUILD_EQUIVALENCE=PASS | **BLOCKED** |
| RES_REBUILD_EQUIVALENCE=PASS | **BLOCKED** |
| ORU_CACHE_DB_EQUIVALENCE=PASS | **PASS** |
| RES_CACHE_DB_EQUIVALENCE=PASS | **PASS** |
| ORU_REORG_RECOVERY=PASS | **PASS** |
| RES_REORG_RECOVERY=PASS | **PASS** |
| CONNECT_DISCONNECT_SYMMETRY=PASS | **PASS** (UTXO) |
| FAILED_CONNECT_NO_PERSISTENCE=PASS | **PASS** |
| DATABASE_CONSENSUS_AUTHORITY_COUNT=0 | **0** |
| DATABASE_CONSENSUS_DIVERGENCE=0 | **0** |
| FUZZ_STATUS=PASS | **PASS** (smoke) |
| MUTATION_STATUS=PASS | **PASS** |
| CORRUPTION_STATUS=PASS | **PASS** (random fail-closed) |
| DATABASE_FORMAT_CHANGED=FALSE | **FALSE** |
| CONSENSUS_CODE_CHANGED=FALSE | **FALSE** |
| FORMAL_THEOREMS_CHANGED=FALSE | **FALSE** |
| WALLET_SQLITE_AUDIT_STATUS | **CLOSED** (P0-089; still not merged with CoinsDB) |

```
RIGOR_DATABASE_ARCHITECTURE_AUDIT = CLOSED
```

Closed by P0-091 using P0-088 production `-reindex` / `-reindex-chainstate`
field-level compare (`PRODUCTION_REBUILD_DIVERGENCE_COUNT=0`) and P0-089
wallet SQLite crash-recovery / fail-closed campaign. Wallet display is still
not mixed into CoinsDB PASS.

---

## M. Release Guardian

P0-087 did not add the stage. P0-091 did, after this audit became closable.

`tools/run-genesis-release-guardian.sh` still runs P0-086 as Stage 5b
(CRC fail-closed; that TASK's own `DATABASE_RELEASE_GATE=PARTIAL`). After
Stage 5b it now runs a **real** `Stage DATABASE_SECURITY`:

```
bash tools/run-database-security-guardian-stage.sh
```

which executes

```
bash tools/run-production-reindex-reconstruction-audit.sh
bash tools/run-wallet-sqlite-audit.sh
```

and requires `PRODUCTION_FULL_REINDEX=PASS` and
`WALLET_SQLITE_AUDIT_STATUS=CLOSED`. Document presence does not pass that
stage.

```
RELEASE_GUARDIAN_DATABASE_SECURITY_STAGE = ADDED
```

---

## Final principle (this TASK)

Bitcoin Core DB foundation  
+ Scheme A integrity trailer  
+ script-as-asset (one Coin)  
+ Kernel-only validity  
+ fail-closed random corruption  
+ honest production reindex (P0-088)  
+ wallet SQLite CLOSED (P0-089)  
+ Guardian Stage DATABASE_SECURITY (P0-091)  

No second CoinsDB. No ORU_DB/RES_DB. No BDB wallet. No format rewrite. No
forged CLOSED.

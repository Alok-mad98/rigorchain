# RIGOR Database Performance Baseline (P0-077)

**Task:** `TASK-RIGOR-MAINNET-PERFORMANCE-BASELINE-OPTIMIZATION-P0-077`  
**Date:** 2026-08-31  
**Nature:** Chainstate / UTXO storage performance baseline. Consistency rules
are not modified.

```
DATABASE_CONSISTENCY_RULES_CHANGED = NO
PRODUCTION_UTXO_MUTATION           = CCoinsViewCache (unchanged)
VERIFY_ONLY_LEDGER                 = not in Makefile.am
```

---

## Production layout

```
ConnectBlock
    |
    CCoinsViewCache          (hot UTXO; coins.h / coins.cpp)
    |
    Chainstate::FlushStateToDisk
    |
    CCoinsViewDB::BatchWrite (txdb.cpp → LevelDB chainstate/)
    |
    CDBWrapper::WriteBatch   (dbwrapper.cpp; optional fsync)
```

`LedgerApply` / `UTXOStateTransition` are verify-only composers. They do not
replace `CCoinsViewDB`. This TASK does not change batch ordering, crash
recovery (`DB_HEAD_BLOCKS` / `DB_BEST_BLOCK`), obfuscation, or fsync policy.

---

## What is measured

| Metric | Production locus | This baseline |
|---|---|---|
| Read latency | `CCoinsViewCache::GetCoin` → `FetchCoin` → parent / LevelDB | in-memory snapshot scan vs hash-map lookup analog in the UTXO bench |
| Write latency | `AddCoin` / `SpendCoin` into `cacheCoins` | `LedgerApply` created/spent delta construction |
| Flush latency | `CCoinsViewDB::BatchWrite` when `batch.SizeEstimate() > batch_write_bytes` | host 1 MiB write+flush analog in `run_mainnet_performance_benchmark.py` |
| Sync latency | `CDBWrapper::WriteBatch(..., fSync=true)` uses LevelDB `syncoptions.sync = true` | host `fsync` analog on 1 MiB |

Exact host analog numbers (this run): write 1 MiB ~0.62 ms, `fsync` 1 MiB ~30 ms,
100k in-memory lookups ~21 ms. They are **not** LevelDB IBD measurements.

---

## Defaults (unchanged)

| Knob | Value | File |
|---|---|---|
| `-dbcache` default | 450 MiB | `txdb.h` `nDefaultDbCache` |
| `-dbbatchsize` default | 16 MiB | `txdb.h` `nDefaultDbBatchSize` |
| Coin DB LevelDB cache cap | 8 MiB | `txdb.h` `nMaxCoinsDBCache` |
| Partial flush | when estimated batch > `batch_write_bytes` | `CCoinsViewDB::BatchWrite` |

Partial flushes are crash-recovery points. The existing `BCLog::COINDB`
counter of mid-batch flushes is diagnostic only (TASK-H2). This TASK does not
change that.

---

## Read path

1. `GetCoin` looks up `cacheCoins` (salted unordered map).
2. Miss fetches from the parent view (`FetchCoin`) and emplaces.
3. Spent coins in the parent can be marked `FRESH` in the child cache.

Hot-path cost is hash lookup + optional `Coin` copy. Allocation of a new
cache entry happens on miss. This TASK does not retune the hasher or the
FRESH/DIRTY flags.

---

## Write / flush / sync path

1. Dirty entries are serialized with `CoinEntry` keys.
2. Spent coins are erased; unspent coins are written.
3. Tip transition is two-phase: erase `DB_BEST_BLOCK`, write `DB_HEAD_BLOCKS`,
   then erase heads and write `DB_BEST_BLOCK` in the final batch.
4. Default `WriteBatch` is `fSync=false`. Full chainstate flush uses the
   existing `FlushStateMode::ALWAYS` path in `validation.cpp`.

Changing fsync frequency would change durability, not consensus formulas, but
it is still a consistency-adjacent policy. **Not changed.**

---

## Honesty

This note is a map of the production database path plus a host analog for
write/sync. It is not a claim that LevelDB was re-benchmarked under IBD, and
it is not a license to relax `DB_HEAD_BLOCKS` crash recovery.

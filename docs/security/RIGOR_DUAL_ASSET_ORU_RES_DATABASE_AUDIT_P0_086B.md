# RIGOR Dual-Asset ORU / RES Database & State Persistence Security Audit (P0-086B)

**Task:** `TASK-RIGOR-DUAL-ASSET-ORU-RES-DATABASE-AUDIT-P0-086B`  
**Date:** 2026-09-02  
**Nature:** Audit-first. Builds on P0-086. Persistence is a carrier of consensus
state. This document does **not** modify the Consensus Kernel, Transaction
Kernel, Ledger, genesis, issuance, RES mint rules, draw, convergence, ORU
rules, PoW, ASERT, consensus parameters, P2P consensus rules, wallet
consensus rules, or existing Coq theorems.

This is not a claim that a green harness implies
`DATABASE_FORMALLY_PROVED`.

```
DUAL_ASSET_DATABASE_AUDIT                  = BLOCKED
ORU_RES_STORAGE_MODEL                      = SHARED_COIN_MODEL
ORU_DATABASE_SEMANTICS                     = SCRIPT_AS_ASSET_COIN
RES_DATABASE_SEMANTICS                     = HYBRID_COIN_PLUS_MINT_SCRIPT_PLUS_INDEX_MEMO
ASSET_ID_AUTHORITY_COUNT                   = 1
ORU_DATABASE_AUTHORITY_COUNT               = 1
RES_DATABASE_AUTHORITY_COUNT               = 3
ORU_SUPPLY_EQUIVALENCE                     = TRUE
RES_SUPPLY_EQUIVALENCE                     = TRUE
ORU_CACHE_DB_EQUIVALENCE                   = PASS
RES_CACHE_DB_EQUIVALENCE                   = PASS
ORU_REORG_RECOVERY                         = PASS
RES_REORG_RECOVERY                         = PASS
ORU_REBUILD_EQUIVALENCE                    = PARTIAL
RES_REBUILD_EQUIVALENCE                    = PARTIAL
ORU_CORRUPTION_FAIL_CLOSED                 = FALSE
RES_CORRUPTION_FAIL_CLOSED                 = FALSE
ORU_RES_IMPLICIT_CONVERSION                = PRESENT_AT_OBSERVATIONAL_BOUNDARY
ORU_RES_CROSS_ACCOUNTING                   = OBSERVATIONAL
FUZZ_STATUS                                = PASS
MUTATION_STATUS                            = RECLASSIFIES
GOLDEN_VECTOR_STATUS                       = LOCKED
FORMAL_COVERAGE                            = NOT_COVERED
RELEASE_GUARDIAN_DUAL_ASSET_COVERAGE       = PARTIAL

CONSENSUS_CODE_CHANGED                     = FALSE
PROTOCOL_PARAMETERS_CHANGED                = FALSE
FORMAL_THEOREMS_CHANGED                    = FALSE

FINAL_VERDICT:
RIGOR_DUAL_ASSET_ORU_RES_DATABASE_AUDIT    = BLOCKED
```

Reproduce: `bash tools/run-dual-asset-database-security-audit.sh`

The audit stays **BLOCKED** because:

1. A corrupted Coin value is still treated as a missing coin (P0-086-01),
   for both ORU and RES.
2. Height / coinbase metadata is trusted with no checksum (P0-086-02).
   RES mint spend maturity is the coinbase flag, not the script field.
3. Because asset identity is inferred from `scriptPubKey`, mutating the
   stored script reclassifies ORU ↔ RES. That is silent identity change,
   not fail-closed.

Do not edit production consensus or CoinsDB code in this TASK to force PASS.

---

## Proof scope

### PASS (limited)

| Claim | Meaning |
|-------|---------|
| Shared Coin container | ORU and RES use one key schema (`0x43 \|\| txid \|\| VARINT(vout)`) and one value layout. No asset column, prefix, or second database. Lint + codec. |
| Canonical asset authority | `ClassifyRigorTxout` / `AssetTypeFromScriptPubKey` in `rigor_classify.cpp`. `ClassifyRigorAsset` is the same function. `IsResScript` calls the same classifier. |
| Consensus rejects conversion | `illegal-res-to-oru-conversion`, `mixed-asset-output`, `cross-asset-input`. Dual-pool RES spend with ORU fee is explicit, not a conversion. |
| Unknown / malformed / retired classify INVALID | Empty, P2PKH, v0/32, CVG2, RES2 → `ASSET_INVALID`. Not default ORU. |
| Harness reconstruction | Clean / cache-heavy / restart / flush+restart / reorg / serialize-rebuild yield one ORU digest, one RES digest, and one per-asset supply on the dual-asset golden chain. |
| Convergence is not a Coin | `AddCoins` skips `IsConvergenceScript`. Undo does not restore it. |

### NOT CLAIM

| Out of scope | Why |
|--------------|-----|
| Coq theorem of CoinsDB | `ORU_DATABASE_PROVED` / `RES_DATABASE_PROVED` = `NOT_COVERED`. |
| Production `-reindex` | Harness only → rebuild PARTIAL. |
| “Shared storage ⇒ identical semantics” | Forbidden. Same container, different script-as-asset rules. |
| LevelDB correctness | External infrastructure (P0-086). |

---

## 1. Core question — one storage path or two?

Traced in production, not guessed from directory layout:

```
ORU
    → CTxOut { nValue, scriptPubKey }     // no asset field
    → Coin { out, fCoinBase, nHeight }    // coins.h
    → CCoinsViewCache::AddCoin / SpendCoin
    → CCoinsViewDB::BatchWrite / GetCoin
    → chainstate/ LevelDB key 0x43 || txid || VARINT(vout)
```

```
RES (ordinary WITNESS_V1_RES)
    → same CTxOut / Coin / CoinsView / CoinsDB / Chainstate

RES mint (RIGOR_MARKER_RES_MINT)
    → same Coin path
    → script bytes carry mint lock + maturity_height
    → plus CBlockIndex memos (has_res, last_res_mint_block_hash, m_block_res_mint_sum)
    → plus memory-only RuntimeSupplyState

Draw / convergence
    → NOT a Coin
    → convergence skipped by AddCoins
    → draw set / paid winner on CBlockIndex
```

```
ORU_RES_STORAGE_MODEL = SHARED_COIN_MODEL
```

There is no ORU column, RES column, asset prefix, or second UTXO database.

---

## 2. Storage same ≠ semantics same

```
ORU_DATABASE_SEMANTICS  = SCRIPT_AS_ASSET_COIN
RES_DATABASE_SEMANTICS  = HYBRID_COIN_PLUS_MINT_SCRIPT_PLUS_INDEX_MEMO
RES persistence class   = C  (ordinary Coin + special mint script + index/supply memos)
```

| Axis | ORU | RES |
|------|-----|-----|
| Creation | Tx output / coinbase | Tx output / coinbase mint |
| Spending | `SpendCoin` erase | Same erase |
| Amount | `CAmount` + `CompressAmount` + `MoneyRange` | Same storage. Extra `IsValidResConsensusAmount` (`n >= 0`; `RES_ATOMIC_UNIT = 1`) |
| Asset identity | v0/20 or CONVERGE script | v1/32 native or `RIGOR_MARKER_RES_MINT` |
| Maturity | `fCoinBase` + `nHeight` vs `nCoinbaseMaturity` | Mint: script `maturity_height` at create; spend: same coinbase check. Wallet `IsTxImmatureResMint` is a second observational check |
| Ownership | script / witness | script / mint pkh / draw winner at mint |
| Issuance | ORU subsidy (kernel) | RES mint (kernel); only coinbase mint creates new RES |
| Burn | CONVERGE skip UTXO | RES has no converge burn |
| Supply | Observational ORU cache | Observational RES mint cache |
| Reorg / undo / restart / rebuild | Shared Coin + undo | Shared Coin + undo; mint memos rebuild from blocks |

---

## 3. ORU path

```
ORU creation → CTxOut → Coin → CoinsView → CoinsDB
→ SpendCoin / LedgerApply / CheckTxInputs_ORU
→ observational ORU supply
```

1. Amount: `CompressAmount` / `DecompressAmount`. No `MAX_MONEY` in the compressor.
2. Asset id: not stored. v0/20 after load.
3. Script: `ScriptCompression` generic path for native v0/20.
4. Height: VARINT code `height << 1`.
5. Coinbase: low bit of the same VARINT.
6. Spent: key erased, not a spent record.
7. Undo: `TxInUndoFormatter` (dummy version if height > 0).
8. Reorg: harness digest match (ORU bucket).
9. Restart: serialize all Coins, new cache, same ORU digest.
10. Rebuild: harness match; production `-reindex` not run.

No ORU-specific database interpreter. Authority after load is
`ClassifyRigorTxout`.

---

## 4. RES path — class C (hybrid)

**A. Ordinary Coin asset** — yes for `WITNESS_V1_RES`.  
**B. Special consensus state** — mint authorization is draw/winner (not in Coin).
Mint lock and `maturity_height` are *inside* the Coin’s script.  
**C. Mixed** — this is the production model.

`ConnectEvaluateCoinbaseResMint` identifies the mint by
`IsResMintScript`, not by “some RES output”. Fee / bootstrap RES on
mockable chains are witness v1, never `RIGOR_MARKER_RES_MINT`.

After mint, the output is a normal Coin. Restart keeps it. Disconnect
erases it and undo restores the spent prevouts. Replay of the same
coinbase overwrites (coinbase `possible_overwrite`). A second mint
script in one coinbase is `bad-rigor-res-amount`.

---

## 5. Keys and values

| | ORU | RES |
|---|-----|-----|
| Key | `0x43 \|\| txid32 \|\| VARINT(vout)` | identical schema |
| Prefix / column / DB | none | none |
| Value | VARINT(code) \|\| compressed amount \|\| compressed script | identical |
| Endian | little / Bitcoin VARINT | identical |
| Asset encoding | absent | absent |
| Optional fields | none | none |

Collision: only the same outpoint. One Coin per outpoint. ORU and RES
cannot occupy the same key unless they are the same output.

```
ORU value decoded as RES
RES value decoded as ORU
```

is **possible** if `scriptPubKey` bytes are mutated to the other
canonical shape. The decoder does not know which asset was intended.
That is P0-086B-01.

---

## 6. Asset identity

Canonical authority (count = 1):

```
ClassifyRigorTxout
    v0/20            → ASSET_ORU
    CONVERGE         → ASSET_ORU
    native v1/32     → ASSET_RES   (not CVG2, not RES2)
    RES_MINT         → ASSET_RES
    OP_RETURN        → ASSET_NONE
    else             → ASSET_INVALID
```

`ClassifyRigorAsset` is a one-line wrapper. `IsResScript` is
`WITNESS_V1_RES || RES_MINT` from the same classifier.

| Injection | Result |
|-----------|--------|
| Default asset in Coin | none (no field) |
| Zero / missing asset field | N/A |
| Unknown / empty / P2PKH / v0/32 | `ASSET_INVALID` |
| Retired RES2 / CVG2 | `ASSET_INVALID` |
| String `"ORU"` / `"RES"` | RPC parse only (`getassetbalance` rejects other strings) |
| Implicit Coin conversion | none in CoinsDB |

Observational replicas (not counted as consensus authorities):

1. Explorer `classifyScriptPubKey` (TypeScript).
2. Qt `AssetContext::fromAssetType` — **non-RES → ORU**.
3. `ValueFromAmountAsset` — **non-RES → ORU 8-decimal scale**.
4. This audit’s `classify_asset` harness.

---

## 7. Implicit conversion

Consensus / CoinsDB: **no** `ORU → RES` or `RES → ORU` conversion
function. CheckTxInputs rejects:

- `illegal-res-to-oru-conversion` (RES inputs, ORU output, no ORU in)
- `mixed-asset-output` (no RES in, RES out — body cannot mint)
- `cross-asset-input` on the ORU-only path

Dual-pool (RES in + ORU fee in) is an explicit rule, not a conversion.

Recorded as `DUAL_ASSET_IMPLICIT_CONVERSION` (do not fix in this TASK):

| Site | What happens |
|------|----------------|
| Script mutation (DB) | ORU record becomes RES after load (P0-086B-01) |
| `GetValueOut()` | Sums every output regardless of asset |
| `ValueFromAmountAsset` | `ASSET_INVALID` formatted as ORU |
| Qt `fromAssetType` | Anything not `ASSET_RES` → ORU context |
| `getreceivedbyaddress` | Non-RES domain → ORU tally |

```
ORU_RES_IMPLICIT_CONVERSION = PRESENT_AT_OBSERVATIONAL_BOUNDARY
```

---

## 8. Amount / money range

Both assets use `CAmount` (`int64_t`), `CompressAmount`, and
`MoneyRange` / `MAX_MONEY` (10e9 × `COIN`). Negative and overflow are
rejected in `AccumulateTxInputPools` via `AddMoneyOverflows`.

RES adds `IsValidResConsensusAmount` (`n >= 0 && n % 1 == 0`) and
`IsValidResOutputValue` (`n >= 1`). Display scale is `RES_COIN = 1e6`
vs `COIN = 1e8`. Storage is the same integer.

```
ORU amount + RES amount
```

is a defined int64 sum and is **not** a consensus monetary unit.
`CheckTxInputs` keeps separate pools. `GetValueOut()` without an asset
argument is the forbidden unfiltered sum (P0-086B-04). Ledger kernel
`AddMoney(in_oru, in_res)` is a formal conservation check, not CoinsDB.

---

## 9. Supply accounting

Spendable supply is the UTXO set, classified after load.

Observational (not consensus, not CoinsDB):

- `WarmOruSupplyCache` / `GetChainOruTotal`
- `WarmResMintCache` / `GetChainResMintTotal`
- `RuntimeSupplyState` on `CBlockIndex` — **memory only**, dropped on
  disconnect, rebuilt from index fields

Harness: after reorg / restart / serialize-rebuild,

```
ORU supply atoms  =  replayed ORU atoms
RES supply atoms  =  replayed RES atoms
```

on the golden chain (`ORU_SUPPLY_EQUIVALENCE` / `RES_SUPPLY_EQUIVALENCE`
= TRUE in the harness). `AggregateUtxoSupplyBalances` lumps non-RES
into one bucket (P0-086B-07). Production corruption of CoinsDB can
still diverge from replay until `-reindex`.

---

## 10. Issuance / RES mint persistence

```
ConnectEvaluateCoinbaseResMint
    → mint CTxOut (RIGOR_MARKER_RES_MINT)
    → AddCoins → Coin in cache / CoinsDB
```

| Event | Production |
|-------|------------|
| After mint | Coin present |
| Restart | Coin still present (script + amount + height + coinbase) |
| Reindex | Reminted by ConnectBlock from `blk*.dat` (not executed here) |
| Disconnect | Created mint Coin spent/erased; undo restores inputs |
| Reorg | Same; no second mint Coin unless the new branch mints |
| Same-block replay | Coinbase overwrite |

Harness does not double-create a mint Coin on reorg-to-clean-alt
equivalence. `RES_STATE_RECONSTRUCTION = UNIQUE` on the harness chain.

---

## 11. Maturity

```
nCoinbaseMaturity = 100 = RES_MINT_MATURITY = COINBASE_MATURITY
```

| Spend height − coin height | Mature? |
|----------------------------|---------|
| 99 | no |
| 100 | no (`< 100`) |
| 101 | yes |

Mint script stores `maturity_height = nBlockHeight + nCoinbaseMaturity`.
That field is checked at **mint**. Spend uses `coin.IsCoinBase()` only.
`Coin::RequiresMaturity()` has no callers. Wallet
`IsTxImmatureResMint` uses chain depth independently.

A DB that reports mature while the kernel would call the coinbase
immature happens if `nHeight` / `fCoinBase` are mutated (P0-086B-03).

---

## 12. Draw / convergence

| State | Persisted? | Where | Undo | Rebuild |
|-------|------------|-------|------|---------|
| Draw set / paid winner | Yes (index memo) | `CBlockIndex` / `CDiskBlockIndex` | Invalidated / rewritten on connect | From blocks; paid winner re-checked |
| Convergence output | No | Never a UTXO | N/A | N/A |
| RES ownership | Script / mint pkh | Coin script | Undo restores script | Replay |
| RES maturity | Height + flag (+ script at mint) | Coin | Undo restores metadata | Replay |

Connect / disconnect / reorg / restart / reindex / replay of **Coin**
state is the shared P0-086 path. Draw memos are not Coin records.

---

## 13. Reorg dual-asset harness

```
Genesis → A → B → C
Genesis → A → D → E
```

A includes ORU coinbase, RES mint, ORU spend, RES output, convergence
skip. B/C spend ORU and RES and include a dual-pool shaped transaction.
D/E are the alternate branch.

```
normal alt apply  ≡  reorg from C back through A then D,E
                  ≡  serialize-rebuild of alt
```

```
ORU_STATE_EQUIVALENCE   = TRUE   (harness)
RES_STATE_EQUIVALENCE   = TRUE
SUPPLY_EQUIVALENCE      = TRUE
```

---

## 14. Corruption

| Injection | Result |
|-----------|--------|
| Truncated / empty ORU or RES value | `Read` false → missing → `TX_CONSENSUS` |
| Flip ORU v0/20 → v1/32 | Decodes as RES (`SILENT_RECLASSIFY`) |
| Flip RES v1/32 → v0/20 | Decodes as ORU (`SILENT_RECLASSIFY`) |
| Flip mint script → v1/32 | Still RES; mint lock / script maturity gone |
| Flip height / coinbase | Trusted; maturity can pass or fail wrongly |
| Oversized script | `RIGOR_MARKER_RETURN` (`ASSET_NONE`) |

```
corruption → silently interpreted as another asset
```

is exactly P0-086B-01. Fail-closed is **FALSE** for both assets.

---

## 15. Cache / DB

Same DIRTY / FRESH overlay for both assets. Harness paths A–D produce
one ORU digest and one RES digest.

```
ORU_CACHE_DB_EQUIVALENCE = PASS
RES_CACHE_DB_EQUIVALENCE = PASS
```

Stale cache after a failed `Flush` remains a node-abort case (P0-086).

---

## 16. RPC / wallet / explorer

| API | Asset handling |
|-----|----------------|
| `getbalance` | ORU only (documented) |
| `getbalances` | Separate ORU / RES via `GetAssetBalance` |
| `listunspent` | `AssetTypeFromScriptPubKey` + `AssetTypeToString` |
| `gettxout` | `res_mint` bool; **value always ORU-scale**; no `asset` field |
| `getassetbalance` | String `"ORU"`/`"RES"` required; RES still `ValueFromAmount` |
| `getreceivedbyaddress` | RES address → RES tally; else ORU |
| Explorer | Independent TS classifier (tests exist) |

`getbalance` is an ORU-only assumption by contract, not a silent default
of unknown → ORU. Qt / `ValueFromAmountAsset` / `gettxout` are the
lossy defaults (P0-086B-05, P0-086B-06).

```
EXPLORER_DUAL_ASSET_SEMANTIC_DIVERGENCE
```

is recorded as a **risk class** (replica + RPC display), not as a
measured live explorer mismatch.

---

## 17. Golden vectors

`formal_verification/dual_asset_database_vectors/` locks:

1. ORU-only coin  
2. RES-only coin  
3. ORU spend (erased)  
4. RES spend (erased)  
5. ORU + RES same block  
6. ORU + RES same transaction (dual-pool shape)  
7. RES mint  
8. RES maturity boundaries  
9–10. Reorg ORU / RES  
11–12. Restart ORU / RES  
13–14. Reindex harness ORU / RES (production reindex not run)  
15–16. Corrupted ORU / RES  
17. Unknown asset → INVALID  
18. Malformed / retired → INVALID  
19. ORU / RES script mutation  

They lock serialization, reconstruction, and the failure boundary.
They are **not** a formal proof.

---

## 18. Fuzz

`fuzz/dual_asset_database/` randomly feeds Coin / undo / key / mutate
records. Smoke: `fuzz/dual_asset_database/run_smoke.sh`.

```
FUZZ_STATUS            = PASS
CRASH_FOUND            = NO
UNDEFINED_BEHAVIOR     = NO
MUTATION_STATUS        = RECLASSIFIES
CONSENSUS_STATE_DIVERGENCE (harness) = 0
ASSET_TYPE_DIVERGENCE (clean data)   = 0
ASSET_TYPE_DIVERGENCE (mutated script) = expected reclassify
```

Not a long-running campaign.

---

## 19. Authority counts

```
ASSET_ID_AUTHORITY_COUNT           = 1
  ClassifyRigorTxout / AssetTypeFromScriptPubKey
  (ClassifyRigorAsset and IsResScript are the same classifier)

ORU_DATABASE_AUTHORITY_COUNT       = 1
  CoinsDB Coin records

RES_DATABASE_AUTHORITY_COUNT       = 3
  1. CoinsDB Coin (shared)
  2. CBlockIndex mint / draw memos
  3. RuntimeSupplyState (memory, observational)

ORU_SUPPLY_AUTHORITY_COUNT         = 1   observational WarmOruSupplyCache
RES_SUPPLY_AUTHORITY_COUNT         = 1   observational WarmResMintCache
```

`RES_DATABASE_AUTHORITY_COUNT > 1` because RES mint is also reflected
in index memos and the observational supply mirror. Those are not
second accept/reject kernels. ConnectBlock does not read the mirror.

Wallet and RPC call the C++ classifier. Explorer is a TypeScript
replica (P0-086B-09). Qt / display helpers are lossy (P0-086B-05).

---

## 20. Formal verification boundary

| Layer | Status |
|-------|--------|
| `ORU_DATABASE_PROVED` | `NOT_COVERED` |
| `ORU_DATABASE_DIFFERENTIAL_TESTED` | harness |
| `ORU_DATABASE_FUZZED` | smoke |
| `ORU_DATABASE_VECTOR_LOCKED` | yes |
| `RES_DATABASE_PROVED` | `NOT_COVERED` |
| `RES_DATABASE_DIFFERENTIAL_TESTED` | harness |
| `RES_DATABASE_FUZZED` | smoke |
| `RES_DATABASE_VECTOR_LOCKED` | yes |

```
FORMAL_COVERAGE          = NOT_COVERED
DATABASE_FORMALLY_PROVED = FALSE
```

No Coq theorem was added or claimed.

---

## 21. Artifacts

| Path | Role |
|------|------|
| `docs/security/RIGOR_DUAL_ASSET_ORU_RES_DATABASE_AUDIT_P0_086B.md` | This report |
| `formal_verification/dual_asset_database_audit_manifest.json` | Machine verdict |
| `formal_verification/dual_asset_database_vectors/` | Golden catalog + shards |
| `tools/dual_asset_database_codec.py` | Harness |
| `tools/lint-dual-asset-database-boundary.py` | Authority lint |
| `tools/run-dual-asset-database-security-audit.sh` | Gate |
| `core/src/test/test_dual_asset_database_security.cpp` | Standalone C++ |
| `tests/dual_asset_database/` | Python tests |
| `fuzz/dual_asset_database/` | Fuzz + smoke |

Genesis Release Guardian was inspected only. A blocked dual-asset
audit must not be registered as a passing Guardian stage.

```
RELEASE_GUARDIAN_DUAL_ASSET_COVERAGE = PARTIAL
```

---

## 22. Findings

### P0

| ID | Impact |
|----|--------|
| P0-086B-01 | Corrupt script → other asset. No checksum. |
| P0-086B-02 | Decode failure → missing → `TX_CONSENSUS` (both assets). |
| P0-086B-03 | Trusted height/coinbase; mint script maturity not used on spend. |

### P1

| ID | Impact |
|----|--------|
| P0-086B-04 | Unfiltered `GetValueOut()` cross-asset sum. |
| P0-086B-05 | Display / Qt default non-RES → ORU. |
| P0-086B-06 | `gettxout` / `getassetbalance` ORU-scale for RES. |

### P2

| ID | Impact |
|----|--------|
| P0-086B-07 | Observational UTXO scan `non-res` bucket. |
| P0-086B-08 | `getreceivedbyaddress` non-RES → ORU. |
| P0-086B-09 | Explorer replica classifier. |

Inherited P0-086-03…08 (oversized script rewrite, ignored decompress,
`AddCoins` membership, undo height 0, cursor skip, BIP30 residue)
apply to the shared container and are not restated as new IDs.

### COVERAGE_GAPS

- Production `-reindex` / delete-`chainstate` not executed.
- No Coq model of dual-asset CoinsDB.
- Fuzz is smoke-only.
- Explorer vs C++ classifier not wired into this gate as a triple-diff.
- `gettxout` asset field absent (display gap, not kernel).

---

## Audit principle

The goal was not to prove that ORU and RES are the same in the
database.

```
ORU_DB_STATE  ≠  RES_DB_STATE
    unless the stored script canonically classifies as that asset

DB_REBUILD(ORU)  ≡  CONSENSUS_REPLAY(ORU)    (harness; production PARTIAL)
DB_REBUILD(RES)  ≡  CONSENSUS_REPLAY(RES)    (harness; production PARTIAL)

CORRUPTED_ORU  →  FAIL_CLOSED     FALSE  (missing or reclassify)
CORRUPTED_RES  →  FAIL_CLOSED     FALSE

ORU ↛ RES     consensus yes; DB script mutation no
RES ↛ ORU     consensus yes; DB script mutation no
```

Shared infrastructure is allowed. Silent reclassification and mapping
database failure onto consensus-invalid are not.

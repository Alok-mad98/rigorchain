# Inventory: ORU coinbase maturity vs RES mint spendability


> **Shipping issuance (RFC-020).** Continuous `destroy_cost(h)`, pole predicate
> `R · (2²⁵+h)² ≤ B << 40`, cap `2²²` RES, flat 128 ORU. Numeric SSOT:
> `tools/res_continuous_price_schedule.py`. Formal status:
> `audit/formal_bridge/FORMAL_STATUS_SOURCE_OF_TRUTH_v1.md`
> (`REFINEMENT_PROVED=false`).


**Date:** 2026-08-18  
**Tree:** this repository
**Scope:** Solo mining. Consensus spendability of created UTXOs — not issuance rate.

**RES origin (normative):** RES is created only as a **non-coinbase body mint** when the same transaction destroys ORU (`OP_CONVERGE`, amount `B`) and MAY create Witness v1 RES summing to `R` under `CheckResMintArithmetic`. It is **not** mined, **not** 1:1 ORU exchange, and **not** a coinbase output.

---

## ORU coinbase maturity

| Item | Location |
|------|----------|
| Constant | `core/src/consensus/consensus.h:21` `COINBASE_MATURITY = 100` |
| Per-network runtime | `core/src/consensus/params.h:165` `nCoinbaseMaturity{100}` |
| Networks | `core/src/kernel/chainparams.cpp` — main/test/devtest/regtest **100**; slowtest **1** (`:684`) |
| Consensus spend check | `core/src/consensus/tx_verify.cpp:203-212` `AccumulateTxInputPools`: `coin.RequiresMaturity()` (coinbase **or** RES mint) and `nSpendHeight - coin.nHeight < nCoinbaseMaturity` → `TX_PREMATURE_SPEND` / `bad-txns-premature-spend-of-coinbase` |
| Mempool accept | Same `CheckTxInputs` (`tx_verify.cpp:387`) used from `AcceptToMemoryPool` |
| Mempool reorg | `core/src/validation.cpp:486-497` `GetSpendsCoinbase()` (set when `RequiresMaturity()`, `validation.cpp:946-954`) + same depth predicate |
| UTXO flag | `Coin::fCoinBase` set in `AddCoins` (`core/src/coins.cpp:148-162`) from `tx.IsCoinBase()` — **not** reused for RES mints (`DisconnectBlock` asserts `tx.IsCoinBase() == coin.fCoinBase`) |
| Wallet (non-consensus) | `CWallet::GetTxBlocksToMaturity` / `IsTxImmatureCoinBase` `core/src/wallet/wallet.cpp:3726-3744` |

**Depth (consensus):** a coin created at height `H` may first be spent in a transaction whose spend height is `H + nCoinbaseMaturity` (mempool uses `tip+1`). Wallet display uses `(nCoinbaseMaturity + 1) - confirmations` and is **not** the consensus predicate.

---

## RES mint creation

| Item | Location |
|------|----------|
| Mint predicate | `CheckResMintArithmetic(R, B, h)` in `CheckTxInputs` (`core/src/consensus/tx_verify.cpp`); header `res_issuance.h` |
| Display destroy cost | `ResDestroyCostAtHeight(h)` — not a consensus input |
| Witness v1 output | `IsResScript` (`core/src/script/standard.cpp`) — non-CVG2 witness v1 / 32-byte program |
| Amount | Chosen `R`; legal iff `R > 0 → R · (2²⁵+h)² ≤ B << 40`. Cap `419430400000000` atoms (`2²²` RES) |
| Shape (mempool + block) | Body-tx `OP_CONVERGE` destroy MAY mint Witness v1 RES |
| Cap | `RES_MAX_SUPPLY` / `ResDerivedMaxSupply` |
| UTXO insert | `UpdateCoins` → `AddCoins`. Mint tx is **not** coinbase ⇒ `fCoinBase == false`. Witness v1 payout is marked `COIN_FLAG_RES_MINT` via `ApplyBodyResMintFlag`. |

A plain burn that mints nothing stays legal. Coinbase cannot create RES.

---

## RES **spend** path (post-fix)

**Pre-fix gap:** a body RES mint is a non-coinbase tx, so `fCoinBase` was false and `CheckTxInputs` allowed a spend at depth 1. Wallet `IsTxImmatureResMint` was (and remains) **non-consensus**.

**Fix (consensus impact: yes):** mark the mint payout with `COIN_FLAG_RES_MINT`; apply the **same** `nSpendHeight - nHeight < nCoinbaseMaturity` predicate as ORU coinbase. Do **not** set `fCoinBase` on mint coins.

| Item | Location | Maturity now |
|------|----------|----------------|
| Marker | `COIN_FLAG_RES_MINT` `coins.h:25`; `Coin::IsResMint` / `RequiresMaturity` `coins.h:83-88` | Consensus-visible bit 0 of `nFlags` |
| Shape used to set flag | `TxOutputIsBodyResMint` `coins.cpp:125` — non-coinbase, parseable PCORB, exactly one RES output of `RES_REWARD_AMOUNT` | Only the mint payout, not transfer/change |
| Input accumulation | `AccumulateTxInputPools` `tx_verify.cpp:203-212` | `RequiresMaturity()` → `bad-txns-premature-spend-of-res-mint` (`TX_PREMATURE_SPEND`) |
| RES dual-pool | `CheckTxInputs_RES_DualPool` `tx_verify.cpp:278-315` | Unchanged: `res_in ≥ res_out`; ORU fee required. Maturity does not mint. |
| Domain / mint vs transfer | `CheckTxInputs` `tx_verify.cpp:387` | Transfers cannot mint; ORU→RES only via mint shape. |
| Mempool overlay | `CCoinsViewMemPool::GetCoin` / `PackageAddTransaction` `txmempool.cpp:948-978` | `ApplyBodyResMintFlag`; `MEMPOOL_HEIGHT` cast to `int` stays immature |
| Undo | `TxInUndoFormatter` `undo.h:24-54` | Dummy version byte bit 0 only (no variable-length nFlags tail) |
| Disconnect | `validation.cpp:2113`, `:2180` | `TxOutputIsBodyResMint(tx, o) == coin.IsResMint()` |
| RPC | `gettxout` / scantxoutset `rpc/blockchain.cpp:1187`, `:3491` | `res_mint` bool (observability, not the rule) |

---

## Wallet immature UI — **non-consensus**

| Item | Location |
|------|----------|
| Body mint immature until `nCoinbaseMaturity` | `GetTxBlocksToResMintMaturity` `wallet.cpp:3746-3770` |
| Predicate | `IsTxImmatureResMint` `wallet.cpp:3772-3776` (semantic `ResMint` only; transfers/change not locked) |
| Coin selection | `wallet/spend.cpp:217-222`, `:362` |
| Credit / `res.immature` | `wallet/receive.cpp:168-170`, `:256-267`; `getbalances().res.immature` `wallet/rpc/coins.cpp` |
| Overview copy | `qt/overviewpage.cpp` (~line 34) |

`getbalances().res.trusted` remaining 0 until ~100 confirms is **wallet accounting**, not a consensus rule.

---

## Dead paths — coinbase-derived RES issuance

| Item | Location | Behavior |
|------|----------|----------|
| `ComputeResReward` | `core/src/consensus/convergenceres.cpp:110-118` | Always **0** |
| `GetOruOnlyCoinbaseTxId` / `ResMintWitnessProgram` / `ResMintScriptPubKey` / `IsValidResMintOutput` | `convergenceres.cpp:120-148` | **abort** (`std::runtime_error`) |
| Coinbase RES layout | `connect_block_rigor_checks.cpp:80-85` | `bad-cb-res-amount` unless mockable `-reswalletbootstrap` (test-only extra coinbase RES; still `fCoinBase`, so ORU-style coinbase maturity applies to that backdoor UTXO) |

No second issuance path on production networks.

---

## Functional tests

- Window / solo suites still assert mint via payout block / `mint_count` / `res_balance`, not wallet `res.trusted` alone.
- Maturity attack suite: `core/test/functional/feature_res_maturity.py` (T-RES-1…6, T-ORU-1, T-ISSUE-1). Unit: `res_orum_tests` + `coins_tests/txundo_formatter_res_mint_flag_roundtrip`.

---

## Phase 2 properties

| ID | Status after fix |
|----|------------------|
| MAT-ORU-1 | **Held** (`CheckTxInputs` + mempool reorg). T-ORU-1 PASS. |
| MAT-RES-1 | **Held** — mint input rejected while `nSpendHeight - nHeight < nCoinbaseMaturity`. T-RES-1/2 PASS. |
| MAT-RES-2 | **Held** — identical predicate in mempool (`CheckTxInputs`) and `ConnectBlock` / `generateblock`. T-RES-4 PASS. |
| MAT-RES-3 | **Held** — undo restores mint flag; disconnected mint is missing-inputs; restore keeps immature. T-RES-5 PASS. |
| MAT-RES-4 | **Held** — mature spend is a transfer (`res_in ≥ res_out`); `mint_count` stays 1. T-RES-3 + T-ISSUE-1. |
| MAT-RES-5 | **Held** — `state.Invalid` / `TX_PREMATURE_SPEND` / `bad-txns-premature-spend-of-res-mint` (not AbortNode). |

**Consensus impact: yes** — see `docs/security/RES-ORU-MATURITY.md`.

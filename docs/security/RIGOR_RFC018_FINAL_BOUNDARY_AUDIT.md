# RFC-018 Fee Burn — Final Boundary Audit

**Task:** `TASK-RIGOR-RFC018-FINAL-BOUNDARY-AUDIT-P0-001`  
**Date:** 2026-08-20  
**Scope:** Locate every RFC-018 fee-burn reference, classify it by layer, and verify the shipping boundary.  
**Code changes in this task:** **NONE** (audit report only).  
**Push / merge / tag:** **NONE**

Honesty:

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

This document does **not** claim a Coq/Python refinement of C++ `ConnectBlock`. Share-function pins and explorer statistics are observational. They are not a correspondence proof of the ConnectBlock loop, undo, or chainstate.

Related: [RFC-018](../rfc/RFC-018-RES-TRANSFER-ORU-FEE-BURN.md), [FEE_BURN_AUDIT.md](../formal/rfc018_fee_burn/FEE_BURN_AUDIT.md), [RIGOR_RFC018_FEE_BURN_STATISTICS_DESIGN.md](./RIGOR_RFC018_FEE_BURN_STATISTICS_DESIGN.md) (Option C), [RIGOR_RFC018_EXPLORER_FEE_BURN_IMPLEMENTATION_REPORT.md](./RIGOR_RFC018_EXPLORER_FEE_BURN_IMPLEMENTATION_REPORT.md).

---

## 1. Verdict

| Check | Result |
|-------|--------|
| Fee burn applies only to RES-transfer **ORU** fees | **YES** (`IsORBClaimTx` / ASSET_RES input domain) |
| Burn share | **1/2 floor** (`NUM=1`, `DEN=2`) |
| Destroyed ORU enters a UTXO / OP_RETURN | **NO** (lowered coinbase cap; never an output) |
| `GetBlockSubsidy` / RFC-017 RES mint / RES cap altered by fee burn | **NO** |
| RFC-002 `m_chain_oru_total` subtracts fee burn | **NO** (`S' = S + subsidy − converge`) |
| `CheckTransaction` / `CheckTxInputs` fee identity altered | **NO** (full `ORU_in − ORU_out`) |
| Wallet balance / send / fee estimation subtracts burn | **NO** (zero wallet hits) |
| Cumulative burn on `CBlockIndex` / chainstate | **NO** (`not_persisted_on_block_index`) |
| Explorer cumulative (`rfc018_fee_burn_stats`) | **YES** — Option C, presentation only |
| C++ ConnectBlock refinement claimed | **false** |

Shipping consensus RFC-018 rule (already in tree): `ConnectBlock` / `CreateNewBlock` withhold `OruResTransferFeeBurnShare` from the **claimable** ORU cap. Explorer Option C added indexer statistics only. That split is the intended boundary.

---

## 2. Search inventory

Project-wide search terms: `RFC-018`, `fee burn` / `fee_burn` / `FeeBurn`, `destroyed_fee_oru`, `nFeesOruResTransferBurn`, `burned_oru`, `cumulative_burned_oru`, plus `OruResTransferFeeBurnShare` / `IsORBClaimTx`.

### 2.1 `nFeesOruResTransferBurn` (C++ runtime accumulator)

| File | Role |
|------|------|
| `core/src/validation.cpp` | `ConnectBlock` stack-local; classify before `UpdateCoins`; cap `bad-cb-amount` |
| `core/src/node/miner.cpp` | `CreateNewBlock` / `AddToBlock`; set coinbase `vout[0]` |
| `core/src/node/miner.h` | `BlockAssembler` member (template assembly only) |

**Not** on `CBlockIndex`, **not** in undo, **not** in `DisconnectBlock`, **not** serialized.

### 2.2 `OruResTransferFeeBurnShare` / NUM/DEN

| File | Role |
|------|------|
| `core/src/consensus/rigor_consensus_lock.h` | Locked `NUM=1`, `DEN=2`; `constexpr` floor share |
| `core/src/test/rigor_fee_tests.cpp` | `rfc018_res_transfer_fee_burn_share` pins |
| Formal Python/Coq/explorer TS | Same 1/2 floor (parity / presentation; not refinement) |

### 2.3 `destroyed_fee_oru`

| File | Role |
|------|------|
| `explorer/src/rfc018FeeBurn.ts` | Comment + `rfc018DestroyedFeeOru` |
| `docs/security/RIGOR_RFC018_EXPLORER_FEE_BURN_IMPLEMENTATION_REPORT.md` | Indexer formula |
| `formal_verification/python-spec/rfc018_fee_burn.py` | Comment `delta_oru = coinbase_subsidy - destroyed_fee` |
| `formal_verification/coq/fee-burn/BlockSupply.v` | Model circulating Δ |

**Absent from C++.** Consensus uses the stack name `nFeesOruResTransferBurn`.

### 2.4 `burned_oru` / `cumulative_burned_oru`

**Explorer/indexer SQL and API only:**

- `explorer/migrations/005_rfc018_fee_burn_stats.sql`
- `explorer/src/rfc018FeeBurnStore.ts`, `rfc018FeeBurn.ts`, `rfc018FeeBurn.test.ts`
- `explorer/src/db.ts`, `api/status.ts`, `registerRoutes.ts`, `uiSsr.ts`, `uiSsr.test.ts`
- `explorer/public/status.js`, `explorer/public/oru.html`

**Absent from** `core/src/validation.cpp`, `chain.h`, wallet, miner persistence.

### 2.5 RPC label (not an accumulator)

`core/src/rpc/blockchain.cpp` and `core/src/consensus/runtime_supply_export.cpp`:

```
supply_semantics.rfc018_fee_burn_cumulative = "not_persisted_on_block_index"
oru_converge_burn_block = this_block_convergence_destroyed_oru_not_rfc018_fee_burn
```

Read-only naming. No new chainstate field.

### 2.6 Wallet / Qt / policy

**Zero matches** for `OruResTransferFeeBurn`, `nFeesOruResTransferBurn`, `rfc018`, `destroyed_fee_oru`, `burned_oru` under:

- `core/src/wallet/`
- `core/src/qt/`
- `core/src/policy/`

---

## 3. Classification by layer

### A. Consensus layer

| Site | Fee-burn role | Boundary |
|------|---------------|----------|
| `rigor_consensus_lock.h` `OruResTransferFeeBurnShare` | Floor 1/2 of a non-negative ORU fee | Parameter lock only |
| `ConnectBlock` (`validation.cpp`) | If `IsORBClaimTx` before `UpdateCoins`, add share to `nFeesOruResTransferBurn`. Reject `oru_coinbase_value > (nFeesOru − burn) + GetBlockSubsidy` (`bad-cb-amount`) | **Sole consensus enforcement** |
| `CheckTxInputs` (`tx_verify.cpp`) | Returns full `txfee = ORU_in − ORU_out`. Dual-pool RES path does **not** half the fee | Unchanged fee identity |
| `CheckTransaction` (`tx_check.cpp`) | Script/shape only; **no** fee-burn term | Unchanged |
| Coinbase validation | Still `bad-cb-amount`. Subsidy term is `GetBlockSubsidy`. Burn is subtracted from **fees**, not from subsidy | Cap reduced; subsidy formula not rewritten |
| `IsORBClaimTx` (`tx_verify.cpp`) | `GetTransactionInputAsset == ASSET_RES` | Eligibility; mint/ORU/coinbase excluded |
| Chainstate / `CBlockIndex` | `m_chain_oru_total = oru_supply_before + subsidy − block_converge_sum` (**no** burn). `m_block_converge_sum` is Convergence only. No fee-burn field in `chain.h` serialize | RFC-002 ledger unchanged |
| `DisconnectBlock` | Restores coins from undo; sets `m_chain_oru_total = -1`. **No** burn undo (nothing was a coin) | Correct; no extra consensus undo |

`CheckTxInputs` is called **by** ConnectBlock to obtain `txfee`. Burn is applied **after** that return, on the ConnectBlock stack. Eligibility uses live prevouts (`IsORBClaimTx`) before `UpdateCoins`.

RES mint / cap / winner-must-mint (`bad-rigor-res-*`) sit in the same `ConnectBlock` function **after** the fee loop and **do not** read `nFeesOruResTransferBurn`.

### B. Wallet layer

| Concern | Finding |
|---------|---------|
| Balance | `GetBalance` / UTXO; no burn subtraction |
| Send / tx creation | Full ORU fee still paid (`ORU_in − ORU_out`). RFC-018 §3: wallets pay the full fee; only miner claimable is reduced |
| Fee estimation | `wallet/fees.h`, `rpcspend.cpp` — no RFC-018 symbols |
| RES send | Still cannot subtract the ORU network fee from RES outputs |

Wallet spendability is the UTXO set. Burned ORU was never an output, so it never appeared in a wallet balance.

### C. Mining layer

| Site | Role |
|------|------|
| `BlockAssembler::nFeesOruResTransferBurn` | Template-local, reset each `CreateNewBlock` |
| `AddToBlock` | Same `IsORBClaimTx` + share as ConnectBlock |
| Coinbase payout | `vout[0].nValue = (nFeesOru − nFeesOruResTransferBurn) + GetBlockSubsidy` |
| RES mint / body payout | Independent; RFC-017 `W(h)` |

This is **enforcement of the claimable cap**, not a second burn rule. Overclaim remains consensus-invalid; underclaim remains legal (voluntary extra sink, **not** labeled RFC-018 burn).

Draw weight `f_i` for Convergence still uses the **full** ORU fee (`RFC-018` §7 non-goal). Burn does not retarget `T` or `f_i`.

### D. Explorer / indexer layer (Option C)

| Site | Role |
|------|------|
| `rfc018FeeBurn.ts` | `destroyed_fee_oru = floor(ORU_leg_fee / 2)` if RES input, not coinbase, not mint |
| `rfc018FeeBurnStore.ts` + migration `005` | Persist `burned_oru`, `cumulative_burned_oru` |
| `importer.ts` | Compute at pass 4; backfill; reorg via `DELETE FROM blocks` **CASCADE** |
| `/api/status` `rfc018_fee_burn` + `supply_chart` | Four series never merged |
| `/api/block/:hash`, `/api/blocks`, `/api/chain/tip` | Optional nested stats |
| UI (`status.js`, `oru.html`, `uiSsr.ts`) | “Convergence destroyed” vs “RFC-018 fee burned” vs circulating |

Explorer `fee.ts` still shows **N/A** for RES-involving tx display fees. RFC-018 stats are a **separate** path using the ORU leg. Indexer numbers can lag and can undercount missing prevouts. They are **not** fed back into consensus, mining, or wallet.

---

## 4. Property verification

### 4.1 Only ORU transfer fees on RES transfers

```
eligible  ⇔  IsORBClaimTx  ⇔  input domain ASSET_RES
burn_tx   =  floor(txfee_ORU · 1/2)   if eligible
          =  0                         otherwise
```

Excluded: pure ORU, Convergence, ORU-funded mint (`IsORBMintTx` / ORU domain), coinbase.

`CheckTxInputs_RES_DualPool` still forbids RES-denominated fees (`nFeesRes == 0` asserted in ConnectBlock). There is no RES-denominated burn.

### 4.2 Burn share = 1/2

```
ORU_RES_TRANSFER_FEE_BURN_NUM = 1
ORU_RES_TRANSFER_FEE_BURN_DEN = 2
share(fee) = (fee * 1) / 2     // integer floor; fee ≤ 0 → 0
```

Pins: `static_assert` in `rigor_consensus_lock.h`; `rigor_fee_tests.cpp`; Coq `Accounting.v`; Python spec; explorer `rfc018FeeBurn.test.ts`. `1 ORU = 1e8` atoms, `1 RES = 1e6` atoms. No 100-atom scale.

### 4.3 Destroyed ORU does not enter the UTXO set

RFC-018 does not write a burn output. The unclaimed share is simply never assigned to coinbase `vout[0]`. `UpdateCoins` therefore never creates a coin for it. Circulating ORU is the remaining UTXO set (CVG2 already excluded from spendable index). Coq model theorem `fee_burn_cannot_enter_circulating_supply` is a **model** statement, not a ConnectBlock refinement.

### 4.4 Must not change (confirmed)

| Rule | Evidence |
|------|----------|
| **Subsidy** | `GetBlockSubsidy(height)` still added in full to both `oru_reward_limit` and `m_chain_oru_total`. Burn is not subtracted from `R(h)`. |
| **RES mint** | Mint amount / winner / cap checks do not read `nFeesOruResTransferBurn`. |
| **RES supply** | `m_chain_res_mint_total` / `RES_MAX_SUPPLY` independent. Formal `res_isolation`. |
| **ORU consensus supply rule (RFC-002)** | `oru_supply_after = oru_supply_before + block_subsidy − block_converge_sum` (`validation.cpp` ~3139). Fee burn is **not** a term. |
| **Coinbase validation shape** | Still `bad-cb-amount` vs `(fees − burn) + subsidy`. `CheckTransaction` unchanged. Explorer did not edit this path. |

The only consensus delta RFC-018 introduced (shipping, not this audit) is **reducing the claimable fee portion** of the coinbase cap. That is the specified rule, not a subsidy, mint, or ledger rewrite.

---

## 5. What is *not* RFC-018 fee burn

Do not merge or relabel:

| Quantity | What it is |
|----------|------------|
| `m_block_converge_sum` / `cumulative_convergence` | Convergence destroyed ORU (CVG2) |
| `m_chain_oru_total` / `oru_supply_cumulative` | RFC-002 ledger `S_ORU` |
| `S_ORU − UTXO_ORU` | Mixed residual: fee burn **plus** voluntary underclaim **plus** any index gap |
| Wallet / explorer circulating | UTXO ORU (not `issued − destroyed`) |
| Explorer `cumulative_burned_oru` | Option C statistic; may lag; `equivalence_claimed = false` |

---

## 6. Formal / docs (observational, not layer A–C)

`formal_verification/coq/fee-burn/*.v`, `python-spec/rfc018_fee_burn.py`, `golden-vectors/rfc018_fee_burn.json`, `tests/test_rfc018_fee_burn_parity.py`, `CirculatingSemantics.v`.

These prove model properties (determinism, RES isolation, circulating ≤ ledger). Parity checks the **share function** against C++ `OruResTransferFeeBurnShare`, not the full ConnectBlock control flow.

Security docs that cite RFC-018 (supply-layer audits, claim traceability, current-state formal report) are documentation. They do not execute.

---

## 7. Residual limits (honest)

1. **No ConnectBlock refinement.** Stack-local accumulation, `IsORBClaimTx` vs explorer `asset_kind == RES`, and same-block prevout ordering are not Coq-extracted.
2. **No consensus cumulative.** Exact genesis-to-tip `Σ burn` lives only in the explorer table (or a full derived walk). Node RPC correctly says `not_persisted_on_block_index`.
3. **Indexer lag / missing prevouts** undercount explorer burn; they must not be treated as consensus.
4. **Voluntary underclaim** is a second UTXO sink. Do not label it RFC-018.

---

## 8. Conclusion

RFC-018 fee burn’s **consensus boundary** is a 1/2 floor of RES-transfer ORU fees withheld from coinbase. It does not mint or burn RES, does not change subsidy `R(h)`, does not rewrite RFC-002 `S_ORU`, does not alter `CheckTransaction` / `CheckTxInputs` fee identity, and does not create a burn UTXO.

**Wallet** pays and balances the full fee/UTXO. **Mining** withholds the same share when assembling the template. **Explorer** stores Option C cumulative statistics for charts and APIs, separate from Convergence destruction and circulating spendable ORU.

```
equivalence_claimed = false
```

No C++ `ConnectBlock` refinement is claimed.

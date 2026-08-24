# RFC-018 Fee-Burn Statistics Design

**Task:** `TASK-RIGOR-RFC018-FEE-BURN-STATISTICS-DESIGN-AUDIT-P1-001`  
**Date:** 2026-08-20  
**Scope:** Visibility of cumulative RFC-018 ORU fee burn. Recommendation only.  
**Consensus impact:** **NONE** — no ConnectBlock, chainstate, `CBlockIndex`, subsidy, or RFC-018 rule changes.  
**Honesty:**

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

Related: [RFC-018](../rfc/RFC-018-RES-TRANSFER-ORU-FEE-BURN.md) §5, [RIGOR_SUPPLY_ACCOUNTING_MODEL.md](./RIGOR_SUPPLY_ACCOUNTING_MODEL.md), [RIGOR_ORU_CIRCULATING_SUPPLY_ACCOUNTING_AUDIT.md](./RIGOR_ORU_CIRCULATING_SUPPLY_ACCOUNTING_AUDIT.md).

---

## Recommendation

**Choose C: explorer / indexer-only statistics** for historical cumulative RFC-018 fee burn.

| Option | Role |
|--------|------|
| **A. Consensus chain accumulator** | **Reject.** Not required for validity, reorg, or circulating supply. Would add a chainstate / index surface RFC-018 §5 explicitly avoided. |
| **B. Runtime derived RPC value** | **Optional later, diagnostic only.** Per-block burn can be recomputed from that block’s txs + undo. A tip residual `S_ORU − UTXO_ORU` is **not** RFC-018 burn (it mixes underclaim). Do not persist. |
| **C. Explorer / indexer statistics** | **Primary.** Historical cumulative, charts, and “ORU destroyed by fee burn vs Convergence” belong here. |

Circulating spendable ORU remains the UTXO set. Users do **not** need a consensus cumulative burn figure to know how much ORU they can spend.

---

## 1. Current fee-burn calculation path

RFC-018 does not write a burn output. It lowers the coinbase claimable cap.

```
burn_tx   = floor(fee_ORU · 1/2)     if IsORBClaimTx (ASSET_RES input domain)
          = 0                        otherwise (pure ORU, Convergence, ORU-funded mint)
claimable = (Σ fees_ORU − Σ burn_tx) + subsidy
```

**ConnectBlock** (`validation.cpp`):

1. `CheckTxInputs` yields `txfee` (always ORU).
2. Before `UpdateCoins`, `IsORBClaimTx` → add `OruResTransferFeeBurnShare(txfee)` to function-local `nFeesOruResTransferBurn`.
3. Reject `coinbase_ORU > claimable` (`bad-cb-amount`).
4. Update RFC-002 ledger: `m_chain_oru_total += subsidy − block_converge_sum` (**no** fee-burn term).

**CreateNewBlock** (`miner.cpp`): same classification; template `vout[0] = claimable`.

`nFeesOruResTransferBurn` is stack-local. It is not stored on `CBlockIndex`, not in undo, not in `RuntimeSupplyState`, not in coinbase commitment. `getruntimesupply.supply_semantics.rfc018_fee_burn_cumulative` already records `not_persisted_on_block_index`.

Voluntary underclaim (`coinbase < claimable`) is a **second** UTXO sink. Consensus allows it. It is not RFC-018 burn and must not be labeled as such.

---

## 2. Reorg behavior

`DisconnectBlock` restores spent coins from undo and invalidates `m_chain_oru_total` (`= -1`). CVG2 was never in the UTXO set. The unclaimed fee share was never a coin, so there is **nothing to unwind**.

That is the correct consensus behavior. A cumulative burn field on the index would be a second cache to invalidate and recompute — complexity without a consensus consumer.

Coq model `reorg_safety` restores pre-block circulating scalars; the new branch replaces burn, it does not sum abandoned-branch burn. Not a C++ refinement (`equivalence_claimed = false`).

---

## 3. Why A (consensus accumulator) is the wrong layer

Consensus already enforces the only rule that matters: this block’s coinbase must not claim the burn share.

Cumulative `Σ burn` is unused by:

- RFC-002 `S_ORU`
- Convergence draw / `f_i` (still the **full** ORU fee)
- RES mint / cap
- `state_hash` / replay snapshot
- Wallet spendability

Putting it on chainstate or a serialized `CBlockIndex` field would:

1. Invite treating a statistic as a consensus scalar (same class of confusion as calling `m_chain_oru_total` “circulating”).
2. Require a new reconnect/recompute path on every reorg and restart.
3. Mix underclaim unless the field is defined as *strict* RFC-018 floor-share only — still unused by validation.

**Do not add chainstate fields. Do not fold burn into `m_block_converge_sum` or `m_chain_oru_total` (RFC-018 §5).**

---

## 4. RPC requirements (option B, if ever)

| Need | Shipping today | Design |
|------|----------------|--------|
| Circulating ORU | `utxo_non_res_balance` / explorer circulating SUM | Sufficient. No burn total required. |
| This-block RFC-018 burn | Absent (miner log only) | **Derived:** walk that block with undo / prevouts; classify `IsORBClaimTx`; sum `OruResTransferFeeBurnShare`. Read-only. No new fields. |
| Tip residual sink | Absent | `S_ORU − utxo_non_res_balance ≈ B_fee + B_underclaim` (+ any genesis/index discrepancy). Label **“unnamed UTXO sinks”**, never “RFC-018 cumulative.” |
| Exact historical `Σ B_fee` | Absent | Do **not** scan the full chain on every `getruntimesupply`. That is O(blocks × txs) and needs bodies + undo. |

A runtime RPC that recomputes **one** block is reasonable later. A runtime RPC that claims exact genesis-to-tip RFC-018 cumulative without an indexer is the wrong cost model.

---

## 5. Explorer requirements (option C)

The indexer already records `res_transfers` (RES-input spends that are not the mint tx). That is the RFC-018 eligibility class.

Gap: tx fee helper returns **N/A** when a tx involves RES (`explorer/src/fee.ts`), so the ORU leg `fee = ORU_in − ORU_out` is not shown and `floor(fee/2)` is not accumulated.

For statistics (future work, not this task):

1. For each indexed RES-transfer, compute ORU-leg fee from indexed prevouts (do not invent a RES-denominated fee).
2. `burn_atoms = floor(fee_oru / 2)` with the locked NUM/DEN.
3. Store per-block and chain cumulative **off-node** (SQL). On reorg, drop disconnected heights and re-sum — same as other explorer aggregates.
4. Charts: **Convergence destroyed** vs **RFC-018 fee burn** as two series. Never add CVG2 back into circulating ORU.

Explorer cumulative is presentation. It can lag the tip. It must not be fed back into consensus or wallet balances.

---

## 6. Do users need historical cumulative fee burn?

| Audience | Need cumulative RFC-018 burn? | What they actually need |
|----------|------------------------------|-------------------------|
| Wallet / spender | **No** | Own spendable UTXO; they already paid the **full** fee |
| Circulating-supply consumers | **No** | Asset-filtered UTXO ORU |
| Miners | **No** (per-block cap is enough) | Template already withholds the share |
| Explorer / public dashboard | **Yes, as a chart** | Compare fee-burn sink vs Convergence sink over time |
| Formal / RFC narrative | **Yes, as a model quantity** | Already in Coq/Python; not a node RPC |

Historical cumulative is an **economic statistic**, analogous to “fees burned” on an explorer. It is not a safety or consensus input.

---

## 7. Identity (do not over-claim)

When the miner claims the full cap and genesis matches subsidy:

```
circulating_UTXO_ORU ≈ S_ORU − B_fee − B_underclaim
```

Therefore:

```
S_ORU − circulating_UTXO_ORU ≈ B_fee + B_underclaim
```

That residual is **not** RFC-018 burn. Exact `B_fee` requires classified RES-transfer fees along the active chain (option C, or a full derived walk).

`equivalence_claimed = false`: this document does not claim the C++ UTXO set equals the Coq circulating scalar.

---

## 8. Decision record

| Question | Answer |
|----------|--------|
| Does consensus need cumulative fee burn? | **No.** |
| Does circulating supply need it? | **No** (use UTXO). |
| Should it live on chainstate / `CBlockIndex`? | **No** (reject A). |
| Where should historical cumulative live? | **Explorer/indexer (C).** |
| May a later RPC expose per-block derived burn? | **Yes, derived, unstored (B-lite).** |
| May a later RPC expose tip residual sinks? | **Yes, if labeled mixed (fee burn + underclaim).** |

**Ship nothing in this task.** Next implementation, if approved: explorer cumulative series from `res_transfers` + ORU-leg fee; keep node consensus and chainstate unchanged.

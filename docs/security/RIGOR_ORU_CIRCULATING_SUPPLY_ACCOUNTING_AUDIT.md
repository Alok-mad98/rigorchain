# RIGOR ORU Circulating Supply Accounting Audit

**Task:** `TASK-RIGOR-ORU-CIRCULATING-SUPPLY-ACCOUNTING-AUDIT-P0-001`  
**Date:** 2026-08-20  
**Scope:** Post–RFC-018 ORU monetary accounting across consensus, wallet, RPC, explorer, and formal verification.  
**Consensus impact:** **NONE** — no consensus parameters, ConnectBlock rules, or UTXO semantics were changed.  
**Honesty:**

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

| Field | Value |
|-------|-------|
| Audit result | **PASS** for consensus monetary rules; **GAP** for named circulating / destroyed-fee surfaces |
| Critical consensus findings | **None** |
| Highest open finding | **Medium** — public RPC / explorer mix ledger, UTXO, and mixed-asset totals without naming RFC-018 fee burn |
| Formal coverage | **Partial** (model theorems exist; C++ equivalence is **not** claimed) |

This audit does **not** retarget fee-burn share, subsidy, or Convergence. It determines whether every shipping accounting surface uses the correct ORU concept after RFC-018 fee burn activation.

Related: [RFC-018](../rfc/RFC-018-RES-TRANSFER-ORU-FEE-BURN.md), [RFC-002](../rfc/RFC-002-supply-evolution.md), [FEE_BURN_AUDIT.md](../formal/rfc018_fee_burn/FEE_BURN_AUDIT.md).

---

## 1. Executive verdict

RFC-018 **does** destroy ORU. It does so by lowering the coinbase claimable cap. The destroyed share never enters the UTXO set. Consensus enforcement (`bad-cb-amount`) is consistent with that rule.

The chain scalar `m_chain_oru_total` is **not** circulating supply after that destruction. It is also **not** total historical issuance. It is the RFC-002 ledger:

```
S_ORU(t+1) = S_ORU(t) + R(t) − C(t)
```

Fee burns and voluntary coinbase underclaims are **UTXO-level sinks** that RFC-018 §5 explicitly leaves out of `m_chain_oru_total` and `m_block_converge_sum`.

Answer to the posed A/B question:

| Option | Meaning | Is `m_chain_oru_total` this? |
|--------|---------|------------------------------|
| **A** | Total historical ORU issuance `Σ R(h)` | **No** |
| **B** | Circulating ORU after all destruction | **No** |
| **Intended (C)** | Issued minus **Convergence** burn only | **Yes** — RFC-002 `S_ORU`, RFC-018 §5 |

No consensus patch is required. The ledger and the UTXO set are internally consistent once those two numbers are not treated as the same quantity.

What is missing is **named product accounting**: no shipping RPC currently exposes cumulative RFC-018 fee-burn destruction, and several surfaces invite the reader to treat `S_ORU` or `gettxoutsetinfo.total_amount` as circulating ORU.

---

## 2. Four concepts (normative glossary)

Use these names. Do not collapse them.

| # | Concept | Symbol | Shipping source of truth |
|---|---------|--------|--------------------------|
| 1 | **Total historical ORU issuance** | `I(h) = Σ_{i=0..h} R(i)` | Optional `getruntimesupply.oru_subsidy_cumulative_derived` (`ComputeDerivedOruSubsidyCumulative`). Not stored on `CBlockIndex`. |
| 2 | **Destroyed ORU through fee burn** | `B_fee(h) = Σ floor(fee_RES_xfer · 1/2)` plus voluntary underclaim of the remaining claimable cap | **Not accumulated.** Function-local `nFeesOruResTransferBurn` in `ConnectBlock` / `CreateNewBlock` only. |
| 3 | **RFC-002 ledger ORU** | `S_ORU(h) = I(h) − Σ C(i)` | `CBlockIndex::m_chain_oru_total` (memory-only cache). RPC: `getruntimesupply.oru_supply_cumulative`. |
| 4 | **Spendable ORU UTXO supply** | Sum of unspent ORU-domain outputs in chainstate | Node UTXO set, asset-filtered. Closest observational split: `getruntimesupply.utxo_non_res_balance`. Wallet: `getbalances.oru.*` for **this wallet only**. |

**Destroyed through Convergence** is a fifth quantity, already named: `C(h)` per block (`m_block_converge_sum`), cumulative via coinbase commitment / `getblockchaininfo.cumulative_convergence`. CVG2 outputs are validated in-block and **must not occupy the UTXO set**.

Approximate identity when the miner claims the full RFC-018 cap and genesis matches the subsidy:

```
spendable_UTXO_ORU(h)
  ≈ S_ORU(h) − B_fee(h) − B_underclaim(h)
  = I(h) − Σ C − B_fee − B_underclaim
```

Immature coinbase remains in the UTXO set, so **available** spendable ORU (mature, wallet-owned) is a strict subset of UTXO ORU.

---

## 3. Current accounting model

### 3.1 Issuance

`R(h) = GetBlockSubsidy(h)`. ORU has tail emission (RFC-002). Cumulative issuance can exceed `MAX_MONEY` (that constant is a per-output sanity bound, not a supply cap). `ComputeDerivedOruSubsidyCumulative` therefore does **not** gate the sum on `MoneyRange()`.

### 3.2 Convergence destruction

Valid Converge transactions add `nValue` of CVG2 outputs into `block_converge_sum`. `AddCoins` skips `IsConvergenceScript`. `DisconnectBlock` skips the same outputs (comment: “convergence outputs are not represented in the UTXO set”).

That ORU is destroyed for spendability. It **is** subtracted from `m_chain_oru_total`.

### 3.3 RFC-018 fee burn

Eligibility: `IsORBClaimTx` (ASSET_RES input domain). Excluded: pure ORU, Convergence, ORU-funded RES mint.

```
burn_tx      = floor(fee_ORU · ORU_RES_TRANSFER_FEE_BURN_NUM / ORU_RES_TRANSFER_FEE_BURN_DEN)
             = floor(fee_ORU / 2)     // NUM=1, DEN=2, locked
claimable    = (Σ fees_ORU − Σ burn_tx) + R(h)
```

`ConnectBlock` rejects `coinbase_ORU > claimable` (`bad-cb-amount`). `CreateNewBlock` sets `vout[0] = claimable` when the template claims the max.

The payer still pays the **full** `fee_i = ORU_in − ORU_out`. Half of a RES-transfer fee is not claimable by the miner and is never written as a UTXO. There is no OP_RETURN parking output (RFC-018 §7).

### 3.4 Voluntary underclaim

Paying strictly less than `claimable` remains legal. That extra unclaimed ORU is also a UTXO sink and is also **not** folded into `m_chain_oru_total`. Same asymmetry existed for unclaimed fees before RFC-018.

### 3.5 Intended meaning of `m_chain_oru_total`

Documented on `CBlockIndex` and in `GetChainOruTotal`:

```
S_ORU(t+1) = S_ORU(t) + R(t) − C(t)
```

ConnectBlock (`validation.cpp`):

```
oru_supply_after = oru_supply_before + block_subsidy - block_converge_sum
pindex->m_chain_oru_total = oru_supply_after
```

RFC-018 §5 (normative for this audit):

> `m_chain_oru_total` continues to track `S' = S + subsidy − converge` (RFC-002). Fee burns are UTXO-level destructions via the lowered coinbase cap and are **not** folded into `m_block_converge_sum`.

**Do not** retarget this scalar to circulating supply without a separate RFC. Mixing fee burn into `S_ORU` would break RFC-002, runtime-supply Coq hooks (`rs_oru_supply` ↔ `m_chain_oru_total`), and `cb_oru_burn_delta` (which means **Convergence** `C(h)`, not fee burn).

---

## 4. Consensus accounting behavior

### 4.1 ConnectBlock supply updates

Order that matters:

1. `CheckTxInputs` yields `txfee` (always ORU; `nFeesRes == 0`).
2. If `IsORBClaimTx`, add `OruResTransferFeeBurnShare(txfee)` to `nFeesOruResTransferBurn` **before** `UpdateCoins`.
3. If `IsConvergenceTx`, add converge amounts to `block_converge_sum`.
4. Ledger: `S' = S + subsidy − block_converge_sum`. Negative `S'` is `bad-oru-supply-negative`.
5. Coinbase cap: `oru_coinbase_value ≤ (nFeesOru − nFeesOruResTransferBurn) + subsidy`.
6. Write `m_chain_oru_total = oru_supply_after`.

`nFeesOruResTransferBurn` is **not** written to `CBlockIndex`. A restart cannot recover cumulative fee burn from the block index alone; it is implied only by the UTXO set vs `S_ORU` gap (plus underclaims).

### 4.2 Rollback / reorg

`DisconnectBlock`:

- Restores spent coins from undo data.
- Does not `SpendCoin` CVG2 / unspendable outputs (they were never in the UTXO set).
- Sets `m_chain_oru_total = -1` (and RES mint cache / runtime mirror).
- `GetChainOruTotal` / `WarmOruSupplyCache` recompute `S + R − C` from ancestors.

Reorg safety for fee burn is UTXO-level: disconnecting a block restores the pre-block coins, so burned-but-never-created coinbase share simply never existed on the abandoned branch. There is no separate fee-burn accumulator to unwind. That is the correct consensus behavior.

### 4.3 Runtime / Coq observational mirror

`RuntimeSupplyState`:

| Field | Maps to | RFC-018 fee burn? |
|-------|---------|-------------------|
| `oru_supply_cumulative` | `m_chain_oru_total` | No |
| `oru_converge_burn_block` | `m_block_converge_sum` | No (Convergence only) |
| `oru_subsidy_cumulative_derived` | `Σ R` | No |
| `utxo_res_balance` / `utxo_non_res_balance` | UTXO scan by `IsResScript` | Indirect: non-RES UTXO is the circulating-ORU **proxy** |

`coq_supply_field_hook.cpp` compares `rs_oru_supply` to `m_chain_oru_total` and `cb_oru_burn_delta` to **converge** sum. The word “burn” in that hook means Convergence, not RFC-018.

### 4.4 Consensus inconsistency check

| Question | Result |
|----------|--------|
| Can a miner claim burned RES-transfer fees? | No (`bad-cb-amount`) |
| Can burned ORU re-enter via a UTXO? | No (never created) |
| Does `m_chain_oru_total` double-count or omit Convergence? | No — `− C(t)` only |
| Does folding fee burn into `S_ORU` happen accidentally? | **No** — matches RFC-018 §5 |
| Reorg restore of `S_ORU`? | Yes, via cache invalidation + ancestor walk |

**No consensus accounting bug.** The gap is observability, not enforcement.

---

## 5. Wallet / RPC / explorer consistency

### 5.1 Wallet

Wallet displayed ORU is **this wallet’s spendable UTXO**, not chain circulating supply. That is the correct concept for “available ORU.”

| Surface | Concept used | RFC-018 |
|---------|--------------|---------|
| `getbalances.oru.trusted` | Mature, trusted, wallet-owned ORU UTXO | Excludes burned fees (they are not UTXOs) |
| `untrusted_pending` | Mempool | Full fee already deducted from change |
| `immature` | Immature coinbase | Still ORU that exists in UTXO |
| `listunspent` / `getbalance` | Same asset split (`ValueFromAmountAsset`) | ORU 8 dp; RES 6 dp via `FormatResAmount` |
| Tx `fee` / `fee_oru` | `ORU_in − ORU_out` (full fee) | Does **not** split miner share vs burn share |

RFC-018 §3: wallets still pay the full fee; only the miner’s claimable amount is reduced. Not showing a “burned fee” line is consistent with the payer model. A **reporting** field (burned vs miner) would be additive and is not required for balance correctness.

**Help-text leftover (fixed in this task):** `getbalances` help still said “RES with 2 decimals.” Runtime values already use six-decimal `FormatResAmount`. Help text updated to 6 decimals. Not a consensus change.

**Verdict:** wallet displayed ORU **=** actual spendable UTXO ORU for that wallet (Bitcoin-style trusted / pending / immature / avoid_reuse caveats). It is **not** chain circulating supply and must not be labeled as such.

### 5.2 RPC map

| RPC | Field | Actual concept | Safe to call “circulating ORU”? |
|-----|-------|----------------|---------------------------------|
| `getblockchaininfo` | `cumulative_convergence`, `current_convergence_sum` | Convergence destroyed (commitment / window) | No — not supply |
| `getblockchaininfo` | (no ORU supply field) | — | N/A |
| `gettxoutsetinfo` | `total_amount` | Sum of **all** UTXO `nValue` with **no asset split** (`kernel/coinstats.cpp` / index) | **No.** Mixes ORU (`COIN=1e8`) and RES (`RES_COIN=1e6`). Help still says “the total amount of coins.” |
| `getruntimesupply` | `oru_supply_cumulative` | RFC-002 `S_ORU` | **No** — ledger, not circulating |
| `getruntimesupply` | `oru_subsidy_cumulative_derived` | Total issued `I(h)` | Yes for **issuance**, not circulating |
| `getruntimesupply` | `oru_converge_burn_block` | This block’s `C(h)` only | Not cumulative fee burn |
| `getruntimesupply` | `utxo_non_res_balance` | UTXO nValue where `!IsResScript` (includes immature coinbase; excludes CVG2) | **Best node proxy** for spendable-chain ORU; still includes immature |
| `getbalances` | `oru.*` | Wallet spendable | Wallet only |
| `getconvergencestats` | `active_oru` | ORU committed by **window candidates**, sat-style atoms | Not chain supply |
| (none) | destroyed fee-burn cumulative | — | **Missing** |

**Recommendation if any RPC is ambiguous:**

1. Never document `oru_supply_cumulative` as “circulating supply.”
2. Never use `gettxoutsetinfo.total_amount` as ORU circulating supply (Qt dashboard fallback currently does — see explorer / GUI spec).
3. If a public supply RPC is added later, expose four named fields: `oru_issued_cumulative`, `oru_converge_destroyed_cumulative`, `oru_fee_burn_cumulative` (new accumulator), `oru_utxo_spendable` (asset-filtered). Keep `m_chain_oru_total` as `oru_ledger_rfc002`.

Adding `oru_fee_burn_cumulative` on `CBlockIndex` would be an **observational cache**, analogous to `m_chain_oru_total`, not a consensus rule change — but it is a serialization / reorg-cache design task, out of scope here.

### 5.3 Explorer

Preferred dashboard supply: `/api/status` → `economic_snapshot.oru_indexed_utxo_total_display` from

```sql
SUM(oru_utxos.amount_atoms) WHERE spent = FALSE
```

Importer inserts **every** `asset_kind === "ORU"` output into `oru_utxos`, including CVG2 (classified ORU because the v1 program starts with `CVG2`). Those outputs are never spent on-chain and remain `spent = FALSE`.

| Destruction path | In node UTXO? | In explorer `oru_utxos` unspent sum? |
|------------------|---------------|--------------------------------------|
| RFC-018 fee burn (no output) | No | **No** — correctly excluded from circulating |
| Convergence CVG2 | No | **Yes** — overcounts circulating ORU |
| Voluntary underclaim | No | No |
| Immature coinbase | Yes | Yes |

**Fee burn is not counted as circulating** in the explorer ORU index (good). **Convergence burn is counted as circulating** in that same index (bad for a circulating-supply chart).

Explorer tx fee helper (`explorer/src/fee.ts`): `fee = ORU_in − ORU_out` for pure ORU txs; **N/A** if the tx involves RES. RES-transfer burned share is therefore not shown on tx pages.

Fallback `gettxoutsetinfo.total_amount` labeled “UTXO set total” is still mixed-asset. `docs/rigor-qt/FUTURE_GUI_RPC_SPEC.md` already prefers the explorer ORU index then that fallback.

**RES display leftover (not ORU consensus):** explorer still parses and formats RES with `base = 100` / 2 decimal places (`amount.ts` `resStringToAtoms(..., 2)`, `status.ts`, `addressIntelligence.ts`, `witnessPanel.ts`, `uiSsr.ts`, `public/tx.html`, `public/res.html`). Node RPC emits six-decimal `FormatResAmount`. Indexer docs (`docs/explorer/RES_INDEXING_RULES.md`) already say 6 decimals. This can reject or mis-scale RES RPC strings (`too many decimals … max=2`). Classified below; not an ORU supply-rule bug.

---

## 6. Formal verification coverage

Coq lives in `formal_verification/coq/fee-burn/`. Python spec + golden vectors + parity test exist. **Not** a ConnectBlock refinement.

| Requested / existing | Status | Notes |
|----------------------|--------|-------|
| `supply_conservation` | **Have** | Circulating ΔORU = subsidy − fee burn when `converge = 0`. Model circulating view, **not** `m_chain_oru_total`. |
| `res_isolation` | **Have** | Fee burn cannot mint/burn RES or change the cap. |
| `reorg_safety` | **Have** | Rollback restores pre-block ORU/RES; new branch replaces burn, does not sum A+B. |
| `replay_safety` | **Have** | Same pre-state + body ⇒ same burn. |
| `circulating_le_ledger` | **Have** (lemma) | `subsidy − C − B ≤ subsidy − C` for `B ≥ 0`. |
| `burn_is_unspendable` | **Have** | `fs_burn_utxo = None`. |
| `circulating_supply_definition` | **Suggested, not named** | Would alias `circulating_oru_delta` / `apply_block_circulating`. Content exists. |
| `fee_burn_reduces_circulating_supply` | **Suggested, not named** | Follows from `supply_conservation` when `burn > 0`. |
| `destroyed_oru_never_reenters_supply` | **Suggested, not named** | Model form is `burn_is_unspendable` plus rollback (reorg “unburns” by restoring pre-state — correct). |
| `circulating_supply_matches_utxo_ledger` | **Must not claim** | Would be a C++ UTXO refinement. `equivalence_claimed` stays **false**. |

Honest split already recorded in `FEE_BURN_AUDIT.md`: Coq circulating ORU ≠ C++ `m_chain_oru_total`.

**Recommendation:** add the four *named* aliases in Coq for audit traceability if a follow-up formal task is opened. Do not claim they prove `validation.cpp`.

---

## 7. Precision audit

### 7.1 Units (unchanged, correct)

| Asset | Atomic scale | Display |
|-------|----------------|---------|
| ORU | `COIN = 100_000_000` (1e8) | 8 decimals. Unchanged. |
| RES | `RES_COIN = 1_000_000` (1e6), `RES_ATOMIC_UNIT = 1` | 6 decimals (`FormatResAmount`). |

`1 RES = 1,000,000` atoms is the shipping lock (`rigor_consensus_lock.h` `static_assert`).

### 7.2 Remaining `100` / `100.00` / `RES_COIN=100`

| Hit | Layer | Classification |
|-----|-------|----------------|
| `COINBASE_MATURITY_DISPLAY = 100` (`explorer/src/api/status.ts`) | Explorer | **Not atom scale** — maturity blocks. Leave. |
| `RES_COIN = 1000000` | Consensus | Correct shipping lock. |
| Explorer `base = 100n` / `resStringToAtoms(..., 2)` / tests `atomsToResDisplay(100) == "1.00 RES"` | Active shipping UI/indexer | **Leftover 2-decimal RES display/parse.** Does not change ORU. Follow-up: align to 1e6 / 6 dp. Do not rewrite historical RFC prose. |
| `getbalances` help “RES with 2 decimals” | Wallet RPC help | **Leftover** — corrected this task to 6 decimals. |
| RFC-017 table row `RES_COIN = 100` vs `10^6` | Historical RFC | **Labeled comparison** of old vs new. Keep. |
| RFC-001 / archive `PROOF-*` / ALPHAPLUS whitepaper consistency report citing `RES_COIN=100` | Historical / archive | **Keep labeled.** Not shipping consensus. |
| `explorer/src/uiSsr.test.ts` `"100.00 RES paid in block 306"` | Explorer test copy | Historical mint **amount** wording (RFC-015-era 100.00 RES reward), not atom scale. RFC-017 shipping mint is `W(h)` (genesis 40.000000 RES). Display leftover. |
| Pool / contrib copy still saying 100.00 RES mint | Non-consensus docs | Historical RFC-015 reward; label, do not treat as atom scale. |

No `RES_COIN = 100` remains in `core/src/` shipping consensus headers.

---

## 8. Remaining gaps

| ID | Severity | Gap |
|----|----------|-----|
| G-1 | Medium | No chain accumulator or RPC for cumulative RFC-018 fee burn / underclaim. Circulating ORU cannot be reconstructed from `m_chain_oru_total` alone. |
| G-2 | Medium | `gettxoutsetinfo.total_amount` is a mixed-asset sum; GUI/explorer fallback treats it as ORU supply. |
| G-3 | Medium | Explorer `oru_utxos` includes never-spent CVG2, so “circulating” charts overcount Convergence destruction. |
| G-4 | Low | RPC names: `oru_supply_cumulative` / Coq `cb_oru_burn_delta` sound like circulating / fee burn. |
| G-5 | Low | Explorer RES 2-decimal parse/display vs node 6-decimal RPC. |
| G-6 | Low | Explorer RES-involving tx fees shown as N/A; burned share not reported. |
| G-7 | Info | Suggested Coq names not aliased; `circulating_supply_matches_utxo_ledger` must stay unclaimed. |

None of G-1…G-7 is a consensus inflation or double-spend bug.

---

## 9. Recommended next actions

**Do not change consensus** unless a later task explicitly retargets RFC-002 `S_ORU`.

1. **Naming (docs / RPC help only):** Document `m_chain_oru_total` as “RFC-002 ledger (issued − converge),” never “circulating.” Relabel Qt/explorer fallback away from `gettxoutsetinfo.total_amount` as ORU supply.
2. **Explorer circulating chart:** Exclude `IsConvergenceScript` / CVG2 from `oru_utxos` unspent SUM (or mark them burned, never spendable). Keep RFC-018 fee burns excluded (already true).
3. **Observational accumulator (optional, non-consensus):** Memory-only `m_chain_oru_fee_burn_total` updated in ConnectBlock / invalidated on DisconnectBlock, exposed on `getruntimesupply`. Same cache pattern as `m_chain_oru_total`. Requires a dedicated task and tests; not done here.
4. **Asset-split UTXO RPC:** Either fix `gettxoutsetinfo` help to say mixed nValue, or add `total_amount_oru` / `total_amount_res` using `IsResScript`.
5. **Explorer RES 6-decimal:** Align `amount.ts` and display helpers with `RES_COIN = 1e6`. Coordinated importer + UI + tests. Historical RFCs stay as-is.
6. **Formal follow-up:** Name `circulating_supply_definition`, `fee_burn_reduces_circulating_supply`, `destroyed_oru_never_reenters_supply` as aliases. Do **not** add `circulating_supply_matches_utxo_ledger` as a claimed C++ theorem.

---

## 10. Changes made with this audit

| File | Change |
|------|--------|
| This document | Audit record |
| `core/src/wallet/rpc/coins.cpp` | `getbalances` help: RES “2 decimals” → “6 decimals” (docs only; `ValueFromAmountAsset` already used six-decimal `FormatResAmount`) |

No ConnectBlock, subsidy, fee-burn share, or UTXO rule changes.

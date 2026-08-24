# RIGOR Supply Accounting Model

**Task:** `TASK-RIGOR-SUPPLY-LAYER-SEMANTIC-CONSISTENCY-P1-001`  
**Date:** 2026-08-20  
**Consensus impact:** **NONE**  
**Honesty:** `equivalence_claimed = false`

This is the canonical glossary for ORU/RES **product** accounting. It does not retarget RFC-002, RFC-017, RFC-018, ConnectBlock, subsidy, or UTXO rules.

Related: [RIGOR_ORU_CIRCULATING_SUPPLY_ACCOUNTING_AUDIT.md](./RIGOR_ORU_CIRCULATING_SUPPLY_ACCOUNTING_AUDIT.md), [RFC-002](../rfc/RFC-002-supply-evolution.md), [RFC-018](../rfc/RFC-018-RES-TRANSFER-ORU-FEE-BURN.md).

---

## Units

| Asset | Atomic scale | Display |
|-------|----------------|---------|
| ORU | `COIN = 100_000_000` | 8 decimals |
| RES | `RES_COIN = 1_000_000` | 6 decimals (`FormatResAmount`) |

Historical RFC-001 / RFC-015 `1 RES = 100` is **not live**. Those RFC sections are labeled historical.

---

## Canonical ORU quantities

Do not collapse these four names.

### 1. Total issued ORU

```
I(h) = Σ_{i=0..h} R(i)
```

`R(i) = GetBlockSubsidy(i)`.

**RPC:** `getruntimesupply.oru_subsidy_cumulative_derived` (optional derived sum).

### 2. Convergence destroyed ORU

```
C_cum(h) = Σ_{i=0..h} C(i)
```

CVG2 outputs are validated in-block and **must not occupy the UTXO set**.

**RPC:** `getblockchaininfo.cumulative_convergence` (commitment); per-block `m_block_converge_sum` / `oru_converge_burn_block`.

FENCE-W2: the committed cumulative is audit data. It must not be fed into `T`, `f_i`, mint authorization, or script evaluation. Draw weight remains the named candidate’s CVG2 amount. `bad-convergence-cumulative` is unchanged.

This is **not** RFC-018 fee burn. Coq hook `cb_oru_burn_delta` means Convergence `C(h)`.

### 3. RFC-018 fee-burn destroyed ORU

```
B_fee(h) = Σ floor(fee_RES_xfer · 1/2)
```

plus voluntary underclaim of the remaining coinbase cap.

Destroyed by **never creating** a UTXO (lowered `bad-cb-amount` cap). No OP_RETURN parking.

**RPC:** not persisted. `getruntimesupply.supply_semantics.rfc018_fee_burn_cumulative = not_persisted_on_block_index`.

### 4. Circulating spendable ORU

Sum of unspent **ORU-domain** coins in the node UTXO set:

- includes immature coinbase
- excludes CVG2 (never in UTXO)
- excludes RFC-018 fee burn (never created)
- excludes RES scripts

**RPC proxy:** `getruntimesupply.utxo_non_res_balance`  
**Explorer:** `/api/status` `economic_snapshot.oru_circulating_spendable_display` (CVG2 filtered)  
**Wallet:** `getbalances.oru.*` is **this wallet’s** spendable UTXO, not chain circulating supply.

Approximate identity when the miner claims the full RFC-018 cap:

```
circulating_spendable ≈ I(h) − C_cum(h) − B_fee(h) − B_underclaim(h)
                      = S_ORU(h) − B_fee(h) − B_underclaim(h)
```

`S_ORU` is the RFC-002 **ledger**, not circulating:

```
S_ORU(t+1) = S_ORU(t) + R(t) − C(t)
```

Shipping field: `m_chain_oru_total` / `oru_supply_cumulative`.

FENCE-W1: this scalar is an observational RFC-002 ledger, **not** a spendable balance. See [RIGOR_NO_GLOBAL_FENCE.md](../architecture/RIGOR_NO_GLOBAL_FENCE.md).

---

## What each surface must say

| Surface | Must mean |
|---------|-----------|
| `oru_supply_cumulative` | RFC-002 ledger (issued − converge) |
| `gettxoutsetinfo.total_amount` | Mixed UTXO `nValue` (ORU 1e8 + RES 1e6). **Not** circulating ORU |
| Explorer indexed ORU UTXO sum | Circulating spendable ORU (no CVG2) |
| Wallet `getbalances.oru` | Wallet spendable UTXO (8 dp ORU, 6 dp RES) |

---

## Formal boundary (model only)

Coq `formal_verification/coq/fee-burn/CirculatingSemantics.v`:

- `fee_burn_cannot_enter_circulating_supply`
- `cvg2_locked_outputs_excluded_from_circulating`
- `wallet_balance_matches_spendable_utxo_model`

`equivalence_claimed = false`. These are not ConnectBlock refinements.

# RIGOR ORU / RES Economic Interaction Model

**Task:** `TASK-RIGOR-ECONOMIC-MODEL-VALIDATION-PHASE-D-P0-076`  
**Date:** 2026-08-31  
**Nature:** Simulation and specification analysis. Not a consensus change.  
**Formula source:** `RigorConsensusKernel` (`GetBlockSubsidy`, `GetResMintReward`, `RigorSupplyDelta`)  
**Vectors:** `formal_verification/release_consensus_vectors.json`

This note describes how ORU and RES interact as protocol assets. It does not
reimplement issuance formulas and does not claim market price, adoption, or
external equilibrium.

---

## ORU

ORU is the security asset.

| Property | Protocol meaning |
|----------|------------------|
| **Security issuance** | Each height `h ≥ 1` may credit exactly `GetBlockSubsidy(h)` ORU in coinbase. Height 0 credits 0 (no premine). |
| **Miner reward** | The subsidy is the protocol-native security expenditure. Fees are additional and are not modelled as a price. |
| **Maturity** | An ORU coinbase created at height `C` is immature until `spend_height - C ≥ nCoinbaseMaturity` (`100`). Locked ORU is issued but not spendable. |
| **Supply cap** | None. After three halving shifts the per-block subsidy holds at the last frozen floor. Cumulative ORU is unbounded by design. |

Observational circulating Δ at a block is `subsidy − C(h)` (`RigorSupplyDelta`). Convergence burns and RFC-018 RES-transfer fee burns are ORU sinks; they do not mint RES.

---

## RES

RES is the scarce scheduled asset.

| Property | Protocol meaning |
|----------|------------------|
| **Scheduled reward** | `GetResMintReward(h) = W(⌊h / 210000⌋)` is the *authorization* for a mint, not an automatic credit. |
| **Actual mint** | `M(h) ∈ {0,1}`. A mint occurs only when the parent draw has a winner and exactly one `W(h)` body mint is present. Empty `F` ⇒ no mint. |
| **Maturity** | A body RES mint uses the same `nCoinbaseMaturity = 100` predicate as ORU coinbase. Maturity is spendability, not a second issuance path. |
| **Scarcity** | Hard cap `RES_MAX_SUPPLY = 21_000_000 RES`. The compile-time identity `RES_REWARD_EPOCH * ResScheduleSum() ≤ RES_MAX_SUPPLY` makes exhaustion of the cap unreachable even if every block mints. |

PoW does not create RES. Burned ORU is draw weight only.

---

## Interaction

```
block h
   │
   ├─ ORU coinbase  ← GetBlockSubsidy(h)     (always the subsidy path)
   ├─ ORU converge  ← C(h)                   (optional sink; weight for the draw)
   └─ RES mint      ← GetResMintReward(h)·M(h)
                      M(h)=1 only with a winner
```

Consequences:

1. **Independent ledgers.** ORU total and RES total are separate asset classes. Adding scheduled RES into ORU `total_supply` would be an accounting error. The simulator never does this.
2. **Schedule ≠ issuance.** `res_reward_scheduled` may be positive while `res_delta = 0`. Genesis and every empty-`F` block exhibit this. That is not hidden inflation; it is an unused authorization.
3. **No duplicate issuance.** At most one RES mint per block (`RES-I1`). ORU subsidy is a pure function of height. The two paths do not credit the same atoms.
4. **Maturity is not minting.** Moving a UTXO from locked → matured does not increase `issued`. Circulating rises; total does not.

---

## Validation: RES does not cause hidden inflation

| Failure mode | Why it cannot occur under the frozen rules |
|--------------|--------------------------------------------|
| **Hidden inflation** | RES atoms enter the UTXO set only as a body mint of size `GetResMintReward(h)`. ConnectBlock rejects any other amount (`bad-rigor-res-amount`). Observational `S_RES` is not a second mint path. |
| **Duplicate issuance** | Winner-must-mint and single-mint gates are in `RigorSupplyDelta`. Two successful mints at one height are consensus-invalid. ORU coinbase cannot pay RES (`bad-cb-res-amount` on production). |
| **Supply accounting mismatch** | Baseline identity (no burns): `locked + circulating + burned = issued` for ORU. RES `minted_res_supply` is counted only when `M(h)=1`. Scheduled cumulative RES is a diagnostic upper bound, not an ORU field. |

Evidence: `tools/economic_model_simulator.py` (frozen-vector snapshots), `core/src/test/test_economic_model_validation.cpp` (`res_delta = 0` while schedule is live; circulating Δ from `RigorSupplyDelta`).

---

## What this does not prove

- RES market price versus ORU
- Equilibrium burn rate or mint rate
- That every scheduled epoch will actually mint
- External economic equilibrium

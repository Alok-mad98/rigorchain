# RIGOR Economic Attack Surface (P0-076)

**Task:** `TASK-RIGOR-ECONOMIC-MODEL-VALIDATION-PHASE-D-P0-076`  
**Date:** 2026-08-31  
**Nature:** Economic attack-surface classification for the frozen mainnet
schedule. Simulation / analysis only. Consensus is not modified.

Risk labels are protocol-structural. They are not a prediction of profit,
hashrate, or price.

| Label | Meaning |
|-------|---------|
| **LOW** | Frozen rules make the attack ineffective or bounded without extra assumptions. |
| **MEDIUM** | The attack can distort distribution or timing; it cannot mint off-schedule or hide inflation. |
| **HIGH** | The attack can change issued supply, duplicate issuance, or break the circulating identity. |

No HIGH findings. Mainnet is not yet live; this is a pre-genesis baseline.

---

## 1. Early supply concentration

**Mechanism.** Epoch 0 pays 1000 ORU per block (height ≥ 1). Early hashrate
captures a larger per-block subsidy than later eras. Genesis itself issues 0
ORU (no premine).

**Bound.** The schedule is public and height-only. There is no founder
allocation, no height-0 coinbase UTXO, and no way to pull later-era subsidy
forward.

**Risk:** **MEDIUM**

Distribution is front-loaded by design (same shape as other halving
schedules). It is not a hidden premine and it does not violate
`circulating ≤ total_supply`.

---

## 2. Maturity manipulation

**Mechanism.** An attacker tries to spend coinbase / RES mint before
`spend_height − coin_height ≥ 100`, or tries to treat maturity release as
new issuance.

**Bound.** The predicate is a consensus constant (`nCoinbaseMaturity = 100`).
Miners cannot choose a shorter maturity. Releasing a coin from locked →
matured does not increase `issued`. The identity
`locked + circulating + burned = issued` is preserved.

Heights 99 and 100 remain immature for a height-1 coinbase; 101 is the first
mature spend height (frozen vectors).

**Risk:** **LOW**

---

## 3. RES timing attack

**Mechanism.** Delay or accelerate a draw so that a mint lands in a higher
`W(k)` epoch, or stack mints across a boundary.

**Bound.** `W(k)` is a function of the mint block height, not of wall-clock
time. At most one mint per block. The sliding window `W = 32` removes
single-block exclusivity of `F` (see lock-header derivation). Timing can
change *which* epoch a given burn competes in; it cannot mint `W(k)` twice
or mint after `RES_REWARD_LAST_EPOCH`.

**Risk:** **MEDIUM**

Timing is a distribution / participation issue, not a supply-inflation bug.

---

## 4. Draw gaming incentive

**Mechanism.** A large miner censors competitor convergence txs, self-deals
minimum 1 ORU burns, or re-rolls `F` to capture RES in proportion to
hashrate rather than burn.

**Bound.** Window `W = 32` makes a 50% miner’s probability of authoring the
entire window ≈ `2.3×10⁻¹⁰`. Eligibility still requires `H_i < T · f_i`
with `f_i` the on-chain burn. PoW does not credit RES. Empty `F` mints
nothing (`res_delta = 0` while `res_reward_scheduled > 0`).

Residual: sparse third-party arrival (`λ` small) still raises the chance of
a quiet window. That is a participation problem, not a second mint path.

**Risk:** **MEDIUM**

---

## 5. Miner exit pressure

**Mechanism.** After halvings, per-block subsidy falls 1000 → 500 → 250 →
125 ORU and stays there. If fees are negligible, miners might exit and
security expenditure falls.

**Bound.** The tail is strictly positive (`ORU_MIN_BLOCK_SUBSIDY` pin).
There is no epoch that pays 0 ORU. Protocol-native security budget at year
50 remains `125 ORU × ~52,596 blocks/year` under the frozen spacing. This
note does not convert that figure to fiat or hashprice.

**Risk:** **MEDIUM**

Permanent tail issuance removes a “subsidy cliff to zero.” Absolute security
in external units is out of scope.

---

## 6. Emission shock

**Mechanism.** A halving (every 210,000 blocks) halves the ORU per-block
reward; RES `W(k)` also drops on the same cadence. An attacker tries to
treat the drop as a bug, or to manufacture an extra shock.

**Bound.** Boundaries are pinned: 209999 vs 210000, 419999 vs 420000. No
operator parameter can retune them. The drop is 50% of the prior era rate,
not a jump to zero. RES schedule decay is independent of ORU and cannot
credit ORU.

**Risk:** **LOW**

Scheduled, public, and already encoded in frozen vectors.

---

## Summary

| Attack | Risk |
|--------|------|
| Early supply concentration | **MEDIUM** |
| Maturity manipulation | **LOW** |
| RES timing attack | **MEDIUM** |
| Draw gaming incentive | **MEDIUM** |
| Miner exit pressure | **MEDIUM** |
| Emission shock | **LOW** |

```
HIGH findings                 = NONE
CONSENSUS_MODIFIED            = FALSE
ISSUANCE_MODIFIED             = FALSE
FORMULA_DUPLICATION           = FALSE
```

This classification does not prove the absence of all economic attacks. It
does not prove market stability.

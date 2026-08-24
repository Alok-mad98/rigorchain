# Chapter 2 — Why RIGOR Uses Two Assets

RIGOR maps the two economic roles onto two native assets.

| Role | Asset | Rule |
|------|-------|------|
| Medium of exchange | **ORU** | Circulates; pays miners and fees; tail emission never goes to zero |
| Store of value | **RES** | Hard-capped; created only through ORU Convergence |

```
PoW  →  ORU  →  Convergence  →  RES
```

Proof-of-work discovers blocks and orders history. Proof-of-work **does not**
create RES. RES is created only when ORU is committed through Convergence and
the frozen selection function authorizes a mint.

---

## ORU — medium of exchange

**ORU** is the work-backed circulating asset.

| Property | Frozen rule |
|----------|-------------|
| Role | Medium of exchange; miner security budget; transaction utility |
| Script path | Witness v0 |
| Unit | `1 ORU = 100,000,000` base units |
| Initial subsidy | **1000 ORU** per block |
| Halving | Every **210,000** blocks |
| Sequence | **1000 → 500 → 250 → 125** |
| Tail emission | **125 ORU** per block forever — no further halving |
| Supply law | `S_ORU(h+1) = S_ORU(h) + R(h+1) − C(h+1)` |

**Miner security budget.** After three halvings, `R(h)` remains 125 ORU. That
tail is explicit: miners are paid in the **exchange** asset, not by inflating
the reserve.

**Transaction utility.** ORU is what users transfer and what they pay as fees.
It is meant to move.

**Convergence cost.** A convergence transaction commits ORU under consensus
rules. Committed ORU is removed from circulating supply (`C(h)`). That is how
exchange-asset use becomes reserve-asset eligibility.

Minimum commitment: **1 ORU**.

---

## RES — store of value

**RES** is the scarce settlement asset.

| Property | Frozen rule |
|----------|-------------|
| Role | Store of value |
| Script path | Witness v1 |
| Unit | `1 RES = 1,000,000` atoms |
| Maximum supply | **21,000,000 RES** |
| Creation | **Only** through ORU Convergence |
| Per successful mint | **`W(h)`** (40.000000 RES at genesis) |
| Per block | At most one successful mint (`M(h) ∈ {0,1}`) |
| Not mined | Pool policy and PoW cannot mint RES |

```
S_RES(0) = 0
S_RES(h+1) = S_RES(h) + GetResMintReward(h+1) · M(h+1)
S_RES(h) ≤ 21,000,000
```

There is no premine of RES. There is no administrative mint.

RES is **not** taken from coinbase and **not** a 1:1 swap for ORU. It is a
new Witness v1 UTXO minted in the **block body** when the draw has a
winner. Full noun list and issuance guarantees:
[Chapter 9 — Glossary](./09-glossary.md).

### Where each asset is born

| | **ORU** | **RES** |
|--|---------|---------|
| Ledger origin | Coinbase subsidy `R(h)` | Non-coinbase body mint |
| Who receives it | Miner | Draw winner’s RES program — not the miner |
| Per block | 1000 → 500 → 250 → 125 forever | `W(h)` RES or 0 |
| Cap | No hard cap (tail inflation) | 21,000,000 RES (schedule cannot exhaust the cap even if every block mints) |
| Calendar | Height-indexed subsidy | Amount `W(h)` is height-indexed; occurrence of a mint is not |

**ORU = created in coinbase. RES = created in the body after Convergence.**

Protocol-enforced: `S_RES ≤ 21,000,000`, and a winner **must** be paid `W(h)` RES
or the block is invalid.  
Not guaranteed: a mint in every block, or that 21 million is filled on a
date.

**RES has a maximum supply of 21,000,000. Actual circulating supply depends on
Convergence participation.** The schedule cannot exhaust the cap even if every
block mints; that does **not** mean 21 million will necessarily be reached.

**Deterministic Reserve Issuance:** constant draw target `T`; reward `W(h)`
decays as `floor(40·RES_COIN·(k+1)^(-3/2))` with `k = height/210000`, then
`W = 0` for `k > 250`. RES is not mined. Occurrence of a mint is
demand-driven; the amount if minted is deterministic; the cost floor
`C/W(k)` weakly rises as rewards decay.
Formally verified monetary invariants (not a refinement proof): see
`docs/security/RES_MONETARY_POLICY_FINAL_AUDIT_REPORT.md`.

---

## Closed economic loop

```
        ┌───────────────┐
        │   ORU (PoW)   │
        │ Medium of     │
        │ Exchange      │
        └───────┬───────┘
                │
                ▼
        ┌───────────────┐
        │ Convergence   │
        │ ORU committed │
        └───────┬───────┘
                │
                ▼
        ┌───────────────┐
        │  Selection    │
        │     (σ)       │
        └───────┬───────┘
                │
                ▼
        ┌───────────────┐
        │  RES (scarce) │
        │  window W=32  │
        └───────┬───────┘
                │
                ▼
        ┌───────────────┐
        │ Market demand │
        └───────┬───────┘
                │
                ▼
          (back to ORU)
```

**No Convergence → No RES.**  
**More demand → more Convergence → harder to win.**

Demand for RES is expressed by committing ORU. More committed ORU increases
competition in the draw. The protocol does not print RES to meet demand.

---

## Separation, not duplication

| Job | Asset |
|-----|-------|
| Circulate, pay fees, pay miners | ORU |
| Remain scarce; enter only by Convergence | RES |

Users **mine ORU**. They **converge ORU**. They may receive **RES** if the
deterministic draw selects their eligible commitment. That sentence must not
be shortened to “mine RES”.

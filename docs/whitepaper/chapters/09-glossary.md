# Glossary — Convergence, Roots, and Where RES Comes From

Narrative Chapter 9 is now
[`09-product-surfaces.md`](./09-product-surfaces.md)
(Chinese: [`09-product-surfaces.zh.md`](./09-product-surfaces.zh.md)).
This file remains the official noun list for the website and PDF. It does
not change consensus. Terms follow RFC-020 shipping issuance, not older
“destroy / forge / Merkle root / draw” drafts.

---

## What is Convergence?

**Convergence** is a consensus action: a user destroys **ORU** in an
**`OP_CONVERGE`** output. That output is not spendable. The ORU leaves
circulating supply. The **same transaction** MAY mint Witness v1 RES at
protocol cost `destroy_cost(h)`.

```
ORU destroyed (OP_CONVERGE, amount B)
        ↓
same body transaction
        ↓
Witness v1 RES (sum R) if R · (2²⁵ + h)² ≤ B << 40
```

Convergence is not mining. It is not a pool payout. It is not an exchange
counter. **No Convergence → no new RES.**

---

## What is `convergence_root`?

**`convergence_root`** (Convergence Root, **`C`**) is a public commitment
to eligible convergence transactions in a block.

```
eligible transactions
        ↓
sorted txids
        ↓
Hash256
        ↓
convergence_root  (C)
```

```
C = Hash256( concat( sort_memcmp( txid_i ) ) )
```

- The sort is byte-wise on native txids, not arrival order.
- It is **not** a Merkle tree.
- Anyone with the same eligible list computes the same `C`.

Explorers may show `convergence_root`. That display is not a verdict. A
verifier recomputes `C` from the public txid list. The root does not mint
RES.

---

## Where does RES come from?

**RES is not mined from coinbase. RES is not a 1:1 conversion of ORU.**

RES appears only in a **body transaction** that destroys ORU (`OP_CONVERGE`,
amount `B`) and creates **Witness v1** RES outputs (sum `R`) in the
**same transaction**, subject to

```
R > 0  →  R · (2²⁵ + h)² ≤ B << 40
```

at the confirming height. Coinbase `OP_RES_MINT` is not this path.
Mint-output maturity 100 is reorg protection, not a draw wait.

| | |
|--|--|
| Ledger origin | A non-coinbase mint in the block body |
| Economic origin | User-initiated Convergence (ORU destroyed) |
| Amount | Any `R ≥ 0` admitted by the inequality |
| Cap | Derived `419430400000000` atoms (`4194304 RES`) |
| Calendar | Mint **cost** `destroy_cost(h)` is height-indexed; **whether** a mint occurs depends on Convergence |

Destroyed ORU does **not** become RES one-for-one. It is subtracted from ORU
supply and pays the protocol cost.

```
S_ORU(h+1) = S_ORU(h) + R(h+1) − C(h+1)
```

PoW does not create RES. PoW only provides irreversible order so every
honest node evaluates the same height’s `destroy_cost(h)`.

---

## Contrast: where ORU comes from, where RES comes from

| | **ORU** | **RES** |
|--|---------|---------|
| Role | PoW security budget + fees + Convergence destroy | Independent scarce asset |
| Classification | Witness v0 (`scriptPubKey` parse) | Witness v1 (not CVG2) |
| Ledger origin | Coinbase subsidy `R(h)` | Body-tx destroy-to-mint at `destroy_cost(h)` |
| Per block | 128 ORU forever | Any `R` admitted by `R · (2²⁵ + h)² ≤ B << 40` |
| Who receives it | The miner (coinbase script) | The minting transaction’s RES outputs — **not** the miner |
| Cap | No hard cap (permanent tail inflation) | **Derived** `419430400000000` atoms |
| Who can create it | Consensus coinbase rule only | Consensus body mint only |

**One sentence:** ORU is created in **coinbase**. RES is newly minted in
the **same body transaction** that destroys ORU, never in coinbase.

---

## The full path — how RES “appears”

```
User destroys ORU in OP_CONVERGE (unspendable; leaves ORU supply)
        ↓
Same transaction may create Witness v1 RES outputs (sum R)
        ↓
Confirming height h
        ↓
Accept  ⇔  R > 0  →  R · (2²⁵ + h)² ≤ B << 40
        ↓
New R atoms of Witness v1 RES → the transaction’s RES programs
        ↓
Mint output is immature for 100 blocks (reorg protection, not a draw wait)
```

`destroy_cost(h)` is a frozen height function. It does **not** move with mining
difficulty.

Selected pins:

| Height | price (atoms) | price (ORU per 1 RES) |
|------:|-------------:|---------------------:|
| 0 | 102400000000 | 1024 |
| 210000 | 103685749152 | 1036.85749152 |
| 420000 | 104979520049 | 1049.79520049 |
| 630000 | 106281312688 | 1062.81312688 |

From block 210000 the destroy cost becomes 1036.85749152 ORU per RES.

### What miners and pools cannot do

- A miner cannot mint RES by changing a payout address.
- A body transaction that creates RES without an `OP_CONVERGE` destroy is
  invalid (`bad-txns-res-mint-no-burn`).
- A mint that fails `R · (2²⁵ + h)² ≤ B << 40` is invalid
  (`bad-txns-res-mint-underpaid`).
- A mint that would exceed the closed-form cap is invalid
  (`bad-txns-res-mint-cap-overshoot`).
- Pool software is not an issuer.

There is no second issuance door: not miner, not pool, not RPC, not wallet,
not coinbase.

---

## Can RES be over-issued?

**Not at consensus.** An over-issuing block is invalid.

Frozen constants:

| Constant | Value |
|----------|-------|
| `RES_COIN` | 100000000 |
| `destroy_cost(0)` | 102400000000 (1024 ORU per 1 RES) |
| `RES_DESTROY_POLE` | `2²⁵` |
| `RES_DESTROY_SHIFT` | 40 |
| `RES_MAX_SUPPLY` | 419430400000000 atoms (`2²²` RES) |
| Mint predicate | `R · (2²⁵ + h)² ≤ B << 40` |

Connect-block rules (public form):

- Positive RES outputs ⇒ exactly one `OP_CONVERGE` destroy, and
  `R · (2²⁵ + h)² ≤ B << 40`
- `S_RES + R > 419430400000000` ⇒ reject (`bad-txns-res-mint-cap-overshoot`)
- Ordinary RES transfers: `res_in ≥ res_out` — a transfer cannot create RES

The only way circulating RES can fall is a user destroying RES
(`res_in > res_out`). That tightens supply. It never loosens the cap.

**Audit sentence:** RES issuance is isolated to the Convergence
body-tx destroy-to-mint.

---

## Is issuance guaranteed?

Three different questions. Three different answers.

### 1. Hard cap — guaranteed

```
S_RES(h)  ≤  419430400000000
```

This is a consensus predicate, not a policy. The number is **derived**
from the ORU subsidy and `destroy_cost(h)`, not chosen as 21M.

### 2. The mint predicate — guaranteed (fail-closed)

If a body transaction creates RES, it **must** satisfy
`R · (2²⁵ + h)² ≤ B << 40`. The miner cannot change the cost function
or mint from coinbase. The transaction is invalid if they try.

### 3. A calendar of continuous issuance — **not** guaranteed

New RES enters only through Convergence destroys that request `R > 0`. How
fast it appears depends on participation (how much ORU is destroyed). The
protocol **does** give the mint cost a height schedule (`destroy_cost(h)`). It
does **not** give a date curve for whether a mint occurs.

Therefore:

| Situation | What happens |
|-----------|----------------|
| Nobody converges | RES minted can stay **0** for a long time (pure PoW blocks) |
| Heavy Convergence | Realised issuance approaches `ρ · RES_MAX_SUPPLY`; the derived cap is the 100% bound |
| First ~100 blocks of a new chain | Coinbase ORU is immature; Convergence cannot spend it; RES warmup is 0 |
| At very large height | `destroy_cost(h)` is high; a whole block subsidy may buy less than one RES atom |

“Issue steadily for 100–1000 years” is an **aspiration**, not a protocol
requirement. `destroy_cost(h)` has no difficulty-style negative feedback that
forces a mint in every block.

---

## One sentence

**ORU is mined in coinbase. RES is newly minted in the same body
transaction that destroys ORU, at `destroy_cost(h)`, never in coinbase.**

The lock is: **`R · (2²⁵ + h)² ≤ B << 40` × derived cap.**

What is protocol-enforced: **cannot print extra; a mint must pay `destroy_cost(h)`.**
What is not guaranteed: **a mint every block, or a year-by-year fill of
the derived cap.**

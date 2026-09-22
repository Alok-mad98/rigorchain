# RIGOR Convergence Mathematical Design Note

**Document status:** Mathematical Design Note  
**Date:** 2026-09-15  
**Whitepaper placement:** Appendix D (companion to Chapters 4 and 5)

This note explains the algebra of Convergence minting: how
`destroy_cost(h)` becomes an integer predicate, why that predicate is
not a price oracle, why the RES ceiling is derived, and what fairness
means for a public decision rule.

It is **not**:

- an RFC
- a protocol specification
- consensus authority
- a formal proof of C++ equivalence
- a protocol upgrade
- a replacement for RFC-020, RFC-018, the C++ consensus kernel, the
  Coq model, or the whitepaper chapters

Live issuance remains RFC-020. This note restates identities already
fixed there. It does not change a parameter, a predicate, transaction
serialization, or a byte of consensus.

```
equivalence_claimed = false
documentation_consensus_authority = false
refinement_proved = false
cpp_formal_equivalence = false
```

---

## 1. Central identity

The display cost and the consensus predicate are one relation:

```
destroy_cost(h)

        ↓ exact algebraic transformation
          (exact integer-preserving equivalence)

R · (2^25 + h)^2 ≤ B << 40
```

The transformation is an **exact integer-preserving equivalence**. It
is not an approximation. It applies:

- no rounding
- no floor
- no integer truncation
- no discarded remainder

A **display helper** may evaluate that quotient with integer division
by `2^50` (a right-shift by 50). That floor is for reading only.
Display rounding and floor **do not enter consensus**. Consensus
never divides by `2^50`. It evaluates `B << 40`. Feeding the floored
display value back as `B` would re-introduce floor. The predicate
does not.

### Display form

```
destroy_cost(h) = 1024 · (2^25 + h)^2 / 2^50
```

`2^50` is the **curve normalization denominator**: `(2^25)^2 = 2^50`.
It is not a consensus shift. Consensus never right-shifts by 50.

Units: whole ORU per whole RES. At genesis this is exact:

```
destroy_cost(0) = 1024
```

The atomic form of the same display cost is that quantity times the
eight-decimal scale `10^8`:

```
destroy_cost_atoms(h) = 1024 · 10^8 · (2^25 + h)^2 / 2^50
```

### Original mint condition

Let `R` be RES atoms minted, `B` ORU atoms destroyed in the same
transaction, and `h` the confirming height. `RES_COIN = 10^8`.

The mint is admitted iff the destroyed ORU covers the requested RES at
the display cost. In whole-coin amounts:

```
(R / 10^8) · destroy_cost(h)  ≤  B / 10^8
```

Equivalently, in atoms, without evaluating the display quotient:

```
R · destroy_cost_atoms(h)  ≤  B · 10^8
```

That is the original mint condition. Substituting the uncleared atomic
form:

```
R · 1024 · 10^8 · (2^25 + h)^2 / 2^50  ≤  B · 10^8
```

The rest of this section clears that inequality over the positive
rationals. Each step multiplies or divides both sides by a positive
non-zero integer, which preserves the inequality direction and the
truth value. No term is dropped. No remainder is discarded.

### Eliminate `10^8`

`10^8` stands on both sides. Divide both sides by `10^8` (`10^8 ≠ 0`):

```
R · 1024 · (2^25 + h)^2 / 2^50  ≤  B
```

The decimal scale of the two assets is gone. It was a unit conversion,
not a parameter of the curve.

### Eliminate `2^50` (curve normalization denominator)

`2^50` is the display-curve denominator, not a consensus shift.
Multiply both sides by `2^50` (`2^50 ≠ 0`):

```
R · 1024 · (2^25 + h)^2  ≤  B · 2^50
```

The quotient `/ 2^50` is gone. Consensus never evaluates that
division.

### Eliminate `2^10`

Rewrite the genesis cost as a power of two:

```
1024 = 2^10
```

```
R · 2^10 · (2^25 + h)^2  ≤  B · 2^50
```

Divide both sides by `2^10` (`2^10 ≠ 0`):

```
R · (2^25 + h)^2  ≤  B · 2^(50 − 10)
R · (2^25 + h)^2  ≤  B · 2^40
```

`1024` is gone as a written factor. Its exponent remains as the
difference `50 − 10`.

### Rewrite `· 2^40` as `<< 40` (consensus scaling factor)

`2^40` is the integer scaling factor of the consensus predicate, not
the `destroy_cost` denominator. For a non-negative integer `B`, left
shift is multiplication by a power of two:

```
B · 2^40  =  B << 40
```

That equality is the definition of a left shift on a non-negative
integer. It is not a rounding mode. It is not an approximation of a
real multiplication.

### Final predicate

```
R > 0  →  R · (2^25 + h)^2 ≤ B << 40
R = 0  →  B unconstrained
```

`2^40` is the **integer consensus scaling factor** after `1024 = 2^10`
is cancelled from the display form (`50 − 10 = 40`). It is not the
`destroy_cost` denominator. The display denominator remains `2^50`.

`R = 0` is a legal plain destroy. A positive mint is admitted iff the
cleared inequality holds at the confirming height.

After `10^8`, `2^10`, and `2^50` are eliminated, both sides of the
original condition are the same integer relation as
`R · (2^25 + h)^2 ≤ B << 40`. `<< 40` is therefore an exact
identity transformation of the original mint condition.

`check_mint` is exact. There is no rounding direction to specify.
`destroy_cost(0)` remains exactly 1024.

---

## 2. Integer consensus advantage

The conversion above is what consensus evaluates. It does not
re-introduce the cancelled factors.

Consensus does not require:

- division
- floating point
- square root
- an epoch table
- a price lookup

It requires only:

- multiply
- square
- shift
- compare

One multiplication, one squaring, one shift, one comparison. That is
the mint check.

Clearing the display quotient removes division from the live path.
Keeping the relation integer removes floating point and square root.
A closed function of `(R, B, h)` removes any need to index an epoch
or to look up a price. The mechanism’s consensus work is therefore
the arithmetic of one inequality, not a schedule of auxiliary tables.

This note states that reduction for this mechanism. It does not
compare it with any other project.

---

## 3. Why the consensus scaling factor is 40

`RES_DESTROY_SHIFT = 40` is the integer consensus scaling factor
(`B << 40`). It is not the `destroy_cost` denominator. That
denominator remains `2^50`.

The display form is

```
destroy_cost(h) = 1024 · (2^25 + h)^2 / 2^50
```

Two exponents in that **display** definition fix the consensus
scaling factor. They are not interchangeable with it:

```
1024 = 2^10                 genesis whole-RES cost
(2^25)^2 = 2^50             curve normalization denominator
```

`2^50` stays the display denominator. Consensus does not shift by 50.
Section 1 cancelled them in order:

```
50 − 10 = 40                integer consensus scaling factor
```

So:

```
RES_DESTROY_SHIFT = 50 − log₂(1024) = 40
```

The scaling factor is forced by cancelling the display genesis cost
against the display denominator. A different genesis cost moves `40`
and nothing else. Change `1024` and leave the scaling factor, or
change the pole (and thus `2^50`) and leave the scaling factor, and
the identity breaks: the predicate no longer expresses that display
curve.

`40` is therefore forced. It is not a separately chosen “nice number.”
It is not `2^50`.

---

## 4. Why this is not a price oracle

`destroy_cost(h)` is:

- a protocol work requirement
- a deterministic issuance condition

It is **not**:

- a market price
- an exchange rate
- an investment valuation

The protocol defines the amount of ORU destruction required for
deterministic RES issuance. It does not define the market value of
RES.

A confirming node evaluates whether `(R, B, h)` satisfies the
predicate. It does not quote RES in an external unit, does not publish
a secondary-market ratio, and does not forecast what a holder can
sell the minted amount for. Secondary-market ratios, if they exist,
are external observations. They are not consensus inputs.

If a secondary ratio sits above the protocol destroy requirement,
arbitrage may mint. If it sits below, minting may stop. Those are
market responses to a published condition. They are not the protocol
setting a price.

The Convergence curve defines deterministic protocol issuance cost.
It does not define market price, profitability, or future demand.
Differences in issuance cost across height do not constitute a protocol
guarantee of economic return.

---

## 5. Why the RES ceiling is derived, not declared

The value

```
RES_MAX_SUPPLY = 2^22
```

is not a governance parameter. It is not written as an independent
stock that later has to be enforced by a counter.

It comes from three facts already fixed by the issuance rule:

```
ORU emission
        +
destroy_cost(h)
        +
theoretical maximum destruction path
```

Write `S = 128 = 2^7` (ORU subsidy per block for `h ≥ 1`),
`P0 = 1024 = 2^10` (genesis whole-RES cost), `H = 2^25` (the pole).
Then

```
destroy_cost(h) = P0 · (H + h)^2 / H^2
```

The most RES one block’s subsidy can buy at height `h` is
`S / destroy_cost(h)`. The closed-form integral of that per-block
ceiling is

```
RES_MAX_SUPPLY = (S / P0) · H
               = 2^7 / 2^10 · 2^25
               = 2^22
               = 4 194 304 RES
```

The theoretical maximum destruction path is the schedule that destroys
every newly emitted ORU at the height it appears. ORU cannot be
destroyed before it exists, and `destroy_cost` is strictly increasing,
so `1 / destroy_cost` is strictly decreasing. Any delay yields strictly
less RES. No hoarding strategy reaches the bound from above.

The ceiling is therefore:

- a mathematical upper bound

It is **not**:

- a mining promise
- a schedule guarantee

Consensus does not claim that anyone will destroy every subsidy, that
the bound will be walked, or that realised stock will equal `2^22`.
Realised stock is `ρ · 2^22`, where `ρ` is the share of emission ever
destroyed to create RES. `ρ` is observed, not set.

The ceiling therefore needs no global issuance counter and no
governance parameter. Block validation evaluates the local predicate
on `(R, B, h)`. It does not consult “how much RES has been minted so
far.”

---

## 6. Monetary separation model

The issuance path is:

```
PoW
 |
 v
ORU
 |
 | destroy
 v
Convergence
 |
 v
RES
```

Proof-of-work produces ORU. ORU pays miners and moves as the
circulation asset. Convergence destroys ORU in a body transaction and
may mint RES under the integer predicate. RES is not a block reward.

ORU is assigned:

- security budget
- transaction utility
- miner payment

RES is assigned:

- scarce reserve asset
- deterministic issuance
- not mined

The separation exists because one asset cannot simultaneously
maximise:

- circulation
- scarcity
- security budget

A unit asked to circulate cheaply is asked not to be held. A unit
asked to be scarce is asked not to be spent. A unit asked to fund
hashpower forever is asked to keep paying miners after holders have
stopped transacting. Those pressures conflict when assigned to a
single native stock.

This note does not claim that the split produces adoption, a price, or
economic success. It states the assignment the protocol defines:
miner payment and circulation on ORU; derived scarcity on RES;
one-way destroy as the bridge.

---

## 7. Fairness

Fairness of the decision rule is:

```
same (R, B, h)
      =>
same accept / reject
```

Fairness means:

- same public input
- same deterministic result

Fairness does **not** mean:

- equal mint amount
- equal timing
- equal economic outcome

`R`, `B`, and `h` sit in the transaction and the confirming block.
There is no fourth hidden parameter.

The predicate therefore admits no:

- random selection
- miner discretion
- pool allocation

A verifier who re-evaluates `R · (2^25 + h)^2 ≤ B << 40` on the same
public inputs must obtain the same yes or no as every other verifier.
Earlier height has a lower unit destroy cost; that is the published
schedule, not a defect of the decision rule. Participants who mint
different amounts at different heights receive different realised
quantities. That is permitted by the curve. It is not what this note
calls fairness.

---

## 8. Engineering limitation

Current verification of the integer predicate uses `unsigned __int128`
boundary checks. `R` is bounded by `RES_MAX_SUPPLY`, `B` by
`MAX_MONEY`, `h` by `INT32_MAX`. In that box the left side of the
predicate reaches about `2^110` and the right side about `2^99`,
against `2^128` for `unsigned __int128`. The multiplication, square,
and shift fit that width.

Arithmetic overflow arguments of this kind prove:

- bounded integer safety

They do **not** prove:

- C++ refinement
- compiler correctness
- deployed binary correctness

Width facts and overflow assertions do not prove that the C++ kernel
matches a Coq model, that `ConnectBlock` is refined, or that the
running binary is the object that was checked.

```
equivalence_claimed = false
cpp_formal_equivalence = false
```

---

## 9. Limitations

This note does **not** claim:

- market adoption
- price
- economic success
- C++ equivalence

It also does not claim that anyone will walk the bridge, that the
derived ceiling will be filled, or that `destroy_cost(h)` is a market
quote. The identities above fix the shape of the mint transform. They
do not fix demand, secondary-market ratios, or implementation
equivalence.

---

## 10. References and design context

### Related documents

| Document | Role |
|----------|------|
| [RFC-020](../protocol/RFC-020.md) | Live RES issuance: `destroy_cost(h)` and the mint predicate |
| [RFC-018](../protocol/RFC-018.md) | RES-transfer ORU fee destroy (circulation sink) |
| [RIGOR Protocol Whitepaper](./RIGOR_PROTOCOL_WHITEPAPER.md) | Public protocol narrative |
| [Chapter 4 — Convergence](./chapters/04-convergence.md) | The one-way destroy-to-mint bridge |
| [Chapter 5 — Checkable Mathematics](./chapters/05-mathematical-fairness.md) | Predicate, forced consensus scaling factor, derived ceiling |

This note is Appendix D of that whitepaper folder. It does not
supersede RFC-020 or Chapters 4–5.

### Design context

RIGOR follows the broader cryptoeconomic observation that different
monetary functions may create conflicting pressures when assigned to
one asset.

This discussion is informed by prior work including:

- Satoshi Nakamoto, *Bitcoin: A Peer-to-Peer Electronic Cash System*,
  2008. <https://bitcoin.org/bitcoin.pdf>
- Nervos RFC-0015, *Crypto-Economics of the Nervos Common Knowledge
  Base*.
- later fee security budget research after Nakamoto, including Miles
  Carlsten, Harry Kalodner, S. Matthew Weinberg, and Arvind Narayanan,
  *On the Instability of Bitcoin Without the Block Reward*, ACM CCS,
  2016.

Bitcoin introduced permissionless proof-of-work ordering and
UTXO-based transfer. Subsequent research has treated long-term miner
payment, after subsidy decline, as an open question rather than a
settled conclusion.

RIGOR does not copy those models. It applies the observation to a
separate dual-asset UTXO design: ORU for circulation and miner
payment; RES for derived scarcity; Convergence as the one-way
destroy-to-mint bridge. It does not use state rent, state capacity, or
a smart-contract asset platform.

These references describe intellectual and research context only. They
do not imply protocol inheritance, implementation dependency,
consensus compatibility, or economic-outcome guarantees.

---

## What this note is

A mathematical design explanation of one identity:

```
destroy_cost(h)

        ↓ exact integer-preserving equivalence

R · (2^25 + h)^2 ≤ B << 40
```

The conversion is exact. The consensus scaling factor is forced. The
decision is public.
The ceiling is derived. The protocol defines a destroy requirement; it
does not define a market value. None of that is a protocol change.

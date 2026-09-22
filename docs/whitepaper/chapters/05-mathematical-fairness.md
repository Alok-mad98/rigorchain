# Chapter 5 — Checkable Mathematics: Predicate, Forced Scaling Factor, Derived Ceiling

Chapter 4 fixed the verification act. This chapter unfolds the predicate
so anyone can check it with paper or an integer program: the display
form and the consensus form are the same relation; the ceiling is
derived, not declared; fairness is the same public inputs yielding
the same decision.

The claim is still architectural. The mathematics fixes the **shape of
the transform**. It does not fix that anyone will walk the bridge, and
it does not fix an external price.

The numeric SSOT is the RFC-020 script. The identities in this chapter
are the ones that script prints. None was computed by hand.

---

## 5.1 What fairness is

On a one-way bridge, fairness is not “everyone has the same chance of
being drawn.” A draw hands assignment to a hash: the same public inputs
can yield different receivers after one hash. A verifier must trust a
randomness source, or a windowed rule.

Fairness here is narrower, and harder:

```
same (R, B, h)  →  same admit or reject
```

`R` is RES atoms minted, `B` is ORU atoms visibly destroyed, `h` is the
confirming height. All three sit in the block bytes. There is no fourth
hidden parameter. No unpublished rate. No random term a miner can
rewrite.

So fairness can be checked on the spot. Given a confirmed transaction,
anyone who re-evaluates the inequality must get the same yes or no as
the network. In this chapter, fairness means a deterministic and
publicly reproducible admit-or-reject. It does not mean equal mint
amount, equal timing, or equal economic outcome. Within the consensus
rule, that decision is a public, exact function of `(R, B, h)`.

---

## 5.2 Display and consensus are one identity

The destroy cost for reading is

```
destroy_cost(h) = 1024 · (2²⁵ + h)² / 2⁵⁰
```

`2⁵⁰` is the **curve normalization denominator** in `destroy_cost(h)`:
`(2²⁵)² = 2⁵⁰`. It is not a consensus shift.

A display helper may evaluate that quotient with integer division by
`2⁵⁰` (a right-shift by 50). That floor is for reading only. Display
rounding and floor **do not enter consensus**. Consensus never divides
by `2⁵⁰`. It evaluates `B << 40`.

At genesis it is exactly 1024 ORU per whole RES. Consensus never
performs that division. The integer predicate is the exact integer
form of the mint condition, not an approximation, a simplified
estimate, or an optimisation formula. It is algebraically equivalent
to the display-cost condition. No rounding or truncation occurs.
Consensus evaluates that relation with the division cleared:

```
R > 0  →  R · (2²⁵ + h)² ≤ B << 40
R = 0  →  B unconstrained
```

`2⁴⁰` is the **integer consensus scaling factor** after `1024 = 2¹⁰`
is cancelled (`50 − 10 = 40`). It is not the `destroy_cost`
denominator. The display denominator remains `2⁵⁰`.

The step from display form to consensus form is an exact
integer-preserving equivalence, not an approximation:

```
R · destroy_cost(h)                 ≤  B · RES_COIN
R · 1024·10⁸ · (2²⁵+h)² / 2⁵⁰      ≤  B · 10⁸
R · 1024   · (2²⁵+h)²              ≤  B · 2⁵⁰
R · 2¹⁰    · (2²⁵+h)²              ≤  B · 2⁴⁰ · 2¹⁰
R          · (2²⁵+h)²              ≤  B · 2⁴⁰
```

`RES_COIN = 10⁸` cancels. `1024 = 2¹⁰`. The consensus scaling factor
is therefore forced by the genesis destroy cost:

```
RES_DESTROY_SHIFT = 50 − log₂(1024) = 40
```

A different genesis cost moves the scaling factor and nothing else.
`40` is not an independently chosen “nice number.” Change the genesis
cost and leave the scaling factor, and the identity breaks: the
predicate no longer expresses that display curve.

Consensus evaluates one multiplication, one squaring, and one shift.
No division, no floor, no square root, no epoch index, no price table.
`check_mint` is exact, so there is no rounding direction to specify.
`destroy_cost(0)` is exactly 1024.

No free mint is representable: `destroy_cost(h) > 0` at every `h ≥ 0`,
and the minimum destroy that mints one atom is at least 1.

---

## 5.3 Why the exponent is 2

Write `S = 128 = 2⁷` (ORU subsidy per block), `P0 = 1024 = 2¹⁰`
(genesis cost of one whole RES), `H = 2²⁵` (the pole). The display form
is

```
destroy_cost(h) = P0 · (H + h)² / H²
```

The most RES one block’s subsidy can buy at that height is
`S / destroy_cost(h)`. Integrate in height:

```
∫_0^∞ S / destroy_cost(h) dh
  = (S / P0) · H² · ∫_0^∞ (H + h)⁻² dh
  = (S / P0) · H
```

The exponent being exactly 2 is what makes the integral exactly
`(S / P0) · H` with no residual. The cap therefore lands *on* a power
of two rather than near one:

```
RES_MAX_SUPPLY = 2⁷ / 2¹⁰ · 2²⁵
               = 2²²
               = 4 194 304 RES
               = 419 430 400 000 000 atoms
```

The primary issuance constants are selected as powers of two to
simplify arithmetic verification. The derivation of the cap follows
directly from the exponent relationships. Edit any one constant and a
visible identity breaks, rather than a curve quietly shifting.

Cumulative issuance through any height also has a closed form:

```
cumulative(h) = RES_MAX_SUPPLY · h / (h + 2²⁵)
```

That is the indefinite limit of the same integral. The operational
meaning of the pole `h = 2²⁵` is that, at full participation, the
median of the issuance distribution sits there. Cumulative RES is then
half the cap, and destroy cost is about 4 096 ORU per RES.

Tooling does not need a running counter. The ceiling at any height can
be evaluated on the spot.

---

## 5.4 Why the ceiling needs no consensus counter

The closed-form integral is an **upper bound**, not a calendar promise.
The argument is the exchange argument. Its premise — `destroy_cost`
strictly increasing — is itself checkable.

ORU cannot be destroyed before it exists, so destroy through any height
cannot exceed emission through that height. Cost is strictly
increasing, so `1 / destroy_cost` is strictly decreasing. The schedule
that maximises total RES destroys every newly emitted ORU at the height
it appears. That schedule is the integral above. Any delay yields
strictly less RES.

Therefore:

- no hoarding strategy reaches the bound from above;
- block validation does not consult “how much has been minted so far”;
- realised stock is exactly `ρ · 2²²`, where `ρ` is the share of
  emission ever destroyed to create RES.

A runtime “must not exceed the cap” check may still sit in the node. A
check that can never fire is cheap; discovering that the derivation had
a hole with no check in place is not.

Minting ends when a whole block’s subsidy no longer buys one RES atom.
That height is decided by the same integer arithmetic. It is not a
separate parameter.

---

## 5.5 128 sits in the numerator of the identity

ORU subsidy is flat:

```
subsidy(0) = 0
subsidy(h) = 128 ORU          for every h ≥ 1
```

`128 = 2⁷` is not decoration. It is the numerator of the cap identity.
Change it, and `2²²` does not hold unless `P0` or `H` moves with it. The
protocol floor for miner payment and the closed ceiling on the scarce
stock are two places in the same list of powers of two.

The inflation rate decays as `1/t` toward zero and never reaches it.
Residual inflation level remains `1 − ρ`. The mathematics does not
create `ρ`. It only implies that if `ρ` is observed, realised RES and
residual ORU inflation can be read off the definition on the spot.

---

## 5.6 Pins that can be checked on the spot

The following pins are printed by the numeric SSOT. An implementation
that misses them at these heights is not this curve.

| Height | Atoms | ORU / 1 RES |
|-------:|------:|------------:|
| 0 | 102 400 000 000 | 1 024 |
| 210 000 | 103 685 749 152 | 1 036.857 491 52 |
| 420 000 | 104 979 520 049 | 1 049.795 200 49 |
| 630 000 | 106 281 312 688 | 1 062.813 126 88 |

Destroy cost changes by about 0.000 089% per block. Wallets construct
at the confirm height plus a flat margin. An over-generous margin is a
rounding error, not a strategy. The display form is not back-filled as
`B`: construction must satisfy the inequality itself.

Worst case the predicate’s left side reaches about `2¹¹⁰` and the
right side about `2⁹⁹`, against `2¹²⁸` for `unsigned __int128`. The
multiplication, square, and shift do not overflow that width. That is
an arithmetic fact, not a supply policy.

---

## 5.7 How “same inputs, same decision” becomes fairness

Putting 5.1 with the sections above, fairness has four layers. Each
layer can be checked:

| Layer | Public object | What is checked |
|-------|---------------|-----------------|
| Predicate | `(R, B, h)` | Admit or reject has no random term and no hidden rate |
| Identity | Display form ↔ `<< 40` | The shift is forced by genesis cost; both sides are one relation |
| Ceiling | `(S / P0) · H = 2²²` | The cap is read off an integral, not written by governance |
| Exchange | Cost strictly increasing | Delay yields less; validation does not consult a counter |

Nobody needs to trust an operator to “follow the table.” The table is
the inequality. Nobody needs to trust a miner not to over-mint.
Over-minting bytes fail the inequality and the transaction is invalid.
Nobody needs to trust that “someone will enforce the cap later.” The
cap is held by the exchange argument, not by a global counter that
must be updated.

This is not equality of outcomes. Earlier minters enter at a lower unit
cost. That is Chapter 1’s duration-proportional payment in mathematical
form, not a hole. This creates a duration-sensitive cost schedule:
earlier participation receives a lower protocol-defined destroy cost
because it commits capital earlier in the height timeline. Fairness
within the consensus rule remains deterministic public evaluation of
that curve, not a private table that was edited.

Height `h` itself comes from the order that proof-of-work produces. The
mathematics assumes the confirming height is the one the network
shares. How that height is discovered and ordered is Chapter 6.

---

## 5.8 What this chapter does not claim

- It does not write the pins as market prices, or the closed integral
  as a promise that the ceiling will be walked.
- It does not claim C++ equivalence to the kernel model. Assertions in
  the script and Coq statements share names; the model proves selected
  invariants, not `ConnectBlock`.
- It does not write “fairness” as a draw chance or a return.
- It does not write `128`, `1024`, or `2²⁵` as independent lucky
  numbers. They are three exponents in one identity.

**The decision is public. The ceiling is derived. The consensus
scaling factor is forced.** The algebraic derivation is in Appendix D:
[`RIGOR_CONVERGENCE_MATHEMATICAL_NOTE.md`](../RIGOR_CONVERGENCE_MATHEMATICAL_NOTE.md).
That note is explanatory documentation. It does not introduce new
consensus rules. The Convergence curve defines deterministic protocol
issuance cost. It does not define market price, profitability, or
future demand. Differences in issuance cost across height do not
constitute a protocol guarantee of economic return. Chapter 6 states
how proof-of-work supplies the height everyone shares — mining pays
ORU and does not mint RES.

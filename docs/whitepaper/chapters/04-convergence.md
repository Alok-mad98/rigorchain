# Chapter 4 — Convergence: How the One-Way Bridge Is Verified

Chapter 3 gave two native identities. Without a public, deterministic
bridge they are two unrelated ledger objects: the miner budget and the
scarce stock come apart again. This chapter states how every full node
verifies that bridge.

The claim is still architectural. The protocol fixes **one visible
destroy, and an integer predicate in the same transaction**. It does not
fix that anyone will destroy, and it does not fix a secondary ratio.

```
ORU inputs
        ↓
exactly one OP_CONVERGE output, amount B (destroyed, not a UTXO)
        ↓
Witness v1 RES outputs summing to R, in the same body transaction
        ↓
at confirming height h: R > 0 → R · (2²⁵ + h)² ≤ B << 40
```

No Convergence → no new RES.

---

## 4.1 The bridge is one-way, atomic, and replayable

Chapter 1 assigned the three vertices to two assets. Chapter 2 wrote the
transform as rules. Chapter 3 wrote the identities as classification.
One step remains: the transform must happen in **the same transaction
bytes**, and every honest node replaying those bytes must reach the same
decision.

If destroy is in one transaction and mint in another, the ledger has a
window in which RES is credited and the destroy is “to follow.” That is
credit, not destroy. If destroy is inferred from
`inputs − outputs − fee`, it may become indistinguishable from ordinary
fee accounting, and a verifier cannot point at the output that was
burned. If a miner mints RES in coinbase, the bridge becomes miner
discretion.

Convergence therefore has three requirements that do not come apart:

1. **Destroy is visible** — one explicit, unspendable `OP_CONVERGE`
   output whose value is `B`.
2. **Mint is same-transaction** — if RES outputs exist, they sit in the
   same body transaction as that destroy.
3. **The predicate is integer** — admit or reject looks only at `R`,
   `B`, and `h`. The same numbers, the same result.

The direction is one-way: ORU may disappear and (when the predicate
holds) RES may appear. RES cannot convert back to ORU.

---

## 4.2 What it is

**Convergence** is the consensus act of destroying ORU into
`OP_CONVERGE` (`RIGOR_MARKER_CONVERGE`, `<20-byte HASH160> 0xbd`).
Destroyed ORU leaves circulation. The same transaction **may** mint
Witness v1 RES. The cost is the protocol predicate at the confirming
height.

It is not:

- mining. Proof-of-work only orders history and pays ORU.
- a 1:1 counter. Destroyed ORU is not renamed into an equal amount of
  RES.
- a smart contract. No bytecode is deployed. No state machine steps.
- a miner prize. The receiver is the transaction’s RES outputs, not
  coinbase.

A wallet may construct the destroy output. A wallet cannot spend it.
After validation the node does not insert it into the UTXO set.

`R = 0` is a legal plain destroy: ORU is removed and no RES is minted.
The destroy is a complete act. The mint is an optional, costly sequel.

---

## 4.3 Destroy must be visible

Destroy is declared by an explicit output, not inferred from a balance
difference.

```
scriptPubKey = PUSHBYTES_20 || HASH160(...) || 0xbd
nValue       = B  ≥ 1 atom
```

`B` is the destroy amount in the mint arithmetic. The one-atom floor is
the threshold that says “this is a destroy output,” not a mint price.
The smallest `B` that mints one RES atom at genesis is 1 024 ORU atoms;
the smallest `B` that mints one whole RES is the atom count implied by
`destroy_cost(h)`.

| Rule | Why |
|------|-----|
| At most one positive-value Convergence output per transaction | `B` must be a single declaration |
| ORU inputs only | RES cannot be the destroy feedstock; that would reverse the direction |
| Output not inserted into the UTXO set | What was burned cannot be spent again |
| Classified ORU-domain | It is a sink of the flow, not a piece of RES |

A RES output with no such destroy is invalid:
`bad-txns-res-mint-no-burn`. No visible destroy, no admitted mint.

---

## 4.4 Mint must be in the same transaction

The mint lives in the **block body**, not in coinbase. A Convergence
transaction looks like this:

```
inputs        ORU only
outputs       exactly one OP_CONVERGE, amount B
              zero or more Witness v1 RES, sum R
              optional ORU change
fee           oru_in − B − oru_change, still paid in ORU
```

`R` is the sum of all new RES outputs in that transaction. The predicate
is evaluated once on that sum. The miner who includes the transaction
does not receive RES. The miner still receives only coinbase ORU.

Same-transaction is atomicity. A verifier sees either “`B` is destroyed
and `R` is admitted” or a wholly invalid transaction. There is no
ledger state in which RES has appeared and ORU has not yet been
destroyed.

Ordinary RES transfers cannot create RES: `res_in ≥ res_out`. Destroying
RES (`res_in > res_out`) only tightens stock. It does not convert back
to ORU.

---

## 4.5 An integer predicate, not a quote

Consensus never evaluates the display formula. The integer predicate
is the exact integer form of the mint condition, algebraically
equivalent to the display-cost condition. No rounding or truncation
occurs. Consensus evaluates that relation with the division cleared:

```
R > 0  →  R · (2²⁵ + h)² ≤ B << 40
R = 0  →  B unconstrained
```

`h` is the **confirming height**, not the wallet’s construction guess.
Display form, for reading only:

```
destroy_cost(h) = 1024 · (2²⁵ + h)² / 2⁵⁰
destroy_cost(0) = 1024 ORU per whole RES
```

`2⁵⁰` is the curve normalization denominator. It is not a consensus
shift. `2⁴⁰` in `B << 40` is the integer consensus scaling factor. It
is not the `destroy_cost` denominator. Display rounding/floor from a
`/ 2⁵⁰` helper do not enter consensus.

Same height, same `B`, same `R` ⇒ every honest node admits or rejects
alike. No miner discretion. No unpublished rate. No division, square
root, or epoch index enters the predicate.

`destroy_cost(h)` is a protocol destroy requirement, not a market quote.
While minting is open, a secondary ratio above that requirement is
bounded by mint arbitrage; a ratio below it stops minting. The protocol
does not publish a quote.

`R` may be any positive number of RES atoms. A mint need not be a whole
RES. “What one RES costs in ORU” is the headline at `R = RES_COIN`, not
the integer a wallet must burn — construction must satisfy the
inequality itself.

There is no per-block mint cap. One block may absorb an unbounded
destroy. The absence of rate limiting is deliberate: demand should be
absorbable in a single block, not queued by the protocol.

---

## 4.6 Why it is not a 1:1 counter

A 1:1 swap would weaken the intended separation between the two
identities: destroy this much flow, appear that much stock. That is a
rename, not a transform. Chapter 1’s triangle would no longer assign
the three vertices to two distinct assets.

The protocol cost rises with height. An earlier mint pays less per RES
for a longer remaining span of protection. Destroyed ORU leaves
`S_ORU`. New RES enters `S_RES` as a new Witness v1 UTXO. No field
paints the old output as RES.

The reverse path does not exist. Spending RES as ORU, or spending a
Convergence output, is an invalid transaction.

---

## 4.7 Mint is not miner-allocated

Given `R`, `B`, and `h`, the result is admit or reject. A miner cannot
write RES into coinbase. A miner cannot assign RES to their own address
as a block prize. Pool policy cannot allocate RES. Changing a miner
payout address cannot create RES.

Fairness within the consensus rule is defined as deterministic
evaluation of the same public inequality. The mathematics is Chapter 5.
This chapter only fixes the verification act.

---

## 4.8 What happens when verification fails

Live reject codes name the failing clause:

| Reject | Meaning |
|--------|---------|
| `bad-txns-res-mint-underpaid` | `R · (2²⁵ + h)² > B << 40`: destroy does not cover the requested mint |
| `bad-txns-res-mint-no-burn` | RES output present, no `OP_CONVERGE` |
| `bad-txns-res-mint-cap-overshoot` | Runtime defence; Chapter 2’s closed-form integral makes it unreachable |
| `illegal-res-to-oru-conversion` | Wrong direction, or an attempt to spend a Convergence output |

Underpayment invalidates the whole transaction. There is no partial
fill that credits RES and waits for a later destroy.

Mint outputs mature after 100 blocks. That is the same depth as
coinbase maturity: reorg protection. The
kernel model proves selected rewind identities: disconnect an orphan
suffix and apply another, equals never applying the orphans. That is
not a `ConnectBlock` equivalence proof.

---

## 4.9 What this chapter does not claim

- It does not assert that anyone will destroy ORU, or that `ρ` lands in
  a band.
- It does not write `destroy_cost(h)` as a market price.
- It does not write maturity 100 as anything other than reorg protection.
- It does not claim C++ equivalence to the kernel model.
- It does not write Convergence as a contract, a counter, or a miner
  prize.

**Destroy is visible. Mint is an integer predicate in the same
transaction.** Chapter 5 unfolds that predicate as checkable
mathematics: the closed-form integral, the forced consensus scaling
factor, and why “same
inputs, same decision” is the fairness claim.

This chapter states the conceptual mechanism. Algebraic derivation,
the integer-predicate transformation, ceiling derivation, and the
fairness interpretation are in Appendix D:
[`RIGOR_CONVERGENCE_MATHEMATICAL_NOTE.md`](../RIGOR_CONVERGENCE_MATHEMATICAL_NOTE.md).
That Mathematical Design Note is explanatory documentation. It does
not introduce new consensus rules.

The Convergence curve defines deterministic protocol issuance cost.
It does not define market price, profitability, or future demand.
Differences in issuance cost across height do not constitute a protocol
guarantee of economic return.

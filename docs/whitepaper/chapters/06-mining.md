# Chapter 6 — Proof-of-Work: The Shared Height, Paid in ORU

Chapter 5’s predicate contains the confirming height `h`. The display
form, the shift, and the derived ceiling are all functions of `h`. If
nodes disagree on which block includes a transaction, the same destroy
is evaluated at different heights. This chapter states how height is
shared under proof-of-work, and what the block finder is paid.

The claim is architectural. Proof-of-work fixes **order and the payment
asset**. It does not fix who will find a block, and it does not fix an
external price of hashpower.

```
hashpower searches for a valid header
        ↓
the network admits one main-chain order
        ↓
confirming height h is the same for every honest node
        ↓
the predicate decides Convergence at that h
coinbase pays 128 ORU at that h
```

Mining **pays ORU**. Mining **does not mint RES**.

```
PoW
 |
 v
ORU security budget

Convergence
 |
 v
RES issuance
```

Mining security and reserve issuance are intentionally separate
mechanisms. Proof-of-work does not determine RES supply.

---

## 6.1 The predicate needs a shared height

`destroy_cost(h)` is strictly increasing. Chapter 1’s
duration-proportional payment and Chapter 5’s derived ceiling both
depend on later height costing more. If height could be a local clock,
a wallet guess, or an arbitrary integer written by a miner, the same
transaction bytes could be evaluated at different `h` on different
nodes.

Proof-of-work does not report wall-clock time. It produces a chain
order. Rewriting that order requires repeating the corresponding work.
The protocol does not claim that history cannot be rewritten under
sufficient hashpower. It claims that honest nodes replaying the same
admitted chain obtain the same height index.

Each later block acknowledges the previous one; height increases by
one. Convergence’s confirming height is the height of the block that
includes the transaction, not the height guessed at construction.

Without that shared index, Chapter 5’s fairness — same `(R, B, h)`,
same decision — has no public source for the third component.

---

## 6.2 What mining does, and does not

Proof-of-work does two jobs: it discovers valid headers, and it orders
history into a chain. It does not create RES, does not choose who
receives RES, and does not rewrite `destroy_cost`.

| | Mining supplies | Mining does not supply |
|--|-----------------|------------------------|
| Order | The admitted height `h` | A market quote |
| Payment | 128 ORU in coinbase (`h ≥ 1`) | RES |
| Role for the bridge | Everyone evaluates the predicate at the same `h` | Minting or assigning RES in coinbase |

Genesis subsidy is 0. Every later block pays `128` ORU to the Witness
v0 script in that block’s coinbase. That is Chapter 1’s miner-budget
vertex: the flow asset carries a protocol floor, independent of whether
store-of-value holders transact.

Paying RES as a mining reward would inflate the stock to buy security
and would recombine the two monetary functions on one ticker. So if RES
appears, it appears only in a Convergence transaction in the block
body, and only if Chapter 4’s visible destroy and integer predicate
hold.

---

## 6.3 How a valid header is admitted

The live proof-of-work digest is **OpuHash**. It is domain-tagged
SHA3-256:

```
OpuHash(header) = SHA3-256("RGRPOW01" || header_80)
valid           ⇔  OpuHash(header) ≤ target
```

OpuHash is used for **PoW validity**. Block **identity** remains
SHA256d of the same 80-byte header. Merkle roots and txids remain
SHA256d. The two hashes are not interchangeable: checking work uses
OpuHash; naming a block uses SHA256d.

The node assembles the block template (GBT). A SOLO miner copies the
node’s coinbase bytes, leaves time as the mutable field, grinds the
nonce, and `submitblock`. Coinbase states this block’s height and pays
ORU only.

A Convergence transaction included in the block is valid because the
destroy is visible and the predicate holds — not because the finder
chose it as a prize. The finder still receives only coinbase ORU.

---

## 6.4 How difficulty keeps height moving

Target spacing is 600 seconds. Difficulty follows ASERT (aserti3-2d).
Mainnet half-life is 43 200 seconds. Per-block clamp is ±12.5%.
Evaluation uses parent time and height only; child `nTime` is excluded.
The anchor is height 1 once it exists; genesis while producing height 1.

ASERT does not change issuance. It adjusts difficulty so that block
spacing tends toward the 600-second target, given parent timestamps.
Height itself remains a block-count index. ASERT does not bind `h` to
wall-clock time, and it does not enter the mint predicate. Difficulty
is not mint cost. `destroy_cost(h)` still changes only with height —
about 0.000 089% per block.

The kernel model selects a finite tip by most work on `GetBlockProof`.
It does not prove `ActivateBestChain`, and it does not prove ASERT’s
envelope versus `2ˣ`. That is not why this chapter exists.

---

## 6.5 The two birthplaces are not mixed

| | ORU subsidy | RES mint |
|--|-------------|----------|
| Locus | Coinbase | Block body, same Convergence transaction |
| Who receives it | The miner’s script | That transaction’s RES outputs |
| Per block | 128 ORU (`h ≥ 1`) | Any `R` the predicate admits, including zero |
| Who sets the amount | The protocol function `subsidy(h)` | The destroyer chooses `B` and requested `R`; the predicate decides |

Pool policy cannot allocate RES. Changing a miner payout address cannot
create RES. GBT coinbase address must be ORU Witness v0. Writing RES
into coinbase invalidates the block.

No Convergence → no new RES. Convergence whose predicate fails
invalidates the whole transaction. Having found a header does not turn
an underpaid mint into a legal one.

---

## 6.6 How this returns to Chapter 1

In a single-asset system, the block reward and the scarce stock are the
same unit: either issuance dilutes the reserve story, or issuance ends
and security is left to fees. RIGOR pays the reward in ORU, 128 per
block after genesis, so the security budget does not depend on whether
store-of-value users transact. It keeps the stock in RES, so mining
does not inflate the reserve.

Miners sell order and security — continuous protection of the whole
history, including RES that already exists. They collect that flow
payment in ORU. Anyone who wants to enter the stock walks Chapter 4’s
bridge and destroys ORU on Chapter 5’s curve. The two user groups meet
at the same height. They do not withdraw from the same coinbase.

The visible mining path may show shares, height, ORU maturity, and
(if it happens) Convergence in the wallet. The product flow is
[`07-mining-windows.md`](./07-mining-windows.md). That is a use path,
not a second consensus rule.

---

## 6.7 What this chapter does not claim

- It does not claim honest majority, or any particular hashpower
  distribution.
- It does not claim that proof-of-work makes history unrewritable.
- It does not claim C++ equivalence to the kernel model, and it does
  not prove `ActivateBestChain`.
- It does not write OpuHash as the block-identity hash. Identity
  remains SHA256d.
- It does not write difficulty or wall-clock time as mint cost.
- It does not write a pool dashboard or a website as a source of
  validity. Validity is equality of commitment and recomputation —
  Chapter 7.

**Height is shared. Payment is ORU. RES does not come from mining.**
Chapter 7 states how public data can be validated independently by
local recomputation.

# RIGOR Protocol Identity — One Page

**Document status:** one-page protocol explanation (not an RFC, not consensus SSOT, not a market note)  
**Date:** 2026-09-15  
**Chinese:** [`RIGOR_PROTOCOL_IDENTITY_ONE_PAGE.zh.md`](./RIGOR_PROTOCOL_IDENTITY_ONE_PAGE.zh.md)

```
equivalence_claimed = false
cpp_formal_equivalence = false
documentation_consensus_authority = false
```

This page answers three questions with the fewest words that remain
checkable: why RIGOR exists, how it implements the split it defines, and
where the claim stops. Live rules remain RFC-020 and RFC-018.

---

## One sentence

RIGOR separates the monetary functions of circulation and scarcity by
using two native assets: ORU secures the network through Proof-of-Work,
while RES is created only through deterministic Convergence destruction
of ORU under a public mathematical predicate.

---

## The problem

Single-asset Proof-of-Work chains ask one asset to perform three
conflicting roles:

- medium of exchange
- store of value
- security budget

High circulation favors spending.  
High scarcity favors holding.  
Miner security requires sustainable payment.

A single asset cannot maximize all three roles simultaneously.

---

## The separation

**ORU**

- transaction asset
- fee asset
- PoW subsidy
- security budget

**RES**

- scarce reserve asset
- not mined
- not a coinbase reward
- created only by Convergence

```
PoW
  ↓
ORU
  ↓
destroy ORU
  ↓
Convergence predicate
  ↓
RES
```

---

## The core identity

The entire RES issuance decision reduces to one public integer relation:

```
destroy_cost(h)

        ↓ exact integer-preserving transformation

R · (2²⁵ + h)² ≤ B << 40
```

This is not a price oracle.  
It is not a market prediction.  
It is a deterministic admission rule.  
Same public inputs produce the same result.

---

## Verification boundary

RIGOR does not claim:

- that C++ equals the formal model
- that the running binary is formally verified
- that economic success is guaranteed

The claim is narrower:

> Inside the protocol model, selected consensus invariants are machine
> checked with explicit trust boundaries.

---

## Design philosophy

RIGOR follows a small consensus surface:

- UTXO state
- deterministic predicates
- explicit issuance rules
- public recomputation

A display may explain.  
A node decides.  
A model may prove properties.  
A running implementation remains engineering.

---

## Final sentence

RIGOR is not a promise of outcomes. It is a protocol design that
separates conflicting monetary functions and makes issuance decisions
publicly reproducible.

---

## Read next

| Document | Role |
|----------|------|
| [`RIGOR_PROTOCOL_WHITEPAPER.md`](./RIGOR_PROTOCOL_WHITEPAPER.md) | Public protocol narrative |
| [`RIGOR_CONVERGENCE_MATHEMATICAL_NOTE.md`](./RIGOR_CONVERGENCE_MATHEMATICAL_NOTE.md) | Algebra of the mint identity |
| [`../protocol/RFC-020.md`](../protocol/RFC-020.md) | Live RES issuance |
| [`../protocol/RFC-018.md`](../protocol/RFC-018.md) | RES-transfer ORU fee destroy |

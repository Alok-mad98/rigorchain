# Chapter 11 — Design Trade-offs

Chapter 10 bounded consensus, economic, and external risks. This
chapter states what RIGOR intentionally does not provide, and what
those omissions are intended to buy.

The claim is architectural. A smaller surface is a design choice. It
is not a ranking of all public chains, and it is not a claim that
every application belongs on this ledger.

```
closed monetary templates
        +
UTXO-local validation
        +
one-way Convergence
        ≠
a general-purpose execution platform
```

---

## 11.1 What RIGOR does not optimise for

RIGOR intentionally does not optimise for:

- smart-contract execution
- arbitrary token issuance
- unlimited scripting
- account-based global state
- programmable asset logic

Those capabilities are useful in other systems. They are out of scope
here because they enlarge the consensus surface: more execution paths,
more ways for “what is this asset?” to depend on an indexer, a
contract store, or a later upgrade of a VM.

Chapter 3 already closed the template list. Production has no Script
VM. Identity is classification of `scriptPubKey`. Legitimacy is the
mint predicate and pool isolation. This chapter does not add a further
restriction; it records the trade-off.

---

## 11.2 Dual-asset complexity

The dual-asset design introduces complexity. That is a different
trade-off, not a claim that two assets are better than one.

Benefits, within this model:

- a separate security budget on ORU
- a separate scarcity mechanism on RES
- clearer assignment of monetary roles

Costs:

- two-asset accounting
- wallet UX complexity
- additional verification paths (destroy, mint predicate, reverse ban)

The protocol separates those functions. It does not claim that every
single-asset design is inferior, or that users will prefer two
balances.

---

## 11.3 What the restrictions are intended to buy

Under this model, the intended returns are:

- a smaller consensus surface
- deterministic validation of public bytes
- easier independent verification (Chapter 7)
- simpler formal modelling (Chapter 8)

Deterministic validation means: the same `(R, B, h)` and the same
script shapes produce the same admit-or-reject. It does not mean
equal economic outcomes. Independent verification remains a full-node
act; a browser page is not `ConnectBlock`. Formal modelling remains a
model; it is not C++ equivalence.

---

## 11.4 These choices are not universal

A closed monetary surface is a poor fit for applications that need
general computation, custom issuance, or contract-mediated conversion.
Those applications belong on platforms designed for them.

RIGOR is an alternative architecture for two native monetary
functions coupled by a one-way destroy-to-mint. It is not presented
as strictly better than Bitcoin, and not as a replacement for
programmable chains.

---

## 11.5 What this chapter does not claim

- It does not claim that a smaller surface is always safer in
  operation.
- It does not claim that every omitted feature is a defect of other
  designs.
- It does not claim that independent verification is costless, or
  that users will run full nodes.
- It does not add a new template, opcode, or asset rule.

**The protocol buys a smaller, checkable monetary surface by omitting
a general execution platform.** Chapter 12 states how that surface is
maintained after deployment.

# RIGOR Protocol Identity

**Task:** `TASK-RIGOR-FORMAL-VERIFICATION-DESIGN-P0-009`  
**Date:** 2026-08-22  
**Type:** Public protocol identity  
**Code changes:** **NONE**  
**Consensus changes:** **NONE**

```
equivalence_claimed = false
documentation_consensus_authority = false
refinement_proved = false
```

This is one of **three public core whitepapers**. It answers: **What is RIGOR?**

```
RIGOR is a dual-asset UTXO protocol built on script-derived asset
classification, stateless monetary architecture, and formally verified
consensus invariants.
```

“Formally verified consensus invariants” means properties of the
**protocol model**. It does not mean C++ equivalence, production-binary
proof, or complete system verification.

```
equivalence_claimed = false
```

The other two public cores:

| Paper | Question |
|-------|----------|
| [`RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md`](./RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md) | How does the mathematics run? |
| [`RIGOR_FORMAL_VERIFICATION_DESIGN.md`](./RIGOR_FORMAL_VERIFICATION_DESIGN.md) | Why is it trustworthy? |

Explorer is an independent product:
[`../explorer/EXPLORER_ARCHITECTURE.md`](../explorer/EXPLORER_ARCHITECTURE.md).

---

## Protocol identity

```
RIGOR =
    Dual Native Assets
  +
    UTXO Monetary Model
  +
    Deterministic Issuance
  +
    Deflationary Mechanisms
  +
    Probabilistic Selection
  +
    Adaptive Proof-of-Work
  +
    Cryptographic Ownership
```

---

## Dual native assets

| Asset | Role |
|-------|------|
| **ORU** | Base transaction asset, fees, mining subsidy, Convergence commitment cost |
| **RES** | Reserve asset; deterministic issuance after commitment selection — not a PoW reward |

There are exactly two native assets. There is no explorer asset, no
indexed token, and no third consensus money.

---

## No global monetary state

Monetary architecture is **stateless** in the account-ledger sense:

- no global asset table
- no account-balance consensus object
- no explorer database as ledger of record
- fee burn is not a UTXO and not a wallet balance

Consensus state is the UTXO set plus validation rules. Rebuildable
indexes do not hold monetary truth.

```
Consensus State ≠ UTXO Set + Explorer Database + Global Asset Table
```

---

## Script-derived asset classification

```
CTxOut = (value, scriptPubKey)

AssetClass = f(scriptPubKey)
```

Script is the **compliance / classification layer**. Classification is
not issuance authority. Script alone does not create ORU or RES.

There is no `(asset, value, script)` storage field.

---

## UTXO architecture

Spendable ownership is UTXO transitions: consume old outputs, create new
ones. Ordinary transfers cannot mint. Authorized mint paths are explicit
consensus exceptions (ORU coinbase subsidy; RES mint after selection).

**Consensus defines reality.** Explorer displays consensus-derived data.

---

## Proof-of-Work

Blocks enter history only if Proof-of-Work is valid. The mix-hash must
satisfy `pow ≤ target`. Difficulty follows ASERT. PoW orders blocks and
issues ORU subsidy. PoW does not mint RES.

---

## Convergence

Live Convergence is `OP_CONVERGE`: ORU destruction and draw entry.
Selection is an integer rule over a committed candidate set. A legal
winner authorizes Deterministic Reserve Issuance. Witness v1 `CVG2` is
Historical / retired design.

Equations live in the mathematics paper. Invariants and proof boundaries
live in the verification paper.

---

## Observer architecture

Explorer and indexer are observation systems. Their databases are
rebuildable. Deleting them does not change the UTXO set, ORU, RES, or
blocks.

```
database_reset ≠ monetary_reset
```

---

## Security model

| Layer | Role |
|-------|------|
| UTXO conservation | Ordinary transfers cannot create ORU or RES |
| Script validation | Closed monetary script families; classification ≠ issuance |
| Proof-of-Work | OpuHash with ASERT difficulty |
| Ed25519 | Cryptographic ownership of spendable outputs |
| Formal protocol properties | Model invariants — not C++ refinement |

---

## Final identity

```
RIGOR Protocol Reality
=
Consensus Validation
+ UTXO Ledger
+ Script Rules
+ Monetary Rules
+ PoW Rules
```

```
equivalence_claimed = false
```

**Consensus impact: NONE**

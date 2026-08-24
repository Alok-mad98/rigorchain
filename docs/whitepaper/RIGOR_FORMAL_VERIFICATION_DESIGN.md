# RIGOR Formal Verification Design

**Task:** `TASK-RIGOR-FORMAL-VERIFICATION-DESIGN-P0-009`  
**Date:** 2026-08-22  
**Type:** Formal verification architecture documentation  
**Scope:** Whitepaper / protocol verification / mathematical security documentation  
**Code changes:** **NONE**  
**Consensus changes:** **NONE**

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
mathematical_documentation_is_code_proof = false
formal_verification_is_protocol_property_proof = true
```

This is the **unique public document** for “Why is RIGOR trustworthy?”

It explains the state model, invariants, theorem targets, proof
boundary, and what is **not** proved.

It is independent of the identity paper and of the mathematical design
paper. It does not replace either.

```
RIGOR_PROTOCOL_IDENTITY.md
        |
        |  What is RIGOR?
        v
RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md
        |
        |  How does the mathematics run?
        v
RIGOR_FORMAL_VERIFICATION_DESIGN.md
        |
        |  Why is it trustworthy?
```

---

## Opening statement

```
Consensus defines reality.
Explorer displays reality.

Formal verification defines mathematical properties
of the protocol model.

It does not automatically prove implementation equivalence.
```

```
Formal Protocol Verification
≠
C++ Implementation Equivalence
≠
Production Binary Proof
≠
Complete System Verification
```

```
equivalence_claimed = false
refinement_proved = false
```

This paper defines **verification targets** and **verification
boundaries**. It does not invent completed proofs. Theorem inventories
and operator gates live in [`../verification/`](../verification/).

---

## 1. Formal verification identity

RIGOR uses formal methods to describe protocol mathematics as an
explicit model: state, transitions, and invariants that can be stated
without referring to a website, an explorer database, or a production
binary.

What the model is for:

| Surface | Role in the model |
|---------|-------------------|
| Protocol state | What consensus is allowed to remember |
| State transitions | Which block/transaction steps are legal |
| Monetary rules | Issuance, destruction, conservation |
| Asset conservation | Unauthorized creation is rejected |
| Issuance constraints | Amounts are functions of consensus inputs |
| Selection properties | Eligibility and winner order are deterministic |
| Security invariants | Invalid history cannot be accepted |

```
RIGOR Formal Verification Model

=
State Model
+
Transition Rules
+
Monetary Invariants
+
Selection Properties
+
Security Properties
```

```
formal_verification_is_protocol_property_proof = true
```

That flag means: formal verification, in RIGOR, is proof (or attempted
proof) of **properties of the protocol model**. It does not mean the
entire deployed system is proved. It does not mean C++ is a refinement
of the model.

---

## 2. Formal state model

Consensus state is the UTXO set together with the validation rules that
accept or reject a candidate transition.

```
Consensus State = UTXO Set + Validation Rules
```

Not:

```
Consensus State =
    UTXO Set
  + Explorer Database
  + Global Asset Table
```

RIGOR does not rely on a global account-style asset ledger. Spendable
ownership is represented through UTXO transitions. Script rules determine
classification and authorization boundaries. Explorer indexes and
rebuildable databases are observation, not consensus state.

```
CTxOut = (value, scriptPubKey)

AssetClass = f(scriptPubKey)
```

There is no consensus field `(asset, value, script)`. Asset class is
derived by parsing `scriptPubKey`. Classification is not issuance
authority: recognizing a script family does not create ORU or RES.

```
classification ≠ issuance authority
```

Authorized issuance is a separate, constrained transition (ORU subsidy
on a valid coinbase; RES mint only on the authorized mint path). Ordinary
spends move existing outputs. `OP_CONVERGE` outputs are not inserted
into the UTXO set.

---

## 3. Core formal properties

These are the major properties **suitable for verification**. They are
targets. Stating them here does not claim each one is already proved of
C++, of a production binary, or of the complete system.

Equations match
[`RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md`](./RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md).

---

### 3.1 UTXO conservation property

Ordinary transfers:

```
oru_in ≥ oru_out

res_in ≥ res_out
```

Verification goal:

```
Unauthorized asset creation is impossible
```

Ordinary transfers preserve conservation. Only authorized mint paths
may create RES. ORU subsidy is the authorized coinbase issuance
function `R(h)`, not an arbitrary credit. Fee-burn never becomes a
UTXO.

---

### 3.2 Asset classification property

```
CTxOut ≠ (asset, value, script)
```

Asset class is derived from script. Script classification does not
itself create assets. No hidden asset-state table exists as consensus
truth.

Verification goal: every accepted output is classified by
`f(scriptPubKey)` alone. A model that required a parallel global asset
map would not be this protocol.

---

### 3.3 Deterministic issuance property

ORU:

```
R(h)
```

RES:

```
W(k)
```

with `k = ⌊h / 210000⌋`.

Verification goal:

```
Same consensus state
+
Same height
+
Same parameters

=

Same issuance result
```

Issuance is a function of consensus inputs. It is not a miner-chosen
amount, not an explorer balance, and not a floating-point policy.

---

### 3.4 RES supply bound property

```
S_RES(h) ≤ 21_000_000 RES
```

Verification goal: the issuance schedule cannot exceed the defined
maximum supply. A non-zero mint occurs only if authorization and
`W(h) > 0` both hold. Amount, if minted, is exactly `W(h)`.

There is no corresponding absolute ORU cap. `MAX_MONEY` is a per-value
sanity bound, not an ORU supply ceiling.

---

### 3.5 Convergence selection determinism

Eligibility:

```
σ_i < T · f_i
```

Winner comparator (the only legal consensus form):

```
i ≺ j   iff   H_i · f_j  <  H_j · f_i
```

Verification goals:

- deterministic winner selection
- no floating-point dependency
- deterministic tie resolution (`txid_i < txid_j`, unsigned byte-wise)
- identical inputs produce identical result

`T` is the frozen windowed probability target. `T` is not the ASERT /
`nBits` Proof-of-Work target.

---

### 3.6 Proof-of-Work acceptance property

```
pow ≤ target
```

Verification goal: invalid Proof-of-Work states cannot enter consensus
history. Header acceptance is a predicate on the mix-hash and the
ASERT target, not on explorer metadata.

---

## 4. Monetary safety invariants

| Invariant | Statement |
|-----------|-----------|
| **Supply safety** | Unauthorized mint = impossible |
| **Conservation safety** | Valid transition preserves monetary rules |
| **Determinism** | Same input state produces same validation result |
| **Rejection safety** | Invalid transition = rejected |

These invariants are why a closed UTXO model plus deterministic
functions is amenable to formal reasoning. They are protocol-property
targets, not a claim that wallets, P2P, or miners are proved.

---

## 5. Verification boundary

Formal verification does **not** automatically prove:

- C++ implementation equivalence
- production binary equivalence
- compiler correctness
- wallet implementation correctness
- P2P implementation correctness
- miner implementation correctness
- complete blockchain system correctness

```
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
equivalence_claimed = false
mathematical_documentation_is_code_proof = false
```

A proved property of the **model** is a proved property of the model.
Correspondence to C++ is a separate, currently unproved obligation.
Operator status of that boundary:
[`../verification/FORMAL-VERIFICATION-VERDICT.md`](../verification/FORMAL-VERIFICATION-VERDICT.md).

---

## 6. Relationship between model and implementation

Correct relationship:

```
Formal Model
        |
        v
Protocol Specification Reference
        |
        v
Implementation
```

Incorrect claim:

```
Formal Model = C++ Code
```

Forbidden statements:

- Whitepaper proves C++
- Formal model guarantees implementation correctness
- Mathematical documentation is a code proof

The mathematical design paper extracts consensus equations. This paper
states which of those equations are verification targets and where the
proof stops. Neither paper is the C++ node.

---

## 7. Verification as security moat

Formal verification is not merely testing. Testing samples behaviors.
Formal protocol-property verification makes critical invariants
explicit and subjects them to machine-checked reasoning **about the
model**.

```
RIGOR trust architecture

=
Dual Native Asset Design
+
UTXO Local State Model
+
Deterministic Monetary Functions
+
Formal Protocol Property Verification
```

That combination is a **protocol trust layer**:

- two native assets with closed script families
- local UTXO state instead of a global account ledger
- issuance and selection as deterministic functions
- invariants written so they can be modeled

The trust layer is the *existence and explicitness* of those properties.
It is not a certificate that the production binary matches the model.

---

## 8. Relationship with existing documents

| Document | Purpose |
|----------|---------|
| [`RIGOR_PROTOCOL_IDENTITY.md`](./RIGOR_PROTOCOL_IDENTITY.md) | Project identity — what RIGOR is |
| [`RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md`](./RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md) | Mathematical extraction — equations and consensus functions |
| [`../architecture/RIGOR_EXECUTION_MODEL.md`](../architecture/RIGOR_EXECUTION_MODEL.md) | Runtime and execution boundaries |
| [`../verification/FORMAL-VERIFICATION-VERDICT.md`](../verification/FORMAL-VERIFICATION-VERDICT.md) | What is (and is not) currently proved in the Coq model |
| [`../explorer/EXPLORER_ARCHITECTURE.md`](../explorer/EXPLORER_ARCHITECTURE.md) | Explorer implementation — observation, not consensus |

Explorer remains an independent product. Formal verification does not
make an indexer into a ledger of record.

---

## 9. Final identity statement

```
RIGOR Security Identity

=
Mathematical Consensus Design
+
Formal Protocol Properties
+
Explicit Verification Boundaries
```

```
equivalence_claimed = false

hash_kernel_shared_binary_with_cpp = false

refinement_proved = false

mathematical_documentation_is_code_proof = false

formal_verification_is_protocol_property_proof = true
```

**Consensus impact: NONE**

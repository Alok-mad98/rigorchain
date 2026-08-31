# SPEC-004: RIGOR Trust Anchors

**Task:** `TASK-SPEC-005`  
**Track:** Formal Specification Program  
**Status:** ESTABLISHED  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Parents:** [SPEC-000](./SPEC-000-CONSENSUS-SPECIFICATION.md), [SPEC-001](./SPEC-001-STATE-MODEL.md), [SPEC-002](./SPEC-002-ECONOMIC-MODEL.md), [SPEC-003](./SPEC-003-CONSENSUS-RULES.md)  
**Normative sources:** `RFC-000`, `RFC-005`, `RFC-008`, `RFC-014`, `RFC-015`  
**Operational closure process:** [PROOF-010](../protocol/proof/PROOF-010-AUDIT-ITEM-CLOSURE-FRAMEWORK.md)  
**Freeze authority:** `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` / `TASK-MAINNET-FREEZE-001`

---

## Document Role

This document catalogs **trust anchors**: external dependencies that RIGOR relies
upon but cannot fully verify within the current formal program.

Trust anchors are declared explicitly so that:

- Proofs and alignment claims state their assumptions
- Equivalence is not silently claimed over unverified components
- Open audit items have a single registry for resolution tracking

This document is specification-only:

- No code changes
- No RPC changes
- No consensus changes
- No proofs
- **Equivalence Claimed:** false

---

## 1. Purpose

### 1.1 What is a trust anchor?

A **trust anchor** is any component, environment, or assumption that:

1. Is **required** for correct consensus validation, economic accounting, or
   formal-runtime alignment; and
2. Is **not mechanically proven** equivalent to the specification within the
   current Coq / verification scope.

Trust anchors are not specification defects. They are **declared boundaries**
between what the spec defines and what the formal program must assume or validate
by external evidence (golden vectors, cross-implementation tests, operational
audits).

### 1.2 Role in RIGOR formal verification

The formal program hierarchy is:

```
Specification  →  Formal Model  →  Proof  →  Runtime Alignment Evidence
```

Trust anchors sit **below** proofs and **beside** runtime alignment:

- **Consensus correctness** (given identical block history) requires only
  **deterministic evaluation** of frozen predicates — not collision resistance
  for liveness, not network synchrony for validity.
- **Economic security** and **alignment confidence** require stronger anchors
  (hash collision resistance, vector-backed primitive correctness, etc.).
- **Equivalence claims** are prohibited unless every relied-upon anchor is either
  proven or explicitly listed here.

Silent reliance on an unlisted anchor is a specification violation.

---

## 2. Hashing Oracle

### 2.1 Anchor definition

**TA-HASH:** The hashing oracle provides:

```
Hash256(x) = SHA256(SHA256(x))
```

for all byte strings `x` used in consensus (PoW, txid, convergence root, σ,
parent block hash, merkle commitments).

### 2.2 Assumed properties

| Property | Required for | Mechanically verified? |
|----------|--------------|----------------------|
| **Determinism** | Consensus correctness | Partially (vectors; not FIPS proof) |
| **Collision resistance** | Economic security | **No** (assumed) |
| **Preimage resistance** | PoW soundness arguments | **No** (assumed) |
| **Domain separation discipline** | σ vs asset domains | Spec-defined (RFC-008) |

### 2.3 Verification status

The Coq `RigorSha256` model is an **implementation**, not a proof of equivalence
to FIPS 180-4. Current authority chain:

```
FIPS 180-4 (external)
    → runtime / reference library (trusted operationally)
    → golden vectors (regression anchor)
    → Coq model (structural alignment target)
```

Golden vectors are the operative trust anchor for byte-level hash agreement
across layers until a FIPS-equivalence proof exists (open item **A-4**).

### 2.4 Failure conditions

| Failure | Effect |
|---------|--------|
| **Non-determinism** | Two correct validators disagree on `Hash256(x)` for the same `x` → consensus break |
| **Collision** | Distinct inputs `x ≠ y` with `Hash256(x) = Hash256(y)` → potential fork / identity confusion (security break) |
| **Layer mismatch** | Coq, reference oracle, and runtime disagree on hash output for vector input → alignment failure (not a spec change) |

Consensus validity for a **fixed block sequence** does not require collision
resistance; it requires deterministic evaluation only.

---

## 3. Proof-of-Work Oracle

### 3.1 Anchor definition

**TA-POW:** Miners produce block headers. Validators treat PoW as an oracle that
answers whether:

```
OpuHash(SHA256d(header)) < ExpandCompact(nBits)
```

and whether `nBits` is the correct ASERT output from frozen difficulty rules.

### 3.2 Assumed properties

| Property | Scope |
|----------|-------|
| **Work measurement** | Cumulative chainwork increases along the active chain |
| **ASERT correctness** | Difficulty adjustment matches frozen integer ASERT semantics |
| **Separation (RFC-014)** | PoW target does **not** define RES draw inequality RHS |
| **Integer comparison** | No floating-point on consensus paths |

### 3.3 Trust boundary

PoW validation rules are specified in [SPEC-003](./SPEC-003-CONSENSUS-RULES.md)
(CR-003, CR-004). The trust anchor is that:

- The **ExpandCompact** mapping and ASERT recurrence are implemented identically
  across validators (determinism), and
- Sufficient work is **actually required** for acceptance (economic anchor).

### 3.4 Failure conditions

| Failure | Effect |
|---------|--------|
| **Insufficient difficulty accepted** | Invalid block accepted → chain integrity break |
| **ASERT miscalculation** | Divergent `nBits` across validators → consensus fork |
| **PoW target used as draw RHS** | Variable-T / dual-T violation → economic and consensus spec violation |

---

## 4. System Clocks

### 4.1 Anchor definition

**TA-CLOCK:** Header timestamp validation uses **chain-derived time** (median-time-past
and related frozen predicates), not any single node's wall clock, as the consensus
time reference.

Wall clocks on individual nodes are external and non-authoritative.

### 4.2 Allowable drift

Let **δ** denote the frozen maximum forward timestamp allowance relative to the
validator's network-adjusted clock. Under the ratified validation state machine
(RFC-005), block header time MUST satisfy:

- `time > median-time-past` of prior blocks (chain rule)
- `time ≤ network_adjusted_now + δ` (forward bound; frozen constant)

**δ is a protocol constant**, not an operator-tunable parameter. Its exact value
is defined by the frozen RFC bundle (RFC-005: forward bound of two hours).

### 4.3 Consensus vs non-consensus use

| Use | Authority |
|-----|-----------|
| Header timestamp predicates (CR-005) | Consensus — uses chain time + δ |
| Logging, RPC timeouts, peer eviction | Non-consensus — local clock |

### 4.4 Failure conditions

| Failure | Effect |
|---------|--------|
| **Timestamp outside allowable drift** | Block correctly rejected (expected) |
| **Local clock used as consensus input** | Non-deterministic acceptance → spec violation |
| **MTP miscalculation** | Divergent validity across validators → consensus fork |

---

## 5. Network Model

### 5.1 Anchor definition

**TA-NET:** The network is modeled as **semi-synchronous**: messages and blocks
propagate with bounded delay, but validators do not share a global synchronous clock.

### 5.2 Assumed properties

| Property | Role |
|----------|------|
| **Eventual delivery** | Honest nodes eventually receive the same block history for the active chain (liveness) |
| **Non-authority of relay** | Peer tables, ban scores, and mempool ordering are **not** consensus inputs |
| **Partition handling** | During partition, nodes may diverge on tip; consensus resolves by chainwork on reunification |

### 5.3 Consensus vs liveness

**Critical distinction:**

- **Validity** is defined over `(State(h), Block(h+1))` only — network is not a
  consensus input given the same block sequence.
- **Liveness** (chain progress, convergence of tips) assumes eventual delivery.

### 5.4 Failure conditions

| Failure | Effect |
|---------|--------|
| **Permanent partition** | Tips diverge indefinitely → liveness failure (not a validity spec violation) |
| **Relay order affects validity** | Mempool or peer order changes accept/reject → spec violation (CR-050) |
| **Network assumptions used in proofs without declaration** | Hidden trust anchor → formal program defect |

---

## 6. Storage and External State

### 6.1 Anchor definition

**TA-STORAGE:** Validators persist blocks, chain index, and UTXO state on disk.
Runtime mirrors and audit artifacts may also be persisted or cached.

### 6.2 Authoritative vs non-authoritative persistence

| Stored artifact | Authority | Corruption effect |
|-----------------|-----------|-------------------|
| **Committed blocks + UTXO** | Consensus-critical | Replay must detect inconsistency; must not silently change validity |
| **Block index / chainwork** | Consensus-critical | Wrong tip selection if corrupted |
| **Runtime supply mirrors** | **Non-authoritative** | Audit export wrong; MUST NOT affect validation |
| **Witness chains / Phase-5 catalogs** | **Non-authoritative** | Alignment evidence wrong; MUST NOT affect validation |
| **Mempool** | Non-consensus | No effect on `State(h)` definition |

### 6.3 Integrity assumptions

For correct operation, validators assume:

- Disk reads return data previously written (or detectable failure)
- Block bodies match committed headers (merkle root)
- Replay from genesis reconstructs identical `State(h)` when storage is intact

### 6.4 Failure conditions

| Failure | Effect |
|---------|--------|
| **Missing authoritative block data** | Cannot validate or replay → node halt (operational) |
| **Corrupted UTXO / index** | Replay divergence or rejection → must not produce silent wrong acceptance |
| **Corrupted supply mirror treated as authoritative** | Spec violation if validation reads mirror |
| **Mempool loss** | No consensus effect |

---

## 7. Eligibility and Draw Context

### 7.1 Anchor definition

**TA-DRAW:** The RES convergence draw is **fully deterministic** from consensus
inputs. No external randomness oracle participates.

Draw outputs are computed from:

- Candidate set `F` (convergence-ticket transactions in the parent window)
- Convergence root `C` committed by the parent coinbase
- Parent block hash `B_{h-1}` as the σ seed (Sigma-PoW-01; historical `bind_commit` superseded)
- Per-candidate σ digests and eligibility inequality
- Cross-multiply winner selection with txid tie-break

See [SPEC-003](./SPEC-003-CONSENSUS-RULES.md) CR-040..CR-046.

### 7.2 External randomness

**There is no external randomness assumption for consensus correctness.**

`H_i = Hash256(σ_i preimage)` is pseudorandom for **economic** arguments only.
Consensus requires:

- Identical inputs → identical `H_i`, eligibility, and winner
- At most one winner (CR-042) and at most one mint (CR-043)

### 7.3 Sub-anchors within draw context

| Sub-anchor | Issue | Open item |
|------------|-------|-----------|
| **σ → integer byte order** | Endianness for inequality comparison | **A-1** (HIGH) |
| **uint512 cross-multiply** | Overflow bound for `H_i · f_j` | **A-2** (MEDIUM) |
| **F membership predicate** | Candidate set enumeration equivalence | **A-3** (HIGH) |
| **Hash primitive** | SHA-256d correctness | **A-4** (MEDIUM) |

### 7.4 Failure conditions

| Failure | Effect |
|---------|--------|
| **Non-deterministic winner** | Two validators select different winners for same block → consensus fork |
| **Multiple winners** | Violates CR-042 → block invalid |
| **Eligibility uses PoW-derived RHS** | Violates CR-046 and RFC-014 → spec violation |
| **F set mismatch across layers** | Alignment failure; potential consensus risk if runtime differs (A-3) |

---

## 8. Open Audit Items

The following items are **documented assumptions** pending formal closure.
They MUST be cited in any proof or equivalence claim that depends on them.

### 8.1 Registry (A-1 … A-5)

| ID | Ambiguity | Layers in tension | Risk | Resolution needed | Status |
|----|-----------|-------------------|------|-------------------|--------|
| **A-1** | σ digest → integer **byte order** for inequality comparison | RFC vs reference oracle vs formal model | **HIGH** | Pin single normative endianness statement; prove cross-layer refinement | **`AI_PARTIAL`** — [PROOF-011](../protocol/proof/PROOF-011-AUDIT-A1-CLOSURE.md); PD-15 pending |
| **A-2** | `uint512` cross-multiply semantics; overflow bound (`σ < 2^256`, `f < 2^64` ⇒ product `< 2^512`) | Runtime vs Coq vs reference oracle | **MEDIUM** | Document and prove magnitude bound as explicit lemma | **`AI_PARTIAL`** — [PROOF-013](../protocol/proof/PROOF-013-A2-A3-RUNTIME-OBSERVATION.md); PD-14 pending |
| **A-3** | `F` membership predicate equivalence (mint-classified candidate enumeration) | Runtime vs formal model vs reference oracle | **HIGH** | Cross-check candidate set construction across layers | **`AI_PARTIAL`** — [PROOF-013](../protocol/proof/PROOF-013-A2-A3-RUNTIME-OBSERVATION.md); PD-13 pending |
| **A-4** | SHA-256d correctness vs FIPS 180-4 | FIPS vs Coq vs golden vectors | **MEDIUM** | Prove FIPS-equivalence or designate vectors as operative anchor | **`AI_PARTIAL`** — [PROOF-014](../protocol/proof/PROOF-014-A4-A5-RUNTIME-OBSERVATION.md); PD-11 pending |
| **A-5** | Canonical formal-program status documentation | Documentation internal | **LOW** | Designate one living status document; mark others historical | **`AI_PARTIAL`** — [PROOF-014](../protocol/proof/PROOF-014-A4-A5-RUNTIME-OBSERVATION.md); historical-doc retirement pending |

### 8.2 Additional declared anchors (from SPEC layer)

| ID | Anchor | Source document | Status |
|----|--------|-----------------|--------|
| **A-ORDER** | Fixed validation phase ordering | SPEC-001 §7, SPEC-003 §2 | Specified; proof target |
| **A-SCHEDULE** | ORU subsidy schedule `R(h)` as frozen height function | SPEC-002 §4.4 | Specified; schedule in ORU RFC bundle |
| **A-VECTORS** | Golden vectors as byte-regression oracle | Formal truth source audit | Declared; not consensus authority |
| **A-FVR** | Runtime mirrors/witnesses are non-authoritative | SPEC-001 §4, SPEC-003 §12 | **CLOSED** (spec-boundary) |

### 8.3 Resolution rule

An open item transitions to **CLOSED** only when:

1. A spec predicate pins the ambiguity (for A-1, A-2, A-3), **and**
2. A formal proof or vector-backed lemma demonstrates cross-layer agreement, **or**
3. The item is explicitly scoped out with documented non-reliance.

Until closure, dependent equivalence claims MUST remain `false`.

**Operational process:** lifecycle states (`AI_OPEN` … `AI_CLOSED`), artifact
categories, review requirements, and proof-obligation mapping are defined in
[PROOF-010: Audit Item Closure Framework](../protocol/proof/PROOF-010-AUDIT-ITEM-CLOSURE-FRAMEWORK.md).
This section remains the normative registry; PROOF-010 operationalizes closure
without modifying trust-anchor semantics.

---

## 9. Trust Anchor Summary Table

| Anchor ID | Name | Consensus-critical? | Proven? | Primary failure mode |
|-----------|------|---------------------|---------|----------------------|
| TA-HASH | Hashing oracle | Determinism: yes; collision resistance: security only | Partial (vectors) | Non-determinism / collision |
| TA-POW | Proof-of-work oracle | Yes | Spec-defined; alignment partial | Insufficient work accepted |
| TA-CLOCK | System clocks | Yes (header rules only) | Spec-defined (δ, MTP) | Timestamp rule violation |
| TA-NET | Network model | No (validity); yes (liveness) | Assumed | Partition / relay-order leak |
| TA-STORAGE | Storage integrity | Operational | Assumed | Corrupted authoritative data |
| TA-DRAW | Draw determinism | Yes | Spec-defined; sub-items open | Non-deterministic winner |

---

## 10. Relationship to Formal Verification

```
Trust Anchors (this document)
        │  declares: unproven assumptions and external dependencies
        ▼
Specification (SPEC-000 … SPEC-003)
        │  defines: what SHOULD hold given anchors
        ▼
Formal Model + Proofs
        │  must: cite anchors used; close open items where claimed
        ▼
Runtime Alignment (FVR, Phase 5)
           evidence only; equivalence_claimed = false
```

Proofs MUST NOT silently eliminate a trust anchor. Any theorem that requires
TA-HASH collision resistance, TA-NET eventual delivery, or an open audit item
MUST state the dependency explicitly.

---

## 11. Freeze Compatibility

This document is **specification only**. It documents existing dependencies and
open assumptions; it does not modify consensus, monetary policy, or protocol freeze.

| Guarantee | Status |
|-----------|--------|
| No consensus modifications | **Confirmed** |
| No monetary-policy modifications | **Confirmed** |
| No protocol-freeze modifications | **Confirmed** |

Nothing in this document supersedes `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` or
`TASK-MAINNET-FREEZE-001`.

---

## 12. Task Output Status

| Field | Status |
|-------|--------|
| Trust Anchor Specification | **ESTABLISHED** |
| SHA-256 / Hashing Oracle | **ESTABLISHED** |
| PoW Oracle | **ESTABLISHED** |
| System Clock | **ESTABLISHED** |
| Network Model | **ESTABLISHED** |
| Storage & External State | **ESTABLISHED** |
| Eligibility / Draw Context | **ESTABLISHED** |
| Open Audit Items | **DOCUMENTED** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## 13. Revision History

| Version | Date | Change |
|---------|------|--------|
| 0.1 | 2026-06-05 | `TASK-SPEC-005` — initial trust anchor specification |
| 0.2 | 2026-06-05 | Cross-reference PROOF-010 operational audit closure process (TASK-PROOF-011) |
| 0.3 | 2026-06-05 | A-1 status AI_PARTIAL per PROOF-011 closure procedure (TASK-PROOF-012) |
| 0.4 | 2026-06-05 | A-2 / A-3 AI_PARTIAL runtime witness per PROOF-013 (TASK-PROOF-014) |
| 0.5 | 2026-06-05 | A-4 / A-5 AI_PARTIAL runtime witness per PROOF-014 (TASK-PROOF-015) |

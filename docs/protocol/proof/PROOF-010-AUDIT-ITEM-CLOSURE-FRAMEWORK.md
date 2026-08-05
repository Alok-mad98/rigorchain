# PROOF-010: Audit Item Closure Framework

**Task:** `TASK-PROOF-011`  
**Track:** Formal Verification Program — Proof Infrastructure Layer  
**Status:** ESTABLISHED  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Parents:** [SPEC-004](../../specification/SPEC-004-TRUST-ANCHORS.md), [PROOF-009](./PROOF-009-PROOF-OBLIGATION-REGISTRY.md)  
**Item closures:** [PROOF-011](./PROOF-011-AUDIT-A1-CLOSURE.md) (A-1); runtime witnesses [PROOF-012](./PROOF-012-A1-RUNTIME-OBSERVATION.md) (A-1), [PROOF-013](./PROOF-013-A2-A3-RUNTIME-OBSERVATION.md) (A-2 / A-3), [PROOF-014](./PROOF-014-A4-A5-RUNTIME-OBSERVATION.md) (A-4 / A-5), [PROOF-015](./PROOF-015-A1-A5-BATCH-RUNTIME-OBSERVATION.md) (A-1..A-5 batch)  
**Freeze authority:** `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` / `TASK-MAINNET-FREEZE-001`

---

## Document Role

This document defines the **authoritative process** for closing Trust Anchor audit
items (SPEC-004 §8) and relates that process to proof obligations
([PROOF-009](./PROOF-009-PROOF-OBLIGATION-REGISTRY.md)).

This task is **documentation only**. No consensus, validation, FVR, Coq, or golden
vector changes.

**Equivalence Claimed:** false

---

## §1 Purpose

### 1.1 Concept distinction

| Concept | Definition | Closes what? |
|---------|------------|--------------|
| **Trust Anchor Audit Item** | Documented ambiguity or external dependency (A-1 … A-5) registered in SPEC-004 | Cross-layer alignment assumption |
| **Proof Obligation** | Declared theorem debt (PD-4F-OB-02, etc.) in PROOF-009 | Scoped formal proof target |
| **Runtime Evidence** | Observational FVR export / RPC witness (read-only) | Nothing alone |
| **Formal Proof** | Machine-checked Coq theorem (or designated proof artifact) | Component of closure only |
| **Closure Artifact** | Published bundle satisfying §3 for one audit item | Audit item → `AI_CLOSED` |

### 1.2 What closure is not

Closing an audit item is **not** equivalent to proving global equivalence.

| Claim | Status |
|-------|--------|
| Coq ≡ Runtime (global) | **NOT ESTABLISHED** |
| `equivalence_claimed` | **false** |
| Audit item `AI_CLOSED` | Does not imply `PO_PROVEN` on any obligation |
| Golden vector `GVR_PASS` | Does not imply `AI_CLOSED` |

Audit closure removes a **documented assumption** from the open registry. Proof
obligation closure requires a separate discharge path (PROOF-009 §2).

---

## §2 Audit Item Lifecycle

### 2.1 State codes

| Code | Meaning |
|------|---------|
| `AI_OPEN` | Registered in SPEC-004 §8; no resolution artifacts |
| `AI_PARTIAL` | Some artifact categories present (§3); closure incomplete |
| `AI_READY` | All required artifacts present; awaiting formal review sign-off |
| `AI_CLOSED` | Review complete; item removed from open-assumption set |
| `AI_RETIRED` | Item superseded or scoped out with documented non-reliance |

### 2.2 Transition rules

```
AI_OPEN
   ├─→ AI_PARTIAL     (≥1 closure category started; not all complete)
   ├─→ AI_RETIRED     (explicit scope-out approved; documented non-reliance)
   │
AI_PARTIAL
   ├─→ AI_READY       (all §3 categories satisfied for the item)
   ├─→ AI_OPEN        (regression: required artifact invalidated)
   ├─→ AI_RETIRED
   │
AI_READY
   ├─→ AI_CLOSED      (review sign-off recorded in closure artifact)
   ├─→ AI_PARTIAL     (review finds gap)
   │
AI_CLOSED
   └─→ AI_PARTIAL     (only on demonstrated regression with new evidence)

AI_RETIRED  (terminal)
```

**Rules:**

1. `AI_OPEN` → `AI_CLOSED` is **prohibited** (must pass through `AI_PARTIAL` and `AI_READY`).
2. `AI_CLOSED` requires a **closure artifact** (§3) with review notes and version stamp.
3. Regression from `AI_CLOSED` reopens at `AI_PARTIAL` and must cite the invalidating change.
4. `AI_RETIRED` requires SPEC-004 registry update and proof-program acknowledgment;
   dependent theorems must cite non-reliance explicitly.

---

## §3 Closure Requirements

An audit item becomes `AI_CLOSED` only when **all** required artifact categories
exist for that item.

### 3.1 Artifact categories

| Category | Requirement |
|----------|-------------|
| **Specification** | Normative predicate pins the ambiguity (SPEC-004 §8.3 rule 1) |
| **Formal Model** | RFC / `docs/formal/` encoding matches the pinned predicate |
| **Coq Artifact** | Lemma or audit showing cross-layer agreement (or explicit assumption record) |
| **Runtime Evidence** | FVR export corroboration where the item touches runtime (optional for doc-only items) |
| **Golden Vector Coverage** | Regression vectors exercising the pinned behaviour (where applicable) |
| **Review Notes** | Human review record: reviewer, date, artifact hashes, residual risk |

### 3.2 Category applicability by item

| Item | Specification | Formal Model | Coq | Runtime | Golden Vector | Review |
|------|:-------------:|:------------:|:---:|:-------:|:-------------:|:------:|
| A-1 | Required | Required | Required | If draw-touched | Required (draw σ) | Required |
| A-2 | Required | Required | Required | If draw-touched | Required (winner) | Required |
| A-3 | Required | Required | Required | Required | Required (F set) | Required |
| A-4 | Required | Required | Required | Optional | Required (hash) | Required |
| A-5 | Required | N/A | N/A | N/A | N/A | Required |

### 3.3 Insufficient evidence (explicit)

The following **alone** do not satisfy any category:

- Single-layer unit test pass without cross-layer lemma
- FVR RPC export with `equivalence_claimed: false`
- Golden vector `GVR_PASS` without spec predicate pin
- Coq `Admitted` placeholder or audit catalog consistency without refinement proof

---

## §4 Current Audit Registry

Authoritative items from [SPEC-004](../../specification/SPEC-004-TRUST-ANCHORS.md) §8.1.
Closure detail per-item: [PROOF-011](./PROOF-011-AUDIT-A1-CLOSURE.md) (A-1).

### A-1 — σ digest byte order

| Field | Value |
|-------|-------|
| **Ambiguity** | Endianness for σ → integer inequality comparison |
| **Layers in tension** | RFC vs reference oracle vs formal model |
| **Risk** | HIGH |
| **Purpose** | Ensure draw eligibility uses identical byte order across layers |
| **Status** | **`AI_PARTIAL`** — see [PROOF-011](./PROOF-011-AUDIT-A1-CLOSURE.md); Coq refinement PD-15 pending |
| **Depends on** | TA-HASH (partial), TA-DRAW |

### A-2 — uint512 cross-multiply bound

| Field | Value |
|-------|-------|
| **Ambiguity** | Overflow semantics for `H_i · f_j` winner selection |
| **Layers in tension** | Runtime vs Coq vs reference oracle |
| **Risk** | MEDIUM |
| **Purpose** | Prove product magnitude bound under declared σ, f ranges |
| **Status** | **`AI_PARTIAL`** — runtime witness [PROOF-013](./PROOF-013-A2-A3-RUNTIME-OBSERVATION.md); PD-14 pending |
| **Depends on** | A-1 (eligibility inputs), TA-DRAW |

### A-3 — F membership predicate

| Field | Value |
|-------|-------|
| **Ambiguity** | Candidate set enumeration equivalence |
| **Layers in tension** | Runtime vs formal model vs reference oracle |
| **Risk** | HIGH |
| **Purpose** | Align mint-classified candidate construction across layers |
| **Status** | **`AI_PARTIAL`** — runtime witness [PROOF-013](./PROOF-013-A2-A3-RUNTIME-OBSERVATION.md); PD-13 pending |
| **Depends on** | TA-DRAW, validation phase ordering (A-ORDER) |
| **Blocks** | Draw-scoped alignment; contributes to supply obligation preconditions |

### A-4 — SHA-256d correctness

| Field | Value |
|-------|-------|
| **Ambiguity** | Hash primitive vs FIPS 180-4 |
| **Layers in tension** | FIPS vs Coq vs golden vectors |
| **Risk** | MEDIUM |
| **Purpose** | Anchor σ, C, and bind_commit digests to verified primitive |
| **Status** | **`AI_PARTIAL`** — runtime witness [PROOF-014](./PROOF-014-A4-A5-RUNTIME-OBSERVATION.md); PD-11 pending |
| **Depends on** | TA-HASH |

### A-5 — Canonical formal-program status documentation

| Field | Value |
|-------|-------|
| **Ambiguity** | Multiple living status documents |
| **Layers in tension** | Documentation internal |
| **Risk** | LOW |
| **Purpose** | Single authoritative program status surface |
| **Status** | **`AI_PARTIAL`** — runtime witness [PROOF-014](./PROOF-014-A4-A5-RUNTIME-OBSERVATION.md); historical-doc retirement pending |
| **Depends on** | None (process item) |

### Dependency sketch (audit items)

```
A-4 (hash primitive)
    └── A-1 (σ endianness)
            └── A-2 (uint512 bound)
A-3 (F membership) ── TA-DRAW

A-5 (documentation) — independent
```

---

## §5 Proof Obligation Registry Mapping

Cross-reference: [PROOF-009](./PROOF-009-PROOF-OBLIGATION-REGISTRY.md).

### 5.1 PD-4F-OB-02 — Runtime Supply Alignment Closure

| Required audit items | Role |
|---------------------|------|
| **A-1** | Draw-adjacent fields in supply closure preconditions |
| **A-2** | Winner path affects declared inputs where supply/draw interact |
| **A-3** | Candidate set alignment for any block with draw participation |

**Rule:** `PD-4F-OB-02` **cannot** transition to `PO_PROVEN` while A-1, A-2, or A-3
remain `AI_OPEN` (or `AI_PARTIAL` / `AI_READY` without `AI_CLOSED`), unless the
closure theorem explicitly cites them as **named assumptions** in the theorem statement
(SPEC-007 §7.3).

### 5.2 PD-4F-CPP-01 — Coq Runtime Correspondence Proof

| Required audit items | Role |
|---------------------|------|
| **A-1 … A-3** | Inherited via dependency on PD-4F-OB-02 |
| **A-4** | Hash-dependent trace components in correspondence path |

**Rule:** `PD-4F-CPP-01` **cannot** reach `PO_PROVEN` before `PD-4F-OB-02` is
`PO_PROVEN` and required audit items are `AI_CLOSED` or explicitly assumed.

### 5.3 Mapping table

| Proof Obligation | Status | Required audit closure |
|------------------|--------|------------------------|
| `PD-4F-OB-02` | `PO_OPEN` | A-1, A-2, A-3 (minimum); A-4 where hash fields compared |
| `PD-4F-CPP-01` | `PO_PARTIAL` | Above + PD-4F-OB-02 proven |
| `DRAW-PROOF-001` | `PO_BLOCKED` | N/A (FVR-M); audit closure does not unblock |

---

## §6 Golden Vector Relationship

Golden vectors provide **comparison evidence** only. They **cannot** close audit
items by themselves (§3.3).

| Document | Role in closure pipeline |
|----------|-------------------------|
| [PROOF-000](./PROOF-000-GOLDEN-VECTORS.md) | Authoritative `GV-*` namespace; vector categories |
| [PROOF-001](./PROOF-001-GOLDEN-ORU-VECTORS.md) | ORU supply regression targets |
| [PROOF-002](./PROOF-002-GOLDEN-RES-VECTORS.md) | RES mint regression targets |
| [PROOF-003](./PROOF-003-GOLDEN-DRAW-VECTORS.md) | Draw classification fixtures (FVR-M bounded) |
| [PROOF-004](./PROOF-004-GOLDEN-STATE-VECTORS.md) | End-to-end state projections |
| [PROOF-005](./PROOF-005-RUNTIME-VECTOR-HARNESS.md) | Harness architecture spec |
| [PROOF-006](./PROOF-006-RUNTIME-VECTOR-HARNESS-IMPLEMENTATION.md) | Executable harness |
| [PROOF-007](./PROOF-007-RUNTIME-VECTOR-MATERIALIZATION.md) | Regtest fixture materialization |
| [PROOF-008](./PROOF-008-RUNTIME-VECTOR-VERIFICATION.md) | End-to-end verification reports |

**Golden Vector Coverage** (§3) is satisfied when:

1. Applicable `GV-*` vectors are `ESTABLISHED` and `GVR_PASS` / `GVR_PARTIAL` per FVR-M, **and**
2. Vectors are cited in the closure artifact as regression evidence, **and**
3. A spec predicate pin exists for the audit item.

`GVR_PASS` on supply vectors does not close A-1..A-4 without the remaining categories.

---

## §7 Runtime Evidence Relationship

Runtime witnesses (FVR-A through FVR-N) may **support** closure artifact assembly.
They **cannot independently** close audit items.

| Layer span | Role in audit closure |
|------------|----------------------|
| **FVR-A … FVR-H** | Supply mirror, hook, invariant, witness, CHAS exports |
| **FVR-I … FVR-L** | Candidate equivalence and closure-gate evidence |
| **FVR-M** | Draw impossibility — supports `DRAW-PROOF-001` blocked status |
| **FVR-N** | Phase-5 summary — corroborates program honesty boundary |

**Rules:**

1. FVR exports are read-only and non-authoritative (SPEC-001, SPEC-003).
2. `equivalence_claimed: false` in any FVR JSON is mandatory and consistent with this framework.
3. Runtime evidence counts toward §3 **only** when paired with spec pin + formal proof/lemma.

---

## §8 Coq Relationship

Phase-5 equivalence catalogs may satisfy **Coq Artifact** requirements in §3 when
they contain machine-checked lemmas — not merely catalog definitions.

| Catalog | Path | Closure role |
|---------|------|--------------|
| **Phase5SummaryCatalog** | `Phase5SummaryCatalog.v` | Program status; not audit closure alone |
| **GlobalCandidateEquivalenceCatalog** | `GlobalCandidateEquivalenceCatalog.v` | Evidence toward A-1..A-3 preconditions |
| **EquivalenceClosureCatalog** | `EquivalenceClosureCatalog.v` | Gate evaluation; not item closure |
| **DrawClosureImpossibilityCatalog** | `DrawClosureImpossibilityCatalog.v` | Supports draw obligation blocked state |

**Rule:** Coq proof alone does not close an audit item. **All** applicable §3
categories must be satisfied, including specification pin and review notes.

`Admitted` placeholders, catalog partition lemmas, and audit consistency theorems
do not discharge A-1..A-4.

---

## §9 Future Closure Pipeline

Planned program phases (all **NOT STARTED** at baseline):

| Phase | Name | Scope | Status |
|-------|------|-------|--------|
| **Phase C** | Audit Item Closure | Close A-1 … A-5 per §2–§3 | **IN PROGRESS** (A-1..A-5 → `AI_PARTIAL`; batch witnessed PROOF-015) |
| **Phase D** | Proof Obligation Closure | Discharge PD-4F-OB-02, PD-4F-CPP-01 per PROOF-009 | **NOT STARTED** |
| **Phase E** | Candidate Equivalence Review | FVR-I/J/K CES review at scale | **NOT STARTED** |
| **Phase F** | Formal Equivalence Assessment | SPEC-007 §9 scoped assessment (supply-only shape) | **NOT STARTED** |

**Ordering constraint:**

```
Phase C  ──→  Phase D  ──→  Phase E  ──→  Phase F
(AI_CLOSED)    (PO_PROVEN)   (CES review)  (scoped claim review only)
```

Phase F does **not** authorize global `equivalence_claimed := true` without
published theorem satisfying SPEC-007 §9 and PROOF-009 closure criteria.

---

## §10 Freeze Compatibility

This document is **proof infrastructure only**.

| Guarantee | Status |
|-----------|--------|
| No consensus impact | **Confirmed** |
| No validation impact | **Confirmed** |
| No state_hash impact | **Confirmed** |
| No monetary impact | **Confirmed** |
| No protocol-freeze impact | **Confirmed** |
| No FVR modifications | **Confirmed** |
| No Coq proof modifications | **Confirmed** |
| No golden vector modifications | **Confirmed** |

Subordinate to `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` and `TASK-MAINNET-FREEZE-001`.

---

## Task Output Status

| Field | Status |
|-------|--------|
| Audit Item Closure Framework | **ESTABLISHED** |
| Audit Lifecycle | **ESTABLISHED** |
| Closure Criteria | **ESTABLISHED** |
| Trust Anchor Closure Process | **ESTABLISHED** |
| Proof Obligation Mapping | **ESTABLISHED** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## Revision History

| Version | Date | Change |
|---------|------|--------|
| 0.1 | 2026-06-05 | `TASK-PROOF-011` — audit item closure framework established |
| 0.2 | 2026-06-05 | A-1 status `AI_PARTIAL` per PROOF-011 (TASK-PROOF-012) |
| 0.3 | 2026-06-05 | A-2 / A-3 `AI_PARTIAL` runtime witness per PROOF-013 (TASK-PROOF-014) |
| 0.4 | 2026-06-05 | A-4 / A-5 `AI_PARTIAL` runtime witness per PROOF-014 (TASK-PROOF-015) |
| 0.5 | 2026-06-05 | A-1..A-5 batch runtime witness per PROOF-015 (TASK-PROOF-016) |

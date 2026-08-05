# PROOF-009: Proof Obligation Registry and Closure Tracking

**Task:** `TASK-PROOF-010`  
**Track:** Formal Verification Program — Proof Infrastructure Layer  
**Status:** ESTABLISHED  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Parents:** [SPEC-005](../../specification/SPEC-005-CONSENSUS-INVARIANTS.md), [SPEC-006](../../specification/SPEC-006-RUNTIME-FORMAL-MAPPING.md), [SPEC-007](../../specification/SPEC-007-EQUIVALENCE-BOUNDARIES.md)  
**Audit closure process:** [PROOF-010](./PROOF-010-AUDIT-ITEM-CLOSURE-FRAMEWORK.md)  
**A-1 closure:** [PROOF-011](./PROOF-011-AUDIT-A1-CLOSURE.md) (`AI_PARTIAL`)  
**A-2 / A-3 runtime witness:** [PROOF-013](./PROOF-013-A2-A3-RUNTIME-OBSERVATION.md) (`AI_PARTIAL`)  
**A-4 / A-5 runtime witness:** [PROOF-014](./PROOF-014-A4-A5-RUNTIME-OBSERVATION.md) (`AI_PARTIAL`)  
**A-1..A-5 batch witness:** [PROOF-015](./PROOF-015-A1-A5-BATCH-RUNTIME-OBSERVATION.md) (TASK-PROOF-016)  
**PD-4F-OB-02 dependency model:** [PROOF-019](./PROOF-019-PD4F-OB02-DEPENDENCY-MODEL.md) (TASK-PROOF-019)  
**Freeze authority:** `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` / `TASK-MAINNET-FREEZE-001`

---

## Document Role

This document is the **authoritative registry** of formal proof obligations
required by the Phase-5 program. It sits above SPEC-005, SPEC-006, and SPEC-007
and below future proof-closure tasks.

This task **does not** attempt to prove any obligation.

**Equivalence Claimed:** false

---

## §1 Purpose

### 1.1 What a proof obligation is

A **proof obligation** is a required future proof target: a statement that must
eventually be machine-checked (or explicitly retired) before a scoped equivalence
claim may be advanced.

| Concept | Role |
|---------|------|
| **Specification** | Normative law (SPEC-000 … SPEC-007) |
| **Formal Model** | Mathematical encoding of spec intent (`docs/formal/`, RFCs) |
| **Proof Obligation** | Declared theorem / alignment debt **not yet closed** |
| **Proven Theorem** | Machine-checked result that discharges an obligation (`PO_PROVEN`) |

### 1.2 What a proof obligation is not

- **Not evidence of correctness** — registering an obligation does not imply the
  property holds.
- **Not a runtime witness** — FVR exports corroborate; they do not close obligations.
- **Not a golden vector pass** — vector MATCH is comparison evidence, not proof closure.
- **Not an equivalence certificate** — `equivalence_claimed` remains `false` until
  SPEC-007 §9 conditions are met **and** relevant obligations are `PO_PROVEN`.

Proof obligations **operationalize** [SPEC-007](../../specification/SPEC-007-EQUIVALENCE-BOUNDARIES.md);
they may not redefine equivalence boundaries.

Trust anchor audit items (A-1 … A-5) close per
[PROOF-010](./PROOF-010-AUDIT-ITEM-CLOSURE-FRAMEWORK.md). `PD-4F-OB-02` cannot
reach `PO_PROVEN` while required audit items remain open (PROOF-010 §5).

---

## §2 Proof Obligation States

### 2.1 State codes

| Code | Meaning |
|------|---------|
| `PO_OPEN` | Obligation registered; no substantive proof progress |
| `PO_PARTIAL` | Partial lemmas, evidence, or scoped checks; not closed |
| `PO_BLOCKED` | Closure impossible or prohibited under current architecture |
| `PO_PROVEN` | Machine-checked discharge recorded in Coq (or designated proof artifact) |
| `PO_RETIRED` | Obligation withdrawn; scope removed or superseded |

### 2.2 Lifecycle

```
PO_OPEN
   ├─→ PO_PARTIAL ──→ PO_PROVEN
   ├─→ PO_BLOCKED
   └─→ PO_RETIRED
```

**Rules:**

- `PO_PROVEN` requires published proof artifact and registry update; observational
  RPC export alone is **insufficient** (SPEC-007 §7.4).
- `PO_BLOCKED` obligations may not transition to `PO_PROVEN` without architectural
  change and a new obligation identifier.
- `PO_RETIRED` is terminal except via a new superseding obligation with explicit
  migration note.

---

## §3 Existing Obligations

Authoritative registry (initial population).

### PO-REG-001 — PD-4F-OB-02

| Field | Value |
|-------|-------|
| **ID** | `PD-4F-OB-02` |
| **Title** | Runtime Supply Alignment Closure |
| **Description** | Coq supply replay aligns with frozen runtime consensus behaviour on all declared inputs in the supply domain (ORU/RES recurrence, hook fields, witness chain) |
| **Status** | `PO_OPEN` |
| **Depends on** | `A-1`, `A-2`, `A-3` (trust anchors; draw-adjacent fields) |
| **Blocks** | Global equivalence claim; `coq_equiv_runtime`; unscoped `equivalence_claimed := true` |
| **Equivalence Claimed** | false |

### PO-REG-002 — PD-4F-CPP-01

| Field | Value |
|-------|-------|
| **ID** | `PD-4F-CPP-01` |
| **Title** | Coq Runtime Correspondence Proof |
| **Description** | Imported frozen-runtime trace components agree with Coq trace on all comparable components; C++ bridge placeholder replaced by verified correspondence |
| **Status** | `PO_PARTIAL` |
| **Depends on** | `PD-4F-OB-02` |
| **Blocks** | Scoped equivalence claim (supply-first); contributes to global closure |
| **Evidence** | Per-component compare (partial); `bridge_cpp_equivalence_placeholder` still Admitted |
| **Equivalence Claimed** | false |

### PO-REG-003 — DRAW-PROOF-001

| Field | Value |
|-------|-------|
| **ID** | `DRAW-PROOF-001` |
| **Title** | Draw Equivalence Closure |
| **Description** | Coq draw semantics ≡ runtime `RunSelection` / winner projection on all inputs |
| **Status** | `PO_BLOCKED` |
| **Reason** | FVR-M proved draw closure **IMPOSSIBLE** under current architecture |
| **Blocks** | None (downstream global draw equivalence already prohibited) |
| **Future state** | `PO_RETIRED` only (when registry formally withdraws the obligation) |
| **Equivalence Claimed** | false (permanent under current architecture) |

---

## §4 Closure Conditions

For each obligation: required inputs, proofs, artifacts, Coq theorems, runtime
evidence, and closure criteria.

### 4.1 PD-4F-OB-02 — Runtime Supply Alignment Closure

| Category | Requirement |
|----------|-------------|
| **Required Inputs** | Golden vectors GV-ORU/RES/STATE (PROOF-001..004); frozen subsidy/RES constants; trust-anchor resolutions or explicit assumptions |
| **Required Proofs** | Refinement lemmas: `rs_oru_supply`, `rs_res_supply`, hook projection (`cb_*` deltas); parent-delta witness chain step |
| **Required Artifacts** | Coq `ReplayState` / `ConnectBlockMint` alignment; discharged `bridge_cpp_equivalence_placeholder` successor |
| **Required Coq Theorems** | Supply-domain `coq_equiv_runtime_supply` (scoped shape per SPEC-007 §8.2) — **not yet admitted** |
| **Required Runtime Evidence** | FVR-B/C/E/F witness exports at sampled heights; golden harness `GVR_PASS` on supply vectors |
| **Closure Criteria** | **Closes only when:** (1) `A-1`, `A-2`, `A-3` resolved or cited as assumptions in theorem; (2) FVR-L `closure_allowed` can evaluate true for supply partition; (3) FVR-K candidate equivalence no longer blocked for supply scalars; (4) no reliance on observational export alone; (5) SPEC-007 §9 scoped-equivalence checklist satisfied for supply domain |

### 4.2 PD-4F-CPP-01 — Coq Runtime Correspondence Proof

| Category | Requirement |
|----------|-------------|
| **Required Inputs** | Frozen C++ trace import path; Coq `ReplaySnapshot` / trace catalog |
| **Required Proofs** | Component-wise correspondence (supply agree; draw component excluded per FVR-M) |
| **Required Artifacts** | Closed PD-4F-OB-02 supply bridge; updated Coq audit without Admitted placeholders in correspondence path |
| **Required Coq Theorems** | Trace import soundness lemmas per component |
| **Required Runtime Evidence** | FVR exports + optional golden vector batch (`PROOF-008`) |
| **Closure Criteria** | **Closes only when:** PD-4F-OB-02 supply closure complete; all non-draw components proven; draw component explicitly excluded in theorem statement |

### 4.3 DRAW-PROOF-001 — Draw Equivalence Closure

| Category | Requirement |
|----------|-------------|
| **Required Inputs** | — (obligation blocked) |
| **Required Proofs** | — |
| **Required Artifacts** | FVR-M impossibility catalog |
| **Required Coq Theorems** | `DrawClosureImpossibilityAudit` (impossibility, not equivalence) |
| **Required Runtime Evidence** | FVR-M RPC / Phase-5 summary (`draw_status: IMPOSSIBLE`) |
| **Closure Criteria** | **Does not close.** Future transition: `PO_RETIRED` when program formally withdraws draw-equivalence target |

---

## §5 Dependency Graph

ASCII dependency graph (authoritative for this registry):

```
Trust Anchors (SPEC-004)
    A-1  (σ endianness)
    A-2  (uint512 bound)
    A-3  (F membership)
         \
          +----->  PD-4F-OB-02  (Runtime Supply Alignment Closure)  [PO_OPEN]
                         |
                         v
                  PD-4F-CPP-01  (Coq Runtime Correspondence)  [PO_PARTIAL]
                         |
                         x----> Global equivalence claim  [BLOCKED until OB-02 + CPP-01]


DRAW-PROOF-001  (Draw Equivalence Closure)  [PO_BLOCKED — permanent]
    |
    +-- FVR-M impossibility (no outgoing block edge)
```

**Legend:** `---->` = depends on / blocks; `x---->` = blocks claim until upstream closed.

---

## §6 Relationship to FVR

FVR layers (Formal-Friendly Runtime Refactor) produce **evidence**. They **do not**
close proof obligations.

| Layer | Role | Maps to obligations |
|-------|------|---------------------|
| **FVR-I** | Per-field candidate equivalence status (CES) | Evidence for `PD-4F-OB-02`, `PD-4F-CPP-01` |
| **FVR-J** | Coq-readable candidate export | Same; feeds proof tooling |
| **FVR-K** | Global candidate equivalence index | Precondition monitor for OB-02 closure |
| **FVR-L** | Equivalence closure gate (`closure_allowed`) | Gate for OB-02; not proof of closure |
| **FVR-M** | Draw closure impossibility | **Blocks** `DRAW-PROOF-001`; does not close supply obligations |
| **FVR-N** | Phase-5 summary snapshot | Registry status corroboration only |

**Rule:** Any RPC or export with `equivalence_claimed: false` is consistent with
this registry. FVR must not be interpreted as discharging `PO_OPEN` or `PO_PARTIAL`
obligations.

---

## §7 Relationship to Coq

Phase-5 equivalence catalogs map to obligations as **classification and gate**
artifacts, not as closure proofs.

| Coq catalog | File | Registry relationship |
|-------------|------|------------------------|
| **Phase5SummaryCatalog** | `Phase5SummaryCatalog.v` | Consolidates layer status; links to FVR-N; documents `PD-4F-OB-02` / `PD-4F-CPP-01` as OPEN/PARTIAL |
| **GlobalCandidateEquivalenceCatalog** | `GlobalCandidateEquivalenceCatalog.v` | Feeds FVR-K; evidence toward `PD-4F-OB-02` preconditions |
| **EquivalenceClosureCatalog** | `EquivalenceClosureCatalog.v` | FVR-L gate; `closure_allowed` must be evaluable before OB-02 closure claim |
| **DrawClosureImpossibilityCatalog** | `DrawClosureImpossibilityCatalog.v` | Supports `DRAW-PROOF-001` = `PO_BLOCKED`; draw CES permanently blocked |

Coq **audits** (`*Audit.v`) prove catalog consistency, not runtime equivalence.

---

## §8 Relationship to SPEC-007

[SPEC-007](../../specification/SPEC-007-EQUIVALENCE-BOUNDARIES.md) is **authoritative**
for equivalence boundaries. This registry **operationalizes** SPEC-007 §7 pending
proof items and §9 future closure conditions.

| SPEC-007 rule | Registry enforcement |
|---------------|---------------------|
| Global Coq ≡ Runtime **NOT ESTABLISHED** | No obligation may set `equivalence_claimed` until SPEC-007 §9 satisfied |
| Draw equivalence **PROHIBITED** | `DRAW-PROOF-001` remains `PO_BLOCKED` |
| PD-4F-OB-02 / PD-4F-CPP-01 documented | PO-REG-001, PO-REG-002 |
| FVR-M permanent impossibility | `DRAW-PROOF-001` blocked; no registry path to global draw closure |
| Observational export alone insufficient | Closure criteria §4.1 (4) |

**This registry may not:**

- Redefine equivalence boundaries in SPEC-007
- Elevate FVR evidence to `PO_PROVEN`
- Override `MAINNET_PROTOCOL_FREEZE_DECLARATION.md`

---

## §9 Future Proof Program

Reserved obligation identifiers (no content yet):

| ID | Status | Domain (reserved) |
|----|--------|-------------------|
| `PD-5A-001` | `RESERVED` | Phase-5A supply refinement package |
| `PD-5A-002` | `RESERVED` | Phase-5A witness-chain refinement package |
| `PD-5B-001` | `RESERVED` | Phase-5B global candidate closure package |
| `PD-5C-001` | `RESERVED` | Phase-5C scoped equivalence claim package |

Promotion from `RESERVED` to `PO_OPEN` requires a dedicated proof task and
revision to this registry.

---

## §10 Freeze Compatibility

This document is **proof infrastructure only**.

| Guarantee | Status |
|-----------|--------|
| No consensus modifications | **Confirmed** |
| No validation modifications | **Confirmed** |
| No state_hash modifications | **Confirmed** |
| No monetary-policy modifications | **Confirmed** |
| No runtime / FVR modifications | **Confirmed** |
| No Coq theorem changes | **Confirmed** |
| No protocol-freeze modifications | **Confirmed** |

Subordinate to:

- `MAINNET_PROTOCOL_FREEZE_DECLARATION.md`
- `TASK-MAINNET-FREEZE-001`

---

## Task Output Status

| Field | Status |
|-------|--------|
| Proof Obligation Registry | **ESTABLISHED** |
| Proof Obligation Lifecycle | **ESTABLISHED** |
| PD-4F-OB-02 Tracking | **ESTABLISHED** |
| PD-4F-CPP-01 Tracking | **ESTABLISHED** |
| Draw Proof Registry | **ESTABLISHED** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## Revision History

| Version | Date | Change |
|---------|------|--------|
| 0.1 | 2026-06-05 | `TASK-PROOF-010` — initial proof obligation registry and closure tracking |
| 0.2 | 2026-06-05 | Cross-reference PROOF-010 audit item closure framework (TASK-PROOF-011) |
| 0.3 | 2026-06-05 | A-1 AI_PARTIAL closure documented in PROOF-011 (TASK-PROOF-012) |
| 0.4 | 2026-06-06 | Cross-reference PROOF-019 PD-4F-OB-02 dependency model (TASK-PROOF-019) |

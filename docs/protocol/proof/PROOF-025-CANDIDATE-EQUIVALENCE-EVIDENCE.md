# PROOF-025: Candidate Equivalence Evidence Model

**Task:** `TASK-PROOF-025`  
**Track:** Formal Verification Program — Proof Infrastructure Layer  
**Status:** ESTABLISHED  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Parents:** [PROOF-009](./PROOF-009-PROOF-OBLIGATION-REGISTRY.md), [PROOF-024](./PROOF-024-PD4F-CPP01-CLOSURE-MODEL.md)  
**Scope:** Coq evidence model only — no consensus, validation, runtime, or FVR changes.

**Equivalence Claimed:** false  
**Coq ≡ Runtime:** NOT ESTABLISHED

---

## §1 Purpose

Formally model **Candidate Equivalence Evidence** without claiming equivalence.

Current baseline:

| Object | Status |
|--------|--------|
| PD-13 / PD-14 / PD-15 | DS_Closed |
| PD4F_OB_02 | DS_Ready |
| PD4F_CPP_01 | DS_Open |
| `cpp01_candidate_equivalence_ready` | false |

This task defines what candidate equivalence means, which evidence categories
exist, what is required, what is prohibited, and why equivalence is not
established today.

It does **not**:

- Set `equivalence_claimed := true`
- Close `PD4F_CPP_01`
- Modify consensus, validation, monetary policy, or protocol freeze artifacts
- Admit draw-component equivalence (FVR-M prohibition remains active)

---

## §2 Candidate Equivalence Meaning

**Candidate equivalence** (FVR-I/K scope) means: for comparable (non-draw)
runtime trace components, imported frozen-runtime observations align with Coq
trace predictions under the Phase-5 equivalence framework.

Evidence readiness is **pre-closure**: it may enable the `cpp01_candidate_equivalence_ready`
gate in PROOF-024 but never implies global equivalence or `PO_PROVEN`.

---

## §3 Evidence Hierarchy

```
EvidenceCategory (per-component states)
        │
        ▼
candidate_equivalence_evidence_ready  (all non-draw categories sufficient)
        │
        ▼
candidate_equivalence_ready           (+ OB-02 DS_Ready, CPP-01 not DS_Closed)
        │
        ▼
cpp01_candidate_equivalence_ready     (PROOF-024 CPP-01 gate — frozen false)
        │
        ▼
pd4f_cpp01_closable                   (full CPP-01 closure — blocked)
        │
        x──→ equivalence_claimed        (permanently false at baseline)
```

---

## §4 Evidence Categories

| Category | Coq constructor | Role |
|----------|-----------------|------|
| Runtime | `CEE_Runtime` | Frozen-runtime witness / supply alignment |
| Golden Vector | `CEE_GoldenVector` | Harness / golden-trace compare evidence |
| Coq | `CEE_Coq` | Coq trace ↔ frozen C++ import correspondence |
| Invariant | `CEE_Invariant` | Cross-component invariant preservation |
| Audit | `CEE_Audit` | Audit-layer re-export and verdict checks |
| Draw Prohibited | `CEE_DrawProhibited` | **Prohibited** — draw cannot contribute (FVR-M) |

---

## §5 Evidence States

| State | Meaning |
|-------|---------|
| `CEE_Missing` | No substantive evidence (**baseline for all non-draw categories**) |
| `CEE_Partial` | Per-component compare exists; not closure-grade |
| `CEE_Sufficient` | Category satisfies readiness threshold (not equivalence) |

Lookup: `candidate_equivalence_evidence_state : EvidenceCategory -> EvidenceState`.

---

## §6 Readiness Predicates

**Evidence-layer readiness:**

```coq
Definition candidate_equivalence_evidence_ready : bool :=
  (* all five non-draw categories CEE_Sufficient *)
  cee_draw_prohibition_active &&
  negb equivalence_claimed.
```

**Master readiness:**

```coq
Definition candidate_equivalence_ready (catalog) : bool :=
  ds_ready (dependency_status PD4F_OB_02 catalog) &&
  candidate_equivalence_evidence_ready &&
  negb (ds_closed (dependency_status PD4F_CPP_01 catalog)) &&
  negb equivalence_claimed.
```

**Baseline:** `candidate_equivalence_ready = false`.

---

## §7 Insufficiency Results

| Theorem | Meaning |
|---------|---------|
| `cee_runtime_alone_insufficient` | Runtime `CEE_Sufficient` alone → not ready |
| `cee_golden_vector_alone_insufficient` | Golden-vector alone → not ready |
| `cee_coq_alone_insufficient` | Coq alone → not ready |
| `cee_all_sufficient_allows_evidence_ready` | All five categories sufficient → evidence ready |
| `cee_draw_prohibited_never_sufficient` | Draw category never `CEE_Sufficient` |

Readiness never implies equivalence:

```coq
Theorem candidate_equivalence_not_equivalence_holds :
  candidate_equivalence_not_equivalence.
```

---

## §8 Dependency Theorems

**OB-02 ready is necessary:**

```coq
Theorem candidate_equivalence_ready_requires_ob02 :
  forall catalog,
  candidate_equivalence_ready catalog = true ->
  ds_ready (dependency_status PD4F_OB_02 catalog) = true.
```

**CPP-01 must not be closed while evidence-readiness holds:**

```coq
Theorem candidate_equivalence_ready_forbids_cpp01_closed :
  forall catalog,
  candidate_equivalence_ready catalog = true ->
  ds_closed (dependency_status PD4F_CPP_01 catalog) = false.
```

**CPP-01 relationship:**

```coq
Definition candidate_equivalence_requires_cpp01 : Prop :=
  forall catalog,
  candidate_equivalence_ready catalog = true ->
  ds_closed (dependency_status PD4F_CPP_01 catalog) = false.
```

Audit additionally re-exports: `pd4f_cpp01_closable = true -> cpp01_candidate_equivalence_ready = true`.

---

## §9 Prohibited Evidence

| Prohibition | Baseline | Source |
|-------------|----------|--------|
| Draw component equivalence | Active (`cee_draw_prohibition_active = true`) | FVR-M draw impossibility |
| Draw evidence toward closure | `CEE_DrawProhibited` always `CEE_Missing` | PROOF-024 / FVR-M |
| Global equivalence claim | `equivalence_claimed = false` | PROOF-009 honesty boundary |

---

## §10 Relationship to SPEC-007

SPEC-007 §8 scoped equivalence checklist gates remain unsatisfied
(`cpp01_spec007_closure_gate_satisfied = false` in PROOF-024). PROOF-025 models
the **evidence layer** that would feed those gates; it does not discharge them.

---

## §11 Relationship to CPP-01 (PROOF-024)

| PROOF-024 gate | PROOF-025 model |
|----------------|-----------------|
| `cpp01_candidate_equivalence_ready` | Aggregated gate flag (frozen `false`) |
| `candidate_equivalence_evidence_ready` | Semantic evidence decomposition |

Both are `false` at baseline (`audit_cpp01_candidate_gate_baseline_aligned`).

---

## §12 Relationship to Future Global Equivalence

```
PD4F_OB_02 ──→ candidate_equivalence_ready ──→ PD4F_CPP_01 ──x──→ equivalence_claimed
```

Global equivalence remains blocked until CPP-01 reaches `DS_Closed` under
SPEC-007 boundaries. Evidence readiness is a **necessary intermediate** layer,
not a global equivalence claim.

---

## §13 Coq Artifacts

| Module | Role |
|--------|------|
| `CandidateEquivalenceEvidenceModel.v` | Categories, states, readiness, baseline |
| `CandidateEquivalenceEvidenceAudit.v` | Audit re-exports, CPP-01 alignment, verdict |

---

## §14 Verification

```bash
cd ./formal_verification/coq
coqc -R . Rigor Phase5Equivalence/CandidateEquivalenceEvidenceModel.v
coqc -R . Rigor Phase5Equivalence/CandidateEquivalenceEvidenceAudit.v

cd .
python3 formal_verification/tests/test_phase4f_consensus_structure.py
```

All proofs end with `Qed`. No `Admitted`.

---

## §15 Required Output Table

| Field | Status |
|-------|--------|
| Candidate Equivalence Evidence Model | **ESTABLISHED** |
| Evidence Categories | **ESTABLISHED** |
| Evidence Readiness Model | **ESTABLISHED** |
| Runtime Evidence Mapping | **ESTABLISHED** |
| Golden Vector Mapping | **ESTABLISHED** |
| Coq Mapping | **ESTABLISHED** |
| Candidate Equivalence Ready | **false** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## §16 Revision History

| Version | Date | Change |
|---------|------|--------|
| 1.0 | 2026-06-06 | TASK-PROOF-025 — Candidate equivalence evidence model |

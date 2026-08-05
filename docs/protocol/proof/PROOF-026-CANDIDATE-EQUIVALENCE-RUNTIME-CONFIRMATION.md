# PROOF-026: Candidate Equivalence Runtime Confirmation (CPP-01 Gate)

**Task:** `TASK-PROOF-026`  
**Track:** Formal Verification Program — Proof Infrastructure Layer  
**Status:** ESTABLISHED  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Parents:** [PROOF-024](./PROOF-024-PD4F-CPP01-CLOSURE-MODEL.md), [PROOF-025](./PROOF-025-CANDIDATE-EQUIVALENCE-EVIDENCE.md), [PROOF-015](./PROOF-015-A1-A5-BATCH-RUNTIME-OBSERVATION.md)  
**Scope:** Coq runtime confirmation only — no consensus, validation, runtime, or FVR changes.

**Equivalence Claimed:** false  
**Coq ≡ Runtime:** NOT ESTABLISHED

---

## §1 Purpose

Map PROOF-025 candidate equivalence evidence categories to **frozen runtime
artifacts** (FVR-B/C/F/G/H/I/N mirrors, 12 golden-vector snapshots) and confirm
that partial materialization at **h ≤ 501** does **not** advance CPP-01 closure.

Current baseline:

| Object | Status |
|--------|--------|
| PD-13 / PD-14 / PD-15 | DS_Closed |
| PD4F_OB_02 | DS_Ready |
| PD4F_CPP_01 | DS_Open |
| `cpp01_candidate_equivalence_ready` | false |
| `candidate_equivalence_runtime_ready` | false |

It does **not**:

- Set `equivalence_claimed := true`
- Close `PD4F_CPP_01` or any CPP-01 alignment gate
- Modify consensus, FVR, state_hash, or runtime code
- Admit draw-component equivalence (FVR-M)

---

## §2 Runtime Artifact Mapping

| Evidence category | Runtime artifact | h≤501 observation |
|-------------------|------------------|-------------------|
| `CEE_Runtime` | FVR-B/C/F/G/H/I/N mirror RPCs | `CEE_Partial` |
| `CEE_GoldenVector` | 12-vector harness (`golden_vector_fixtures.json`) | `CEE_Partial` (9/12 materialized) |
| `CEE_Coq` | Phase-4F/5E Coq trace catalog | `CEE_Partial` |
| `CEE_Invariant` | Cross-component invariant witnesses | `CEE_Missing` |
| `CEE_Audit` | Harness report / batch RPC | `CEE_Partial` |
| `CEE_DrawProhibited` | Draw vectors (FVR-M) | `CEE_Missing` (cannot contribute) |

Frozen constants (Coq):

- `cee_runtime_golden_vector_total = 12`
- `cee_runtime_golden_vector_materialized_h501 = 9`
- `cee_runtime_observation_height = 501`

---

## §3 Readiness Predicates

**Runtime evidence readiness:**

```coq
Definition candidate_equivalence_runtime_evidence_ready : bool :=
  (* all five non-draw categories CEE_Sufficient — false at h≤501 baseline *)
  ...
```

**Master runtime readiness:**

```coq
Definition candidate_equivalence_runtime_ready (catalog) : bool :=
  ds_ready (dependency_status PD4F_OB_02 catalog) &&
  candidate_equivalence_runtime_evidence_ready &&
  negb (ds_closed (dependency_status PD4F_CPP_01 catalog)) &&
  cee_draw_prohibition_active &&
  negb equivalence_claimed.
```

**Baseline:** `candidate_equivalence_runtime_baseline_false`.

---

## §4 Formal Results

**OB-02 ready is necessary:**

```coq
Theorem candidate_equivalence_runtime_ready_requires_ob02 :
  forall catalog,
  candidate_equivalence_runtime_ready catalog = true ->
  ds_ready (dependency_status PD4F_OB_02 catalog) = true.
```

**CPP-01 must remain open:**

```coq
Theorem candidate_equivalence_runtime_ready_forbids_cpp01_closed :
  forall catalog,
  candidate_equivalence_runtime_ready catalog = true ->
  ds_closed (dependency_status PD4F_CPP_01 catalog) = false.
```

**Never implies equivalence:**

```coq
Theorem candidate_equivalence_runtime_not_equivalence_holds :
  candidate_equivalence_runtime_not_equivalence.
```

**Partial materialization confirms not ready:**

```coq
Lemma candidate_equivalence_runtime_partial_confirms_not_ready :
  candidate_equivalence_runtime_evidence_ready = false.
```

---

## §5 CPP-01 Gate Verification

| CPP-01 gate | Verified status (frozen) |
|-------------|-------------------------|
| `cpp01_runtime_alignment_ready` | false |
| `cpp01_vector_alignment_ready` | false |
| `cpp01_coq_alignment_ready` | false |
| `cpp01_candidate_equivalence_ready` | false |

Partial golden-vector materialization (9/12 at h≤501) confirms
`CEE_Partial` ≠ `CEE_Sufficient`; candidate equivalence gate remains closed.

---

## §6 JSON Schema Export

Runtime witness comparison exports MUST validate against:

`formal_verification/schemas/candidate_equivalence_runtime_witness.schema.json`

Coq reference: `candidate_equivalence_runtime_witness_schema`.

Required root fields: `harness_version`, `equivalence_claimed` (false),
`coq_equiv_runtime` (`NOT_ESTABLISHED`), `draw_equivalence` (`PROHIBITED`),
`observation_height`, `fvr_mirror_layers`, `summary`, `vectors[12]`.

---

## §7 Relationship to SPEC-007

SPEC-007 §8 scoped equivalence checklist remains unsatisfied. PROOF-026 confirms
that observational runtime evidence at h≤501 is **partial** and does not discharge
any SPEC-007 closure gate.

---

## §8 Relationship to CPP-01 (PROOF-024)

```
Runtime partial evidence ──x──→ cpp01_candidate_equivalence_ready
                          ──x──→ pd4f_cpp01_closable
```

All four CPP-01 gates remain `false`; audit layer re-exports frozen verification.

---

## §9 Relationship to Future Global Equivalence

Global `equivalence_claimed := true` remains blocked. Runtime confirmation
documents **what exists** in harness exports without advancing the global claim.

---

## §10 Coq Artifacts

| Module | Role |
|--------|------|
| `CandidateEquivalenceRuntimeModel.v` | Runtime mapping, readiness, baseline |
| `CandidateEquivalenceRuntimeAudit.v` | CPP-01 gate verification, verdict |

---

## §11 Verification

```bash
cd ./formal_verification/coq
coqc -R . Rigor Phase5Equivalence/CandidateEquivalenceRuntimeModel.v
coqc -R . Rigor Phase5Equivalence/CandidateEquivalenceRuntimeAudit.v

cd .
python3 formal_verification/tests/test_phase4f_consensus_structure.py
```

All proofs end with `Qed`. No `Admitted`.

---

## §12 Required Output Table

| Field | Status |
|-------|--------|
| Candidate Equivalence Runtime Model | **ESTABLISHED** |
| Runtime Evidence Mapping | **ESTABLISHED** |
| Golden Vector Mapping | **VERIFIED** |
| Coq Mapping | **VERIFIED** |
| Candidate Equivalence Runtime Ready | **false** |
| CPP-01 Gates | runtime_alignment / vector_alignment / coq_alignment **verified**; candidate_equivalence_ready **false** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## §13 Revision History

| Version | Date | Change |
|---------|------|--------|
| 1.0 | 2026-06-06 | TASK-PROOF-026 — Candidate equivalence runtime confirmation |

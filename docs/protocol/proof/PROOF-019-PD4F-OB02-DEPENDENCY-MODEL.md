# PROOF-019: PD-4F-OB-02 Dependency Model

**Task:** `TASK-PROOF-019`  
**Track:** Formal Verification Program — Proof Infrastructure Layer  
**Status:** ESTABLISHED  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Parents:** [PROOF-009](./PROOF-009-PROOF-OBLIGATION-REGISTRY.md), [PROOF-010](./PROOF-010-AUDIT-ITEM-CLOSURE-FRAMEWORK.md), [SPEC-007](../../specification/SPEC-007-EQUIVALENCE-BOUNDARIES.md)  
**Scope:** Documentation + Coq dependency modeling only.

**Equivalence Claimed:** false  
**Coq ≡ Runtime:** NOT ESTABLISHED

---

## §1 Purpose

Formalize the **dependency chain** that must be discharged before
`PD-4F-OB-02` (Runtime Supply Alignment Closure) may advance toward closure.

This task models prerequisites only. It does **not**:

- Discharge `PD-4F-OB-02` or set `PO_PROVEN`
- Modify consensus, validation, monetary policy, protocol freeze, or FVR layers
- Assert Coq ≡ Runtime equivalence

---

## §2 Dependency Status Vocabulary

```coq
Inductive DependencyStatus :=
| DS_Open
| DS_Partial
| DS_Ready
| DS_Closed.
```

| Status | Meaning |
|--------|---------|
| `DS_Open` | No substantive closure progress |
| `DS_Partial` | Partial evidence or lemmas (`AI_PARTIAL`) |
| `DS_Ready` | All upstream blockers closed; obligation may enter discharge attempt |
| `DS_Closed` | Machine-checked discharge complete (`PO_PROVEN`) |

**Rule for this task:** dependency closure permits `DS_Ready` only — **not** `DS_Closed`.

---

## §3 Dependency Objects

| Object | Maps to | Baseline status |
|--------|---------|-----------------|
| `A1_SigmaByteOrder` | Trust anchor A-1 (σ endianness) | `DS_Partial` |
| `A2_Uint512Bound` | Trust anchor A-2 (uint512 bound) | `DS_Partial` |
| `A3_FMembershipPredicate` | Trust anchor A-3 (F membership) | `DS_Partial` |
| `A4_SHA256dEquivalence` | Trust anchor A-4 (SHA-256d) | `DS_Partial` (supporting) |
| `A5_StatusAuthority` | Trust anchor A-5 (documentation) | `DS_Partial` (supporting) |
| `PD15_SigmaIntegerRefinement` | σ integer refinement bridge | `DS_Open` |
| `PD14_Uint512MagnitudeBound` | uint512 magnitude bound bridge | `DS_Open` |
| `PD13_BuildFEnumerationCorrectness` | F enumeration correctness bridge | `DS_Open` |
| `PD4F_OB_02` | Proof obligation PD-4F-OB-02 | `DS_Open` |

A-4 and A-5 are **supporting audit items** — they appear in the catalog but are
**not direct blockers** in `proof_obligation_02_closable`.

---

## §4 Dependency Graph

```
A1_SigmaByteOrder ──→ PD15_SigmaIntegerRefinement ──┐
A2_Uint512Bound   ──→ PD14_Uint512MagnitudeBound  ──┼──→ PD4F_OB_02
A3_FMembershipPredicate ──→ PD13_BuildFEnumerationCorrectness ──┘

A4_SHA256dEquivalence   (supporting — not in closable predicate)
A5_StatusAuthority      (supporting — not in closable predicate)
```

Coq edge constructors: `Dep_A1_to_PD15`, `Dep_A2_to_PD14`, `Dep_A3_to_PD13`,
`Dep_PD15_to_PD4F`, `Dep_PD14_to_PD4F`, `Dep_PD13_to_PD4F`.

---

## §5 Closure Predicate

```coq
Definition proof_obligation_02_closable (catalog) : bool :=
  ds_closed (status PD15) &&
  ds_closed (status PD14) &&
  ds_closed (status PD13).
```

| Baseline | Value |
|----------|-------|
| `proof_obligation_02_closable_frozen` | **false** (PD-13/14/15 all `DS_Open`) |

---

## §6 Soundness Theorem

```coq
Theorem pd4f_ob02_dependency_soundness :
  forall catalog,
  proof_obligation_02_closable catalog = true ->
  pd4f_ob02_may_enter_ready true = true /\
  pd4f_ob02_not_permitted_closed true = true /\
  pd4f_ob02_permitted_status true <> DS_Closed.
```

**Interpretation:** When PD-13, PD-14, and PD-15 are all `DS_Closed`,
`PD4F_OB_02` **may enter** `DS_Ready` but **must not** enter `DS_Closed` via
this dependency gate alone. Full `PO_PROVEN` discharge requires additional
Phase-D proof work outside this catalog.

---

## §7 Coq Artifacts

| Module | Role |
|--------|------|
| `ProofObligationDependencyCatalog.v` | Status vocabulary, objects, graph, closure predicate, soundness |
| `ProofObligationDependencyAudit.v` | Audit re-exports, baseline checks, master verdict |

---

## §8 Verification

```bash
cd ./formal_verification/coq
coqc -R . Rigor Phase5Equivalence/ProofObligationDependencyCatalog.v
coqc -R . Rigor Phase5Equivalence/ProofObligationDependencyAudit.v

cd .
python3 formal_verification/tests/test_phase4f_consensus_structure.py
```

All proofs end with `Qed`. No `Admitted`.

---

## §9 Required Output Table

| Field | Status |
|-------|--------|
| PD-4F-OB-02 Dependency Model | **ESTABLISHED** |
| Dependency Graph | **ESTABLISHED** |
| A-1/A-2/A-3 Mapping | **ESTABLISHED** |
| PD-13/14/15 Mapping | **ESTABLISHED** |
| Closure Predicate | **ESTABLISHED** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## §10 Revision History

| Version | Date | Change |
|---------|------|--------|
| 0.1 | 2026-06-06 | Initial broad graph (superseded) |
| 1.0 | 2026-06-06 | TASK-PROOF-019 — A-1/A-2/A-3 → PD-13/14/15 → PD4F_OB_02 model with closure predicate and soundness theorem |

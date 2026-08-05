# PROOF-023: PD-4F-OB-02 Readiness Closure

**Task:** `TASK-PROOF-023`  
**Track:** Formal Verification Program — Proof Infrastructure Layer  
**Status:** ESTABLISHED  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Parents:** [PROOF-009](./PROOF-009-PROOF-OBLIGATION-REGISTRY.md), [PROOF-010](./PROOF-010-AUDIT-ITEM-CLOSURE-FRAMEWORK.md), [PROOF-019](./PROOF-019-PD4F-OB02-DEPENDENCY-MODEL.md), [PROOF-022](./PROOF-022-PD13-FSET-MEMBERSHIP.md)  
**Scope:** Coq readiness model only — no consensus, validation, runtime, or FVR changes.

**Equivalence Claimed:** false  
**Coq ≡ Runtime:** NOT ESTABLISHED

---

## §1 Purpose

Formally advance `PD4F_OB_02` to **DS_Ready** now that the PD-13/14/15
refinement bridges (PROOF-020 / PROOF-021 / PROOF-022) are **DS_Closed**.

This task proves:

\[
\text{PD13} = \text{DS\_Closed} \land \text{PD14} = \text{DS\_Closed} \land \text{PD15} = \text{DS\_Closed}
\Rightarrow \text{PD4F\_OB\_02} \text{ may enter } \text{DS\_Ready}
\]

and explicitly **not** `DS_Closed`.

It does **not**:

- Discharge `PD4F_OB_02` to `PO_PROVEN` / `DS_Closed`
- Close `PD4F_CPP_01`
- Modify consensus, validation, monetary policy, or protocol freeze artifacts
- Assert Coq ≡ Runtime equivalence

---

## §2 Dependency Chain

```
A1_SigmaByteOrder ──→ PD15_SigmaIntegerRefinement ──┐
A2_Uint512Bound   ──→ PD14_Uint512MagnitudeBound  ──┼──→ PD4F_OB_02 ──→ PD4F_CPP_01
A3_FMembershipPredicate ──→ PD13_BuildFEnumerationCorrectness ──┘

A4_SHA256dEquivalence   (supporting — not in closable predicate)
A5_StatusAuthority      (supporting — not in closable predicate)
```

| Edge | Meaning |
|------|---------|
| `Dep_A1_to_PD15` | σ byte-order trust anchor feeds PD-15 bridge |
| `Dep_A2_to_PD14` | uint512 bound anchor feeds PD-14 bridge |
| `Dep_A3_to_PD13` | F membership anchor feeds PD-13 bridge |
| `Dep_PD15/14/13_to_PD4F` | All three bridges must close before OB-02 readiness |
| `Dep_PD4F_to_CPP01` | CPP-01 depends on OB-02 (future closure path) |

---

## §3 Readiness Criteria

Closure predicate (from PROOF-019):

```coq
Definition proof_obligation_02_closable (catalog) : bool :=
  ds_closed (status PD15) &&
  ds_closed (status PD14) &&
  ds_closed (status PD13).
```

Readiness predicate (PROOF-023):

```coq
Definition pd4f_ob02_ready (catalog) : bool :=
  proof_obligation_02_closable catalog &&
  pd4f_ob02_may_enter_ready (proof_obligation_02_closable catalog).
```

Soundness:

```coq
Theorem pd4f_ob02_ready_soundness :
  forall catalog,
  proof_obligation_02_closable catalog = true ->
  pd4f_ob02_ready catalog = true /\
  pd4f_ob02_permitted_status true = DS_Ready /\
  pd4f_ob02_permitted_status true <> DS_Closed.
```

---

## §4 Remaining Blockers

| Object | Status after PROOF-023 | Blocker role |
|--------|------------------------|--------------|
| `PD4F_OB_02` | **DS_Ready** | Supply alignment discharge still required for `DS_Closed` |
| `PD4F_CPP_01` | **DS_Open** | Coq ≡ frozen C++ correspondence unproven |
| `equivalence_claimed` | **false** | Global equivalence remains blocked |

CPP-01 closure gate:

```coq
Theorem pd4f_ob02_not_closed_without_cpp01 :
  forall catalog,
  ds_closed (dependency_status PD4F_CPP_01 catalog) = false ->
  pd4f_ob02_permitted_status (proof_obligation_02_closable catalog) <> DS_Closed.
```

`DS_Ready ≠ DS_Closed` is proven as `ds_ready_ne_ds_closed`.

---

## §5 Future Path Toward PD4F_CPP_01

Per [PROOF-009](./PROOF-009-PROOF-OBLIGATION-REGISTRY.md) PO-REG-002:

1. Complete OB-02 supply-domain discharge (`PO_PROVEN` / `DS_Closed`) with explicit A-1/A-2/A-3 assumptions or closures
2. Prove component-wise Coq trace ↔ frozen C++ correspondence (draw excluded per FVR-M)
3. Close `bridge_cpp_equivalence_placeholder` successors without Admitted
4. Advance `PD4F_CPP_01` from `DS_Open` → `DS_Partial` → `DS_Closed`
5. Only then may a scoped equivalence claim be evaluated under SPEC-007 §8.2

---

## §6 Coq Artifacts

| Module | Role |
|--------|------|
| `PD4FOB02Readiness.v` | Readiness predicate, soundness, CPP-01 blocker |
| `PD4FOB02ReadinessAudit.v` | Audit re-exports, baseline checks, verdict |
| `ProofObligationDependencyCatalog.v` | Updated baseline: OB-02 `DS_Ready`, CPP-01 `DS_Open` |

---

## §7 Verification

```bash
cd ./formal_verification/coq
coqc -R . Rigor Phase5Equivalence/PD4FOB02Readiness.v
coqc -R . Rigor Phase5Equivalence/PD4FOB02ReadinessAudit.v

cd .
python3 formal_verification/tests/test_phase4f_consensus_structure.py
```

All proofs end with `Qed`. No `Admitted`.

---

## §8 Required Output Table

| Field | Status |
|-------|--------|
| PD4F_OB_02 Readiness Model | **ESTABLISHED** |
| Dependency Closure | **VERIFIED** |
| PD-13 | **DS_Closed** |
| PD-14 | **DS_Closed** |
| PD-15 | **DS_Closed** |
| PD4F_OB_02 | **DS_Ready** |
| PD4F_CPP_01 | **DS_Open** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## §9 Revision History

| Version | Date | Change |
|---------|------|--------|
| 1.0 | 2026-06-06 | TASK-PROOF-023 — PD-4F-OB-02 readiness closure |

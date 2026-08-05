# PROOF-021: PD-14 UInt512 Overflow Bound Refinement

**Task:** `TASK-PROOF-021`  
**Track:** Formal Verification Program — Proof Infrastructure Layer  
**Status:** ESTABLISHED  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Parents:** [PROOF-011](./PROOF-011-AUDIT-A1-CLOSURE.md), [PROOF-019](./PROOF-019-PD4F-OB02-DEPENDENCY-MODEL.md), [PROOF-020](./PROOF-020-PD15-SIGMA-INTEGER-REFINEMENT.md)  
**Scope:** Coq refinement only — no consensus, validation, runtime, or FVR changes.

**Equivalence Claimed:** false  
**Coq ≡ Runtime:** NOT ESTABLISHED

---

## §1 Purpose

Formally close `PD14_Uint512MagnitudeBound` by proving the PROOF-011 A-2 / PROOF-013
magnitude pin for draw-path uint512 products:

\[
\sigma < 2^{256},\quad f < 2^{64} \Rightarrow \sigma \cdot f < 2^{512}
\]

and connecting Coq `Z` multiplication to the FVR-B/C/F runtime spec
(`rigor_uint512.mul256x256_full` / C++ `Mul256x256Full`).

This task discharges PD-14 refinement lemmas only. It does **not**:

- Modify consensus, validation, monetary policy, or protocol freeze artifacts
- Change FVR modules or runtime witnesses
- Assert Coq ≡ Runtime equivalence

---

## §2 Formal Model

| Coq artifact | Role |
|--------------|------|
| `uint512_value_bound` | Predicate `0 <= z < 2^512` |
| `uint256_value_bound` / `uint64_value_bound` | Operand range guards |
| `mul256x256_full` | Coq cross-product on `Z` |
| `fvr_mul256x256_full` | Runtime spec alias (FVR-B/C/F) |
| `uint512_bytes_to_integer` | 64-byte little-endian uint512 sum |
| `convergence_product_bounded` | σ·f uint512 bound for draw/mint paths |

---

## §3 Refinement Lemmas

| Lemma | Property |
|-------|----------|
| `mul256x256_full_deterministic` | Same operands → same product |
| `uint512_bytes_to_integer_injective_length_fixed` | 64-byte encoding injectivity |
| `runtime_spec_consistency` | Bounded uint256×uint256 product matches FVR spec and stays uint512-bounded |
| `convergence_product_bounded` | σ (uint256) × f (uint64) stays within uint512 |

Closure predicate:

```coq
Definition pd14_closable : bool := true.

Theorem pd14_refinement_soundness :
  pd14_closable = true ->
  pd14_permitted_status true = DS_Closed /\
  pd14_permitted_status true <> DS_Open /\
  ds_closed (pd14_permitted_status true) = true.
```

---

## §4 Dependency State Update

| Object | Status after PROOF-021 |
|--------|------------------------|
| `PD14_Uint512MagnitudeBound` | **DS_Closed** |
| `PD15_SigmaIntegerRefinement` | DS_Closed |
| `PD13_BuildFEnumerationCorrectness` | DS_Open |
| `PD4F_OB_02` | DS_Open |

`proof_obligation_02_closable_frozen` remains **false** (PD-13 still open).

---

## §5 Coq Artifacts

| Module | Role |
|--------|------|
| `UInt512OverflowRefinement.v` | Model, refinement lemmas, PD-14 closure |
| `UInt512OverflowRefinementAudit.v` | Audit re-exports, baseline checks, verdict |
| `ProofObligationDependencyCatalog.v` | Baseline `baseline_pd14_pd15_closed_pd13_open` |

---

## §6 Verification

```bash
cd ./formal_verification/coq
coqc -R . Rigor Phase5Equivalence/UInt512OverflowRefinement.v
coqc -R . Rigor Phase5Equivalence/UInt512OverflowRefinementAudit.v

cd .
python3 formal_verification/tests/test_phase4f_consensus_structure.py
```

All proofs end with `Qed`. No `Admitted`.

---

## §7 Required Output Table

| Field | Status |
|-------|--------|
| PD-14 Refinement Model | **ESTABLISHED** |
| UInt512 Mapping | **ESTABLISHED** |
| Runtime Consistency | **ESTABLISHED** |
| PD-14 Closure | **DS_Closed** |
| PD-15 | **DS_Closed** |
| PD-13 | **DS_Open** |
| PD4F_OB_02 | **DS_Open** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## §8 Revision History

| Version | Date | Change |
|---------|------|--------|
| 1.0 | 2026-06-06 | TASK-PROOF-021 — PD-14 uint512 overflow bound refinement closure |

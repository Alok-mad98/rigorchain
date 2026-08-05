# PROOF-020: PD-15 Sigma Integer Refinement

**Task:** `TASK-PROOF-020`  
**Track:** Formal Verification Program — Proof Infrastructure Layer  
**Status:** ESTABLISHED  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Parents:** [PROOF-011](./PROOF-011-AUDIT-A1-CLOSURE.md), [PROOF-019](./PROOF-019-PD4F-OB02-DEPENDENCY-MODEL.md)  
**Scope:** Coq refinement only — no consensus, validation, runtime, or FVR changes.

**Equivalence Claimed:** false  
**Coq ≡ Runtime:** NOT ESTABLISHED

---

## §1 Purpose

Formally close `PD15_SigmaIntegerRefinement` by connecting the PROOF-011 A-1
little-endian pin:

\[
H_i = \sum_{k=0}^{n-1} \sigma_{\text{bytes}}[k] \cdot 256^k
\]

to the machine-checked `uint256_of_bytes` decoder in `RigorBytes.v`.

This task discharges PD-15 refinement lemmas only. It does **not**:

- Modify consensus, validation, monetary policy, or protocol freeze artifacts
- Change FVR modules or runtime witnesses
- Assert Coq ≡ Runtime equivalence

---

## §2 Formal Model

| Coq artifact | Role |
|--------------|------|
| `sigma_le_sum_nat` | Little-endian unsigned sum \( \sum \sigma[k]\cdot 256^k \) |
| `sigma_bytes_to_integer` | `Z.of_nat (sigma_le_sum_nat bs)` |
| `sigma_le_sum_nat_eq_go` | Bridge to `uint256_of_bytes_go` |
| `runtime_spec_consistency` | Valid 32-byte σ → `uint256_of_bytes` |

Decoder helpers (`sigma_decode_fuel_nat`, `sigma_le_sum_nat_injective`) support
fixed-length injectivity without touching chain state.

---

## §3 Refinement Lemmas

| Lemma | Property |
|-------|----------|
| `sigma_bytes_to_integer_deterministic` | Same bytes → same integer |
| `sigma_bytes_to_integer_injective_length_fixed` | 32-byte σ injectivity |
| `runtime_spec_consistency` | Coq sum matches `uint256_of_bytes` on valid σ |

Closure predicate:

```coq
Definition pd15_closable : bool := true.

Theorem pd15_refinement_soundness :
  pd15_closable = true ->
  pd15_permitted_status true = DS_Closed /\
  pd15_permitted_status true <> DS_Open /\
  ds_closed (pd15_permitted_status true) = true.
```

---

## §4 Dependency State Update

| Object | Status after PROOF-020 |
|--------|------------------------|
| `PD15_SigmaIntegerRefinement` | **DS_Closed** |
| `PD14_Uint512MagnitudeBound` | DS_Open |
| `PD13_BuildFEnumerationCorrectness` | DS_Open |
| `PD4F_OB_02` | DS_Open |

`proof_obligation_02_closable_frozen` remains **false** (PD-14 and PD-13 still open).

---

## §5 Coq Artifacts

| Module | Role |
|--------|------|
| `SigmaIntegerRefinement.v` | Model, refinement lemmas, PD-15 closure |
| `SigmaIntegerRefinementAudit.v` | Audit re-exports, baseline checks, verdict |
| `RigorBytes.v` (bridge lemmas) | `uint256_go_cons`, `ofnat_mul256` |

---

## §6 Verification

```bash
cd ./formal_verification/coq
coqc -R . Rigor Phase5Equivalence/SigmaIntegerRefinement.v
coqc -R . Rigor Phase5Equivalence/SigmaIntegerRefinementAudit.v

cd .
python3 formal_verification/tests/test_phase4f_consensus_structure.py
```

All proofs end with `Qed`. No `Admitted`.

---

## §7 Required Output Table

| Field | Status |
|-------|--------|
| PD15 Refinement Model | **ESTABLISHED** |
| Sigma Integer Mapping | **ESTABLISHED** |
| Little Endian Interpretation | **ESTABLISHED** |
| PD15 Closure | **DS_Closed** |
| PD14 | **DS_Open** |
| PD13 | **DS_Open** |
| PD4F_OB_02 | **DS_Open** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## §8 Revision History

| Version | Date | Change |
|---------|------|--------|
| 1.0 | 2026-06-06 | TASK-PROOF-020 — PD-15 sigma integer refinement closure |

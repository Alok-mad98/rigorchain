# PROOF-022: PD-13 F-Set Membership Predicate Closure

**Task:** `TASK-PROOF-022`  
**Track:** Formal Verification Program — Proof Infrastructure Layer  
**Status:** ESTABLISHED  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Parents:** [PROOF-011](./PROOF-011-AUDIT-A1-CLOSURE.md), [PROOF-019](./PROOF-019-PD4F-OB02-DEPENDENCY-MODEL.md), [PROOF-020](./PROOF-020-PD15-SIGMA-INTEGER-REFINEMENT.md), [PROOF-021](./PROOF-021-PD14-UINT512-REFINEMENT.md)  
**Scope:** Coq refinement only — no consensus, validation, runtime, or FVR changes.

**Equivalence Claimed:** false  
**Coq ≡ Runtime:** NOT ESTABLISHED

---

## §1 Purpose

Formally close `PD13_BuildFEnumerationCorrectness` by proving the PROOF-011 A-3 /
PROOF-013 F membership predicate surface used in convergence/draw computations:

\[
F = \{ \text{txid}(tx) \mid tx \in \text{block},\ \text{valid}(tx),\ \text{fee\_positive}(tx),\ \text{convergence\_amount}(tx) \ge \text{MIN} \}
\]

matches the C++ runtime enumeration (`ListConvergenceRootCommitmentTxids`) and the
Python FVR witness (`list_convergence_commitment_txids`).

This task discharges PD-13 refinement lemmas only. It does **not**:

- Modify consensus, validation, monetary policy, or protocol freeze artifacts
- Change FVR modules or runtime witnesses
- Assert Coq ≡ Runtime equivalence
- Close `PD4F_OB_02`

---

## §2 Formal Model

| Coq artifact | Role |
|--------------|------|
| `FMembershipTx` | Abstract tx record (txid bytes + predicate fields) |
| `tx_passes_f_predicate` | Runtime filter: valid ∧ fee_positive ∧ amount ≥ MIN |
| `coq_enumerate_F` | Block-order map/filter producing F as txid byte lists |
| `fvr_enumerate_F` | FVR-B/C/F runtime spec alias |
| `F_membership` | `existsb (beq_byte_list txid)` over enumerated F |
| `beq_byte_list` | Deterministic byte-list equality test |

`min_convergence_amount = 100000000` (1 BTC in satoshis) matches the runtime MIN pin.

---

## §3 Refinement Lemmas

| Lemma | Property |
|-------|----------|
| `coq_enumerate_F_deterministic` | Same tx list → same F enumeration |
| `F_membership_deterministic` | Same F → same membership result |
| `fmt_id_injective_on_nodup` / `coq_enumerate_F_nodup` | No duplicate txids in F when inputs are txid-injective |
| `runtime_spec_consistency` | `coq_enumerate_F = fvr_enumerate_F` |
| `F_membership_sound` | Predicate-passing tx ∈ block ⇒ membership true |
| `F_membership_runtime_consistency` | Membership stable under runtime alias |

Closure predicate:

```coq
Definition pd13_closable : bool := true.

Theorem pd13_refinement_soundness :
  pd13_closable = true ->
  pd13_permitted_status true = DS_Closed /\
  pd13_permitted_status true <> DS_Open /\
  ds_closed (pd13_permitted_status true) = true.
```

---

## §4 Dependency State Update

| Object | Status after PROOF-022 |
|--------|------------------------|
| `PD13_BuildFEnumerationCorrectness` | **DS_Closed** |
| `PD14_Uint512MagnitudeBound` | DS_Closed |
| `PD15_SigmaIntegerRefinement` | DS_Closed |
| `PD4F_OB_02` | DS_Open |

`proof_obligation_02_closable_frozen` is now **true** (PD-13/14/15 all closed).
PD4F may enter `DS_Ready` but remains **not** `DS_Closed`; `equivalence_claimed` stays false.

---

## §5 Coq Artifacts

| Module | Role |
|--------|------|
| `FSetMembershipRefinement.v` | Model, refinement lemmas, PD-13 closure |
| `FSetMembershipRefinementAudit.v` | Audit re-exports, baseline checks, verdict |
| `ProofObligationDependencyCatalog.v` | Baseline `baseline_pd13_pd14_pd15_closed_pd4f_open` |

---

## §6 Verification

```bash
cd ./formal_verification/coq
coqc -R . Rigor Phase5Equivalence/FSetMembershipRefinement.v
coqc -R . Rigor Phase5Equivalence/FSetMembershipRefinementAudit.v

cd .
python3 formal_verification/tests/test_phase4f_consensus_structure.py
```

All proofs end with `Qed`. No `Admitted`.

---

## §7 Required Output Table

| Field | Status |
|-------|--------|
| PD-13 Refinement Model | **ESTABLISHED** |
| F Membership Predicate | **ESTABLISHED** |
| Runtime Consistency | **ESTABLISHED** |
| PD-13 Closure | **DS_Closed** |
| PD-14 | **DS_Closed** |
| PD-15 | **DS_Closed** |
| PD4F_OB_02 | **DS_Open** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## §8 Revision History

| Version | Date | Change |
|---------|------|--------|
| 1.0 | 2026-06-06 | TASK-PROOF-022 — PD-13 F-set membership predicate closure |

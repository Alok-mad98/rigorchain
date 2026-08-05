# SPEC-007: RIGOR Equivalence Boundaries and Future Closure Conditions

**Task:** `TASK-SPEC-008`  
**Track:** Formal Specification Program  
**Status:** ESTABLISHED  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Parents:** SPEC-000 … SPEC-006  
**Normative sources:** Phase-5 equivalence catalogs; `MAINNET_PROTOCOL_FREEZE_DECLARATION.md`  
**Proof obligation registry:** [PROOF-009](../protocol/proof/PROOF-009-PROOF-OBLIGATION-REGISTRY.md) (operationalizes §7 pending items)  
**Freeze authority:** `TASK-MAINNET-FREEZE-001`

---

## Document Role

This document defines the **remaining boundaries** for Coq ≡ Runtime equivalence
claims and Phase-5 closure conditions. It specifies:

- What equivalence means (and what it does not)
- Per-layer CES / ECS status and blocking reasons
- Permanent draw impossibility (FVR-M)
- Pending proof obligations (PD-4F-OB-02, PD-4F-CPP-01)
- Conditions under which equivalence **may eventually** be claimed (scoped)
- Proofs and claims that are **allowed**, **prohibited**, or **pending**

This document is specification-only:

- No code changes
- No RPC changes
- No consensus modifications
- No new proofs
- **Equivalence Claimed:** false

---

## 1. Purpose

**Equivalence** in the RIGOR formal program means a machine-checked statement that
a Coq refinement of the specification produces identical observable results to
runtime witnesses on all inputs in a declared domain.

**Current global status:**

| Claim | Status |
|-------|--------|
| **Coq ≡ Runtime** (global) | **NOT ESTABLISHED** |
| **Equivalence Claimed** | **false** |
| **Draw equivalence** | **PROHIBITED** (permanent under current architecture) |
| **Supply-field equivalence** | **PENDING** (candidate only; blocked by proof debt) |

Phase-5 layers (FVR-I through FVR-N) provide **alignment evidence** and
**closure preconditions**. They do not constitute equivalence proofs.

This document is the authoritative boundary for any future equivalence claim.
It supersedes informal equivalence language in Phase-5 audit docs for
normative classification only; it does not alter those artifacts.

---

## 2. Candidate Equivalence Mapping (FVR-I .. FVR-J)

### 2.1 FVR-I: Runtime witness candidate layer

**Role:** Classify which runtime witness fields are **theoretically comparable**
to Coq catalog rows — without asserting equivalence.

**Inputs:** Runtime supply witness (FVR-F) + Coq catalog row (FVR-G) + CHAS
alignment report (FVR-H).

**Output:** `WitnessEquivalenceCandidate` with per-field `CandidateEquivalenceStatus`
(CES) and stable `blocking_reasons`.

| CES status | Meaning | Current prevalence |
|------------|---------|------------------|
| `CES_Comparable` | Preconditions for future comparison met | Supply fields at non-genesis heights with parent delta |
| `CES_PartiallyComparable` | Scalars present; parent delta or linkage gap | Genesis height; neutral CHAS |
| `CES_NotComparable` | Missing witness or catalog row | Absent history |
| `CES_FundamentallyBlocked` | Invariant failure, broken chain, or Draw component | Draw always blocked |

**Per-field CES mapping (FVR-I):**

| Field | Maps to spec | Potential Coq closure rule | Current status |
|-------|-------------|---------------------------|----------------|
| `oru_supply_status` | `S_ORU(h)` / CI-021 | `rs_oru_supply` refinement lemma | **PARTIAL** (comparable at sampled heights) |
| `res_supply_status` | `S_RES(h)` / CI-022 | `rs_res_supply` refinement lemma | **PARTIAL** |
| `invariant_status` | CI-* observation | FVR-E pass + witness chain valid | **ACTIVE** |
| `witness_chain_status` | CI-013 step | Parent→child witness linkage | **ACTIVE** |
| Draw (implicit) | CI-030..CI-033 | Draw refinement | **BLOCKED** (permanent) |

**FVR-I layer status:** **ESTABLISHED** (candidate analysis ACTIVE).

---

### 2.2 FVR-J: Coq formal candidate layer

**Role:** Machine-checkable Coq representation of FVR-I analysis. Exports
`CandidateEquivalenceObservation` for offline tooling.

**Coq closure rules (potential, not proven):**

| Rule | Statement shape | Allowed today? |
|------|----------------|----------------|
| `check_candidate_equivalence` | Witness × CoqRow × CHAS → CES per field | **YES** (classification only) |
| `supply_scalar_comparable` | `rs_oru_supply` / `rs_res_supply` present both sides | **YES** |
| `coq_equiv_runtime_supply` | ∀h, runtime supply = Coq supply | **PROHIBITED** |
| `coq_equiv_runtime_global` | Full state equivalence | **PROHIBITED** |

**Frozen catalog partition (FVR-J):** 8 Comparable / 5 PartiallyComparable /
3 NotComparable / 4 FundamentallyBlocked field cells (20 total).

**FVR-J layer status:** **ESTABLISHED** (Coq catalog ACTIVE).

**Binding:** `equivalence_claimed` is **always false** in FVR-I and FVR-J outputs.

---

## 3. Global Candidate Equivalence Mapping (FVR-K)

**Role:** Aggregate FVR-H CHAS report and FVR-J candidate observation into
`GlobalCandidateEquivalenceObservation` per block.

**Merge rule:** `merge_ces` takes the **stricter** of CHAS field lift and
candidate supply/context statuses.

| Input condition | Global ORU/RES CES |
|-----------------|-------------------|
| CHAS partial + candidate comparable + valid chain | `CES_PartiallyComparable` |
| Genesis (no parent delta) | `CES_PartiallyComparable` |
| Broken witness chain | `CES_FundamentallyBlocked` |
| Draw component present | `CES_FundamentallyBlocked` |

**Frozen partition (FVR-K):** 0 Comparable / 4 PartiallyComparable / 0 NotComparable /
6 FundamentallyBlocked (10 global field cells).

**Potential Coq closure rules:**

| Rule | Scope | Status |
|------|-------|--------|
| `global_supply_partial_alignment` | ORU/RES cumulative scalars only | **PENDING** |
| `global_draw_alignment` | Draw winner / eligibility | **PROHIBITED** (FVR-M) |
| `global_state_alignment` | Full `State(h)` | **PROHIBITED** |

**FVR-K layer status:** **ESTABLISHED** (global candidate ACTIVE).

---

## 4. Closure Conditions and ECS (FVR-L)

**Role:** Define when a Coq ≡ Runtime proof is **permitted to be attempted** —
the proof-permission boundary. FVR-L does **not** prove equivalence.

### 4.1 EquivalenceClosureCondition gates

`closure_allowed` is the conjunction of seven boolean gates:

| Gate | Source | Required for closure attempt |
|------|--------|------------------------------|
| `has_runtime_witness_chain` | FVR-F | YES |
| `has_invariant_pass` | FVR-E | YES |
| `has_candidate_equivalence_layer` | FVR-J | YES |
| `has_global_candidate_equivalence_layer` | FVR-K | YES |
| `has_chas_alignment_layer` | FVR-H | YES |
| `has_coq_catalog_alignment_layer` | FVR-G + FVR-M integration | YES |
| `no_fundamentally_blocked_draw` | Draw proof gate | YES — **permanently false** |

### 4.2 EquivalenceClosureStatus (ECS)

| ECS | Meaning | Current global status |
|-----|---------|----------------------|
| `ECS_Open` | Missing prerequisite layers | Subsumed by draw block |
| `ECS_PartiallyClosable` | Layers present; draw or debt blocks | **Not reachable** (draw always blocks) |
| `ECS_Closable` | `closure_allowed = true` | **Never** under current architecture |
| `ECS_Prohibited` | Fundamental block (draw / invariant / chain) | **CURRENT** (all samples) |

**Frozen catalog:** All FVR-L samples are `ECS_Prohibited` because
`DRAW-CES-FundamentallyBlocked` is always present.

### 4.3 What ECS permits and prohibits

| Action | ECS = Prohibited | ECS = Closable (hypothetical) |
|--------|------------------|-------------------------------|
| Attempt supply-field refinement proof | **ALLOWED** (scoped, offline) | ALLOWED |
| Claim `coq_equiv_runtime := true` globally | **PROHIBITED** | Still requires proof + debt closure |
| Claim draw equivalence | **PROHIBITED** | **PROHIBITED** (FVR-M) |
| Modify consensus from closure status | **PROHIBITED** | **PROHIBITED** |

**FVR-L layer status:** **ESTABLISHED** (closure catalog ACTIVE; closure never allowed).

---

## 5. Draw Closure Impossibility (FVR-M)

**Role:** Prove that Draw **cannot** satisfy the Phase-5 equivalence closure
predicate under the **current RIGOR architecture**.

### 5.1 Main result (PROVEN, permanent)

```
draw_permanently_non_closable
```

**Theorem:** `Current_Draw_Architecture → draw_closure_status = DCS_Impossible`

This is a **permanent architectural boundary** unless a Level-5 hard-fork
introduces new runtime artifacts (canonical draw encoder, hash kernel, trace
history) — which would itself be a governance event, not a proof patch.

### 5.2 Missing requirements (frozen)

| Requirement | Status |
|-------------|--------|
| `has_draw_canonical_encoder` | **false** |
| `has_runtime_hash_kernel` | **false** |
| `has_runtime_trace` | **false** |
| `has_runtime_transition_history` | **false** |
| `has_runtime_accept_history` | **false** |

### 5.3 Implications for equivalence

| Claim | Status |
|-------|--------|
| Draw Coq ≡ Runtime | **PROHIBITED** (permanent) |
| Global Coq ≡ Runtime (including draw) | **PROHIBITED** |
| Scoped supply equivalence (excluding draw) | **PENDING** (not blocked by FVR-M alone) |
| `no_fundamentally_blocked_draw` gate | **Permanently false** → `closure_allowed` never true |

**FVR-M layer status:** Draw closure impossibility **PROVEN (permanent)**.

**Draw CES:** Always `CES_FundamentallyBlocked` with reason `DRAW-CES-FundamentallyBlocked`.

---

## 6. Phase-5 Equivalence Summary (FVR-N)

**Role:** Canonical snapshot consolidating FVR-A..M layer status; links to
protocol freeze without modifying consensus.

**Honesty boundary (frozen):**

| Field | Value |
|-------|-------|
| Coq ≡ Runtime | **NOT ESTABLISHED** |
| Equivalence Claimed | **false** |
| PD-4F-OB-02 | **OPEN** |
| PD-4F-CPP-01 | **OPEN** (PARTIAL) |
| Consensus / Validation / State Hash impact | **NONE** |

**Layer partition:** 1 ESTABLISHED / 7 ACTIVE / 0 PARTIAL / 0 BLOCKED / 1 IMPOSSIBLE.

**FVR-N does not upgrade any equivalence status.** It documents the boundary.

---

## 7. Pending Proof Items (PD-4F-OB-02 / PD-4F-CPP-01)

These proof-debt items block full equivalence claims. They are **documented** here
as specification-level blockers; closing them requires Coq work outside this task.

**Authoritative tracking:** lifecycle states (`PO_OPEN`, `PO_PARTIAL`, `PO_BLOCKED`,
`PO_PROVEN`, `PO_RETIRED`), dependency graph, and closure criteria are maintained
in [PROOF-009: Proof Obligation Registry](../protocol/proof/PROOF-009-PROOF-OBLIGATION-REGISTRY.md).
This section remains the normative boundary; PROOF-009 operationalizes it without
redefining equivalence limits.

### 7.1 PD-4F-OB-02: Coq ≡ frozen runtime behaviour

| Attribute | Value |
|-----------|-------|
| **Statement** | Coq model outputs match frozen runtime consensus behaviour on all declared inputs |
| **Status** | **OPEN** |
| **Blocks** | Global `coq_equiv_runtime`; any unscoped equivalence claim |
| **Evidence so far** | Golden vectors; partial draw representation closure; no full bridge proof |
| **Prohibited until closed** | `equivalence_claimed := true`; "Coq ≡ C++" in any audit verdict |

### 7.2 PD-4F-CPP-01: Frozen runtime observations imported but not fully compared

| Attribute | Value |
|-----------|-------|
| **Statement** | Imported frozen-runtime trace components agree with Coq trace on all components |
| **Status** | **PARTIAL** (compared; Draw component disagrees) |
| **Blocks** | Draw-scoped equivalence; contributes to PD-4F-OB-02 |
| **Evidence so far** | Per-component compare (3 agree / 1 disagree / 1 missing on Draw) |
| **Prohibited until closed** | Draw equivalence; silent PD-4F-OB-02 closure |

### 7.3 Interaction with trust-anchor open items

From [SPEC-004](./SPEC-004-TRUST-ANCHORS.md), these also block equivalence:

| ID | Blocks |
|----|--------|
| A-1 (σ endianness) | Draw eligibility alignment |
| A-2 (uint512 bound) | Winner selection alignment |
| A-3 (F membership) | Candidate set alignment |
| A-4 (SHA-256d) | Hash-dependent fields |

**Equivalence may not be claimed while A-1..A-3 remain OPEN** for any theorem
depending on those domains.

### 7.4 Allowed vs prohibited proof activity (summary)

| Activity | Status |
|----------|--------|
| Supply-field refinement lemmas (scoped) | **ALLOWED** |
| CI-* implication theorems (offline) | **ALLOWED** |
| Candidate CES classification (FVR-I/J/K) | **ALLOWED** |
| Closure gate evaluation (FVR-L) | **ALLOWED** |
| Draw equivalence proof | **PROHIBITED** (FVR-M permanent) |
| Global `coq_equiv_runtime` claim | **PROHIBITED** |
| Consensus change justified by alignment | **PROHIBITED** |
| PD-4F-OB-02 closure via observational export alone | **PROHIBITED** |

---

## 8. Coq ↔ Runtime Equivalence Boundary (NOT ESTABLISHED)

### 8.1 Global boundary

```
Coq ≡ Runtime  :=  NOT ESTABLISHED
equivalence_claimed  :=  false
```

No specification-layer document, Phase-5 export, or FVR verdict may override this
global boundary without a published equivalence proof that satisfies §9.

### 8.2 Scoped equivalence (future-permitted shape)

The **only** equivalence shape that may eventually be claimed without
architectural change is **scoped supply alignment**:

```
∀ h ∈ ValidHeights:
  runtime.rs_oru_supply(h) = coq.rs_oru_supply(h)
  ∧ runtime.rs_res_supply(h) = coq.rs_res_supply(h)
```

Subject to:

1. PD-4F-OB-02 closed for supply domain
2. Trust anchors A-1..A-4 resolved or explicitly assumed
3. FVR-M draw exclusion acknowledged in theorem statement
4. No consensus predicate reads runtime mirror

### 8.3 Permanently excluded from equivalence

| Domain | Reason |
|--------|--------|
| **Draw** (winner, eligibility, σ) | FVR-M proven impossible |
| **Full State(h)** | Draw + UTXO + partial mappings |
| **Consensus validity** | Runtime does not define validity |
| **Mempool / peer state** | Non-consensus |

### 8.4 Anti-equivalence safety

The following MUST NOT occur:

- Setting `equivalence_claimed = true` in any export without a matching Coq theorem
- Treating `ECS_PartiallyClosable` as equivalence
- Treating `CES_Comparable` as equality
- Using FVR-E pass as proof of CI-*
- Retroactive spec amendment from runtime alignment

---

## 9. Verification Hierarchy and Future Closure

### 9.1 Current hierarchy

```
Specification (SPEC-000 … SPEC-007)     ← ESTABLISHED
        ↓
Formal Models                             ← ESTABLISHED
        ↓
Coq Definitions                           ← ESTABLISHED (partial)
        ↓
Theorems                                  ← PARTIAL (admits; proof debt)
        ↓
Runtime Alignment (FVR)                   ← ACTIVE
        ↓
Candidate Equivalence (FVR-I, J, K)       ← ACTIVE (CES classification)
        ↓
Closure Gate (FVR-L)                        ← PROHIBITED (draw block)
        ↓
Future Scoped Equivalence Proof             ← NOT STARTED
```

### 9.2 Conditions for eventual equivalence claim

Equivalence **may** be claimed only when **all** of the following hold:

1. **Scoped theorem published** — explicit domain (supply-only initially)
2. **PD-4F-OB-02 closed** for that domain
3. **Trust anchors A-1..A-3 closed** or cited as assumptions in theorem
4. **FVR-M acknowledged** — theorem explicitly excludes draw
5. **SPEC-007 updated** — boundary document revised with proof reference
6. **No consensus impact** — equivalence remains observational
7. **Level-5 governance** — not required for supply-only equivalence; **required**
   for any draw-architecture change that could alter FVR-M

### 9.3 Conditions that never permit global equivalence (current architecture)

- FVR-M draw impossibility remains PROVEN
- Runtime mirrors remain non-authoritative
- Protocol freeze unchanged

Global `Coq ≡ Runtime` including draw is **permanently prohibited** under
current architecture without a constitutional hard-fork.

---

## 10. Freeze Compatibility

This document is **specification only**. It documents equivalence boundaries; it
does not modify consensus, monetary policy, or protocol freeze.

| Guarantee | Status |
|-----------|--------|
| No consensus modifications | **Confirmed** |
| No monetary-policy modifications | **Confirmed** |
| No protocol-freeze modifications | **Confirmed** |

Nothing in this document supersedes `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` or
`TASK-MAINNET-FREEZE-001`.

---

## 11. Task Output Status

| Field | Status |
|-------|--------|
| Equivalence Boundaries Specification | **ESTABLISHED** |
| Candidate Equivalence Mapping | **ESTABLISHED** |
| Global Equivalence Mapping | **ESTABLISHED** |
| Closure Condition Mapping | **ESTABLISHED** |
| Draw Closure | **PROVEN (permanent)** |
| Phase-5 Equivalence Summary | **ESTABLISHED** |
| Pending Proof Items | **DOCUMENTED** |
| Coq ≡ Runtime | **NOT ESTABLISHED** |
| Equivalence Claimed | **false** |
| Consensus Impact | **NONE** |

---

## 12. Revision History

| Version | Date | Change |
|---------|------|--------|
| 0.1 | 2026-06-05 | `TASK-SPEC-008` — initial equivalence boundaries specification |
| 0.2 | 2026-06-05 | Cross-reference PROOF-009 proof obligation registry (TASK-PROOF-010) |

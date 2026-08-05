# PROOF-000: Golden Consensus Vector Framework

**Task:** `TASK-PROOF-001`  
**Track:** Formal Verification Program — Proof Infrastructure Layer  
**Status:** ESTABLISHED  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Parents:** SPEC-000 … SPEC-007  
**Normative sources:** `RFC-000`, `RFC-008`, `RFC-014`, `RFC-015`  
**Proof obligations:** tracked in [PROOF-009](./PROOF-009-PROOF-OBLIGATION-REGISTRY.md)  
**Freeze authority:** `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` / `TASK-MAINNET-FREEZE-001`

---

## Document Role

This document establishes the **canonical Golden Consensus Vector framework** for
the RIGOR formal verification program. Golden vectors are the authoritative
comparison mechanism between specification intent, formal models, Coq proof
models, and the Bitcoin-v25-derived runtime implementation under **identical
consensus inputs**.

This document is **proof infrastructure only**:

- No consensus modifications
- No validation modifications
- No state_hash modifications
- No runtime witness modifications
- **Equivalence Claimed:** false

---

## 1. Purpose

**Golden Consensus Vectors** are frozen, normative test cases that instantiate
consensus behavior under declared inputs. They serve as the single comparison
oracle across four layers:

```
Specification (SPEC-000 … SPEC-007)
        ↓ instantiates
Golden Consensus Vectors
        ↓ compared against
Formal Model (docs/formal/)
        ↓ compared against
Coq Proof Model (formal_verification/coq/)
        ↓ compared against
Bitcoin-v25 Runtime (core/ — RIGOR consensus fork)
```

**Golden vectors answer one question:** Given identical consensus inputs, do all
four layers produce the same declared outputs?

They do **not** answer whether Coq ≡ Runtime globally. That claim remains
**NOT ESTABLISHED** (SPEC-007 §8). Vectors are comparison fixtures and proof
targets — not equivalence certificates. Proof obligations tracked in
[PROOF-009](./PROOF-009-PROOF-OBLIGATION-REGISTRY.md) (e.g. `PD-4F-OB-02`) remain
**OPEN** until machine-checked discharge; vector MATCH does not close them.

**Relationship to prior vector corpora:**

| Corpus | Role under this framework |
|--------|---------------------------|
| `formal_verification/golden_vectors.json` | Legacy draw-kernel regression (σ, C, winner) |
| `formal_verification/phase3_golden_vectors.json` | PHASE-3 executable alignment corpus |
| `ConsensusGoldenVectorsData.v` (73 registry slots) | Coq-encoded trace cases (evidence; not normative) |
| **PROOF-000 registry** (`GV-*`) | **Authoritative** canonical vector namespace |

Prior corpora remain alignment evidence until migrated into the PROOF-000
registry. This framework does not retroactively elevate legacy JSON to
normative status.

---

## 2. Vector Categories

Each category maps to specification objects and proof obligations. Category
identifiers are stable; individual vector IDs are assigned in §8.

### GV-001 — Header Validation

**Scope:** Block header acceptance/rejection under CR-001..CR-020 predicates.

| Maps to | Objects |
|---------|---------|
| SPEC-003 | CR-001 (version), CR-002 (prev hash), CR-003 (MTP), CR-004..CR-010 (PoW/ASERT), CR-011..CR-020 (structural) |
| SPEC-005 | CI-001 (chain continuity), CI-003 (deterministic validation) |
| Trust anchors | TA-POW, TA-CLOCK |

**Typical inputs:** Parent tip context, candidate header bytes, chainwork state.  
**Typical outputs:** Accept/reject; rejection reason code; no state mutation on reject.

---

### GV-002 — ORU Supply Transition

**Scope:** ORU subsidy and burn recurrence across a single valid block transition.

| Maps to | Objects |
|---------|---------|
| SPEC-002 | ORU subsidy recurrence, burn component `C(h)` |
| SPEC-005 | CI-020 (non-negativity), CI-021 (recurrence preservation) |
| Coq families | `oru_supply_equation_theorems`, `cb_oru_mint_delta`, `cb_oru_burn_delta` |

**Typical inputs:** `State(h)` with `S_ORU(h)`, block containing ORU-affecting txs.  
**Typical outputs:** `S_ORU(h+1)`, per-block subsidy/burn deltas.

---

### GV-003 — RES Mint Transition

**Scope:** RES mint eligibility, at-most-one mint, and cap enforcement per block.

| Maps to | Objects |
|---------|---------|
| SPEC-002 | RES mint recurrence, cap `MAX_RES_SUPPLY` |
| SPEC-005 | CI-022 (recurrence), CI-023 (cap), CI-024 (single mint) |
| Coq families | `res_supply_equation_theorems`, `res_cap_theorems`, `cb_res_mint_delta` |

**Typical inputs:** `State(h)` with `S_RES(h)` and draw context, block with RES mint candidate.  
**Typical outputs:** `S_RES(h+1)`, mint count `M(h) ∈ {0,1}`, cap proximity flags.

---

### GV-004 — Draw Eligibility

**Scope:** Candidate set membership, σ computation, and eligibility inequality.

| Maps to | Objects |
|---------|---------|
| SPEC-003 | CR-030..CR-039 (draw predicates) |
| SPEC-005 | CI-030 (draw determinism) |
| Trust anchors | TA-DRAW (A-1, A-2, A-3 open) |
| Coq families | `draw_determinism_theorems`, `KernelEligibility` |

**Typical inputs:** Fee set `F`, convergence root `C`, bind_commit, frozen target `T`, height `h`.  
**Typical outputs:** Per-txid eligibility bitvector; σ values; inequality results.

**Note:** Draw equivalence proofs remain **PROHIBITED** (FVR-M). GV-004 vectors
support classification and refinement only — not draw closure claims.

---

### GV-005 — Draw Winner Selection

**Scope:** Deterministic winner selection including tie-break (RFC-000 §9).

| Maps to | Objects |
|---------|---------|
| SPEC-003 | CR-040..CR-045 (winner selection) |
| SPEC-005 | CI-031 (single winner), CI-033 (valid mint requires valid draw) |
| Coq families | `selection_uniqueness_theorems`, `KernelSelection`, `COQ_SELECTION_CLOSURE` |

**Typical inputs:** Eligible candidate set, σ cross-products, txid ordering.  
**Typical outputs:** Winner txid or `NONE`; tie-break path taken.

**Note:** Same FVR-M prohibition applies. Vectors compare outputs; they do not
establish draw equivalence.

---

### GV-006 — State Transition

**Scope:** Full consensus state successor `State(h) → State(h+1)` for a valid block.

| Maps to | Objects |
|---------|---------|
| SPEC-001 | `State(h)`, transition function, consensus state partition |
| SPEC-005 | CI-012 (deterministic state transition) |
| Coq families | `state_transition_theorems`, `Transition.v`, `ConnectBlock` |

**Typical inputs:** Complete `State(h)`, valid block `B(h+1)`.  
**Typical outputs:** `State(h+1)` projection (supply, UTXO digest, height, last hash).

---

### GV-007 — Replay Determinism

**Scope:** Identical block prefix yields identical terminal state.

| Maps to | Objects |
|---------|---------|
| SPEC-000 | CO-5 (replay determinism) |
| SPEC-005 | CI-013 (replay equivalence) |
| Coq families | `replay_determinism_theorems`, `ReplayStateEncodingTheorem` |

**Typical inputs:** Genesis `State(0)`, ordered block sequence `B₁..Bₙ`.  
**Typical outputs:** `State(n)` at each checkpoint height; cross-platform identity.

---

### GV-008 — Reorg Determinism

**Scope:** Reorganization to an alternate valid chain produces deterministic state.

| Maps to | Objects |
|---------|---------|
| SPEC-000 | CO-6 (reorg determinism) |
| SPEC-005 | CI-014 (reorg consistency) |
| Coq families | `reorg_consistency_theorems`, `ReorgStateEncodingTheorem` |

**Typical inputs:** Common ancestor state, competing branch sequences.  
**Typical outputs:** Post-reorg `State(h)`; supply preservation under rollback.

---

## 3. Vector Structure

Every Golden Consensus Vector MUST conform to the canonical record below.
Field presence is category-dependent; absent fields are marked `N/A` for that
category.

### 3.1 Canonical record

```
GoldenVector := {
  id              : VectorId          (* e.g. GV-ORU-0001 *)
  category        : GVCategory        (* GV-001 .. GV-008 *)
  status          : VectorStatus      (* RESERVED | DRAFT | ESTABLISHED *)
  spec_refs       : list SpecRef      (* CR-*, CI-*, SPEC section *)
  input           : VectorInput
  state_before    : StateProjection
  state_after     : StateProjection
  invariants      : InvariantExpectations
  witnesses       : WitnessExpectations
  supply          : SupplyExpectations
  draw            : DrawExpectations
  acceptance      : AcceptanceResult
}
```

### 3.2 Field definitions

| Field | Type | Description |
|-------|------|-------------|
| **Input** | `VectorInput` | Declared consensus inputs: block bytes, parent context, height, target, fee set, txids, and any category-specific parameters. Inputs are **byte-exact** where the spec requires bytes (RFC-008). |
| **Expected State Before** | `StateProjection` | Minimal consensus state at height `h` before applying the vector action. Includes `S_ORU(h)`, `S_RES(h)`, height, prev hash; UTXO projection as digest or explicit set per category. |
| **Expected State After** | `StateProjection` | Minimal consensus state at `h+1` (or terminal height for replay/reorg vectors). MUST be uniquely determined by spec + input. |
| **Expected Invariants** | `InvariantExpectations` | CI-* predicates that MUST hold on `state_after`. Listed as stable ids (CI-020, CI-021, …). Failures are vector violations, not runtime redefinitions. |
| **Expected Witnesses** | `WitnessExpectations` | Observational fields FVR layers would export **if** the vector is executed on a conforming runtime. Witnesses are expected evidence shapes — not authoritative truth. |
| **Expected Supply Values** | `SupplyExpectations` | `oru_supply`, `res_supply`, per-block deltas, mint count. Maps to FVR-B/FVR-F field names for comparison tooling. |
| **Expected Draw Values** | `DrawExpectations` | `C`, σ per candidate, eligibility bits, winner txid. `N/A` for non-draw categories. |
| **Expected Acceptance Result** | `AcceptanceResult` | `ACCEPT` or `REJECT(reason)` per SPEC-003 failure model. Header vectors may be reject-only. |

### 3.3 Comparison semantics

For each vector `V` and layer `L ∈ {Spec, Formal, Coq, Runtime}`:

```
compare(V, L) :=
  run(L, V.input) =?= V.expected_outputs
```

All four layers MUST agree on `V.expected_outputs` when `V.status = ESTABLISHED`.
Disagreement is a **layer defect** to be classified — not a vector amendment —
unless a Level-5 governance event updates the specification.

### 3.4 Status lifecycle

| Status | Meaning |
|--------|---------|
| **RESERVED** | ID allocated; no values committed |
| **DRAFT** | Values proposed; not yet normative |
| **ESTABLISHED** | Values frozen; normative for all layers |

---

## 4. Vector Authority

Golden Consensus Vectors are **normative** within the proof infrastructure layer.

| Rule | Statement |
|------|-----------|
| **V-001** | Vectors instantiate SPEC-000 … SPEC-007 behavior; they do not replace specifications. |
| **V-002** | When `status = ESTABLISHED`, implementation MUST conform to vector outputs on declared inputs. |
| **V-003** | Coq proof models MUST conform to vector outputs; vector failure is proof failure, not vector revision. |
| **V-004** | Runtime witnesses MUST conform to vector expected witness shapes when run on conforming inputs. |
| **V-005** | Vectors MUST NOT be amended from runtime observations without spec governance. |
| **V-006** | Legacy corpora (PHASE-3 JSON, draw-kernel JSON) are evidence until promoted to PROOF-000 `ESTABLISHED`. |

**Authority hierarchy:**

```
Specification (SPEC-000 … SPEC-007)     ← highest
        ↓
Golden Consensus Vectors (PROOF-000)      ← normative fixtures
        ↓
Formal Model / Coq / Runtime              ← must conform
        ↓
Runtime Witnesses (FVR)                   ← observational evidence only
```

**Violation:** Treating a runtime witness export or Coq catalog row as grounds to
change an `ESTABLISHED` vector without specification amendment.

---

## 5. Relationship To Runtime Witnesses

The Formal-Friendly Runtime Refactor track (FVR-A through FVR-N) produces
**observational evidence** on post-connect execution. Golden vectors define what
that evidence **should** look like — witnesses never define vectors.

| Principle | Rule |
|-----------|------|
| **GV-W-001** | FVR layers observe; vectors expect. |
| **GV-W-002** | `Expected Witnesses` in a vector is a shape/value template for FVR-B, FVR-E, FVR-F exports. |
| **GV-W-003** | FVR pass on a mirror does not establish vector conformance for all inputs. |
| **GV-W-004** | FVR failure does not amend vector expected values. |
| **GV-W-005** | Witnesses MUST NOT participate in validation (constitutional rule 2). |
| **GV-W-006** | `equivalence_claimed` remains **false** in all witness exports regardless of vector match. |

**Mapping (informative):**

| Vector field | FVR layer | Witness field (informative) |
|--------------|-----------|----------------------------|
| Supply values | FVR-B, FVR-F | `rs_oru_supply`, `rs_res_supply` |
| Invariants | FVR-E | ORU-L* / RES-L* stable ids |
| Witness chain | FVR-F | Parent→child linkage |
| Draw values | FVR-M (blocked) | Not closable; vectors may still declare expected draw for kernel comparison |

---

## 6. Relationship To Coq

Golden vectors are **not proofs**. They are **proof targets**.

| Principle | Rule |
|-----------|------|
| **GV-C-001** | A Coq theorem may cite vector `V` as a refinement obligation: `∀ input = V.input, f input = V.output`. |
| **GV-C-002** | Vector conformance checked by `Compute`, extraction harness, or `vm_compute` is **evidence** — not a substitute for a general theorem. |
| **GV-C-003** | Closing PD-12 (Coq draw ≡ golden vectors) requires per-vector checks against this registry — not legacy JSON alone. |
| **GV-C-004** | Coq `admit` debt is not reduced by informal vector agreement. |
| **GV-C-005** | No Coq theorem may set `equivalence_claimed := true`. |

**Proof target shape (example):**

```coq
(* Target — not yet proven globally *)
Lemma gv_oru_0001_refinement :
  forall s b,
    V_input gv_oru_0001 s b ->
    apply_block s b = V_state_after gv_oru_0001.
```

Individual vector refinements may be proven in Phases B–G without establishing
global Coq ≡ Runtime equivalence.

---

## 7. Relationship To Specifications

SPEC-000 through SPEC-007 define **what RIGOR should do**. Golden vectors
**instantiate** that behavior on concrete inputs.

| Spec | Vector instantiation |
|------|---------------------|
| **SPEC-000** | Consensus objectives CO-1..CO-7 → replay/reorg/header vectors |
| **SPEC-001** | `State(h)` projections in `state_before` / `state_after` |
| **SPEC-002** | ORU/RES supply expectations in GV-002, GV-003 |
| **SPEC-003** | CR-* predicates → acceptance result; draw vectors GV-004, GV-005 |
| **SPEC-004** | Trust anchor assumptions cited in `spec_refs`; open A-1..A-5 block equivalence claims |
| **SPEC-005** | CI-* listed in `Expected Invariants` |
| **SPEC-006** | Layer mapping table routes each category to theorem families |
| **SPEC-007** | Equivalence boundary: vector match ≠ equivalence; draw vectors non-closable |

**Rule:** Specifications are amended only through specification governance
(`TASK-SPEC-*`). Vectors follow specification changes — not the reverse.

---

## 8. Initial Vector Registry

The following vector IDs are **RESERVED**. No input or expected values are
committed in this task. Population occurs in Proof Roadmap Phases B–E.

### ORU supply vectors (GV-002)

| ID | Category | Status | Description |
|----|----------|--------|-------------|
| **GV-ORU-0001** | GV-002 | **RESERVED** | Genesis-adjacent ORU subsidy transition |
| **GV-ORU-0002** | GV-002 | **RESERVED** | ORU burn recurrence with non-zero `C(h)` |
| **GV-ORU-0003** | GV-002 | **RESERVED** | Multi-block ORU supply accumulation |

### RES mint vectors (GV-003)

| ID | Category | Status | Description |
|----|----------|--------|-------------|
| **GV-RES-0001** | GV-003 | **RESERVED** | First RES mint on eligible draw |
| **GV-RES-0002** | GV-003 | **RESERVED** | RES cap boundary approach |
| **GV-RES-0003** | GV-003 | **RESERVED** | Block with zero RES mint (draw ineligible) |

### Draw vectors (GV-004, GV-005)

| ID | Category | Status | Description |
|----|----------|--------|-------------|
| **GV-DRAW-0001** | GV-004 | **RESERVED** | Empty fee set — no eligible candidates |
| **GV-DRAW-0002** | GV-005 | **RESERVED** | Two-tx tie-break (RFC-000 §9) |
| **GV-DRAW-0003** | GV-005 | **RESERVED** | Multi-tx winner selection |

### State / replay vectors (GV-006, GV-007, GV-008)

| ID | Category | Status | Description |
|----|----------|--------|-------------|
| **GV-STATE-0001** | GV-006 | **RESERVED** | Single-block state transition |
| **GV-STATE-0002** | GV-007 | **RESERVED** | Three-block replay checkpoint |
| **GV-STATE-0003** | GV-008 | **RESERVED** | Shallow reorg with supply preservation |

**Registry summary:** 12 IDs reserved; 0 ESTABLISHED; 0 DRAFT.

Header validation vectors (GV-001) are not pre-allocated in this task; IDs
`GV-HDR-0001` … will be assigned in a subsequent proof task.

---

## 9. Proof Roadmap

Phases are sequential within the Proof Infrastructure Layer. No phase establishes
global equivalence.

### Phase A — Vector Framework ✅ (this task)

- Publish PROOF-000
- Define categories GV-001..GV-008
- Initialize reserved registry
- **Deliverable:** `docs/proof/PROOF-000-GOLDEN-VECTORS.md`

### Phase B — ORU vectors

- Populate GV-ORU-0001..0003 from SPEC-002 recurrence
- Cross-check against `ConnectBlockMint` / supply scaffold
- Promote to DRAFT → ESTABLISHED under review

### Phase C — RES vectors

- Populate GV-RES-0001..0003 including cap and single-mint cases
- Align with CI-022..CI-024

### Phase D — Draw vectors

- Populate GV-DRAW-0001..0003 (kernel-level; not equivalence)
- Migrate relevant PHASE-3 / PHASE-4A draw rows where applicable
- Acknowledge FVR-M: draw vectors support comparison, not closure

### Phase E — State vectors

- Populate GV-STATE-0001..0003
- Add GV-HDR and replay/reorg depth as needed

### Phase F — Runtime ↔ Vector comparison

- Harness: run Bitcoin-v25 runtime on vector inputs; diff against expected
- FVR exports as observational corroboration (not authoritative)
- **Does not set** `equivalence_claimed`

### Phase G — Coq ↔ Vector comparison

- Per-vector `Compute` / extraction checks (PD-12 family)
- General refinement theorems where provable
- **Does not set** `equivalence_claimed`

---

## 10. Freeze Compatibility

This document is **proof infrastructure only**. It defines comparison fixtures and
registry namespace; it does not modify frozen protocol semantics.

| Guarantee | Status |
|-----------|--------|
| No consensus modifications | **Confirmed** |
| No validation modifications | **Confirmed** |
| No state_hash modifications | **Confirmed** |
| No runtime witness modifications | **Confirmed** |
| No monetary-policy modifications | **Confirmed** |
| No protocol-freeze modifications | **Confirmed** |

Nothing in this document supersedes `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` or
`TASK-MAINNET-FREEZE-001`.

---

## Task Output Status

| Field | Status |
|-------|--------|
| Golden Vector Framework | **ESTABLISHED** |
| Vector Categories | **ESTABLISHED** |
| Vector Registry | **INITIALIZED** |
| Proof Infrastructure | **ESTABLISHED** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## Revision History

| Version | Date | Change |
|---------|------|--------|
| 0.1 | 2026-06-05 | `TASK-PROOF-001` — Golden Consensus Vector framework established; registry initialized (12 RESERVED) |
| 0.2 | 2026-06-05 | Cross-reference PROOF-009 proof obligation registry (TASK-PROOF-010) |

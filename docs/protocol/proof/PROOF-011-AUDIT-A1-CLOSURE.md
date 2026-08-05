# PROOF-011: Audit Item A-1 Closure (σ Byte Order)

**Task:** `TASK-PROOF-012`  
**Track:** Formal Verification Program — Proof Infrastructure Layer  
**Status:** ESTABLISHED (closure procedure); audit item **`AI_PARTIAL`**  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Parents:** [SPEC-004](../../specification/SPEC-004-TRUST-ANCHORS.md), [PROOF-010](./PROOF-010-AUDIT-ITEM-CLOSURE-FRAMEWORK.md)  
**Runtime witness:** [PROOF-012](./PROOF-012-A1-RUNTIME-OBSERVATION.md) (`TASK-PROOF-013`); batch [PROOF-015](./PROOF-015-A1-A5-BATCH-RUNTIME-OBSERVATION.md); related [PROOF-013](./PROOF-013-A2-A3-RUNTIME-OBSERVATION.md), [PROOF-014](./PROOF-014-A4-A5-RUNTIME-OBSERVATION.md)
**Freeze authority:** `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` / `TASK-MAINNET-FREEZE-001`

---

## Document Role

This document implements the [PROOF-010](./PROOF-010-AUDIT-ITEM-CLOSURE-FRAMEWORK.md) closure
procedure for trust-anchor audit item **A-1** (σ digest → integer byte order for
inequality comparison).

This task is **documentation only**. No consensus, validation, state_hash, monetary,
protocol-freeze, FVR, Coq, or golden-vector code changes.

**Equivalence Claimed:** false

---

## §1 Purpose

### 1.1 A-1 scope

| Field | Value |
|-------|-------|
| **ID** | A-1 |
| **Ambiguity** | How 32-byte σ digest output maps to an integer for `H_i < T·f_i` |
| **Risk** | HIGH (SPEC-004 §8.1) |
| **Layers** | RFC-008 / formal model / C++ `arith_uint256` / Python oracle / Coq `uint256_of_bytes` |

### 1.2 Expected closure outcome

Successful **full** closure (`AI_CLOSED`) requires all PROOF-010 §3 categories,
including a machine-checked cross-layer refinement lemma for σ→integer (proof debt
**PD-15**). This document records:

1. **Normative specification pin** for σ integer interpretation (frozen LE convention).
2. **Observed cross-layer agreement** on draw golden vectors and Sigma-CR-01 audits.
3. **Explicit residual** — Coq refinement theorem not yet discharged.

Closing A-1 removes the **byte-order ambiguity** from the open-assumption set once
`AI_CLOSED`. At baseline after this task: **`AI_PARTIAL`** (pin + evidence; Coq
refinement pending).

### 1.3 Relationship to proof obligations

| Obligation | Relationship |
|------------|--------------|
| **PD-4F-OB-02** | Cannot reach `PO_PROVEN` while A-1 is not `AI_CLOSED` (unless A-1 cited as named assumption) |
| **PD-4F-CPP-01** | Inherits A-1 dependency via PD-4F-OB-02 |
| **DRAW-PROOF-001** | Unrelated to A-1 closure (`PO_BLOCKED` per FVR-M) |

A-1 partial closure **does not** advance PD-4F-OB-02 to `PO_PROVEN`.

---

## §2 Prerequisites

Before executing §4 verification:

| Prerequisite | Source | Verified |
|--------------|--------|----------|
| SPEC-004 §7.3 sub-anchor A-1 registered | [SPEC-004](../../specification/SPEC-004-TRUST-ANCHORS.md) | Yes |
| σ preimage layout frozen (100 bytes, Sigma-CR-01) | RFC-008 §3.2, §5 | Yes |
| `LE32(h)` and `LE32(target_prev)` in preimage | RFC-008, `sigma_hash.cpp` | Yes |
| PROOF-010 closure framework | [PROOF-010](./PROOF-010-AUDIT-ITEM-CLOSURE-FRAMEWORK.md) | Yes |
| Golden vectors GV-STATE-0001..0003 materialized (optional) | [PROOF-007](./PROOF-007-RUNTIME-VECTOR-MATERIALIZATION.md) | Evidence only — **do not test σ** |
| Draw golden corpus (σ inequality) | `phase3_golden_vectors.json`, [PROOF-003](./PROOF-003-GOLDEN-DRAW-VECTORS.md) | Primary σ evidence |
| Runtime FVR mirrors (observation) | FVR-B/C/F via `getruntimesupply` / `getruntimevectorstatus` | Supply-path only |

---

## §3 Closure Artifacts

Per [PROOF-010](./PROOF-010-AUDIT-ITEM-CLOSURE-FRAMEWORK.md) §3.

### 3.1 Specification confirmation — **SATISFIED**

**Normative pin (frozen for A-1 closure):**

Let `σ_bytes = Hash256(txid ‖ C ‖ bind_commit ‖ LE32(h))` be the 32-byte digest
(RFC-008 §5). The integer used in draw inequality is:

```
H_i = Σ_{k=0}^{31} σ_bytes[k] · 256^k     (little-endian, unsigned)
```

**Scope boundaries:**

- `LE32(h)` and `LE32(target_prev)` apply to **preimage subfields only** (offsets 96
  and inside `bind_commit`), not to digest→integer conversion.
- σ values MUST NOT be interpreted as asset amounts (RFC-008 §1).
- Inequality RHS uses constitutional `RES_PROBABILITY_TARGET` (RFC-014); `target_prev`
  binds only via `bind_commit` (Sigma-CR-01).

**Authoritative spec citations:**

| Document | Section |
|----------|---------|
| RFC-008 | §3.2 (100-byte layout), §5 (σ definition), §7 (inequality) |
| SPEC-000 | §5 trust-anchor row (σ → integer byte order) |
| SPEC-003 | Draw rules CI-030..CI-033 |
| SPEC-004 | §7.3 sub-anchor A-1 |

### 3.2 Formal model mapping — **SATISFIED**

| Layer | Mapping |
|-------|---------|
| **RFC / formal** | `H_i ∈ [0, 2^256−1]`; LE uint256 interpretation |
| **C++** | `arith_uint256` over `uint256` internal bytes (`convergence_logic.cpp`, `rigor_math.h`) |
| **Python** | `HashHexToUint256` / LE convention in `rigor_consensus_spec.py` |
| **Coq** | `uint256_of_bytes` in `RigorBytes.v` — documented as matching `arith_uint256` |

**Audit reference:** `SIGMA_CR01_ALIGNMENT_AUDIT.md` — C++ ↔ Python **MATCH**; Coq preimage **byte-exact MATCH**.

### 3.3 Coq artifact — **PARTIAL**

| Artifact | Status |
|----------|--------|
| `sigma_bytes` / `sigma_preimage_bytes` (`SigmaCR01.v`) | Byte pipeline **MATCH** (audit) |
| `uint256_of_bytes` (`RigorBytes.v`) | LE definition **present** |
| Cross-layer refinement lemma σ_bytes ≡ runtime `arith_uint256` | **OPEN** (PD-15 / proof debt) |
| `Phase5Equivalence` catalogs | Classification only; **do not** close A-1 alone |

**Residual:** Machine-checked theorem `forall bs, uint256_of_bytes bs = InterpretLE(bs)` aligned
with C++ inequality path is **not yet Qed**.

### 3.4 Runtime observation validation — **SATISFIED (indirect; witnessed TASK-PROOF-013)**

FVR-B/C/F mirrors do not export σ integers directly. Runtime validation for A-1 is
**indirect**. Regtest observation recorded in
[PROOF-012](./PROOF-012-A1-RUNTIME-OBSERVATION.md) §3:

| Evidence | Role |
|----------|------|
| `ComputeSigmaHash` / `SelectionDigest` in `validation.cpp` | Production σ path |
| `ConvergenceDrawInequalityHolds` | Uses `arith_uint256` on σ output |
| FVR supply mirrors at GV-STATE heights | Confirms witness pipeline operational (not σ-specific) |
| `getconvergenceresult` (when draw block mined) | Observational draw output |

Supply golden vectors (GV-STATE-0001..0003) corroborate FVR export integrity only.

### 3.5 Golden vector coverage — **SATISFIED (draw domain)**

| Corpus | Coverage |
|--------|----------|
| `phase3_golden_vectors.json` (57 vectors) | σ, inequality, winner — Python ↔ C++ **57/57 MATCH** |
| `selection_golden_tests.cpp` | C++ draw-kernel regression |
| GV-DRAW-0001..0003 ([PROOF-003](./PROOF-003-GOLDEN-DRAW-VECTORS.md)) | Normative draw/supply fixtures; harness `GVR_PARTIAL` (FVR-M) |
| GV-STATE-0001..0003 | **Supporting only** — no σ field comparison |

### 3.6 Review notes — **SATISFIED (procedural)**

| Field | Value |
|-------|-------|
| **Reviewer role** | Proof infrastructure maintainer |
| **Date** | 2026-06-05 |
| **Spec pin reviewed** | RFC-008 §5 + §3.2 LE32 subfields + PROOF-011 §3.1 integer pin |
| **Cross-layer audit** | SIGMA_CR01_ALIGNMENT_AUDIT.md; EXECUTABLE_ALIGNMENT_AUDIT.md (σ row PARTIAL) |
| **Residual risk** | Coq refinement lemma PD-15 OPEN; equivalence_claimed remains false |
| **Verdict** | Proceed to `AI_PARTIAL`; `AI_CLOSED` blocked on PD-15 |

---

## §4 Verification Procedure

Execute locally after regtest materialization ([PROOF-007](./PROOF-007-RUNTIME-VECTOR-MATERIALIZATION.md)).

### 4.1 Layer checks

| Step | Check | Command / artifact | PASS criterion |
|------|-------|-------------------|----------------|
| 1 | σ preimage 100-byte layout | Read `core/src/consensus_rigor/sigma_hash.cpp` | txid‖C‖bind_commit‖LE32(h) |
| 2 | LE32 `target_prev` in bind_commit | SIGMA_CR01 audit §2 | 4-byte little-endian |
| 3 | Python ↔ C++ draw vectors | `python3 formal_verification/tests/test_phase3_oracle.py` | 57/57 MATCH |
| 4 | C++ selection golden | `./src/test/test_bitcoin --run_test=selection_golden_tests` | All pass |
| 5 | Supply harness (supporting) | `./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimevectorstatus GV-STATE-0001` | `GVR_PASS`, `equivalence_claimed: false` |
| 6 | Batch harness | `getruntimevectorstatus batch` | 9 PASS + 3 PARTIAL draw (full materialization) |
| 7 | Runtime supply mirror | `./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimesupply` | JSON export present |
| 8 | Coq byte pipeline (offline) | `SigmaCR01.v` + audit doc | Byte-exact MATCH documented |
| 9 | PD-15 refinement lemma | `PROOF_DEBT_REGISTER.md` PD-15 | Still OPEN → step **FAIL** for full closure |

**Note:** `getruntimesupply` accepts an optional block hash, not a vector id. Use
`getruntimevectorstatus GV-STATE-0001` for vector-scoped harness reports. For `verify_fvr=true`,
use JSON-RPC boolean (CLI positional `true` is rejected as string) — see
[PROOF-012](./PROOF-012-A1-RUNTIME-OBSERVATION.md) §2.5.

**Runtime witness log:** [PROOF-012](./PROOF-012-A1-RUNTIME-OBSERVATION.md) §3 (TASK-PROOF-013).

### 4.2 Optional fields

Harness `GVR_PARTIAL` on draw vectors is **expected** (FVR-M). Do not treat draw
`GVR_PARTIAL` as A-1 failure.

### 4.3 Aggregate result

| Category | Result |
|----------|--------|
| Steps 1–8 | **PASS** (when commands succeed on materialized regtest) |
| Step 9 (Coq refinement) | **FAIL** (expected at baseline) |
| **Overall A-1 status** | **`AI_PARTIAL`** |

---

## §5 Closure Status

### 5.1 Lifecycle transition

```
AI_OPEN  ──(PROOF-011 artifact bundle)──►  AI_PARTIAL
AI_PARTIAL  ──(PD-15 Coq lemma + review)──►  AI_CLOSED   [future]
```

| State | Effective |
|-------|-----------|
| **Prior** | `AI_OPEN` (SPEC-004 §8.1) |
| **Current** | **`AI_PARTIAL`** |
| **Target** | `AI_CLOSED` after PD-15 discharge + PROOF-010 §3 review sign-off |

### 5.2 Registry updates

| Document | Update |
|----------|--------|
| [PROOF-010](./PROOF-010-AUDIT-ITEM-CLOSURE-FRAMEWORK.md) §4 | A-1 status → `AI_PARTIAL` |
| [SPEC-004](../../specification/SPEC-004-TRUST-ANCHORS.md) §8.1 | A-1 references PROOF-011 |
| [PROOF-009](./PROOF-009-PROOF-OBLIGATION-REGISTRY.md) | PD-4F-OB-02 still blocked on A-1 full closure |

### 5.3 What A-1 partial closure does **not** authorize

- Global or scoped `equivalence_claimed := true`
- `PD-4F-OB-02` → `PO_PROVEN`
- Draw equivalence (`DRAW-PROOF-001` remains `PO_BLOCKED`)
- Closure of A-2 or A-3 to `AI_CLOSED` (A-2 / A-3 runtime partial witness: [PROOF-013](./PROOF-013-A2-A3-RUNTIME-OBSERVATION.md))

---

## §6 Notes / Dependencies

### 6.1 Limitations

1. **GV-STATE vectors** exercise ORU/RES supply mirrors, not σ integer paths.
2. **FVR exports** are read-only evidence; they cannot independently close A-1.
3. **Coq catalogs** (`Phase5SummaryCatalog`, etc.) classify alignment; they are not
   refinement proofs.
4. **PD-15** (`σ→int endianness normative equivalence`) remains **OPEN** in
   `PROOF_DEBT_REGISTER.md`.

### 6.2 Dependencies

```
A-4 (SHA-256d) ── supports σ digest bytes (separate item)
A-1 (σ → int LE) ── blocks full PD-4F-OB-02 closure
A-2 (uint512) ── depends on A-1 integer interpretation
```

### 6.3 Assumptions (explicit)

Until `AI_CLOSED`, theorems depending on draw inequality may cite **Assumption A-1**
(SPEC-007 §7.3): all layers use the LE uint256 pin in §3.1.

---

## Task Output Status

| Field | Status |
|-------|--------|
| Audit Item A-1 Closure Documentation | **ESTABLISHED** |
| Audit Lifecycle Update | **`AI_PARTIAL`** |
| Closure Artifacts Verified | **ESTABLISHED** (§3; Coq refinement partial) |
| Verification Procedure Completed | **ESTABLISHED** (§4; runtime witnessed PROOF-012) |
| Runtime Witness Observation | **VERIFIED** ([PROOF-012](./PROOF-012-A1-RUNTIME-OBSERVATION.md)) |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## Revision History

| Version | Date | Change |
|---------|------|--------|
| 0.1 | 2026-06-05 | `TASK-PROOF-012` — A-1 closure procedure; status AI_PARTIAL pending PD-15 |
| 0.2 | 2026-06-05 | Cross-reference PROOF-012 runtime witness (TASK-PROOF-013) |

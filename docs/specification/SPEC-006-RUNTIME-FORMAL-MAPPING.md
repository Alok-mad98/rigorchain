# SPEC-006: RIGOR Runtime / Formal Mapping

**Document status:** Production specification  
**Track:** Formal Specification Program  
**Status:** ESTABLISHED (F-3 errata 2026-08-25: map only to real Coq identifiers)  
**Baseline:** RFC-020 shipping issuance (continuous `destroy_cost(h)`, flat 128 ORU)  
**Parents:** SPEC-000 … SPEC-005  
**Normative sources:** `RFC-000`, `RFC-008`, `RFC-020`  
**Freeze authority:** `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` / `historical verification record`

---

## Document Role

This document is the **authoritative mapping layer** between the specification
hierarchy and the formal verification stack. It defines how every normative object
is represented — and how representations relate — across:

```
Specification objects
        ↓
Formal model objects
        ↓
Coq definitions
        ↓
Runtime witnesses
```

The mapping layer **prevents silent divergence** between specification intent and
verification artifacts. It does not claim equivalence.

This document is specification-only:

- No code changes
- No RPC changes
- No proof changes
- **Equivalence Claimed:** false

---

## 1. Purpose

Every normative object in SPEC-000 … SPEC-005 has a declared representation at
each verification layer. When a layer disagrees with the specification, the
specification governs classification; alignment work must close the gap or document
an open obligation.

**Mapping is directional:**

| Direction | Authority |
|-----------|-----------|
| Specification → formal model | Specification wins |
| Formal model → Coq | Formal model refines spec |
| Coq → runtime witness | Witness observes; does not define |
| Runtime → specification | **Forbidden** as normative back-flow |

---

## 2. Mapping Principles

### MP-001: Specification Is Authoritative

The specification layer (SPEC-000 … SPEC-005) defines what RIGOR **SHOULD** do.
Formal models refine specification; they do not replace it.

**Violation:** A formal model or runtime export introduces behavior not implied by
any spec predicate without a Level-5 governance event.

---

### MP-002: Coq Definitions Refine Formal Models

Coq modules express machine-readable refinements of the formal model objects in
`docs/formal/` and `formal_verification/coq/`. Coq definitions are correct only
insofar as they refine the specification.

**Violation:** A Coq definition used in a theorem is not traceable to a spec object
(CR-*, CI-*, State field, or economic recurrence).

---

### MP-003: Runtime Witnesses Observe Consensus Execution

Runtime witnesses (FVR track, Phase-5 artifacts) **observe** post-connect execution.
They do not define consensus validity, supply truth, or mint outcomes.

**Violation:** A validation predicate reads a runtime mirror, witness export, or
alignment catalog field.

---

### MP-004: No Runtime Artifact May Override Specification

No runtime artifact — mirror, witness, invariant report, candidate export, or
summary snapshot — may override any specification predicate.

**Violation:** Block acceptance depends on observational state; or a failing FVR
report is treated as consensus truth.

---

## 3. Consensus Mapping

Consensus objectives from [SPEC-000](./SPEC-000-CONSENSUS-SPECIFICATION.md) map to
**real** Coq identifiers in the protocol-core cone
(`formal_verification/coq/_CoqProject.protocol-core`; inventory:
[`FORMAL_COQ_LOAD_BEARING_THEOREMS.md`](../security/FORMAL_COQ_LOAD_BEARING_THEOREMS.md)).
Invented family names are not a mapping. Citable theorems are protocol-core only;
Phase-4F/4G/5 catalogs are paperwork.

| Spec objective | Coq identifiers (protocol-core) | Status |
|----------------|-------------------------------|--------|
| **CO-1** Chain validity / max chainwork | — | **NOT VERIFIED.** No chainwork / heaviest-chain theorem. |
| **CO-2** Deterministic block acceptance | `Replay_Determinism`; `supply_deterministic` | **Partial.** Model `apply_block` only; not C++ `ConnectBlock`. |
| **CO-3** Deterministic state transition | `apply_block`; `rollback_restores_previous_state` | **Partial.** Replay model. |
| **CO-4** Supply integrity | `max_issuance_within_cap`; `cap_overshoot_rejects_apply_block`; `apply_block_res_net_bounded_by_scheduled_mints`; `oru_supply_bounded_by_block_count` | **Model.** Not a UTXO-set equality proof. |
| **CO-5** Replay determinism | `Replay_Determinism`; `supply_deterministic` | **Model.** Finite golden vectors are a differential gate, not this theorem. |
| **CO-6** Reorg determinism | `rollback_restores_previous_state`; DualPool rollback lemmas in `DualPoolConservation.v` | **Model.** |
| **CO-7** Protocol freeze preservation | `p5_phase5_paperwork_recorded` (catalog flag) | **NOT VERIFIED** as freeze authority. Catalogs record status; they do not prove RFC freeze. |

**Example (CO-4, real path):**

```
CO-4 Supply integrity
    → max_issuance_within_cap / cap_overshoot_rejects_apply_block (ResSchedule / ResMintObligation)
    → apply_block_res_net_bounded_by_scheduled_mints (DualPoolConservation)
    → runtime: ConnectBlock reject + unit suites (not Coq)
```

Consensus rules (CR-*) from [SPEC-003](./SPEC-003-CONSENSUS-RULES.md) refine these
objectives. A CR-* with no row above is **NOT VERIFIED** in Coq until a real
identifier is named here.

---

## 4. State Mapping

Every consensus state field from [SPEC-001](./SPEC-001-STATE-MODEL.md) has a
declared cross-layer mapping.

| Spec field | Formal state object | Coq definition | Runtime witness |
|------------|--------------------|--------------------|-----------------|
| **ChainTip** | `ChainTip` record (height, hash, chainwork, header context) | `ReplayState` tip component; `BlockAbs` parent linkage | Witness `block_hash`, `parent_hash`; tip metadata in supply export |
| **BlockHeight** | `h ∈ ℕ` | `cm_height` / `ReplayState.height` | `height` in RuntimeSupplyState mirror |
| **S_ORU** | Formal supply scalar `S_ORU(h)` | `rs_oru_supply` in `ReplayState` | `oru_supply_cumulative` → projected `rs_oru_supply` (FVR-C) |
| **S_RES** | Formal supply scalar `S_RES(h)` | `rs_res_supply` in `ReplayState` | `res_mint_cumulative` → projected `rs_res_supply` (FVR-C) |
| **UTXO** | `UTXO : OutPoint ⇀ OutputRecord` | `Coq UTXO` record / `ReplayState` UTXO fragment | UTXO diagnostic aggregates (non-authoritative cross-check) |
| **ProtocolVersion** | Frozen rule-set identity | Version constants in `Types.v` / params records | Not directly exported; implied by accepted block semantics |
| **FrozenParameters** | Immutable constant bundle | `RES_COIN`, pole mint, derived `RES_MAX_SUPPLY` in `res_issuance.h` / params | `res_max_supply` in mirror (read-only constant) |
| **MintContext** | `(R, B, h)` | Mint-arithmetic lemmas (`R · (2²⁵+h)² ≤ B << 40`) | Observational mint totals. Lottery `DrawContext` (`F`, σ, winner) is **archived Category C**. |
| **PoWContext** | ASERT difficulty state | **Does not exist in Coq.** No PoW / ASERT / `CheckProofOfWork` module. | C++ `pow_tests` is a **security unit gate**, not an FV gate. `nBits` is not a mint price. **NOT VERIFIED.** |

**Observational boundary:** Only `S_ORU`, `S_RES`, and height/hash linkage have
active Phase-5 witness mapping. UTXO runtime mapping is **partial**;
equivalence not claimed. Lottery DrawContext is archived, not shipping.

---

## 5. Economic Mapping

Economic recurrences from [SPEC-002](./SPEC-002-ECONOMIC-MODEL.md) map across layers.

### 5.1 ORU recurrence

```
S_ORU(h+1) = S_ORU(h) + R(h+1) − C(h+1)
```

| Layer | Representation |
|-------|----------------|
| **Specification** | CI-021, CR-030..CR-033 |
| **Formal model** | `RIGOR_FORMAL_STATE_MODEL` supply recurrence; `CONSENSUS_TRANSITION_SYSTEM` §7 |
| **Coq** | `cb_oru_subsidy`, `cb_oru_mint_delta`; `rs_oru_supply`; `oru_supply_bounded_by_block_count`. Converge-sum destroy is a C++ / witness field, not a Coq mint-delta identifier. |
| **Runtime witness** | `oru_subsidy_block`, `oru_converge_burn_block` (runtime key; product term: converge sum), `oru_supply_cumulative`; FVR-E ORU-L* checks |

### 5.2 RES recurrence

```
S_RES(h+1) = S_RES(h) + body_mint_atoms(h+1)
```

with `S_RES(h) ≤ 419430400000000`. `body_mint_atoms(h)` is the sum of
Witness v1 RES outputs from body transactions that pass
`CheckResMintArithmetic`.

| Layer | Representation |
|-------|----------------|
| **Specification** | CI-022, CI-024, CR-043..CR-045 (RFC-020 recurrence) |
| **Formal model** | `S_RES` recurrence; body-mint sum |
| **Coq** | `rs_res_supply`; derived-cap lemmas. Lottery-cone `cb_res_mint_delta` / `M(h)` identifiers are **archived Category C** |
| **Runtime witness** | `res_mint_block`, `res_mint_cumulative`; FVR-E RES-L* checks |

**Known mapping gap:** Coq has `cb_oru_mint_delta` (subsidy step). The product
converge-sum `C(h)` alignment is **PARTIAL** (Phase-5 catalogs). Does not
affect consensus authority.

---

## 6. Invariant Mapping

Every CI-* from [SPEC-005](./SPEC-005-CONSENSUS-INVARIANTS.md) maps to a **real**
Coq identifier or is marked **NOT VERIFIED**. No proofs of C++ are claimed.

### 6.1 CI-CONSENSUS

| Invariant | Coq identifier | Status |
|-----------|----------------|--------|
| CI-001 Chain continuity | — | **NOT VERIFIED** |
| CI-002 Single active chain | — | **NOT VERIFIED** (no fork-choice development) |
| CI-003 Deterministic validation | `Replay_Determinism`; `supply_deterministic` | Model `apply_block` only |

### 6.2 CI-STATE

| Invariant | Coq identifier | Status |
|-----------|----------------|--------|
| CI-010 UTXO consistency | `utxo_spend_removes_output`; `utxo_create_adds_output` | Model UTXO fragment |
| CI-011 No double spend | `utxo_spend_removes_output` | Model |
| CI-012 Deterministic state transition | `apply_block`; `supply_deterministic` | Model |
| CI-013 Replay equivalence | `Replay_Determinism` | Model; `equivalence_claimed = false` vs C++ |
| CI-014 Reorg consistency | `rollback_restores_previous_state` | Model |

### 6.3 CI-ECONOMIC

| Invariant | Coq identifier | Status |
|-----------|----------------|--------|
| CI-020 ORU supply non-negativity | `oru_supply_bounded_by_block_count` | Bound on the model scalar, not a full recurrence |
| CI-021 ORU recurrence preservation | `oru_supply_bounded_by_block_count` | **Partial.** Not a closed `S_ORU(h+1)` identity vs C++ |
| CI-022 RES recurrence preservation | `apply_block_res_net_bounded_by_scheduled_mints` | Model net bound |
| CI-023 RES supply cap | `max_issuance_within_cap`; `cap_overshoot_rejects_apply_block` | Model; C++ ConnectBlock also rejects |
| CI-024 RES mint predicate | Mint-arithmetic / cap lemmas | RFC-020: a block MAY contain zero or more body mints; each MUST satisfy `R · (2²⁵+h)² ≤ B << 40`. |

### 6.4 CI-MINT (RFC-020)

| Invariant | Coq identifier | Status |
|-----------|----------------|--------|
| CI-030 Mint determinism | Integer `CheckResMintArithmetic` | Height + amounts only |
| CI-031 Body mints | — | Zero or more body mints per block |
| CI-032 Mint independent of PoW | `destroy_cost(h)` is a height function | Parent `nBits` is not a mint price |
| CI-033 Valid mint requires destroy inequality | `R · (2²⁵+h)² ≤ B << 40` | Production: `tx_verify.cpp` |

### 6.5 CI-FREEZE

| Invariant | Coq identifier | Status |
|-----------|----------------|--------|
| CI-040 Frozen parameter preservation | `p5_phase5_paperwork_recorded` | Paperwork catalog; **NOT VERIFIED** as a freeze proof |
| CI-041 Monetary policy preservation | `max_issuance_within_cap` | Schedule/cap in the model |
| CI-042 Protocol freeze preservation | `p5_phase5_paperwork_recorded`; `CS_ArtifactsConsolidated` | Catalog / certification paperwork; not C++ freeze authority |

---

## 7. Trust Anchor Mapping

Trust anchors from [SPEC-004](./SPEC-004-TRUST-ANCHORS.md) map to formal oracles
and assumptions.

| Anchor | Formal oracle / assumption | Formalizable? | Notes |
|--------|---------------------------|---------------|-------|
| **TA-HASH** | Hash oracle (`Hash256` / `RigorSha256`) | **PARTIAL** | Determinism: vector-backed. FIPS equivalence: open (A-4) |
| **TA-POW** | PoW oracle (`ExpandCompact`, work comparison) | **NO** (Coq) | **NOT VERIFIED.** No Coq PoW development. Security gate: C++ `pow_tests`. |
| **TA-CLOCK** | Clock assumption (MTP + forward bound δ) | **YES** | Chain-derived; δ frozen in RFC-005 |
| **TA-NET** | Network assumption (semi-synchronous delivery) | **NO** | Liveness only; out of consensus validity proofs |
| **TA-STORAGE** | Persistence assumption (durable block/UTXO store) | **NO** | Operational; replay detects corruption |
| **TA-MINT** | Mint assumption (integer inequality, no external RNG) | **PARTIAL** | Lottery TA-DRAW is archived. Shipping mint is `CheckResMintArithmetic`. |

Open audit items A-1 … A-5 from SPEC-004 block full formalization of TA-HASH and
TA-DRAW sub-components until closed.

---

## 8. Runtime Witness Mapping

Formal-Friendly Runtime Refactor layers (FVR-A through FVR-N) map to observational
artifacts. **Every layer: Authoritative = NO; Consensus Impact = NONE.**

| Layer | Artifact | Role | Maps to |
|-------|----------|------|---------|
| **FVR-A** | Runtime invariant checks on captured mirror | Pre-export sanity checks | CI-* observation (non-authoritative) |
| **FVR-B** | RuntimeSupplyState mirror | Post-connect supply snapshot | `S_ORU`, `S_RES`, per-block deltas |
| **FVR-C** | Coq field hook projection | Name alignment to `rs_oru_supply`, `rs_res_supply`, `cb_*` | Coq catalog field names |
| **FVR-D** | Supply export payload | Diagnostic read-only export | FVR-B + FVR-C composite |
| **FVR-E** | Runtime invariant verification report | Active CI-* pattern checks on mirror | ORU-L* / RES-L* stable ids |
| **FVR-F** | Runtime supply witness chain | Historical per-height witnesses | `rs_oru_supply`, `rs_res_supply` sequence |
| **FVR-G** | Coq catalog row integration | Witness → Coq field row | `ReplayState` projection |
| **FVR-H** | CHAS alignment vectors | Canonical-hash alignment export | `chas_oru_state`, `chas_res_state` |
| **FVR-I** | Equivalence candidate identification | Comparable-field status | Candidate equivalence (no claim) |
| **FVR-J** | Coq-readable candidate export | Structured candidate for proof tooling | FVR-I + formal layer |
| **FVR-K** | Global candidate equivalence index | Cross-height candidate aggregation | FVR-H + FVR-J composite |
| **FVR-L** | Equivalence closure gate | Closure predicate evaluation | Alignment preconditions catalog |
| **FVR-M** | Draw closure impossibility | Draw non-closability audit | CI-030..CI-033 gap catalog; **IMPOSSIBLE** |
| **FVR-N** | Phase-5 summary snapshot | FVR-A..M consolidated status | Freeze-protocol link; honesty boundary |

**FVR does not define invariants** (see SPEC-005 §10). FVR-E may reference CI-* or
parallel stable ids; passing FVR-E does not prove CI-*.

---

## 9. Equivalence Boundary

### 9.1 Current status

| Claim | Status |
|-------|--------|
| **Coq ≡ Runtime** | **NOT ESTABLISHED** |
| **Equivalence Claimed** | **false** |
| **Alignment evidence** | **ACTIVE** (Phase-5, FVR track) |
| **Equivalence theorem** | **Does not exist** |

### 9.2 What alignment evidence provides

Alignment evidence may show:

- Comparable fields exist at runtime and in Coq catalog rows
- Scalar values match on golden / sampled heights
- Invariant checker reports pass on captured mirrors
- Candidate equivalence status is `Comparable` or `PartiallyComparable`

Alignment evidence does **not** provide:

- Proof that Coq definitions refine specification on all inputs
- Proof that runtime mirrors equal consensus state on all paths
- Closed C++ ↔ Coq equivalence (`equivalence_claimed` remains false)

### 9.3 Preconditions for future equivalence

Future equivalence proof ([SPEC-007](./SPEC-007-EQUIVALENCE-BOUNDARIES.md)) requires:

1. Closure of open audit items A-1 … A-3 (minimum)
2. Coq theorems implying CI-* under declared trust anchors
3. Vector-backed refinement proofs for hash and supply fields
4. Explicit `equivalence_claimed = false` (no C++ refinement)

Until then, all cross-layer statements remain **candidate** or **observational**.

---

## 10. Verification Hierarchy

```
Specification (SPEC-000 … SPEC-005)     ← ESTABLISHED
        ↓
Formal Models (docs/formal/)            ← ESTABLISHED
        ↓
Coq Definitions (formal_verification/coq/)  ← ESTABLISHED (partial; admits remain)
        ↓
Theorems (Coq Proof Obligations)        ← PARTIAL (many admits; candidates cataloged)
        ↓
Runtime Alignment (FVR, Phase 5)        ← ACTIVE (evidence only)
        ↓
Candidate Equivalence (FVR-I, J, K)     ← ACTIVE (equivalence_claimed = false)
        ↓
Future Equivalence Proof                  ← NOT STARTED
```

**Only the first five levels exist today** with meaningful artifacts. Levels 6–7
are explicitly future work. Level 5 does not imply level 4 completeness.

---

## 11. Freeze Compatibility

This document is **specification only**. It maps existing frozen semantics; it does
not modify them.

| Guarantee | Status |
|-----------|--------|
| No consensus modifications | **Confirmed** |
| No monetary-policy modifications | **Confirmed** |
| No protocol-freeze modifications | **Confirmed** |

Nothing in this document supersedes `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` or
`historical verification record`.

---

## 12. Task Output Status

| Field | Status |
|-------|--------|
| Runtime / Formal Mapping Specification | **ESTABLISHED** |
| Consensus Mapping | **ESTABLISHED** |
| State Mapping | **ESTABLISHED** |
| Economic Mapping | **ESTABLISHED** |
| Invariant Mapping | **ESTABLISHED** |
| Trust Anchor Mapping | **ESTABLISHED** |
| Runtime Witness Mapping | **ESTABLISHED** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## 13. Revision History

| Version | Date | Change |
|---------|------|--------|
| 0.1 | 2026-06-05 | `historical verification record` — initial runtime/formal mapping specification |
| 0.2 | 2026-08-25 | F-3 (P0-016): replace nonexistent Coq families with protocol-core identifiers; mark CO-1 / PoW / CI-033 **NOT VERIFIED** |

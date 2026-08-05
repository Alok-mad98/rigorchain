# SPEC-006: RIGOR Runtime / Formal Mapping

**Task:** `TASK-SPEC-007`  
**Track:** Formal Specification Program  
**Status:** ESTABLISHED  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Parents:** SPEC-000 … SPEC-005  
**Normative sources:** `RFC-000`, `RFC-008`, `RFC-014`, `RFC-015`  
**Freeze authority:** `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` / `TASK-MAINNET-FREEZE-001`

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
They do not define consensus validity, supply truth, or draw outcomes.

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
formal predicates, Coq theorem families, and runtime evidence layers.

| Spec objective | Formal consensus predicate | Coq theorem family | Runtime evidence |
|----------------|---------------------------|-------------------|------------------|
| **CO-1** Chain validity | `ValidChain` — well-formed active chain from genesis; max chainwork | `chain_validity_theorems` (`Transition.v`, `ReplayReorgCore`) | Header validation trace; chain tip witness |
| **CO-2** Deterministic block acceptance | `ValidBlock(S, B)` — total accept/reject function | `determinism_theorems` (`Determinism.v`, `Execution.v`) | Per-block validation trace; no disagreement vectors |
| **CO-3** Deterministic state transition | `Transition(S, B)` — unique successor | `state_transition_theorems` (`Transition.v`, `State.v`) | Supply witness step; state encoding obligations |
| **CO-4** Supply integrity | `ValidSupplyUpdate` — ORU/RES laws hold | `supply_integrity_theorems` (`State.v`, `Properties.v`) | Runtime supply witness; invariant checker (FVR-E) |
| **CO-5** Replay determinism | `Replay(B₁..h) = S(h)` — platform-independent | `replay_determinism_theorems` (`ReplayStateEncodingTheorem`, `ReplayEquivalence`) | Witness chain replay (FVR-F); golden vectors |
| **CO-6** Reorg determinism | `Reorg(Σ) = Sequential(Σ)` | `reorg_consistency_theorems` (`ReorgStateEncodingTheorem`, `ReplayReorgCore`) | Reorg witness invalidation; replay snapshots |
| **CO-7** Protocol freeze preservation | `FrozenParamsValid` — RFC-014/015 semantics only | `freeze_preservation_theorems` (`AntiEquivalenceSafety`, catalog audits) | Phase-5 summary freeze link (FVR-N) |

**Example trace (CO-1):**

```
CO-1 Chain Validity
    → ValidChain predicate (CONSENSUS_TRANSITION_SYSTEM)
    → chain_validity_theorems
    → runtime witness chain (header + tip observability)
```

Consensus rules (CR-*) from [SPEC-003](./SPEC-003-CONSENSUS-RULES.md) refine these
predicates into layer-specific checks. Every CR-* maps to at least one CO-* and
one formal predicate family.

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
| **FrozenParameters** | Immutable constant bundle | `RES_PROBABILITY_TARGET`, caps, gates in `Selection.v` / params | `res_max_supply`, `res_reward_amount` in mirror (read-only constants) |
| **DrawContext** | `(F, C, H_i, eligible, winner, bind_commit)` | `Selection.v`, `Sigma.v`, `ConvergeRoot.v` | Draw fields in candidate exports; **draw closure IMPOSSIBLE** (FVR-M) |
| **PoWContext** | ASERT difficulty state | `PoWState` / header validation fragment | Header validation observability; `nBits` not used as draw RHS |

**Observational boundary:** Only `S_ORU`, `S_RES`, and height/hash linkage have
active Phase-5 witness mapping. UTXO and DrawContext runtime mapping is
**partial**; equivalence not claimed.

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
| **Coq** | `cb_oru_subsidy`, `cb_oru_burn_delta` (partial); `rs_oru_supply` step theorems (`THM-05` family) |
| **Runtime witness** | `oru_subsidy_block`, `oru_converge_burn_block`, `oru_supply_cumulative`; FVR-E ORU-L* checks |

### 5.2 RES recurrence

```
S_RES(h+1) = S_RES(h) + RES_REWARD_AMOUNT · M(h+1)
```

| Layer | Representation |
|-------|----------------|
| **Specification** | CI-022, CI-024, CR-043..CR-045 |
| **Formal model** | `S_RES` recurrence; `M(h) ∈ {0,1}` |
| **Coq** | `cb_res_mint_delta`, `rs_res_supply`, `res_mint_count_M_h`; `THM-05` / cap theorems |
| **Runtime witness** | `res_mint_block`, `res_mint_cumulative`, `res_mint_count_block`; FVR-E RES-L* checks |

**Known mapping gap:** `cb_oru_burn_delta` ↔ `C(h)` Coq field alignment is
**PARTIAL** (documented in Phase-5 catalogs). Does not affect consensus authority.

---

## 6. Invariant Mapping

Every CI-* from [SPEC-005](./SPEC-005-CONSENSUS-INVARIANTS.md) maps to a Coq
theorem family. No proofs are claimed here — only obligation routing.

### 6.1 CI-CONSENSUS

| Invariant | Theorem family |
|-----------|----------------|
| CI-001 Chain continuity | `chain_continuity_theorems` |
| CI-002 Single active chain | `fork_choice_theorems` |
| CI-003 Deterministic validation | `determinism_theorems` |

### 6.2 CI-STATE

| Invariant | Theorem family |
|-----------|----------------|
| CI-010 UTXO consistency | `utxo_existence_theorems` |
| CI-011 No double spend | `utxo_uniqueness_theorems` |
| CI-012 Deterministic state transition | `state_transition_theorems` |
| CI-013 Replay equivalence | `replay_determinism_theorems` |
| CI-014 Reorg consistency | `reorg_consistency_theorems` |

### 6.3 CI-ECONOMIC

| Invariant | Theorem family |
|-----------|----------------|
| CI-020 ORU supply non-negativity | `oru_nonnegativity_theorems` |
| CI-021 ORU recurrence preservation | `oru_supply_equation_theorems` (`THM-05`) |
| CI-022 RES recurrence preservation | `res_supply_equation_theorems` |
| CI-023 RES supply cap | `res_cap_theorems` (`THM-04` family) |
| CI-024 Single RES mint | `single_mint_theorems` |

### 6.4 CI-DRAW

| Invariant | Theorem family |
|-----------|----------------|
| CI-030 Draw determinism | `draw_determinism_theorems` (`THM-03`) |
| CI-031 Single winner | `selection_uniqueness_theorems` (`THM-02`) |
| CI-032 Draw independent of PoW | `freeze_target_theorems` (RFC-014) |
| CI-033 Valid mint requires valid draw | `draw_mint_coherence_theorems` |

### 6.5 CI-FREEZE

| Invariant | Theorem family |
|-----------|----------------|
| CI-040 Frozen parameter preservation | `parameter_freeze_theorems` |
| CI-041 Monetary policy preservation | `monetary_policy_theorems` |
| CI-042 Protocol freeze preservation | `protocol_freeze_theorems` (`AntiEquivalenceSafety`) |

---

## 7. Trust Anchor Mapping

Trust anchors from [SPEC-004](./SPEC-004-TRUST-ANCHORS.md) map to formal oracles
and assumptions.

| Anchor | Formal oracle / assumption | Formalizable? | Notes |
|--------|---------------------------|---------------|-------|
| **TA-HASH** | Hash oracle (`Hash256` / `RigorSha256`) | **PARTIAL** | Determinism: vector-backed. FIPS equivalence: open (A-4) |
| **TA-POW** | PoW oracle (`ExpandCompact`, work comparison) | **PARTIAL** | Spec-defined; ASERT alignment partial |
| **TA-CLOCK** | Clock assumption (MTP + forward bound δ) | **YES** | Chain-derived; δ frozen in RFC-005 |
| **TA-NET** | Network assumption (semi-synchronous delivery) | **NO** | Liveness only; out of consensus validity proofs |
| **TA-STORAGE** | Persistence assumption (durable block/UTXO store) | **NO** | Operational; replay detects corruption |
| **TA-DRAW** | Draw assumption (deterministic, no external RNG) | **PARTIAL** | Spec YES; sub-items A-1, A-2, A-3 open |

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
- Closed equivalence for draw layer (FVR-M: permanently non-closable under current architecture)

### 9.3 Preconditions for future equivalence

Future equivalence proof ([SPEC-007](./SPEC-007-EQUIVALENCE-BOUNDARIES.md)) requires:

1. Closure of open audit items A-1 … A-3 (minimum)
2. Coq theorems implying CI-* under declared trust anchors
3. Vector-backed refinement proofs for hash and supply fields
4. Explicit handling of FVR-M draw impossibility (scoped non-equivalence)

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
`TASK-MAINNET-FREEZE-001`.

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
| 0.1 | 2026-06-05 | `TASK-SPEC-007` — initial runtime/formal mapping specification |

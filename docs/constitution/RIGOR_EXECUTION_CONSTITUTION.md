# RIGOR AI Execution Constitution

**Task:** `TASK-AI-CONST-001`  
**Version:** 1.0  
**Date:** 2026-06-18  
**Status:** Canonical governing specification  
**Scope:** RIGOR system development, simulation, and release governance during and after PROOF-034 (2.1M verification)  
**Type:** Documentation only — **no code, consensus, protocol, mining, or GUI modifications**

| Field | Value |
|-------|-------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Mining Impact | **NONE** |
| Freeze Impact | **NONE** |
| `equivalence_claimed` | **false** |

**Related document:** [EXECUTION_DOMAIN_MODEL.md](./EXECUTION_DOMAIN_MODEL.md)

This document is the **canonical execution constitution** for all RIGOR development, simulation, observation, and release governance work. When this constitution conflicts with ad-hoc execution plans, this document wins. Implementations, simulations, presentations, and release decisions MUST comply with every section below.

---

## §1 — Core Principle Definition

The RIGOR Execution Constitution is a formal rule system governing how work is performed, observed, simulated, and released. It rests on four foundational principles.

### 1.1 Deterministic Execution Rule

All system behavior that participates in correctness claims MUST remain **deterministic under identical inputs**.

| Requirement | Meaning |
|-------------|---------|
| Same inputs → same outputs | Given frozen rules and identical observable inputs, execution produces identical canonical results |
| No hidden state | Correctness-relevant state MUST be derivable from observable chain data and frozen rules |
| No non-reproducible randomness in correctness paths | Probabilistic work (e.g. hash search) is permitted only where the protocol explicitly defines it; correctness evaluation paths MUST NOT depend on unseeded or non-auditable randomness |

Determinism applies to consensus evaluation, convergence resolution, RES mint emission under frozen rules, golden vector verification, and simulation replay when replay claims determinism.

### 1.2 Separation of Layers

RIGOR execution is organized into three non-substitutable layers:

| Layer | Role | Authority |
|-------|------|-----------|
| **Protocol Layer** | Frozen consensus and asset rules (ORU, RES, convergence, mint) | **Sole source of on-chain truth** |
| **Scientific Layer** | Formal reasoning models, specifications, proof catalogs | Establishes logical status and impossibility boundaries |
| **Lab Layer** | Experimental simulation, runtime archive collection, observation | Produces empirical evidence; does not redefine protocol |

No layer may override another. The Protocol Layer is frozen. The Scientific Layer does not substitute for incomplete runtime archives. The Lab Layer does not assert production equivalence.

### 1.3 Non-Interference Principle

PROOF-034 and all active runtime verification MUST remain **untouched and authoritative**.

| Rule | Enforcement |
|------|-------------|
| Runtime verification is read-only during active phases | No alteration of archive execution, harness procedures, or golden vector acceptance criteria mid-run |
| Simulation MUST NOT influence consensus | Simulation outputs are observational or experimental only |
| Presentation MUST NOT influence verification | Documentation, GUI, and website content do not change verification state |
| Development MUST NOT bypass freeze | No consensus, protocol, or mining rule changes under verification window policy |

### 1.4 Equivalence Invariance

```
equivalence_claimed = false
```

This invariant is **global and mandatory** during all active verification phases.

| Rule | Meaning |
|------|---------|
| `equivalence_claimed` MUST remain **false** | No system, document, UI, or release artifact may assert final equivalence while PROOF-034 is incomplete |
| No premature certification | Partial runtime evidence, simulation success, or formal proof completion alone do not authorize equivalence claims |
| Governance lock | Only explicit future protocol governance (outside this constitution's scope) may change this field |

Equivalence invariance is preserved until PROOF-034 reaches full completion at height **2,100,001** and all acceptance criteria are met — and even then, `equivalence_claimed` remains **false** under current frozen policy unless separately authorized.

---

## §2 — Execution Domains

RIGOR work is partitioned into four execution domains. Each domain has distinct authority, permitted operations, and hard boundaries.

Detailed domain definitions, interaction rules, and violation classes are specified in [EXECUTION_DOMAIN_MODEL.md](./EXECUTION_DOMAIN_MODEL.md).

| Domain | Identifier | Summary |
|--------|------------|---------|
| **Consensus Domain** | A | Node validation, mining, block acceptance — **strictly frozen** |
| **Runtime Verification Domain** | B | PROOF-034, golden vectors, archive validation — **read-only execution** |
| **Simulation Domain** | C | Phase-3 full system simulation, ORU/RES/convergence replay — **must not affect consensus or runtime state** |
| **Presentation Domain** | D | GUI, website, documentation, narrative — **fully decoupled from execution correctness** |

Cross-domain interference is forbidden unless explicitly permitted by this constitution and the domain model.

---

## §3 — System Invariants

The following invariants MUST always hold. Violation of any invariant is an architecture defect and a freeze violation.

### IV-01 — Simulation Isolation

**No simulation output can affect consensus state.**

Simulation Domain (C) MAY read frozen rules and archived or synthetic inputs. Simulation Domain (C) MUST NOT write to live consensus state, alter chain tips, submit blocks to production paths, or modify node validation behavior.

### IV-02 — Presentation Non-Authority

**No GUI component can modify runtime verification.**

Presentation Domain (D) MAY display verification progress, archive status, and observational metrics. Presentation Domain (D) MUST NOT alter PROOF-034 execution, harness configuration, golden vector fixtures, or verification verdicts.

### IV-03 — Documentation Non-Redefinition

**No documentation can redefine protocol rules.**

All documentation — including whitepapers, task reports, website copy, and AI-generated artifacts — is descriptive. Documentation MUST NOT introduce new consensus semantics, asset issuance rules, convergence logic, or draw/equivalence claims not already frozen in protocol governance documents.

### IV-04 — PROOF-034 Authority

**PROOF-034 remains authoritative until full completion at height 2,100,001.**

Runtime archive verification is the empirical ground-truth collection for the 2.1M regtest execution trace. Until acceptance criteria are met (full golden vector materialization, harness verification, artifact bundle publication), PROOF-034 status is **in progress** and MUST NOT be overstated in any domain.

### IV-05 — Dual-Asset Freeze

**ORU and RES are dual-asset system only under frozen protocol rules.**

ORU and RES semantics, issuance paths, convergence-linked mint behavior, and script-as-compliance classification are governed exclusively by the frozen Protocol Layer. No domain may introduce parallel asset models, account balances as truth sources, synthetic token semantics, RES-as-mining-reward narratives, or alternate issuance mechanisms.

---

## §4 — Phase Transition Model

RIGOR execution follows a defined lifecycle. Phase transitions are **governance events**, not automatic side effects of partial work.

```text
Phase 1: Development          →  COMPLETED
Phase 2: Runtime Verification →  IN PROGRESS (PROOF-034)
Phase 3: Full Simulation      →  POST TASK-SIM-003 (not yet active)
Phase 4: Release Preparation  →  POST verification acceptance
Phase 5: Deployment           →  CONDITIONAL — NOT YET ALLOWED
```

### Phase 1 — Development (completed)

Consensus rules, formal proof catalog (PROOF-001–033), specification layer, closed-testnet infrastructure, mining stack, explorer, and GUI prototype reached established or operational status under frozen protocol baseline.

**Exit condition met.** No return to pre-freeze development without explicit governance action.

### Phase 2 — Runtime Verification (current)

PROOF-034 executes: regtest archive mining to height **≥ 2,100,001**, golden vector materialization, harness verification, artifact bundle publication.

| During Phase 2 | Status |
|----------------|--------|
| Protocol Layer | **Frozen** |
| Runtime Verification Domain | **Active, read-only policy for external alteration** |
| Simulation Domain | **Not authoritative**; preparatory work only if non-interfering |
| `equivalence_claimed` | **`false`** — mandatory |
| Public/mainnet financial positioning | **Prohibited** per freeze roadmap |

**Entry:** PROOF-034 procedure established and archive execution commenced.  
**Exit:** PROOF-034 acceptance (M6–M7 criteria): full vector verification, zero mismatches, published artifact bundle.

### Phase 3 — Full Simulation System (future)

Post TASK-SIM-003: Phase-3 full system simulation including ORU, RES, and convergence replay in an isolated Lab Layer environment.

| Constraint | Requirement |
|------------|-------------|
| Isolation | Simulation MUST NOT affect Consensus Domain or Runtime Verification Domain |
| Determinism | Replay MUST be reproducible given declared inputs |
| Non-certification | Simulation success does not set `equivalence_claimed` |

**Entry:** TASK-SIM-003 completion and steward sign-off.  
**Exit:** Simulation system documented, bounded, and operating under domain model rules.

### Phase 4 — Release Preparation (future)

Post-verification acceptance: release gate evaluation, public repository assembly, narrative alignment, production readiness audits.

| Constraint | Requirement |
|------------|-------------|
| Evidence-driven | Release decisions from gate matrix, not narrative |
| Freeze preserved | No consensus, protocol, or mining changes without governance |
| Equivalence | `equivalence_claimed` remains **false** under current model |

**Entry:** PROOF-034 acceptance and gate prerequisites per release governance.  
**Exit:** Steward-declared readiness for deployment evaluation (not deployment itself).

### Phase 5 — Deployment (conditional, not yet allowed)

Mainnet or unrestricted public deployment.

**Current status: NOT ALLOWED.** Open mandatory gates, incomplete PROOF-034, and global `equivalence_claimed = false` policy block deployment. Phase 5 entry requires explicit governance authorization beyond this constitution.

---

## §5 — Non-Goals

This constitution explicitly does **NOT**:

| Non-Goal | Rationale |
|----------|-----------|
| Modify mining or consensus rules | Protocol Layer is frozen under Consensus Lock |
| Introduce new FORGE or BURN mechanisms | Outside frozen dual-asset model |
| Change ORU/RES issuance logic | IV-05 dual-asset freeze |
| Alter PROOF-034 execution | Non-Interference Principle (§1.3) |
| Claim production readiness | Phase 5 not authorized |
| Set `equivalence_claimed = true` | Equivalence Invariance (§1.4) |
| Substitute simulation for runtime archive | Scientific and Lab layers are complementary, not interchangeable |
| Grant Presentation Domain execution authority | IV-02, IV-03 |

Tasks that pursue any non-goal MUST be rejected or re-scoped before execution.

---

## §6 — Governance and Enforcement

### 6.1 Applicability

This constitution governs:

- All AI-assisted and human development tasks during PROOF-034 and thereafter until superseded by explicit governance version bump
- Simulation system design and operation (Phase 3)
- Release preparation and gate evaluation
- Cross-domain integration reviews

Domain-specific constitutions (e.g. Explorer AI Execution Constitution) MUST remain consistent with this document. On conflict, the more restrictive rule applies.

### 6.2 Task compliance

Every task in scope MUST:

1. Cite `TASK-AI-CONST-001` or this constitution in its task header when touching execution boundaries
2. Declare impact fields: Architecture, Consensus, Protocol, Mining, Freeze, `equivalence_claimed`
3. Identify affected execution domain(s) per [EXECUTION_DOMAIN_MODEL.md](./EXECUTION_DOMAIN_MODEL.md)
4. Confirm non-interference with PROOF-034 when Runtime Verification Domain is active

### 6.3 Violation handling

| Violation class | Response |
|-----------------|----------|
| Cross-domain write (simulation → consensus) | Block merge; remediate isolation |
| Documentation redefining protocol | Reject copy; restore frozen terminology |
| Premature equivalence claim | Revert artifact; audit all surfaces |
| PROOF-034 alteration | Halt task; operator review |
| Hidden non-determinism in correctness path | Block certification; fix or document exclusion |

---

## §7 — Final Constraint Declaration

This task and this constitution explicitly declare:

| Declaration | Status |
|-------------|--------|
| **No consensus modification** | **CONFIRMED** |
| **No protocol modification** | **CONFIRMED** |
| **No mining modification** | **CONFIRMED** |
| **`equivalence_claimed = false` preserved** | **CONFIRMED** |

---

## Required Verdict

| Field | Status |
|-------|--------|
| RIGOR Execution Constitution | **COMPLETE** |
| Core Principles Defined | **VERIFIED** |
| Execution Domains Referenced | **VERIFIED** |
| System Invariants (IV-01–IV-05) | **VERIFIED** |
| Phase Transition Model | **VERIFIED** |
| Non-Goals Documented | **VERIFIED** |
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Mining Impact | **NONE** |
| Freeze Impact | **NONE** |
| Equivalence Claimed | **false** |

---

*End of RIGOR AI Execution Constitution v1.0*

# RIGOR Execution Domain Model

**Task:** `TASK-AI-CONST-001`  
**Version:** 1.0  
**Date:** 2026-06-18  
**Status:** Canonical domain specification  
**Parent:** [RIGOR_EXECUTION_CONSTITUTION.md](./RIGOR_EXECUTION_CONSTITUTION.md)  
**Type:** Documentation only — **no code, consensus, protocol, mining, or GUI modifications**

| Field | Value |
|-------|-------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Mining Impact | **NONE** |
| Freeze Impact | **NONE** |
| `equivalence_claimed` | **false** |

This document defines the four RIGOR execution domains, their boundaries, permitted operations, forbidden operations, and cross-domain interaction rules. It implements §2 of the [RIGOR Execution Constitution](./RIGOR_EXECUTION_CONSTITUTION.md).

---

## 1. Domain Overview

```text
┌────────────────────────────────────────────────────────────────────────────┐
│  A  CONSENSUS DOMAIN                                                        │
│     Frozen · Authoritative for on-chain truth                               │
│     rigord · mining · block validation                                    │
└────────────────────────────────────────────────────────────────────────────┘
         ▲                              │
         │ read-only                    │ read-only (frozen rules)
         │ observational                ▼
┌────────────────────┐    ┌────────────────────────────┐    ┌─────────────────┐
│  B  RUNTIME        │    │  C  SIMULATION           │    │  D  PRESENTATION │
│  VERIFICATION      │    │     Isolated · Replay      │    │     Decoupled    │
│  PROOF-034 · GV    │    │     Phase-3 · ORU/RES      │    │     GUI · docs     │
│  Archive validation│    │     Convergence replay     │    │     website        │
└────────────────────┘    └────────────────────────────┘    └─────────────────┘
         │                              │                              │
         └──────────────────────────────┴──────────────────────────────┘
                    NO WRITE PATH TO DOMAIN A OR B
```

**Cardinal rule:** Domains B, C, and D MAY observe Domain A under defined read paths. Domains B, C, and D MUST NOT alter Domain A consensus state. Domain C MUST NOT alter Domain B verification execution. Domain D MUST NOT alter Domains A, B, or C execution state.

---

## 2. Domain A — Consensus Domain

### 2.1 Definition

The Consensus Domain encompasses all execution that determines **canonical chain state** under frozen protocol rules: node validation, block acceptance, UTXO updates, convergence resolution, RES mint emission, and mining work that produces valid blocks accepted by the network.

### 2.2 Authority

| Property | Value |
|----------|-------|
| Truth source | **Yes** — sole authority for on-chain validity |
| Mutability | **Frozen** — no semantic changes during PROOF-034 window |
| Determinism | **Required** for validation and state transition evaluation |

### 2.3 Components (conceptual)

| Component class | Role |
|-----------------|------|
| Node daemon | Validates and applies blocks; maintains UTXO set |
| Mining stack | Produces proof-of-work candidates and submits valid blocks |
| Convergence engine | Resolves winner selection under frozen rules |
| RES mint engine | Emits mint transactions when protocol conditions are met |

### 2.4 Permitted operations

- Execute frozen validation on incoming blocks and transactions
- Advance chain tip on valid block acceptance
- Serve authoritative observables to read-only consumers (RPC-style interfaces)
- Operate mining and pool infrastructure under frozen rules

### 2.5 Forbidden operations

| Forbidden | Reason |
|-----------|--------|
| Consensus rule changes | Protocol Layer frozen |
| Protocol semantic changes | IV-05, freeze policy |
| Mining rule changes | Constitution non-goals |
| Acceptance of simulation-injected state | IV-01 simulation isolation |
| Equivalence assertion | §1.4 Equivalence Invariance |
| GUI-driven state mutation | IV-02 presentation non-authority |

### 2.6 Current status

**STRICTLY FROZEN** under current ruleset. All development tasks touching Domain A are blocked unless explicitly authorized by protocol governance outside the verification window.

---

## 3. Domain B — Runtime Verification Domain

### 3.1 Definition

The Runtime Verification Domain encompasses PROOF-034 and all activities that collect, materialize, and verify **empirical runtime evidence** against golden vectors and archive acceptance criteria at height **2,100,001**.

### 3.2 Authority

| Property | Value |
|----------|-------|
| Truth source | **Yes** — for runtime archive empirical claims only |
| Mutability | **Read-only policy for external alteration** during active execution |
| Scope | Regtest archive, golden vectors, harness reports, verification manifests |

### 3.3 Components (conceptual)

| Component class | Role |
|-----------------|------|
| Archive mining execution | Produces contiguous regtest chain to target height |
| Golden vector materializer | Extracts fixtures at defined heights |
| Verification harness | Compares runtime output against expected vectors |
| Artifact publisher | Produces manifests, checksums, completion reports |

### 3.4 Permitted operations

- Continue PROOF-034 archive mining to height ≥ 2,100,001
- Materialize and verify golden vectors per established procedure
- Publish verification artifacts and monitoring snapshots
- Read consensus state from Domain A as execution proceeds (normal node operation during archive)

### 3.5 Forbidden operations

| Forbidden | Reason |
|-----------|--------|
| Alter acceptance criteria mid-run | Non-Interference Principle |
| External injection of verification verdicts | IV-02, IV-04 |
| Substitution of simulation output for archive | Layer separation |
| Premature completion claims | IV-04 PROOF-034 authority |
| Setting `equivalence_claimed = true` | Equivalence Invariance |

### 3.6 Interaction with Domain A

Domain B **uses** Domain A node execution to produce the archive. This is not a cross-domain violation: the archive is produced by normal frozen consensus execution on a designated regtest environment. External systems MUST NOT modify Domain A rules or inject alternative validation paths to satisfy Domain B shortcuts.

### 3.7 Current status

**ACTIVE — IN PROGRESS.** PROOF-034 archive execution is ongoing. Domain B is authoritative for empirical runtime claims until acceptance criteria are met.

---

## 4. Domain C — Simulation Domain

### 4.1 Definition

The Simulation Domain encompasses **isolated experimental replay** of RIGOR system behavior: Phase-3 full system simulation, ORU and RES path replay, and convergence scenario replay. Simulation operates in the Lab Layer and produces observational or experimental results only.

### 4.2 Authority

| Property | Value |
|----------|-------|
| Truth source | **No** — not authoritative for consensus or verification |
| Mutability | **Independent** — isolated environment |
| Activation | **Post TASK-SIM-003** — preparatory design may precede; authoritative simulation claims require Phase 3 entry |

### 4.3 Components (conceptual)

| Component class | Role |
|-----------------|------|
| Replay engine | Re-executes frozen rules against declared inputs |
| Scenario fixture store | Holds synthetic or archived inputs for replay |
| Simulation observer | Records replay outputs for analysis |
| Comparison reporter | Differs simulation output from golden or archive references (read-only compare) |

### 4.4 Permitted operations

- Replay ORU, RES, and convergence paths in isolated environments
- Import **read-only** copies of archived chain segments or golden vector fixtures for replay inputs
- Produce deterministic replay reports when inputs and rules are declared
- Support engineering analysis, regression exploration, and educational demonstration

### 4.5 Forbidden operations

| Forbidden | Reason |
|-----------|--------|
| Write to live consensus state | IV-01 simulation isolation |
| Alter PROOF-034 archive or harness | Non-Interference Principle |
| Submit simulated blocks to production network paths | Cross-domain violation |
| Certify equivalence from simulation alone | Equivalence Invariance |
| Introduce non-frozen protocol semantics in replay | IV-03, IV-05 |

### 4.6 Isolation requirements

Simulation environments MUST satisfy:

| Requirement | Description |
|-------------|-------------|
| **Network isolation** | No route for simulated blocks or transactions to reach production or archive nodes |
| **State isolation** | Simulation state stores are separate from consensus datadir |
| **Identity isolation** | Simulation artifacts are labeled non-authoritative in all Presentation outputs |
| **Determinism declaration** | Replay procedures MUST document inputs, rule version, and expected reproducibility |

### 4.7 Current status

**NOT YET ACTIVE** as authoritative Phase-3 system. Preparatory documentation and non-interfering design work MAY proceed. Simulation MUST NOT affect Domains A or B until Phase 3 entry per constitution §4.

---

## 5. Domain D — Presentation Domain

### 5.1 Definition

The Presentation Domain encompasses all human-facing and narrative systems: GUI applications, website, documentation, whitepapers, marketing copy, telemetry dashboards, and AI-generated explanatory content.

### 5.2 Authority

| Property | Value |
|----------|-------|
| Truth source | **No** — displays and explains; does not define |
| Mutability | **Fully independent** for presentation concerns |
| Correctness coupling | **None** — decoupled from execution correctness |

### 5.3 Components (conceptual)

| Component class | Role |
|-----------------|------|
| GUI observer | Read-only display of node and indexer data |
| Explorer and wallet observer UI | Pagination, charts, search, dashboards |
| Website and public narrative | Educational and status communication |
| Documentation corpus | Specifications, task reports, constitutions |
| Telemetry visualization | JSONL metrics, ops dashboards |

### 5.4 Permitted operations

- Display authoritative fields sourced from Domain A read interfaces
- Show PROOF-034 progress and verification status from Domain B artifacts
- Present simulation results from Domain C with explicit non-authoritative labeling
- Improve UX: pagination, filtering, sorting, search, charts, TLS, rate limiting (operational only)

### 5.5 Forbidden operations

| Forbidden | Reason |
|-----------|--------|
| Modify runtime verification execution | IV-02 |
| Redefine protocol rules in copy or schemas | IV-03 |
| Imply `equivalence_claimed = true` | Equivalence Invariance |
| Recompute convergence, eligibility, or draw as source of truth | Domain A authority |
| Describe ORU/RES as generic tokens with merged balances | IV-05 dual-asset freeze |
| Claim production readiness during Phase 2 | Phase model §4 |

### 5.6 Labeling requirements

When presenting cross-domain data, Presentation MUST label provenance:

| Data origin | Required presentation |
|-------------|----------------------|
| Domain A (live node) | "Authoritative chain state" |
| Domain B (archive/harness) | "Runtime verification evidence" |
| Domain C (simulation) | "Experimental replay — non-authoritative" |
| Domain D (derived/indexed) | "Observational — verify against node" |

### 5.7 Current status

**ACTIVE** for read-only observer and documentation work. All Presentation work MUST comply with IV-02 and IV-03.

---

## 6. Cross-Domain Interaction Matrix

Rows: **source domain**. Columns: **target domain**. Cell: permitted interaction type.

| From ↓ / To → | A Consensus | B Verification | C Simulation | D Presentation |
|---------------|-------------|----------------|--------------|----------------|
| **A Consensus** | Internal execution | Archive feed (normal node op) | Read-only rule reference | Read-only observables |
| **B Verification** | **NO write** | Internal harness | Read-only fixtures export | Status artifacts |
| **C Simulation** | **NO write** | **NO write** | Internal replay | Non-auth reports |
| **D Presentation** | **NO write** | **NO write** | **NO write** | Internal display |

### 6.1 Allowed read paths

| Reader | May read from | Condition |
|--------|---------------|-----------|
| B | A | Normal archive node operation only |
| C | A rules (frozen spec) | Documentation of frozen behavior, not live mutation |
| C | B fixtures | Read-only copy for replay input |
| D | A | Via RPC or faithful proxy/cache with staleness handling |
| D | B | Published artifacts and progress reports |
| D | C | Labeled experimental output |

### 6.2 Forbidden write paths

The following write paths are **unconditionally forbidden**:

```text
C → A   (simulation must not affect consensus)
C → B   (simulation must not alter verification)
D → A   (presentation must not modify consensus)
D → B   (presentation must not modify verification)
D → C   (presentation must not drive simulation inputs that feed back to A or B)
B → A   (verification must not alter consensus rules — external alteration)
```

Domain B producing an archive through Domain A node execution is **not** a B→A rule write; it is frozen consensus execution in a designated environment.

---

## 7. Domain–Invariant Mapping

| Invariant | Primary domains | Enforcement summary |
|-----------|-----------------|---------------------|
| IV-01 | C vs A | Simulation isolated; no consensus write |
| IV-02 | D vs B | GUI/docs cannot modify verification |
| IV-03 | D vs A | Documentation cannot redefine protocol |
| IV-04 | B | PROOF-034 authoritative to 2,100,001 completion |
| IV-05 | A (frozen) | ORU/RES dual-asset under protocol rules only |

---

## 8. Phase–Domain Activity Matrix

| Phase | A Consensus | B Verification | C Simulation | D Presentation |
|-------|-------------|----------------|--------------|----------------|
| 1 Development | Frozen baseline | Procedure established | Not active | Active (docs/GUI) |
| 2 Runtime Verification | Frozen · archive runs | **Active** | Preparatory only | Read-only display |
| 3 Full Simulation | Frozen | Complete (expected) | **Active** | May show sim results |
| 4 Release Preparation | Frozen | Artifacts published | Active (non-cert) | Gate-aligned narrative |
| 5 Deployment | Governed separately | Historical reference | Optional | Production UX |

---

## 9. Violation Taxonomy

| ID | Violation | Domains | Severity |
|----|-----------|---------|----------|
| V-01 | Simulation writes consensus state | C → A | **Critical** |
| V-02 | GUI alters verification harness | D → B | **Critical** |
| V-03 | Documentation introduces new issuance rules | D (content) | **Critical** |
| V-04 | Premature PROOF-034 completion claim | D or B | **High** |
| V-05 | `equivalence_claimed = true` anywhere | Any | **Critical** |
| V-06 | Simulation presented as archive substitute | C → D narrative | **High** |
| V-07 | Explorer/indexer recomputes convergence | D acting as A | **High** |
| V-08 | Cross-domain unlabeled data | D | **Medium** |

All Critical violations block release progression and require immediate remediation.

---

## 10. Final Constraint Declaration

This domain model explicitly declares:

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
| Execution Domain Model | **COMPLETE** |
| Four Domains Defined (A–D) | **VERIFIED** |
| Interaction Matrix | **VERIFIED** |
| Invariant Mapping | **VERIFIED** |
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Mining Impact | **NONE** |
| Freeze Impact | **NONE** |
| Equivalence Claimed | **false** |

---

*End of RIGOR Execution Domain Model v1.0*

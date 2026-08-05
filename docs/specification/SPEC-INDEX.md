# RIGOR Specification Index

**Track:** Formal Specification Program  
**Task:** `TASK-SPEC-001` … `TASK-SPEC-008`  
**Purpose:** Canonical index of the specification layer. All future formal models and
proofs MUST reference documents listed here.

---

## Authority

The specification layer defines what RIGOR **SHOULD** do. It does not define what
any implementation currently does. Implementation is evidence for alignment, not a
source of normative semantics.

**Hierarchy:**

```
Specification  →  Formal Model  →  Proof  →  Runtime Alignment Evidence
```

**Specification supersedes implementation** (see `SPEC-000` §7).

**Freeze boundary:** Nothing in this index or its documents supersedes
`MAINNET_PROTOCOL_FREEZE_DECLARATION.md` or `TASK-MAINNET-FREEZE-001`.

---

## Published Documents

| ID | Title | Status | Description |
|----|-------|--------|-------------|
| **SPEC-000** | [Consensus Specification](./SPEC-000-CONSENSUS-SPECIFICATION.md) | **ESTABLISHED** (skeleton) | Scope, system model, consensus objectives, assumptions, trust anchors, non-goals, formal-verification relationship, freeze compatibility |
| **SPEC-001** | [State Model](./SPEC-001-STATE-MODEL.md) | **ESTABLISHED** | State categories, minimal consensus state, runtime/observational/external state, transition model, SI placeholders, FVR boundary, freeze compatibility |
| **SPEC-002** | [Economic Model (ORU/RES)](./SPEC-002-ECONOMIC-MODEL.md) | **ESTABLISHED** | ORU subsidy/burn recurrence, RES mint/cap recurrence, falsifiable economic invariants, diagnostic cross-checks (non-authoritative), freeze compatibility |
| **SPEC-003** | [Consensus Rules](./SPEC-003-CONSENSUS-RULES.md) | **ESTABLISHED** | Seven-layer validation hierarchy, CR-001..CR-063 predicates, failure model, FVR boundary, freeze compatibility |
| **SPEC-004** | [Trust Anchors](./SPEC-004-TRUST-ANCHORS.md) | **ESTABLISHED** | Hashing/PoW/clock/network/storage/draw anchors, A-1..A-5 open audit registry, failure conditions, freeze compatibility |
| **SPEC-005** | [Consensus Invariants](./SPEC-005-CONSENSUS-INVARIANTS.md) | **ESTABLISHED** | CI-CONSENSUS/STATE/ECONOMIC/DRAW/FREEZE invariants, dependency graph, FVR boundary, SI→CI mapping, freeze compatibility |
| **SPEC-006** | [Runtime / Formal Mapping](./SPEC-006-RUNTIME-FORMAL-MAPPING.md) | **ESTABLISHED** | Spec→formal→Coq→witness mappings, MP principles, CI/CO/TA/FVR tables, equivalence boundary, verification hierarchy |
| **SPEC-007** | [Equivalence Boundaries](./SPEC-007-EQUIVALENCE-BOUNDARIES.md) | **ESTABLISHED** | CES/ECS layers, FVR-I..N closure boundaries, draw impossibility, PD-4F debt, future scoped equivalence conditions |

---

## Planned Documents (Placeholders)

All planned specification-layer documents through SPEC-007 are now **ESTABLISHED**.
Future SPEC entries require a new formal-program governance task.

| ID | Title | Status | Scope (planned) |
|----|-------|--------|-----------------|
| *(none)* | — | — | Specification layer PHASE-1 skeleton complete |

---

## Related Normative Sources (External to SPEC Layer)

These ratified documents inform the specification layer until superseded by an
ESTABLISHED SPEC entry:

| Document | Role |
|----------|------|
| `RFC-000-consensus-constitution.md` | Constitutional invariants |
| `RFC-008-canonical-mathematical-specification.md` | Canonical formula interpretation |
| `RFC-009` … `RFC-015` | Frozen convergence, economics, witness rules |
| `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` | Mainnet freeze statement |
| `ARCHITECTURE.md` | Engineering architecture (non-normative for consensus semantics) |

---

## Formal Program Status

| Field | Status |
|-------|--------|
| Specification Layer | **INITIALIZED** |
| Consensus Specification | **ESTABLISHED** (`SPEC-000`) |
| State Specification | **ESTABLISHED** (`SPEC-001`) |
| Economic Specification | **ESTABLISHED** (`SPEC-002`) |
| Consensus Rules Specification | **ESTABLISHED** (`SPEC-003`) |
| Trust Anchor Specification | **ESTABLISHED** (`SPEC-004`) |
| Consensus Invariants Specification | **ESTABLISHED** (`SPEC-005`) |
| Runtime / Formal Mapping Specification | **ESTABLISHED** (`SPEC-006`) |
| Equivalence Boundaries Specification | **ESTABLISHED** (`SPEC-007`) |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |
| Coq ≡ Runtime | **NOT ESTABLISHED** |

---

## Revision History

| Version | Date | Change |
|---------|------|--------|
| 0.1 | 2026-06-05 | `TASK-SPEC-001` — index created; SPEC-000 established |
| 0.2 | 2026-06-05 | `TASK-SPEC-002` — SPEC-001 state model established |
| 0.3 | 2026-06-05 | `TASK-SPEC-003` — SPEC-002 economic model established |
| 0.4 | 2026-06-05 | `TASK-SPEC-004` — SPEC-003 consensus rules established |
| 0.5 | 2026-06-05 | `TASK-SPEC-005` — SPEC-004 trust anchors established |
| 0.6 | 2026-06-05 | `TASK-SPEC-006` — SPEC-005 consensus invariants established |
| 0.7 | 2026-06-05 | `TASK-SPEC-007` — SPEC-006 runtime/formal mapping established |
| 0.8 | 2026-06-05 | `TASK-SPEC-008` — SPEC-007 equivalence boundaries established; specification layer complete |

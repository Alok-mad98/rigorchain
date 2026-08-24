# Document inventory

**Task:** `TASK-RIGOR-DOCUMENTATION-CONSOLIDATION-P0-003`  
**Date:** 2026-08-22  
**Scan:** `docs/`, `docs/whitepaper/`, `docs/architecture/`, `docs/explorer/`, `docs/rfc/`  
**Code / consensus:** **NONE**

```
equivalence_claimed = false
```

---

## Public cores

Exactly three public core whitepapers:

| File | Question | Status |
|------|----------|--------|
| `docs/whitepaper/RIGOR_PROTOCOL_IDENTITY.md` | What is RIGOR? | **KEEP** (public core) |
| `docs/whitepaper/RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md` | How does the mathematics run? | **KEEP** (public core) |
| `docs/whitepaper/RIGOR_FORMAL_VERIFICATION_DESIGN.md` | Why is it trustworthy? | **KEEP** (public core) |

Public one-liner:

```
RIGOR is a dual-asset UTXO protocol built on script-derived asset
classification, stateless monetary architecture, and formally verified
consensus invariants.
```

`equivalence_claimed = false` — invariants of the protocol model, not C++
equivalence.

## Other keep (not public cores)

| File | Purpose | Status |
|------|---------|--------|
| `docs/whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md` | Official narrative paper | **KEEP** (not a fourth core) |
| `docs/architecture/RIGOR_EXECUTION_MODEL.md` | Consensus / runtime boundary | **KEEP** |
| `docs/architecture/ARCHITECTURE.md` | Implementation constraints | **KEEP** |
| `docs/explorer/EXPLORER_ARCHITECTURE.md` | Explorer implementation (independent product) | **KEEP** |
| `docs/explorer/RES_INDEXING_RULES.md` | Explorer engineering | **KEEP** |
| `docs/explorer/WITNESS_V1_DISPLAY_RULES.md` | Explorer engineering | **KEEP** |
| `docs/explorer/EXPLORER_UI_MINIMUM_SPEC.md` | Explorer engineering | **KEEP** |
| `docs/explorer/EXPLORER_RPC_COMPATIBILITY_MATRIX.md` | Explorer engineering | **KEEP** |
| `docs/explorer/EXPLORER_SECURITY_BOUNDARIES.md` | Explorer engineering | **KEEP** |

Explorer is an independent product. Observation authority stays a short section
in identity. There is no separate public “data authority boundary” paper.

---

## Stubs (redirects)

| File | Purpose | Status |
|------|---------|--------|
| `docs/PROTOCOL_IDENTITY.md` | Redirect to whitepaper identity | **KEEP** (pointer) |
| `docs/whitepaper/RIGOR_MATHEMATICAL_CONSENSUS_SPECIFICATION.md` | Redirect to DESIGN.md | **KEEP** (pointer) |
| `docs/rfc/README.md` | Pointer to archived RFC corpus | **KEEP** (pointer) |

---

## RFC corpus

| File | Purpose | Status |
|------|---------|--------|
| `docs/rfc/*.md` (except README) | Historical RFC texts | **ARCHIVE** → `docs/archive/rfc/` |
| `docs/archive/rfc/README.md` | Historical / not public specification | **KEEP** (archive label) |
| `docs/archive/rfc/RFC_HISTORY.md` | RFC name map | **ARCHIVE** (from `docs/internal/`) |

RFCs were not deleted. They are labeled **Historical — not the public protocol
specification**. C++ comments may still say `docs/rfc/`; that directory is a
pointer to the archive.

---

## Deleted (process / duplicate)

| File | Purpose | Status |
|------|---------|--------|
| `docs/internal/DEVELOPMENT_NOTES.md` | Process journal | **DELETE** |
| `RIGOR_DOCUMENTATION_AUTHORITY_SEPARATION.md` | Duplicate boundary | **DELETE** (merged into identity) |
| `EXPLORER_DATA_AUTHORITY_BOUNDARY.md` | Duplicate boundary | **DELETE** (explorer remains its own product via `EXPLORER_ARCHITECTURE.md`) |
| TASK / IMPLEMENTATION / DEBUG / MIGRATION-STEPS journals in the scanned public cores | Process, not protocol | **DELETE** (not restored) |

---

## Archived (one-time / historical)

| File | Purpose | Status |
|------|---------|--------|
| `docs/internal/MIGRATION_NOTES.md` | One-time explorer DB discard | **ARCHIVE** → `docs/archive/migration/` |
| `docs/archive/proof/` | PROOF-000 … development series | **ARCHIVE** (pre-existing) |
| `docs/archive/security/` | SECURITY-001 task drafts | **ARCHIVE** (pre-existing) |
| `docs/archive/architecture/` | Old architecture corpora | **ARCHIVE** (pre-existing) |
| `docs/archive/mining/` | Pilot / task mining docs | **ARCHIVE** (pre-existing) |

---

## Other keep (outside the two public cores)

These remain in-tree. They are not the public identity pair.

| Path | Purpose | Status |
|------|---------|--------|
| `docs/specification/` | Formal specification program | **KEEP** |
| `docs/verification/` | Formal-verification operator map | **KEEP** |
| `docs/security/` | Security audits and models (not process journals) | **KEEP** |
| `docs/whitepaper/chapters/` | Whitepaper chapters | **KEEP** |
| `docs/architecture/` remaining reports | Component graph, determinism map, constitution notes | **KEEP** |
| `docs/internal/rigor-official-tree.json` | Repository layout pin | **KEEP** |

---

## Remaining process files (outside scanned public cores)

Not deleted in this pass. They are still TASK / plan reports, not protocol:

| File | Suggested next status |
|------|------------------------|
| `docs/rigor-qt/TASK-P2-008-MIGRATION-PLAN.md` | **ARCHIVE** or **DELETE** |
| `docs/rigor-qt/TASK-P2-008A-REPORT.md` | **ARCHIVE** or **DELETE** |
| `docs/security/TASK-SECURITY-001-F4-ISSUANCE-RATE-FEEDBACK.md` | **ARCHIVE** (copy already under `archive/security/`) |
| `docs/audit/TASK_MAINNET_CLOSURE_001_COMPLETION.md` | **ARCHIVE** or **DELETE** |

Historical audit reports under `docs/security/`, `docs/audit/`, and
`docs/release/` still cite `docs/rfc/…` as the path at the time of writing.
Those snapshots were not rewritten. Live navigation points at `docs/archive/rfc/`.

---

## Target tree

```
docs/
  README.md
  DOCUMENT_INVENTORY.md

  whitepaper/
    RIGOR_PROTOCOL_IDENTITY.md          # What is RIGOR?
    RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md  # How does the mathematics run?
    RIGOR_FORMAL_VERIFICATION_DESIGN.md     # Why is it trustworthy?
    RIGOR_PROTOCOL_WHITEPAPER.md

  architecture/
    RIGOR_EXECUTION_MODEL.md            # consensus / runtime boundary
    ARCHITECTURE.md

  explorer/
    EXPLORER_ARCHITECTURE.md            # explorer implementation
    (indexing / display / RPC engineering)

  archive/
    rfc/                                # RFC-xxx — Historical, not active spec
    migration/
    proof/
    security/
    architecture/
    mining/
```

---

## Rules applied

1. Process records are not protocol. Deleted, not republished.
2. Duplicate authority papers are not independent public docs. Explorer is an
   independent product; keep `EXPLORER_ARCHITECTURE.md`.
3. RFC texts are not deleted. They are archived and labeled historical.

**Consensus impact: NONE**

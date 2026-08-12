# Public Repository Structure Report

**Task:** `TASK-PUBLIC-REPOSITORY-PROFESSIONALIZATION`  
**Date:** 2026-08-05  
**Repository:** `rigorchain/rigorchain` (local tree `~/rigor-official-tree`)  
**Consensus code modified:** none

---

## 1. Objective

Present the official repository as a mature protocol project: short README, clear
documentation map, public material separated from development archives.

---

## 2. Current structure (after this task)

```
rigorchain/
├── README.md                 # Concise public entry (navigation layer)
├── ARCHITECTURE.md           # Stub → docs/architecture/ARCHITECTURE.md
├── CONTRIBUTING.md
├── INSTALL.md
├── LICENSE
├── SECURITY.md
├── core/                     # Consensus node
├── pool/
├── miner/
├── explorer/
├── web/
├── qt/
├── installer/
├── formal_verification/
└── docs/
    ├── README.md             # Documentation map
    ├── WHITEPAPER.md
    ├── MAINNET_PROTOCOL_FREEZE_DECLARATION.md
    ├── SCRIPT_STRUCTURE.md
    ├── architecture/
    │   └── ARCHITECTURE.md   # Canonical architecture constraints
    ├── rfc/
    ├── specification/
    ├── verification/
    │   └── FORMAL-VERIFICATION-VERDICT.md
    ├── security/             # Public security model + key closure reports
    ├── mining/               # Operator / pool specs (no pilots)
    ├── explorer/
    ├── wallet/
    ├── constitution/
    ├── infrastructure/
    ├── rigor-qt/
    ├── archive/              # Historical records (not primary UX)
    │   ├── proof/
    │   ├── security/
    │   ├── architecture/
    │   └── mining/
    └── internal/             # Build metadata (not visitor-facing)
```

---

## 3. Recommended vs previous

| Previous | Recommended / applied |
|----------|------------------------|
| Long README with full draw mathematics | Short README + links to RFCs |
| `docs/FORMAL-VERIFICATION-VERDICT.md` at docs root | `docs/verification/` |
| Root `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` | Under `docs/` |
| Root `ARCHITECTURE.md` as sole copy | Canonical under `docs/architecture/`; root stub |
| 16 security files including TASK drafts | 6 public security docs; TASK drafts in `archive/security/` |
| 34+ PROOF-* files under `docs/protocol/proof/` | Archived under `docs/archive/proof/` |
| Master knowledge base in architecture/ | Archived |
| Closed-testnet pilot + mining TASK in mining/ | Archived |
| `.rigor-official-tree` at root | `docs/internal/` |

---

## 4. Moved files

| From | To |
|------|----|
| `docs/FORMAL-VERIFICATION-VERDICT.md` | `docs/verification/FORMAL-VERIFICATION-VERDICT.md` |
| `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` | `docs/MAINNET_PROTOCOL_FREEZE_DECLARATION.md` |
| `docs/architecture/RIGOR_MASTER_KNOWLEDGE_BASE.md` | `docs/archive/architecture/` |
| `docs/protocol/proof/*` | `docs/archive/proof/` |
| `docs/mining/POOL_CLOSED_TESTNET_PILOT.md` | `docs/archive/mining/` |
| `docs/mining/TASK-MINING-022-CPU-MINER-AGENT-SPEC.md` | `docs/archive/mining/` |
| `docs/security/TASK-*`, `SECURITY-001-A5-DESIGN.md`, `SECURITY-001-A-DIFF-SCOPE.md`, `PROTOCOL_FREEZE_MIGRATION_CHAIN.md` | `docs/archive/security/` |
| `.rigor-official-tree` | `docs/internal/rigor-official-tree.json` |
| `ARCHITECTURE.md` (full) | `docs/architecture/ARCHITECTURE.md` (+ root pointer) |

Git history was not rewritten. Moves use `git mv` where applicable; content is preserved.

---

## 5. Preserved history

- All archived PROOF / SECURITY / mining documents retain original wording.
- Security closure reports (`SECURITY-001-A-FINAL-REPORT`, A-7, A-8) remain under
  `docs/security/` for public auditability without flooding navigation with TASK drafts.
- Terminology alignment docs from the prior documentation task remain under `docs/security/`.

---

## 6. Acceptance mapping

| Criterion | How met |
|-----------|---------|
| Visitor understands RIGOR in ~5 minutes | Short README + documentation map |
| README concise | Navigation layer; math deferred to RFCs |
| Architecture clear | `docs/architecture/ARCHITECTURE.md` |
| ORU / RES separated | Dedicated README sections |
| RES not a mining reward | Explicit README wording |
| Historical evidence accessible | `docs/archive/` |
| Looks like a protocol project | Root cleaned; archives demoted |
| Consensus unchanged | See `CONSENSUS_CODE_UNCHANGED.md` |

# Protocol Documentation Alignment Report

**Task:** `TASK-PUBLIC-REPOSITORY-PROFESSIONALIZATION`  
**Date:** 2026-08-05  
**Companion:** [`PROTOCOL_TERMINOLOGY_ALIGNMENT.md`](./PROTOCOL_TERMINOLOGY_ALIGNMENT.md),
[`README_ARCHITECTURE_ALIGNMENT_REPORT.md`](./README_ARCHITECTURE_ALIGNMENT_REPORT.md)

---

## 1. Scope

Terminology and presentation alignment for the **official** public tree. No consensus,
parameter, or economic-model changes.

---

## 2. Outdated terms found (active surfaces)

| Term / phrase | Where found (pre-task / residual) | Action |
|---------------|-----------------------------------|--------|
| “the script is the asset” | Prior README | Replaced in public README |
| “script-as-asset” | Specs / architecture (prior task + residual) | Active specs use **script-as-compliance**; see prior alignment report |
| “ORU is the proof-of-work asset” (narrow) | Prior README | Expanded to security + economic coordination |
| “RES mining” / “RES mining reward” / “RES block subsidy” | Active public docs | **Absent** after audit (PASS) |
| “lottery” as protocol label | Prior long README draw section | Public README: “not a lottery”; math in RFCs |
| “asset field” as design | Forbidden | Root architecture stub + architecture doc forbid parallel fields |

### Active-surface re-scan (this task)

```
rg -i 'script is the asset|RES mining reward|RES block subsidy|RES is mined'
  README.md docs/WHITEPAPER.md docs/architecture docs/specification
  docs/verification docs/security/PRODUCTION*
→ no matches in active public entry documents
```

Mentions of the **old** phrase inside `PROTOCOL_TERMINOLOGY_ALIGNMENT.md` and architecture
notes that *document the rename* are intentional and not regressions.

---

## 3. Replacements (public presentation)

| Old | New (active) |
|-----|----------------|
| script is the asset | **script is the compliance layer** |
| script-as-asset (spec titles) | **script-as-compliance** |
| ORU = PoW asset only | ORU = PoW **security and economic coordination** asset |
| RES implied as second subsidy | RES = **independent scarce resource**; not mined |
| Draw as lottery | **Deterministic consensus allocation** |
| Full σ mathematics in README | High-level only; RFCs for detail |

---

## 4. Preserved historical documents

Moved to `docs/archive/` **without rewriting**:

| Archive path | Examples |
|--------------|----------|
| `archive/security/` | TASK-SECURITY-001-A8-DESIGN-REVIEW, A9*, A5 design, DIFF-SCOPE |
| `archive/proof/` | PROOF-000 … development series |
| `archive/mining/` | POOL_CLOSED_TESTNET_PILOT, TASK-MINING-022 |
| `archive/architecture/` | RIGOR_MASTER_KNOWLEDGE_BASE |

These may still contain “lottery”, “script-as-asset”, or pre-A8 single-block draw
descriptions. That is required for audit integrity.

---

## 5. README role after professionalization

README is a **navigation layer**, not a whitepaper:

- What RIGOR is
- Status + links
- Architecture overview (short)
- ORU / RES roles
- RES allocation (high level)
- Documentation map
- Build / security pointers

---

## 6. Status

| Check | Result |
|-------|--------|
| Active docs use compliance-layer language | PASS |
| Active docs do not claim RES is mined | PASS |
| Historical wording preserved in archive | PASS |
| Consensus code unchanged | PASS — `CONSENSUS_CODE_UNCHANGED.md` |

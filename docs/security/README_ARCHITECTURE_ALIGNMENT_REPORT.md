# README / Architecture Alignment Report

**Task:** `TASK-DOC-README-ARCHITECTURE-UPDATE`  
**Date:** 2026-08-05  
**Type:** Documentation only  
**Consensus files modified:** **none**

---

## 1. Objective

Align public-facing README and active architecture documentation with the protocol
philosophy after SECURITY-001-A, A-7, and A-8 design evolution:

- Replace “script is the asset” with “script is the compliance layer”
- Clarify ORU as PoW security and economic coordination asset
- Clarify RES as an independent scarce resource that is **not mined**
- Describe the convergence draw as deterministic consensus allocation (windowed, deferred)
- Preserve UTXO-native Bitcoin-derived constraints

No consensus code, transaction format, block format, RES parameters, or ORU issuance
rules were changed.

---

## 2. Outdated statements found

### 2.1 Active / public surfaces (updated)

| Location | Outdated statement | Disposition |
|----------|-------------------|-------------|
| `README.md` | “the script is the asset” | Replaced with compliance-layer framing |
| `README.md` | “ORU is the proof-of-work asset” | Expanded to security + economic coordination |
| `README.md` | Draw described without explicit “not a lottery / not mining reward” framing | Expanded with allocation requirements and fairness diagram |
| `ARCHITECTURE.md` | “Script-as-Asset” as sole core principle | Replaced; clarified classification vs legitimacy |
| `ARCHITECTURE.md` | “asset field added to `transaction.h`” | **Removed** — contradicted storage invariant |
| `docs/WHITEPAPER.md` §2.1–2.3 | “script-as-asset … authoritative definition of asset type”; RES as “secondary emission”; mining omits “does not mint RES” | Updated |
| `SPEC-000` §1.2–1.5 | “script-as-asset”; ORU/RES role table | Updated |
| `SPEC-001` SI-011 / UTXO § | “derivable from script only / script alone” | Clarified class vs legitimacy |
| `SPEC-003` CR-023 | “inferred from script parsing alone (script-as-asset)” | Retitled Asset Class Derivable |
| `SPEC-005` SI-011 row | “script-as-asset” | Renamed script-as-compliance |
| `RIGOR_GLOBAL_ARCHITECTURE_REPORT.md` §1 | Script-as-Asset; RES as selection-derived “Reward”; stale `T = 2^240` note | Updated |
| `RIGOR_MASTER_KNOWLEDGE_BASE.md` | Multiple “script-as-asset”; executive summary with pre-A7 mint narrative | Principle lines + executive summary updated; historical note added |
| `RIGOR_EXECUTION_CONSTITUTION.md` IV-05 | “script-as-asset classification” | Updated |
| `EXPLORER_ARCHITECTURE.md` | “Script-as-asset” | Updated |
| `CONTRIBUTING.md` | “Assets are parsed, not persisted” | Clarified classified-by-script / enforced-by-consensus |
| `web/.../ProtocolPage.tsx` | Script-as-asset; DurinHash; \|F\|≥2; Ed25519 PCORB | Aligned to current public description |
| `web/.../ProtocolCard.tsx` | Script inference only; “MTH burn” | Updated |
| Web design specs (`COMPONENT_DESIGN_SYSTEM`, `FRONTEND_SPECIFICATION`, `UI_LAYOUT_SPEC`) | “Script-as-asset” label | Renamed |
| `.cursorrules` | 「脚本即资产」 | Updated to 「脚本即合规层」 with iron laws preserved |

### 2.2 Historical / audit records (preserved)

| Location | Statement | Disposition |
|----------|-----------|-------------|
| `docs/security/TASK-SECURITY-001-A8-DESIGN-REVIEW.md` | Uses “lottery” / “mining reward” in **analysis** of failure modes and philosophy requirements | **Preserved** — historical design review |
| `docs/security/SECURITY_001_A8_REPORT.md` | “The lottery was not a lottery” | **Preserved** |
| `docs/security/TASK-SECURITY-001-A9C-*.md` | “lottery ticket” equilibrium language | **Preserved** |
| `docs/security/SECURITY-001-A5-DESIGN.md` | “script-as-asset rule” / lottery ticket metaphor | **Preserved** |
| `docs/security/SECURITY-001-A-FINAL-REPORT.md` | Pre-fix lottery-governed phrasing | **Preserved** |
| `docs/rfc/RFC-014-v1.2.md` | “lottery ticket” in security rationale | **Preserved** (RFC errata history) |
| Older freeze / audit / publication corpora under `docs/` not in the public curated set | Various “script-as-asset” | Left in place where not part of the official public tree; not rewritten wholesale |

Rationale: historical documents record what was believed or attacked at the time. Rewriting
them would destroy the audit trail. Active reader-facing docs now point to
`PROTOCOL_TERMINOLOGY_ALIGNMENT.md`.

---

## 3. Replacements made (summary)

| Theme | Replacement |
|-------|-------------|
| Governing slogan | script is the **compliance layer** |
| ORU | PoW **security and economic coordination** asset |
| RES | Independent scarce resource; **not mined**; PoW does not create RES |
| Draw | Deterministic consensus allocation: burn → candidates → eligibility → winner → maturity |
| Fairness | Bitcoin couples PoW+subsidy; RIGOR separates secure ordering from RES scarcity allocation |
| Spec titles | SI-011 / CR-023 human titles → script-as-compliance / asset class |

Canonical terminology table: [`PROTOCOL_TERMINOLOGY_ALIGNMENT.md`](./PROTOCOL_TERMINOLOGY_ALIGNMENT.md).

---

## 4. README coverage check (task §9)

| Question | Answered in README? |
|----------|---------------------|
| What is RIGOR? | Yes — Bitcoin-derived dual-asset UTXO chain; script as compliance layer |
| Why two assets? | Yes — separation of security/coordination (ORU) from scarce allocation (RES) |
| What is ORU? | Yes — security, incentives, fees, RES burn cost, coordination |
| What is RES? | Yes — independent scarce resource |
| Why RES is not mined? | Yes — explicit; PoW does not create RES |
| Why PoW still important? | Yes — irreversible ordering for fair allocation |
| Difference from Bitcoin? | Yes — comparison table |

Marketing language avoided; technical precision preferred.

---

## 5. Acceptance criteria

| Criterion | Status |
|-----------|--------|
| README accurately represents current architecture | **PASS** |
| No active documentation claims RES is mined | **PASS** (active surfaces audited; historical analysis docs preserved) |
| No active documentation claims script alone is the complete asset definition | **PASS** |
| ORU and RES roles clearly separated | **PASS** |
| Historical audit documents remain intact | **PASS** |
| No consensus files modified | **PASS** — see §6 |

---

## 6. Consensus / non-doc verification

```
# Expected: empty (no core/src consensus paths in this task's diff)
```

Files touched are documentation, website copy, `.cursorrules`, and overlay README/
CONTRIBUTING only. No edits under `core/src/consensus/`, `core/src/validation.cpp`,
transaction/block formats, or parameter headers.

---

## 7. Sources of truth after this task

| Concern | Authority |
|---------|-----------|
| Public narrative | `README.md`, this report, `PROTOCOL_TERMINOLOGY_ALIGNMENT.md` |
| Implementation constraints | `ARCHITECTURE.md`, `.cursorrules` |
| Normative rules | `docs/rfc/` |
| What is proved | `docs/FORMAL-VERIFICATION-VERDICT.md` |
| Historical security decisions | `docs/security/SECURITY-001-*`, `TASK-SECURITY-001-*` (unchanged wording) |

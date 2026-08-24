# RIGOR Historical Boundary Audit

**Task:** Historical design documents vs active (RFC-017) protocol parameters  
**Date:** 2026-08-20  
**Consensus / RFC bodies / Coq / Python spec:** **unchanged**  
**Honesty:** `equivalence_claimed = false`

This audit marks the boundary. RFC-014 v1.3 (2026-08-22) **was rewritten** so its
body matches RFC-017 shipping numbers. RFC-015 Occam’s fixed `100 RES` remains
historical. Other RFC bodies listed below were not rewritten in that pass.

---

## Active vs historical numbers

| Source | Numbers | Boundary |
|--------|---------|----------|
| **RFC-017** | `E = 246`, `C = 1024` ORU, height-based `W(k)` | Active monetary / draw-exponent parameters |
| **RFC-014 v1.3** | Same shipping numbers as RFC-017 (`E = 246`, `C = 1024`, `W(k)`) | **Active architecture + numbers.** v1.0–v1.2 drafts (`2^240` / `65536`) are not live. |
| **RFC-015 Occam** | Fixed `RES_REWARD_AMOUNT` = `100 RES` | **Historical RFC.** **Not an active consensus parameter.** |

RFC-014 remains the constant-T / PATH B-PRIME **architecture** record. RFC-015
remains the `|F| ≥ 1` and `21,000,000 RES` cap **architecture** record. Those
decisions are not erased. Only the **numeric** RFC-014 exponent/`C` and the
**fixed** RFC-015 mint amount sit on the historical side of the boundary.

Corpus index (not an RFC): [`docs/archive/rfc/HISTORICAL_PARAMETER_STATUS.md`](../archive/rfc/HISTORICAL_PARAMETER_STATUS.md).

---

## 1. Files that remain historical (body not rewritten)

These stay as **design evolution**, **security reasoning**, and **protocol
decision history**. Original wording (including period-of-record “frozen” /
“normative”) is preserved.

### RFC corpus (`docs/archive/rfc/`) — no body edits

| File | Why it stays historical |
|------|-------------------------|
| `RFC-014-frozen-convergence-target.md` | **Rewritten v1.3** — shipping `E = 246`, `C = 1024`, `W(k)` |
| `RFC-014-v1.1.md` | A-6 unit-correction method; numbers restated to `E = 246` |
| `RFC-014-v1.2.md` | A-9-C window method; numbers restated to `C = 1024` |
| `RFC-015-OCCAM-REBALANCE.md` | Fixed `100 RES`; status “FROZEN (mainnet constitutional economics)” is original text |
| `RFC-001-deterministic-selection.md` | Cites `T = 2^240` and `100.00 RES` |
| `RFC-002-supply-evolution.md` | `1 successful RES mint = 100 RES` |
| `RFC-005-validation-state-machine.md` | Comment `= 2^240` |
| `RFC-008-canonical-mathematical-specification.md` | `T := 2^240` |
| `RFC-012-golden-test-vector-generator.md` | `2^240` |
| `RFC-015-RIGOR-ALPHA1-CONSENSUS-MODEL.md` | Points at Occam `100.00 RES` (different RFC-015 surface) |
| Other `RFC-*.md` without these numbers | Unrelated to this numeric boundary |

**RFC-017-SCHEDULED-RES-REWARD.md** is the **historical** monetary RFC (archived).
Public mathematics is `docs/whitepaper/RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md`.
Comparison rows that say “Was `2^240`” are RFC-017’s own history table — **not rewritten**.

### Security reports kept as decision records (except banners listed in §2)

| File | Role |
|------|------|
| `SECURITY-001-A-FINAL-REPORT.md` | A-6 unit-error record (banner added; body unchanged) |
| `SECURITY_001_A8_REPORT.md` | Sigma-PoW-01 reasoning (banner added; body unchanged) |
| `F4_LIFETIME_DECISION.md` | No-retarget decision (banner added; body unchanged) |
| Prior consistency / shipping / monetary audits | Inventories of drift; they **describe** history, they are not protocol SSOT |
| Other `docs/security/*` without these mint/`T` numbers | Out of this numeric boundary |

### Archive (`docs/archive/`)

| File | Role |
|------|------|
| `architecture/RIGOR_MASTER_KNOWLEDGE_BASE.md` | 2026-06-25 synthesis (banner added; body unchanged) |
| `security/PROTOCOL_FREEZE_MIGRATION_CHAIN.md` | Freeze-amendment trace (banner added; body unchanged) |
| `security/TASK-SECURITY-001-A8-DESIGN-REVIEW.md` | Dated A-8 design review — **no banner** (already under `archive/`) |
| `security/TASK-SECURITY-001-A9C-CANDIDATE-SET-WINDOWING.md` | Dated A-9C review — **no banner** |
| `proof/PROOF-002/003/004-*.md`, `PROOF034_MILESTONE_DEFINITIONS.md` | Golden / campaign fixtures (banners added; vectors unchanged) |
| Remainder of `docs/archive/**` | Path is the historical bucket |

### Whitepaper dated audits (no body rewrite)

`ALPHAPLUS_*_AUDIT_REPORT.md`, `WHITEPAPER_CONTENT_AUDIT_REPORT.md`,
`AUDIT_REPORT.md` — snapshots of draft vs protocol at audit time.

---

## 2. Files that needed a banner (added this pass)

Markers only. Required phrases: **Historical RFC** and **Not an active consensus
parameter**. No RFC body edits. No consensus / Coq / Python spec edits.

| File | Marker |
|------|--------|
| `docs/archive/rfc/HISTORICAL_PARAMETER_STATUS.md` | **New** corpus index (not an RFC) |
| `docs/specification/SPEC-INDEX.md` | Parameter-boundary block |
| `docs/specification/SPEC-000-CONSENSUS-SPECIFICATION.md` | Parameter-boundary block |
| `docs/specification/SPEC-002-ECONOMIC-MODEL.md` | Parameter-boundary block |
| `docs/whitepaper/chapters/05-mathematical-design.md` | Strengthened existing banner |
| `docs/whitepaper/chapters/04-convergence.md` | Historical line: “not an active consensus parameter” |
| `docs/whitepaper/RIGOR_TECHNICAL_WHITEPAPER_STRUCTURE.md` | §6.1 historical slice |
| `docs/whitepaper/RIGOR_PROTOCOL_WHITEPAPER_V1_STRUCTURE.md` | Header boundary |
| `docs/whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md` | Additive “Not an active consensus parameter” next to existing Historical RFC-014 lines |
| `docs/whitepaper/SOURCE_CORRECTIONS.md` | Drafting-record banner (table not rewritten) |
| `docs/security/SECURITY-001-A-FINAL-REPORT.md` | Security-record banner |
| `docs/security/SECURITY_001_A8_REPORT.md` | Security-record banner |
| `docs/security/F4_LIFETIME_DECISION.md` | Decision-record banner |
| `docs/archive/architecture/RIGOR_MASTER_KNOWLEDGE_BASE.md` | Archive snapshot banner |
| `docs/archive/security/PROTOCOL_FREEZE_MIGRATION_CHAIN.md` | Freeze-chain banner |
| `docs/archive/proof/PROOF-002-GOLDEN-RES-VECTORS.md` | Fixture banner |
| `docs/archive/proof/PROOF-003-GOLDEN-DRAW-VECTORS.md` | Fixture banner |
| `docs/archive/proof/PROOF-004-GOLDEN-STATE-VECTORS.md` | Fixture banner |
| `docs/archive/proof/PROOF034_MILESTONE_DEFINITIONS.md` | Fixture banner |

---

## 3. Places that needed no modification

| Location | Reason |
|----------|--------|
| All `RFC-*.md` bodies | Forbidden to modify RFC content |
| `formal_verification/coq/**` | Forbidden |
| `formal_verification/spec/rigor_consensus_spec.py` | Forbidden |
| `core/src/consensus/**` | Forbidden |
| SPEC-001 / SPEC-003 / SPEC-005 / SPEC-006 inline tables | Already labeled **Historical RFC-014** / **Historical RFC-015** after the shipping-parameter sync; SPEC-000 / SPEC-INDEX now carry the corpus marker |
| SPEC-004 / SPEC-007 | No RFC-014/015 mint/`T` numbers as protocol SSOT |
| Bound whitepaper RFC-017 `E = 246` / `C = 1024` / `W(k)` passages | Already the active description |
| `chapters/04` shipping block (`C = 1024`) | Describes RFC-017 |
| Website / freeze / explorer (out of this task’s directory list) | Already synchronized in the prior shipping-parameter task |
| RFC-017 comparison “Was `2^240`” | Historical contrast **inside** the active RFC; not a claim that `E = 240` is active |

Pre-existing public prose that still uses “shipping” or “live” **as a contrast**
(“not the shipping target”) next to an already-labeled **Historical RFC-014**
line was **not stripped**. Stripping it would rewrite teaching copy. New markers
use only the required phrases.

---

## 4. Forbidden wording (RFC-014 / RFC-015 numbers)

This audit does **not** call RFC-014/RFC-015 numbers “current”, “live”,
“shipping”, or “mainnet parameter” in **new** text.

Original RFC and archive sentences that used those words in 2026 remain in place
as historical wording (see §1). They are not treated as today’s parameter SSOT.

---

## 5. What historical documents are still for

| Use | Yes |
|-----|-----|
| Design evolution record (why constant-T, why Occam, why A-6/A-9C) | Yes |
| Security reasoning record (vacuous `2^240`, Sigma-PoW-01, F-4) | Yes |
| Protocol decision history (freeze tags, errata, golden fixtures at the time) | Yes |
| Active consensus parameter table | **No** — use `docs/whitepaper/RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md` + `params.h` / `rigor_consensus_lock.h` |

---

## Verdict

The RFC corpus is frozen as history under `docs/archive/rfc/`. SPEC and selected whitepaper / security /
archive files that a reader could mistake for active numbers now carry
**Historical RFC** / **not an active consensus parameter** markers. Shipping
`E = 246`, `C = 1024` ORU, and `W(k)` remain the active monetary parameters
(public description: mathematical design paper).

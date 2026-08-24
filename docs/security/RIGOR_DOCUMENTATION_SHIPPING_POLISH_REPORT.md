# RIGOR Documentation Shipping Polish Report

**Task:** `TASK-RIGOR-DOCUMENTATION-SHIPPING-POLISH-P0-001`  
**Date:** 2026-08-20  
**Scope:** Documentation synchronization only.  
**Consensus / wallet / miner / RFC impact:** **NONE**  
**Honesty:** `equivalence_claimed = false`

Resolves residuals from [RIGOR_PROTOCOL_CONSISTENCY_FINAL_AUDIT.md](./RIGOR_PROTOCOL_CONSISTENCY_FINAL_AUDIT.md): unlabeled `2^240` / `65536` shipping copy, root README draw constants, whitepaper §5, website landing/fairness pages, and Documentation sitemap.

---

## Shipping economics (canonical)

| Item | Value |
|------|-------|
| Exponent | `E = 246` |
| Mint cost | `C = 1024` ORU per RES mint |
| Aggregate expected wins | `n / 1024` |
| Per-draw (window 32) | `n / (1024 · 32)` |
| `schedule_sum` | `99450437` atoms |
| Epoch | `210000` blocks |
| Cutoff | `RES_REWARD_LAST_EPOCH = 250` |
| Maximum supply | `21,000,000` RES |

**Historical RFC-014 parameter:** `E = 240` ⇒ `2^240 / 2^256 = 1/65536` ⇒ `C = 65536` ORU per mint. Kept only where labeled.

Forbidden claims not introduced: fully proven blockchain, zero risk, complete equivalence.

---

## PART 1–3 — What changed

| Surface | Change |
|---------|--------|
| `README.md` | Shipping `E = 246`, `C = 1024`; historical RFC-014 `2^240` labeled; honesty sentence |
| Bound whitepaper Ch.5.1 + Appendix E + abstract | Shipping identities first; `65536` only as **Historical RFC-014 parameter** |
| `RIGOR_TECHNICAL_WHITEPAPER_STRUCTURE.md` §6.1 | Same split |
| `chapters/04-convergence.md` | Shipping `n / 1024` |
| `chapters/05-mathematical-design.md` | Banner: draft is historical RFC-014 teaching |
| V1 cover + V1 structure | Banner + abstract use shipping `E = 246` |
| Landing, Whitepaper §5, Science, Lab, ConvergencePanel | Shipping economics; historical 65536 labeled |
| Website design notes (`COMPONENT_DESIGN_SYSTEM`, `FRONTEND_SPECIFICATION`, `ARCHITECTURE_OVERVIEW`, `public-site/UI_LAYOUT_SPEC`) | Fairness panel uses `E = 246` / `C = 1024` |
| `docs/specification/SPEC-000/001/003/005` | Frozen `T` rows: shipping `E = 246`; `2^240` labeled historical |
| `formal_verification/spec/rigor_consensus_spec.py` | Comment only (production `E = 246`, not `2^240`) |
| `core/doc/PROTOCOL-NAMING-TRUTH-SOURCE.md` | RPC key description matches shipping `T` |

Whitepaper RFC-017 numeric lock (`99450437`, epoch `210000`, cutoff `250`, cap 21M) was already correct in Ch.2 and Appendix B; unchanged.

---

## PART 4 — Sitemap

`web/rigorchain-site/src/ia/sitemap.ts` (not `src/sitemap.ts`) Documentation children now include:

`/documentation/monetary-policy` — Monetary Policy — `KEEP`

`SubNav` lists it on all `/documentation/*` routes. Library card and `App.tsx` route already existed.

---

## PART 5 — Drift scan after polish

Search: `2^240`, `65536`, `RFC-014` in `*.md` / `*.tsx` / `*.ts`.

### Allowed — historical RFC / labeled teaching / prior audits

| Class | Examples |
|-------|----------|
| RFC corpus (not edited) | `RFC-014-*.md`, RFC-001/008/015 citations of v1.0 `2^240`; RFC-017 “Was `2^240`” |
| Labeled historical in public copy | README, bound Ch.5.1, Appendix E, landing, site whitepaper, Science, Lab, ConvergencePanel |
| Prior-draft chapter with banner | `chapters/05-mathematical-design.md` |
| Historical security / FV reports | SECURITY-001-A, A-8, `pre_a6_target_was_vacuous`, Selection.v comments, RES schedule audit |
| Prior whitepaper audits | `ALPHAPLUS_*_AUDIT_REPORT.md`, `WHITEPAPER_CONTENT_AUDIT_REPORT.md`, `SOURCE_CORRECTIONS.md` |
| Consistency-audit residuals (this folder, pre-polish record) | `RIGOR_PROTOCOL_CONSISTENCY_FINAL_AUDIT.md` and companions |
| Unrelated `65536` | Explorer tests (ORU amounts); Coq `2^16` fixed-point divisor in `ResSchedule.v` |

### Remaining current-protocol description (not edited this task)

These still state live `T = 2^240` or unlabeled `1/65536` as if shipping. **Out of scope** (wallet / explorer / release freeze text / RFCs). Track for a later docs pass:

| Path | Note |
|------|------|
| `docs/wallet/RIGOR_RPC_REQUIREMENTS.md` | Wallet docs — task forbids wallet changes |
| `docs/rigor-qt/FUTURE_GUI_RPC_SPEC.md` | Same |
| `docs/explorer/EXPLORER_RPC_COMPATIBILITY_MATRIX.md` | Explorer display “verify == 2^240” |
| `docs/explorer/RES_INDEXING_RULES.md`, `WITNESS_V1_DISPLAY_RULES.md` | Explorer |
| `docs/release/RIGOR_MAINNET_*.md`, `RIGOR_PUBLIC_SYSTEM_POSITIONING.md` | Release copy |
| `docs/MAINNET_PROTOCOL_FREEZE_DECLARATION.md` / freeze alignment audits | Operator freeze text (known stale vs `params.h`) |
| `docs/rfc/*` | RFC files — not edited |

SPEC freeze tables still list **100 RES per mint** in places (RFC-015 Occam). That is separate from the `2^240`/`65536` identity; RFC-017 `W(k)` is already in the bound whitepaper and Monetary Policy page. Not expanded here.

---

## Validation

| Check | Result |
|-------|--------|
| `npm run build` in `web/rigorchain-site` | **PASS** (`tsc --noEmit` + Vite production build) |
| Route / sitemap / library card for `/documentation/monetary-policy` | **PASS** |
| GitHub-linked in-tree files from Formal Verification / Security pages (10) | **PASS** |
| Consensus C++ / wallet / miner / `docs/rfc/` | **Unchanged** |

---

## Honesty

```
equivalence_claimed = false
```

No “fully proven blockchain”, “zero risk”, or “complete equivalence” claims added.

# AlphaPlus Whitepaper Website Integration Report

**Task:** `TASK-ALPHAPLUS-WHITEPAPER-WEBSITE-INTEGRATION-001`  
**Tree:** `~/rigor-official-tree/`  
**Date:** 2026-08-18  
**Layer:** Website / documentation only  
**Consensus impact:** **NONE**  
**`equivalence_claimed`:** `false`

This task binds the official whitepaper into the existing React/Vite portal.
It does not change consensus, RFCs, wallet accounting, miner code, or explorer
consensus logic.

---

## 1. Objective

Publish `docs/whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md` as a long-form page at:

```
/documentation/whitepaper
```

on `web/rigorchain-site/`, using the existing black / white / gray technical
theme.

---

## 2. Implementation

| Item | Path | Change |
|------|------|--------|
| Route | `web/rigorchain-site/src/App.tsx` | `<Route path="documentation/whitepaper" element={<WhitepaperPage />} />` |
| Page | `web/rigorchain-site/src/pages/WhitepaperPage.tsx` | Long-form official paper |
| IA | `web/rigorchain-site/src/ia/sitemap.ts` | Documentation child: Whitepaper (KEEP) |
| Navbar | `web/rigorchain-site/src/components/Navbar.tsx` | `Documentation → Whitepaper` cluster |
| Footer | `web/rigorchain-site/src/components/Footer.tsx` | Whitepaper link |
| Docs hub | `web/rigorchain-site/src/pages/DocumentationPage.tsx` | Featured Whitepaper card |
| Theme | `web/rigorchain-site/src/index.css` | Whitepaper layout + 1920 / 1366 / 390 rules |

Reused components: `SubNav`, `DarkCard`, `SectionTitle`, `Button`, existing
`.card`, `.prose-section`, `.data-table`, `.pipeline-diagram`, `.math-block`.

No second design system was added. Tokens remain:

- background `--bg-primary` `#000000`
- text `--text-primary` `#ffffff`
- cards / inputs `--bg-panel` `#1a1a1a`

---

## 3. Page structure

The live page sections match the official paper:

1. Abstract
2. Security Budget Problem
3. Dual-Asset Economic Architecture
4. Convergence Consensus
5. Mathematical Fairness
6. Mining Architecture
7. Self-Proving Blockchain
8. Formal Verification
9. Ecosystem and Roadmap
10. Appendix

Warning banner (required copy):

> This document describes protocol design and architecture.  
> It does not claim completed formal equivalence proof.  
> equivalence_claimed = false.

---

## 4. Navigation

Navbar now shows:

```
Documentation → Whitepaper
```

- `Documentation` → `/documentation`
- `Whitepaper` → `/documentation/whitepaper`

SubNav under Documentation also lists Whitepaper first. Footer includes
Whitepaper beside Documentation.

Existing primary items (Home, Download, Wallet, Mining, Explorer,
Zero-Trust, About) and lab nav are unchanged.

---

## 5. Cross-links

The whitepaper page links to:

| Surface | Route |
|---------|-------|
| Explorer | `/explorer` |
| Zero Trust Verification | `/zero-trust-verification` |
| Mining | `/mining` |
| Wallet | `/wallet` |
| RFC documentation | `/documentation/rfc` |

Additional in-body links: `/wallet/convergence`, `/experiment`, `/status`,
`/lab`.

---

## 6. Terminology

The page uses the required official strings:

| Term | Used |
|------|------|
| `Convergence_Root` | Yes |
| `draw_candidate_root` | Yes |
| Self-Proving Blockchain Commitment | Yes |
| Zero Trust Verification | Yes |
| Runtime Archive (PROOF-034) | Yes |

Formulas match the audited paper: `Hash256(concat(sort(eligible txids)))`,
`|F| < 2 → C = 0`, windowed `T`, `W = 32`, `equivalence_claimed = false`.

---

## 7. Responsive support

| Viewport | CSS |
|----------|-----|
| Desktop 1920px | `@media (min-width: 1920px)` — `--max-width: 1280px`, wider article measure |
| Laptop 1366px | existing `@media (max-width: 1366px)` plus full-width article |
| Mobile 390px | existing `@media (max-width: 390px)` plus stacked CTA, tighter banner, column nav cluster |

Tables and formula blocks use horizontal overflow so they remain readable on
390px.

---

## 8. Validation

```
cd web/rigorchain-site
npm run build
```

Result: **PASS** (`tsc --noEmit && vite build`, 80 modules).

```
npm run dev
```

Vite ready at `http://127.0.0.1:5173/`.

```
GET http://127.0.0.1:5173/documentation/whitepaper
```

Result: **200** `text/html` (SPA `index.html`; client router mounts
`WhitepaperPage`).

---

## 9. Files not changed

Confirmed out of scope:

- `core/src/kernel/`
- `core/src/consensus/`
- `core/src/pow.cpp`
- `core/src/validation.cpp`
- wallet / miner / explorer consensus logic
- `docs/rfc/`

---

## 10. Status

| Gate | Result |
|------|--------|
| Official route `/documentation/whitepaper` | **PASS** |
| Existing theme / components reused | **PASS** |
| Navbar Documentation → Whitepaper | **PASS** |
| Required warning banner | **PASS** |
| Cross-links | **PASS** |
| Required terminology | **PASS** |
| Responsive 1920 / 1366 / 390 | **PASS** |
| Production build | **PASS** |
| Consensus / RFC unchanged | **PASS** |

The whitepaper is now a live documentation surface on the official site.
`equivalence_claimed` remains **false**.

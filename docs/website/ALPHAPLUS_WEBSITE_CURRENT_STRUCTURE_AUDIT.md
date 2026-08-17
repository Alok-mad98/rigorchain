# AlphaPlus Official Website — Current Structure Audit

**Task:** `TASK-ALPHAPLUS-OFFICIAL-WEBSITE-ARCHITECTURE-AUDIT-MVP-001`  
**Domain:** https://www.rigorchain.com  
**Source of truth:** `~/rigor-official-tree/` only  
**Date:** 2026-08-17  
**equivalence_claimed:** `false`

| Field | Value |
|-------|-------|
| Consensus impact | NONE |
| Protocol impact | NONE |
| Existing code deleted | NONE |
| WIP reset / checkout / clean | NONE |

This audit inventories the live official portal and related web surfaces. It does not redesign the product. Valuable unfinished work is classified, not removed.

---

## 1. Search results — website-related code

| Location | Kind | Role vs www.rigorchain.com |
|----------|------|----------------------------|
| `web/rigorchain-site/` | React 18 + TypeScript + Vite SPA | **Official portal implementation** (CNAME `www.rigorchain.com`) |
| `web/public-site/` | Markdown specs only (no `package.json`) | Earlier publication blueprint (TASK-PUBLICATION-002). Spec-only; KEEP |
| `explorer/` | Express indexer + static HTML/CSS/JS | Block explorer (viewer). Linked from the portal |
| `pool/web/` | Yii/PHP Yaamp UI | Mining pool dashboard. Separate product surface |
| `core/src/qt/` | Qt GUI + branding SVG | Wallet UI and logo source. Not the website |
| `docs/publication/`, `docs/release/`, `docs/rfc/`, `docs/wallet/`, `docs/security/` | Markdown | Content sources for Documentation section |
| `installer/windows/` | NSIS + `.cmd` | Windows wallet/miner install scripts referenced by Mining IA |
| `docs/wallet/evidence/runtime/screenshots/` | Screenshot index | Wallet evidence; not yet used on the site |

No Next.js or Vue official-site app exists in this tree. The portal is Vite + React Router.

---

## 2. Existing pages (official portal)

Implementation: `web/rigorchain-site/src/App.tsx`  
Shell: `web/rigorchain-site/src/components/AppShell.tsx`

### 2.1 Live routes (KEEP)

| Path | Page module | Status |
|------|-------------|--------|
| `/` | `LandingPage.tsx` | KEEP — now also states What is AlphaPlus |
| `/download` | `DownloadPage.tsx` | KEEP — no binary URLs yet |
| `/wallet` | `WalletPage.tsx` | KEEP |
| `/mining` | `MiningPage.tsx` | KEEP |
| `/explorer` | `ExplorerPortalPage.tsx` | KEEP — viewer portal, not validator |
| `/documentation` | `DocumentationPage.tsx` | KEEP |
| `/zero-trust-verification` | `ZeroTrustVerificationPage.tsx` | KEEP — working TX / block / raw JSON verifier |
| `/verify` | redirect → `/zero-trust-verification` | KEEP |
| `/protocol` | `ProtocolPage.tsx` | KEEP — lab / frozen rules |
| `/science` | `SciencePage.tsx` | KEEP — state machine narrative |
| `/lab` | `LabPage.tsx` | KEEP — experimental framing |
| `/status` | `StatusPage.tsx` | KEEP — PROOF-034 observer |
| `/experiment` | `ExperimentPage.tsx` | KEEP — PROOF-034 explanation |
| `/verification` | redirect → `/status` | KEEP (legacy alias) |

### 2.2 Nested IA routes added this task (placeholders or wired modules)

These routes did not exist as a public sitemap. They were **added**; nothing was deleted.

| Path | Classification |
|------|----------------|
| `/download/windows-wallet` | PLACEHOLDER |
| `/download/linux-wallet` | PLACEHOLDER |
| `/download/cli` | PLACEHOLDER |
| `/download/miner` | PLACEHOLDER |
| `/wallet/overview` | KEEP (redirects to `/wallet`) |
| `/wallet/oru` | WIRED — existing `ProtocolCard` |
| `/wallet/maturity` | PLACEHOLDER |
| `/wallet/convergence` | WIRED — existing `ConvergencePanel` (was unused) |
| `/wallet/res` | WIRED — existing `RESMintPanel` (was unused) |
| `/mining/overview` | KEEP (redirects to `/mining`) |
| `/mining/windows` | PLACEHOLDER — Windows Miner guide |
| `/mining/pool` | PLACEHOLDER — pool dashboard link |
| `/mining/economics` | PLACEHOLDER |
| `/explorer/blocks` | PLACEHOLDER — maps to explorer `/blocks` |
| `/explorer/oru-supply` | PLACEHOLDER — maps to explorer `/oru.html` |
| `/explorer/res` | PLACEHOLDER — maps to explorer `/res.html` |
| `/explorer/fingerprint` | PLACEHOLDER |
| `/explorer/status` | PLACEHOLDER — explorer home dashboard |
| `/zero-trust-verification/what-is-zero-trust` | KEEP — explanation page |
| `/zero-trust-verification/transaction` | KEEP — redirect to verifier `?mode=tx` |
| `/zero-trust-verification/block` | KEEP — redirect to verifier `?mode=block` |
| `/zero-trust-verification/fingerprint` | PLACEHOLDER |
| `/documentation/user-guide` | PLACEHOLDER |
| `/documentation/developer-guide` | PLACEHOLDER |
| `/documentation/architecture` | PLACEHOLDER |
| `/documentation/rfc` | PLACEHOLDER |
| `/documentation/security` | PLACEHOLDER |
| `/about` | ADD — About AlphaPlus |

### 2.3 Specified but not implemented (MARK)

| Spec route | Where specified | Reality |
|------------|-----------------|--------|
| `/axioms` | `web/public-site/SITE_ARCHITECTURE.md`, `ARCHITECTURE_OVERVIEW.md` | Missing. Science page covers related math. Do not delete Science. |
| `/system-status` | public-site spec | Missing as a dedicated route. Header status bar + `/status` cover part of this. |
| `VerificationPage.tsx` / `ExplorerPage.tsx` | `PROJECT_STRUCTURE.md` | **Stale docs.** Files are not in `src/pages/`. Live replacements are `StatusPage` and `ExplorerPortalPage`. REPORT ONLY. |

### 2.4 Explorer HTML pages (KEEP — separate app)

| Path | File | Notes |
|------|------|-------|
| `/` | `explorer/public/index.html` | Network status dashboard |
| `/blocks` | `blocks.html` | Block list + fingerprint short IDs |
| `/block` | `block.html` | Block detail + Draw Fingerprint |
| `/tx` | `tx.html` | Transaction viewer |
| `/search` | `search.html` | Search |
| `/oru.html` | `oru.html` | Network ORU |
| `/res.html` | `res.html` | RES amounts |
| `/res-events.html` | `res-events.html` | RES events |
| `/address` | `address.html` | Address intelligence |
| `/status.html` | `status.html` | **Redirects to `/`** — MARK as alias, not a unique page |

Explorer nav already links to `https://www.rigorchain.com/zero-trust-verification` (`explorer/public/explorer-routes.js`).

---

## 3. Existing components

### 3.1 Official portal (`web/rigorchain-site/src/`)

| Component | File | Classification |
|-----------|------|----------------|
| Header / status bar | `AppShell.tsx` | KEEP |
| Primary navigation | `AppShell.tsx` + `ia/sitemap.ts` | KEEP |
| Lab navigation | `AppShell.tsx` | KEEP (Protocol / Science / Lab / Status / PROOF-034) |
| Footer | `AppShell.tsx` | KEEP |
| Cards | `.card` in `index.css`; `ProtocolCard.tsx` | KEEP |
| Buttons | `.btn-link`, `.btn-refresh`, `.zt-verify-btn` | KEEP |
| Images | Brand mark from Qt logo (copied to `public/assets/`) | KEEP original; site copy added |
| `VerificationNotice` | `VerificationNotice.tsx` | KEEP |
| `ThreeLayerModel` | `ThreeLayerModel.tsx` | KEEP |
| `ProtocolCard` | `ProtocolCard.tsx` | KEEP |
| `ConvergencePanel` | `ConvergencePanel.tsx` | KEEP — was unused; now WIRED on `/wallet/convergence` |
| `RESMintPanel` | `RESMintPanel.tsx` | KEEP — was unused; now WIRED on `/wallet/res` |
| `SubNav` / `SectionHub` | added this task | Visual IA chrome |
| System state | `context/SystemStateContext.tsx` | KEEP |
| Explorer client | `lib/explorerClient.ts` | KEEP |
| RPC client | `lib/rigordRpcClient.ts` | KEEP (read-only; blocked send/submit methods) |
| Zero-trust library | `lib/zeroTrust/*` | KEEP — WebCrypto verifier |

### 3.2 CSS / JS / build

| Item | Implementation |
|------|----------------|
| CSS framework | **None.** Custom CSS in `src/index.css` (CSS variables). Visual tuning surface. |
| JavaScript framework | React 18.3 + React Router 6 |
| Language | TypeScript 5.6 |
| Build system | Vite 5.4 (`npm run dev`, `npm run build`, `npm run preview`) |
| Tailwind / Next / Vue | **Not used** in the live site |

`COMPONENT_DESIGN_SYSTEM.md` mentions Tailwind class names and KaTeX. The running site does not depend on those packages. **REPORT ONLY** (spec/implementation drift). Do not delete the spec.

### 3.3 Explorer components (KEEP)

`status.js`, `compliance.js`, `convergence-panel.js`, `witness-panel.js`, `lineage-graph.js`, `address-intelligence.js`, `converge-proof-panel.js`, matching CSS files.

### 3.4 Pool UI (KEEP, out of portal IA)

`pool/web/yaamp/` — Yii PHP, jQuery, uni-form CSS. Mining pool operator UI. Official portal Mining section should **link** here later, not absorb the PHP tree.

---

## 4. Existing assets

| Asset | Path | On official site? |
|-------|------|-------------------|
| RIGOR logo SVG | `core/src/qt/resources/branding/logos/rigor-logo.svg` | Copied to `web/rigorchain-site/public/assets/rigor-logo.svg` (original untouched) |
| Qt icon | `core/src/qt/res/icons/rigor.svg` | No |
| Qt toolbar SVGs | `core/src/qt/res/src/*.svg` | No (wallet chrome) |
| Pool coin image | `pool/web/images/coin-OFF.png` | No |
| Favicon | Missing before this audit | Now SVG favicon via copied logo |
| Screenshots | `docs/wallet/evidence/runtime/screenshots/` | Not wired to Download/Wallet pages |
| Illustrations | None dedicated to marketing | N/A — site is scientific, not hype |
| Documentation assets | `docs/rfc/`, `docs/architecture/`, `docs/security/`, `docs/release/` | Linked in copy; not yet rendered as HTML docs |

---

## 5. Preserve existing work — classification table

| Category | Item | Action |
|----------|------|--------|
| Existing useful module | Zero-trust verifier (`lib/zeroTrust` + page) | KEEP |
| Existing useful module | Lab pages (Protocol, Science, Lab, Status, Experiment) | KEEP (secondary nav) |
| Existing useful module | `ConvergencePanel`, `RESMintPanel` | KEEP; WIRED into Wallet IA |
| Existing useful module | Explorer HTML + “Verify independently” | KEEP |
| Existing useful module | Pool PHP UI | KEEP (separate surface) |
| Existing design element | Dark scientific CSS tokens, cards, status pills | KEEP |
| Existing design element | Three-layer model, math blocks, equivalence badge | KEEP |
| Existing design element | Read-only / PROOF-034 banners | KEEP |
| Duplicate code | `web/public-site/*.md` vs `web/rigorchain-site/*SPEC*.md` | REPORT ONLY |
| Duplicate code | Tailwind mentioned in design system vs custom CSS | REPORT ONLY |
| Duplicate code | Stale `PROJECT_STRUCTURE.md` file list | REPORT ONLY |
| Broken component | Download page has no artifact URLs | MARK |
| Broken component | Live RPC/explorer often unconfigured locally (`VITE_*`) | MARK (expected offline) |
| Broken component | `/axioms` specified, not built | MARK |
| Broken component | Explorer `/status.html` only redirects home | MARK |
| Missing section | About AlphaPlus; nested Download/Mining/Docs IA | ADD PLAN (placeholders created) |
| Missing section | Windows Miner guide, pool dashboard embed | ADD PLAN |
| Missing section | Consensus fingerprint as first-class portal page | ADD PLAN |
| Missing section | User / developer / RFC / security doc pages | ADD PLAN |

**Rule followed:** nothing in the existing website tree was deleted. Unused panels were mounted, not discarded.

---

## 6. Data and trust model (current)

```
Explorer HTTP API  ──► explorerClient.ts ──┐
                                           ├──► SystemStateContext ──► pages
rigord JSON-RPC    ──► rigordRpcClient.ts ─┘

Public TXID / block hash ──► lib/zeroTrust (WebCrypto) ──► local VALID / INVALID
```

- Site is a **read-only observer**. No signing, no `send*`, no `submitblock`.
- Explorer is a **viewer**. Verifier does not trust explorer verdicts.
- `equivalence_claimed` must remain `false` (header badge + constitutional fault banner).

---

## 7. Visual preview (current)

| Command | URL |
|---------|-----|
| `cd web/rigorchain-site && npm run dev` | http://127.0.0.1:5173 |
| `npm run preview` (after build) | http://127.0.0.1:4173 |

Visual tokens for layout, spacing, colors, navigation, cards, and sections: `web/rigorchain-site/src/index.css` `:root`.

---

## 8. Related documents in-tree

| Document | Role |
|----------|------|
| `web/rigorchain-site/PROJECT_STRUCTURE.md` | Prior site map (partially stale) |
| `web/public-site/SITE_ARCHITECTURE.md` | 2026-06 publication blueprint |
| `docs/release/ALPHAPLUS_ZERO_TRUST_VERIFIER_WEB_PORTAL_REPORT.md` | Verifier design |
| `docs/website/ALPHAPLUS_OFFICIAL_WEBSITE_ARCHITECTURE_PLAN.md` | Recommended IA and roadmap (this task) |

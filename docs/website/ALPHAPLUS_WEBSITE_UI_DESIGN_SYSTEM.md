# AlphaPlus Website UI Design System

**Task:** `TASK-ALPHAPLUS-OFFICIAL-WEBSITE-UI-DESIGN-MVP-001`  
**Implementation:** `web/rigorchain-site/`  
**Tokens:** `web/rigorchain-site/src/index.css`  
**equivalence_claimed:** `false`

This document is the visual contract for www.rigorchain.com. It is UI only. Consensus, wallet accounting, and explorer backends are out of scope.

---

## Color system

| Token | Hex | Use |
|-------|-----|-----|
| `--bg-primary` | `#000000` | Page background |
| `--bg-secondary` | `#111111` | Status bar, footer, result wells |
| `--bg-panel` | `#1A1A1A` | Cards, panels |
| `--bg-input` | `#1A1A1A` | Input fields |
| `--text-primary` | `#FFFFFF` | Headings, body, nav active |
| `--text-secondary` | `#A0A0A0` | Captions, muted copy |
| `--border` | `#333333` | Cards, nav, tables |
| `--border-input` | `#444444` | Input outline |
| `--verified` | `#3FB950` | VALID / online |
| `--failure` | `#F85149` | INVALID / fault |
| `--degraded` | `#D29922` | Partial / placeholder |

Legacy aliases (`--bg`, `--surface`, `--text`, `--muted`) map to the tokens above so existing modules keep working.

---

## Typography

| Role | Token | Stack |
|------|-------|--------|
| UI | `--font-sans` | system-ui, Segoe UI, Roboto |
| Data | `--font-mono` | ui-monospace, Menlo, Consolas |

Scale: `--text-xs` 12px · `--text-sm` 14px · `--text-md` 16px · `--text-lg` 20px · `--text-xl` 28px · `--text-2xl` 36px.

Section titles are uppercase, tracked, secondary color.

---

## Spacing

`--space-1` 4px through `--space-7` 48px. Content max width `--max-width` 1120px. Card radius 4px. No drop shadows.

---

## Component rules

All visual rules live in `index.css`. Components do not introduce a second stylesheet.

| Component | File | CSS |
|-----------|------|-----|
| Navbar | `components/Navbar.tsx` | `.site-header`, `.top-nav`, `.nav-links` |
| Footer | `components/Footer.tsx` | `.site-footer` |
| DarkCard | `components/DarkCard.tsx` | `.dark-card` |
| DataPanel | `components/DataPanel.tsx` | `.data-panel`, `.panel` |
| SectionTitle | `components/SectionTitle.tsx` | `.ui-section-title` |
| Button | `components/Button.tsx` | `.ui-btn`, `.ui-btn-primary` |
| InputBox | `components/InputBox.tsx` | `.ui-input`, `.ui-textarea` |
| StatusBadge | `components/StatusBadge.tsx` | `.status-badge` |

Do not duplicate these classes. Existing `ConvergencePanel` and `RESMintPanel` keep their markup and inherit `.panel` tokens.

---

## Page layout rules

```
status bar
navbar (RIGOR · Home · Download · Wallet · Mining · Explorer · Documentation · Zero-Trust · About)
lab nav (Protocol · Science · Lab · Status · PROOF-034)
notices
main (max 1120px)
footer
```

| Page | Layout |
|------|--------|
| `/` | Hero (RIGOR AlphaPlus) → identity cards (PoW, ORU, RES, Convergence, Zero Trust) → journey → three-layer model |
| `/download` | Four dark cards with Download buttons |
| `/wallet` | ORU / Maturity / Convergence / RES cards, then existing ConvergencePanel and RESMintPanel |
| `/mining` | Overview + miner download, pool, economics, Windows guide |
| `/explorer` | Block data, ORU stats, RES stats, Consensus Fingerprint |
| `/zero-trust-verification` | INPUT (TXID) → VERIFY → RESULT |
| `/documentation` | Architecture, RFC, Security, Developer Guide |

All previously registered routes remain.

---

## Motion

Allowed: 150ms fade-in on `.page`, hover border on cards/buttons, loading label on Verify.  
Forbidden: gradients as decoration, particle effects, bounce, crypto slot animations.  
`prefers-reduced-motion: reduce` disables transitions.

---

## Responsive breakpoints

| Target | CSS |
|--------|-----|
| Desktop 1920 | default, max-width 1120px centered |
| Laptop 1366 | `--max-width: 1000px` |
| Mobile 390 | hamburger nav, stacked cards, single-column kv grid |

---

## Voice

RIGOR = mathematical trust + transparency + verification. No marketing hype. Failures show `—` or explicit error banners.

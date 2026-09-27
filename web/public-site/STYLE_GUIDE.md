# RIGOR Public Site — Style Guide

**Task:** ``
**Document:** Visual identity and UI design rules  
**Parent:** [SITE_ARCHITECTURE.md](./SITE_ARCHITECTURE.md)  
**Narrative alignment:** `RELEASE_LANGUAGE_STYLE_GUIDE.md`

| Field | Value |
|-------|-------|
| `equivalence_claimed` | **false** (badge styling below) |

---

## 1. Visual identity

### 1.1 Aesthetic

| Attribute | Target |
|-----------|--------|
| **Tone** | Scientific / laboratory — not marketing |
| **Feel** | Deterministic, grid-aligned, calm |
| **Density** | Information-forward; generous whitespace for equations |
| **Motion** | No animation bias; no celebratory effects |

**Forbidden:** gradient hero hype, "moon" imagery, investment CTAs, particle backgrounds, slot-machine or random UI flourishes.

### 1.2 Tagline (header)

```text
RIGOR — Deterministic dual-asset protocol (ORU · RES)
```

Subline: `Runtime-verifiable state machine · equivalence_claimed = false`

---

## 2. Color system

CSS custom properties (implementation):

```css
:root {
  /* Status — semantic only, always pair with text label */
  --color-success: #2d6a4f;   /* ONLINE, deterministic OK */
  --color-error: #9b2226;     /* OFFLINE, fault */
  --color-degraded: #b08900;  /* DEGRADED, partial */
  --color-offline: #6c757d;   /* Static-only / no live */

  /* Neutrals — laboratory */
  --color-bg: #f8f9fa;
  --color-surface: #ffffff;
  --color-border: #dee2e6;
  --color-text: #212529;
  --color-text-muted: #6c757d;

  /* Tags */
  --tag-formal: #495057;
  --tag-runtime: #1864ab;
  --tag-observe: #5f3dc4;

  /* Invariant badge */
  --badge-equiv: #343a40;
}
```

| Token | Use |
|-------|-----|
| **SUCCESS (green)** | RPC OK, field verified, `ONLINE` |
| **ERROR (red)** | Hard failure, `equivalence_claimed === true` fault |
| **DEGRADED (amber)** | Partial data, stale fetch, soft-fail |
| **OFFLINE (gray)** | No live binding; static-only mode |

**Accessibility:** Minimum 4.5:1 contrast for body text; status never indicated by color alone.

---

## 3. Typography

| Role | Font stack | Usage |
|------|------------|-------|
| **Sans (prose)** | `Inter, system-ui, sans-serif` | Narrative, lab text, nav |
| **Mono (protocol)** | `IBM Plex Mono, ui-monospace, monospace` | Heights, hashes, RPC JSON snippets |
| **Math (axioms)** | `KaTeX_Main, Times New Roman, serif` | Equations in Axiom Viewer |

### Scale

| Element | Size | Weight |
|---------|------|--------|
| `h1` | 2rem | 600 |
| `h2` | 1.5rem | 600 |
| `h3` | 1.25rem | 600 |
| Body | 1rem | 400 |
| `code` / mono values | 0.875rem | 400 |
| Status bar | 0.8125rem | 500 |

**Protocol values** (block hash, height, `res_probability_target`) always **monospace**.

**Explanations** always **sans-serif**.

**Axiom equations** always **math font** via KaTeX.

---

## 4. Layout and grid

| Rule | Value |
|------|-------|
| Max content width | `1200px` |
| Grid | 12 columns, `24px` gutter |
| Section spacing | `48px` vertical between major blocks |
| Card padding | `24px` |
| Border radius | `4px` (minimal — not playful) |

**Strict grid alignment:** Status bar, nav, and footer edges align to the same grid. No full-bleed marketing sections.

---

## 5. UI components — visual rules

### 5.1 Equivalence badge

```text
┌─────────────────────────────┐
│ equivalence_claimed: false  │  /* default: --badge-equiv on --color-surface */
└─────────────────────────────┘
```

If live API reports `true`: invert to `--color-error` background, white text, label `FAULT: equivalence_claimed must be false`.

### 5.2 Read-only banner

- Background: `#e7f5ff` (informational blue-gray)
- Border-left: `4px solid #1864ab`
- Text: "Read-only observer. rigord RPC is consensus authority. No signing or transaction submission."

### 5.3 Source label

Every live value:

```text
381443  [rpc · 2026-06-13T12:00:00Z]
```

Muted `0.75rem` mono timestamp suffix.

### 5.4 Invariant panels (axioms)

- White surface, `1px` border `--color-border`
- Equation centered in panel
- No icons that imply "verified" checkmarks without harness scope

### 5.5 Explorer tables

- Zebra optional; prefer simple row borders
- Missing field: em dash `—` in amber cell background `#fff8e1`
- Error row: message in red, no synthetic replacement rows

---

## 6. UI principles

| Principle | Implementation |
|-------------|----------------|
| **No animation bias** | Transitions ≤ 150ms opacity only; no bounce |
| **No probabilistic UI** | No random loaders, shuffle effects, or "luck" metaphors |
| **Every value traceable** | `SourceLabel` on all live fields |
| **No hidden state** | Expandable JSON for raw RPC response (advanced toggle) |
| **Deterministic layout** | Same route → same structure; loading skeletons without fake numbers |
| **No marketing language** | Copy from publication corpus only |

---

## 7. Page-specific styling

| Route | Distinct treatment |
|-------|-------------------|
| `/` | Hero: left-aligned text only; three equal cards |
| `/protocol` | Tree nav + mono side notes for equations |
| `/axioms` | Invariant grid 2×3; math-heavy |
| `/lab` | Layer diagram monospace; experiment tags as chips |
| `/explorer` | Tabbed interface; mono-dense tables |
| `/verification` | Checklist aesthetic; static milestone table |
| `/system-status` | Dashboard grid of probe cards |

---

## 8. Icons and imagery

| Allowed | Forbidden |
|---------|-----------|
| Simple line icons (refresh, external link) | Coin logos as investment signals |
| Layer diagram ASCII/SVG | Stock photos |
| Optional RIGOR wordmark (text) | Animated backgrounds |

---

## 9. Footer (global)

```text
RIGOR Protocol v1.0 publication · Frozen baseline mainnet-freeze-rfc015-2026-05-26
Experimental verification (PROOF-034) · equivalence_claimed = false
Closed testnet: GO · Public testnet: CONDITIONAL GO · Mainnet: NO-GO
Not an investment offering · Native assets: ORU and RES
```

`0.8125rem`, `--color-text-muted`, centered or left per grid.

---

## 10. Dark mode

**v1.0:** Light mode only (laboratory print aesthetic). Dark mode is **out of scope** for .

---

## 11. Acceptance criteria

| Field | Status |
|-------|--------|
| Visual system spec | **COMPLETE** |
| Color semantics | **COMPLETE** |
| Typography roles | **COMPLETE** |
| No marketing / probabilistic UI | **COMPLETE** |
| `equivalence_claimed` badge | **COMPLETE** |
| Freeze compliance | **COMPLETE** |

---

## 12. References

- [SITE_ARCHITECTURE.md](./SITE_ARCHITECTURE.md)
- [UI_LAYOUT_SPEC.md](./UI_LAYOUT_SPEC.md)
- [ROUTE_AND_DATA_BINDING_SPEC.md](./ROUTE_AND_DATA_BINDING_SPEC.md)
- [RIGOR_UNIFIED_WHITEPAPER_v1.0.md](../../docs/WHITEPAPER.md)

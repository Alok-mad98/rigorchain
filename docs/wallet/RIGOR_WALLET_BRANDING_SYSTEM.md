# RIGOR Wallet Branding System

**Task:** `TASK-WALLET-PROGRAM-002`  
**Version:** 1.0  
**Date:** 2026-06-23  
**Type:** Read-only branding specification — **no implementation**

| Field | Value |
|-------|--------|
| Implementation Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

**Reference:** `core/doc/branding-and-terminology-lockdown.md`, [WALLET_BRANDING_GAP_ANALYSIS.md](./WALLET_BRANDING_GAP_ANALYSIS.md)

---

## 1. Product naming

| Surface | Official name | Internal binary | Notes |
|---------|---------------|-----------------|-------|
| Signing desktop app | **RIGOR Wallet** | `rigor-qt` | Public testnet name |
| Full node daemon | **RIGOR Core** (node) | `rigord` | Referenced in docs, not primary UI title |
| CLI tool | **RIGOR CLI** | `rigor-cli` | Advanced users |
| Read-only web app | **RIGOR Observer** | npm package internal | Never "RIGOR Wallet MVP" in release |
| Optional future rename | Shire Wallet | — | **Deferred** — requires marketing decision |

**Forbidden public names:** Bitcoin Core, Arda, bitcoin-qt, combined "Rigor Bitcoin Wallet"

---

## 2. Logo placement

### 2.1 Primary logo

| Context | Placement | Size |
|---------|-----------|------|
| App window title bar | Icon 16×16 / 32×32 | Favicon equivalent |
| Sidebar header (Wallet) | Logo + wordmark horizontal | 120×32 px max |
| Observer top nav | Logo left, product name right | 32 px height |
| Installer welcome | Centered logo + product name | 256 px wide |
| About screen | Logo + version | 64 px |
| Desktop shortcut | App icon only | OS standard |

### 2.2 Logo variants

| Variant | Use |
|---------|-----|
| **Full lockup** | Marketing, installer, About |
| **Icon only** | Taskbar, dock, favicon |
| **Monochrome** | Splash screen low-color boot |

**Asset deliverable (implementation phase):** `brand/rigor-wallet-logo.svg`, `brand/rigor-wallet-icon-1024.png`

---

## 3. Color system

### 3.1 Brand palette (target)

| Token | Hex | Usage |
|-------|-----|-------|
| `--rigor-bg-primary` | `#0F1419` | Dark mode background (default) |
| `--rigor-bg-surface` | `#1A2332` | Cards, panels |
| `--rigor-text-primary` | `#E8EDF4` | Body text |
| `--rigor-text-muted` | `#8B9CB3` | Secondary labels |
| `--rigor-accent` | `#3B82F6` | Links, primary buttons |
| `--rigor-warning` | `#F59E0B` | Lag, stale warnings |
| `--rigor-critical` | `#EF4444` | Root mismatch, errors |
| `--rigor-success` | `#22C55E` | Confirmed, synced |

### 3.2 Asset-specific colors (mandatory separation)

| Asset | Token | Hex | Rule |
|-------|-------|-----|------|
| **ORU** | `--asset-oru` | `#94A3B8` | Cool neutral — mined base asset |
| **RES** | `--asset-res` | `#A78BFA` | Distinct accent — minted asset |

**Hard rule:** ORU and RES panels must use different accent borders/headers. **No gradient blending between them.**

### 3.3 Light mode (optional testnet)

| Token | Hex |
|-------|-----|
| `--rigor-bg-primary` | `#F8FAFC` |
| `--rigor-bg-surface` | `#FFFFFF` |

Dark mode is **default** for rigor-qt (Core heritage); Observer supports both.

---

## 4. Typography

| Role | Font stack | Size |
|------|------------|------|
| **UI body** | `Inter, system-ui, sans-serif` | 14–16 px |
| **Headings** | `Inter, system-ui, semibold` | 18–24 px |
| **Monospace (txid, hash, addresses)** | `JetBrains Mono, ui-monospace, monospace` | 13 px |
| **Numeric balances** | `Inter tabular-nums` | 20–28 px bold |

**Rules:**

- txid/hash always monospace; truncate middle with copy-full action
- Balances use tabular figures for alignment
- No comic or decorative fonts

---

## 5. Icons

### 5.1 Application icons (platform)

| Platform | Format | Sizes |
|----------|--------|-------|
| Windows | `.ico` | 16, 32, 48, 256 |
| macOS | `.icns` | 16–1024 |
| Linux | `.png` + `.xpm` | 16, 32, 48, 256 |

**Generation:** `core/src/qt/res/icons/build_app_icon.py` from `brand/rigor-wallet-icon-1024.png`

### 5.2 In-app icon set

| Icon | Meaning | Style |
|------|---------|-------|
| ORU circle | ORU sections | Neutral stroke + `--asset-oru` |
| RES diamond | RES sections | `--asset-res` fill |
| Convergence arrows | Convergence workflow | Accent |
| Sync spinner | IBD / catch-up | Animated |
| Warning triangle | Degraded state | `--rigor-warning` |
| Shield | Compliance / security | Muted |

Replace Bitcoin heritage SVGs in `core/src/qt/res/src/` with RIGOR set during implementation phase.

### 5.3 Status icons (connection)

| State | Icon |
|-------|------|
| Fully synced | Green dot |
| Syncing | Animated progress |
| Offline | Red dot |
| Warning | Amber dot |

---

## 6. Splash screen

| Attribute | Specification |
|-----------|---------------|
| **Duration** | Max 3 s or until wallet model loaded |
| **Background** | `--rigor-bg-primary` |
| **Center** | RIGOR Wallet logo (full lockup) |
| **Subtitle** | "Dual-native asset wallet for RIGOR" |
| **Footer** | Version string, network badge (Testnet/Mainnet) |
| **Progress** | Thin bar: node startup / wallet load |

**Observer:** No splash — web loading skeleton instead.

---

## 7. Desktop shortcuts

### Windows

| Item | Value |
|------|-------|
| Shortcut name | **RIGOR Wallet** |
| Target | `{InstallDir}\rigor-qt.exe` |
| Icon | `{InstallDir}\share\icons\rigor.ico` |
| Start menu group | **RIGOR** |
| Testnet shortcut | `RIGOR Wallet (Testnet)` with `-testnet` flag |
| Uninstall | Standard Add/Remove Programs entry |

### Linux

| Item | Value |
|------|-------|
| `.desktop` name | `RIGOR Wallet` |
| `Icon=` | `rigor-wallet` |
| Categories | `Finance;Network;` |
| MIME | `x-scheme-handler/rigor` |

### macOS

| Item | Value |
|------|-------|
| App bundle name | `RIGOR Wallet.app` |
| Dock name | RIGOR Wallet |
| CFBundleIdentifier | `com.rigorchain.wallet` (target) |

---

## 8. Installer branding

| Element | Current (gap) | Target |
|---------|---------------|--------|
| InstallDir | `Arda` | `RIGOR Wallet` or `Rigor` |
| Publisher | bitcoincore.org | rigorchain.com |
| Config file | `arda.conf` | `rigor.conf` |
| URL handler | `rigor:` → "Arda" | `rigor:` → "RIGOR" |
| Wizard images | NSIS bitmaps legacy | RIGOR-branded BMP |

---

## 9. Compliance & legal surfaces

| Surface | Required text |
|---------|---------------|
| About | `equivalence_claimed: false` |
| About | Link to disclosure / limitations doc |
| First-run | "RIGOR Wallet is experimental testnet software" (testnet) |
| Observer footer | "Read-only — cannot sign transactions" |

---

## 10. Do / Don't

| Do | Don't |
|----|-------|
| Separate ORU and RES visually | Combined portfolio total |
| Use "convergence" in UI | "burn" as button label |
| Use "Mint" for RES | "issue" / "generate" |
| Show `RES_PROBABILITY_TARGET` | Forbidden target aliases |
| RIGOR wordmark on all surfaces | Bitcoin / Arda remnants |

---

## 11. Output table

| Field | Status |
|-------|--------|
| Branding System | **COMPLETE** |
| Implementation Impact | **NONE** |
| Equivalence Claimed | **false** |

---

*End of RIGOR_WALLET_BRANDING_SYSTEM — TASK-WALLET-PROGRAM-002*

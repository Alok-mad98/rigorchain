# RIGOR Wallet Navigation Specification

**Task:** `TASK-WALLET-PROGRAM-008`  
**Version:** 1.0  
**Date:** 2026-06-23  
**Type:** Build-ready navigation blueprint — **no implementation**

| Field | Value |
|-------|--------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

Defines navigation chrome: toolbar/sidebar, status bar, notifications, dialogs, and hierarchy for RIGOR Wallet.

**Related:** [RIGOR_WALLET_INFORMATION_ARCHITECTURE.md](./RIGOR_WALLET_INFORMATION_ARCHITECTURE.md) · [RIGOR_WALLET_NAVIGATION_COMPARISON.md](./RIGOR_WALLET_NAVIGATION_COMPARISON.md)

---

## 1. Navigation model

### 1.1 Target (blueprint)

**Primary:** Left sidebar + top header strip (post–Phase D).

### 1.2 Phase A (shipping interim)

**Primary:** Top **toolbar tabs** — matches current `RigorGUI` / `WalletView`:

| Tab | Shortcut | Stack widget |
|-----|----------|--------------|
| Overview (Dashboard) | Alt+1 | `OverviewPage` |
| Send | Alt+2 | `SendCoinsDialog` |
| Receive | Alt+3 | `ReceiveCoinsDialog` |
| History | Alt+4 | `TransactionView` |
| Convergence | Alt+5 | `ConvergencePage` |

**Asset context (ORU/RES):** Sub-strip inside Send/Receive/Overview — not separate top-level tabs until Phase B+.

### 1.3 Full hierarchy (target)

```text
RIGOR Wallet
├── Dashboard                    [Alt+1]
├── Wallet
│   ├── Overview
│   ├── Backup & Recovery
│   └── Security
├── ORU (context / future tab)
│   ├── Overview · Receive · Send · Convergence outputs · History
├── RES (context / future tab)
│   ├── Overview · Receive · Send · Mint history · Mint detail
├── Convergence                  [Alt+5]
├── History                      [Alt+4]
├── Explorer                     [Alt+6]
├── Diagnostics                  [Alt+7]
├── Settings                     [menu]
└── About                        [Help menu]
```

---

## 2. Sidebar (target layout)

```
┌──────────────────┐
│ [Logo] RIGOR     │  ← compact mark + wordmark; collapses to mark only
│      Wallet      │
├──────────────────┤
│ ● Dashboard      │
│ ○ Wallet         │
│ ○ ORU            │
│ ○ RES            │
│ ○ Convergence    │
│ ○ History        │
│ ○ Explorer       │
│ ○ Diagnostics    │
├──────────────────┤
│ ○ Settings       │
│ ○ About          │
└──────────────────┘
```

| Behavior | Rule |
|----------|------|
| Active item | `--rigor-accent` left border 3 px |
| Collapsed width | 48 px — icons only + tooltip |
| Expanded width | 200 px |
| Sub-nav | ORU/RES expand inline (accordion) — second click navigates |
| Watch-only wallet | Send + Convergence Send hidden |

**Qt mapping (future):** Extend `bitcoingui.cpp` + `walletview.cpp` stack indices.

---

## 3. Toolbar (current + menus)

### 3.1 Main window toolbar

| Control | Action | Qt class |
|---------|--------|----------|
| Tab group | Switch stack page | `QActionGroup` in `RigorGUI` |
| — | — | `overviewAction`, `sendCoinsAction`, etc. |

### 3.2 Menu bar

| Menu | Items |
|------|-------|
| **File** | Open wallet, Close wallet, Backup wallet, Create wallet, Quit |
| **Settings** | Encrypt wallet, Change passphrase, Options… |
| **Window** | Minimize, Console (RPC) |
| **Help** | About, Debug log |

### 3.3 Context toolbar (Send/Receive)

| Control | Purpose |
|---------|---------|
| Asset context chip | "Sending: ORU" / "Sending: RES" |
| Coin control | Opens `CoinControlDialog` |
| Address book | Opens `AddressBookPage` |

---

## 4. Status bar

**Location:** Bottom of `RigorGUI` main window.

| Zone | Content | Source |
|------|---------|--------|
| Left | Connection icon + peer count | `ClientModel` |
| Center-left | Sync progress / block height | `ClientModel` |
| Center | Testnet badge (if testnet) | Chain params |
| Center-right | Explorer lag chip (Phase D) | `ExplorerClient` |
| Right | Encryption lock icon | `WalletModel` |
| Right | Warning icon (if warnings active) | Warning stack |

| State | Display |
|-------|---------|
| Synced | Green dot + "Up to date" + height |
| Syncing | Orange progress + "Syncing block X…" |
| Offline | Red dot + "No connection" |
| Warning active | Amber icon — click opens Diagnostics |

**Interaction:** Click sync area → `ModalOverlay`; click network → Options Network tab.

---

## 5. Notifications

### 5.1 In-app notification types

| Type | Channel | Example |
|------|---------|---------|
| **Toast** | Non-modal bottom-right | "Address copied" |
| **Banner** | `WarningBanner` top of content | Explorer lag |
| **Tray balloon** | System tray (if enabled) | Incoming ORU tx |
| **Modal** | Blocking | Send confirm, critical mismatch |

### 5.2 Incoming transaction notification

| Field | Value |
|-------|--------|
| Trigger | `TransactionTableModel::rowsInserted` |
| Tray | "Incoming ORU/RES: amount from address" |
| Click | Focus History + tx row |
| Privacy mode | Hide amounts if privacy enabled |

### 5.3 Notification rules

- No promotional notifications
- No price alerts
- RES mint notification optional (Settings toggle): "RES mint confirmed — view details"
- Critical mismatch: modal once per session until acknowledged

---

## 6. Dialogs

| Dialog | Modal | Trigger | Dismiss |
|--------|-------|---------|---------|
| Send confirm | Yes | Send/Convergence confirm | Confirm / Cancel |
| Unlock wallet | Yes | Encrypted action | OK / Cancel |
| Backup reminder | Yes | First spend, 30-day interval | Backup now / Later |
| Options | Yes | Settings menu | OK / Cancel / Apply |
| About | Yes | Help | Close |
| QR fullscreen | Yes | Receive QR click | Close |
| Root mismatch ack | Yes | Critical detection | Acknowledge |
| Coin control | Yes | Send inputs | OK / Cancel |
| Address book | Yes | Send/Receive | Close |
| Create wallet | Yes | File → Create | Wizard flow |
| Intro / first-run | Yes | First launch | Wizard flow |

**Stacking:** Max one modal deep; toasts suppressed while modal open.

---

## 7. Breadcrumb / location (optional Phase D)

When sidebar + sub-nav active:

```text
RES › Mint history › Detail (txid…abc)
```

Click breadcrumb segment navigates up. Not required for Phase A toolbar.

---

## 8. Keyboard shortcuts

| Shortcut | Action |
|----------|--------|
| Alt+1 | Dashboard |
| Alt+2 | Send (ORU default) |
| Alt+3 | Receive |
| Alt+4 | History |
| Alt+5 | Convergence |
| Alt+6 | Explorer (Phase D) |
| Alt+7 | Diagnostics (Phase F) |
| Ctrl+Shift+D | RPC Console |
| Ctrl+Shift+C | Console → Console tab |
| Ctrl+Q | Quit |

---

## 9. Multi-wallet navigation

| Behavior | Rule |
|----------|------|
| Tabs | One `WalletView` per loaded wallet (`WalletFrame`) |
| Asset context | Resets to default send asset on wallet switch |
| Status bar | Shows active wallet name when multi-wallet |
| Nav stack | Independent per wallet tab |

---

## 10. Observer navigation (parallel product)

Top horizontal tabs (web):

Dashboard · Watch · ORU · RES · Convergence · History · Explorer · About

No Send, Convergence Send, Encrypt, or Backup in nav.

---

## 11. Navigation × journey map

| Journey | Entry nav |
|---------|-----------|
| A Receive ORU | Dashboard → Receive (ORU) |
| B Send ORU | Send (ORU context) |
| C Convergence | Convergence tab |
| D RES Mint | RES → Mint history |
| E Backup | File menu or Wallet |
| F Diagnostics | Window → Console or Diagnostics tab |

---

## 12. Output table

| Field | Status |
|-------|--------|
| Navigation Spec | **COMPLETE** |
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Equivalence Claimed | **false** |

---

*End of RIGOR_WALLET_NAVIGATION_SPEC — TASK-WALLET-PROGRAM-008*

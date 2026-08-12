# RIGOR Wallet Navigation Comparison

**Task:** `TASK-WALLET-PROGRAM-003`  
**Version:** 1.0  
**Date:** 2026-06-23  
**Type:** Read-only navigation study — **no implementation**

| Field | Value |
|-------|--------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

Compares **Bitcoin Core v25 Qt** primary navigation with **RIGOR blueprint** navigation and documents **current `rigor-qt` state**.

---

## 1. Bitcoin Core v25 — primary navigation

### 1.1 Toolbar (wallet frame)

| Order | Tab | Shortcut | Widget |
|-------|-----|----------|--------|
| 1 | Overview | Alt+1 | `OverviewPage` |
| 2 | Send | Alt+2 | `SendCoinsDialog` (embedded) |
| 3 | Receive | Alt+3 | `ReceiveCoinsDialog` (embedded) |
| 4 | Transactions | Alt+4 | `TransactionView` |

**Source:** Bitcoin Core v25 `bitcoingui.cpp` / `walletview.cpp` — four stack pages only.

### 1.2 Main window menus (node + wallet shell)

| Menu | Key items |
|------|-----------|
| **File** | Open wallet, Close wallet, Backup wallet, Create wallet, Quit |
| **Settings** | Encrypt wallet, Change passphrase, Options… |
| **Window** | Minimize, Show / hide, **Console** (RPC debug window) |
| **Help** | About, Debug log |

### 1.3 Modal / overlay surfaces (not toolbar tabs)

| Surface | Access |
|---------|--------|
| RPC Console | Window → Console; Ctrl+Shift+D |
| Options | Settings → Options |
| Address book | Send/Receive dialogs; wallet menu |
| Coin control | Send dialog → Inputs button |
| Encrypt / unlock | Settings menu; send-time prompts |
| Network / peers | Inside RPC Console → Network / Peers tabs |
| PSBT operations | File / wallet menus (version-dependent) |

### 1.4 Navigation diagram — Bitcoin v25

```
┌─────────────────────────────────────────────────────────────┐
│  RigorGUI / BitcoinGUI (main window)                        │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ WalletFrame → WalletView                              │  │
│  │  [Overview] [Send] [Receive] [Transactions]           │  │
│  └───────────────────────────────────────────────────────┘  │
│  Menus: File · Settings · Window(Console) · Help            │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. RIGOR blueprint — proposed navigation

**Source:** [RIGOR_WALLET_SCREEN_MAP.md](./RIGOR_WALLET_SCREEN_MAP.md), [RIGOR_WALLET_INFORMATION_ARCHITECTURE.md](./RIGOR_WALLET_INFORMATION_ARCHITECTURE.md)

### 2.1 Primary toolbar (signing wallet)

| Order | Tab | Maps from Bitcoin | Notes |
|-------|-----|-------------------|-------|
| 1 | **Dashboard** | Overview | + supply, sync, warnings |
| 2 | **Wallet** | File menu + wallet summary | Descriptor, backup status |
| 3 | **ORU** | Overview + Send + Receive (ORU context) | Sub-nav or section |
| 4 | **RES** | Overview + Send + Receive (RES context) | Sub-nav or section |
| 5 | **Convergence** | *(new)* | Signing convergence flow |
| 6 | **History** | Transactions | Asset filters |
| 7 | **Explorer** | *(new)* | Read-only chain view |
| 8 | **Diagnostics** | Console subset | Operator tools |
| 9 | **About** | Help → About | Protocol + compliance |

### 2.2 Observer product (separate app)

| Order | Tab |
|-------|-----|
| 1 | Dashboard |
| 2 | Watch Registry |
| 3 | ORU |
| 4 | RES |
| 5 | Convergence (read-only) |
| 6 | History |
| 7 | Explorer |
| 8 | About |

### 2.3 Navigation diagram — RIGOR blueprint

```
┌──────────────────────────────────────────────────────────────────────────┐
│  RIGOR Wallet (rigor-qt)                                                 │
│  [Dashboard][Wallet][ORU][RES][Convergence][History][Explorer][Diag][About]│
│       │        │      │    │       │          │         │        │       │
│       ▼        ▼      ▼    ▼       ▼          ▼         ▼        ▼       │
│   Overview  mgmt   send  send   Convergence  TxView  Explorer  RPC+   About│
│   +supply   panel  recv  recv   Page         +filter  Client   diag        │
└──────────────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────────────┐
│  RIGOR Observer (wallet-observer-mvp) — NO Send / Encrypt                │
│  [Dashboard][Watch][ORU][RES][Convergence][History][Explorer][About]       │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Current rigor-qt — actual navigation (2026-06)

**Source:** `core/src/qt/bitcoingui.cpp`, `core/src/qt/walletview.cpp`

### 3.1 Toolbar

| Order | Tab | Shortcut | vs Bitcoin v25 |
|-------|-----|----------|----------------|
| 1 | Overview | Alt+1 | MODIFY (dual asset) |
| 2 | Send | Alt+2 | MODIFY (asset combo) |
| 3 | Receive | Alt+3 | KEEP |
| 4 | Transactions | Alt+4 | MODIFY (asset roles) |
| 5 | **Convergence** | Alt+5 | **ADD** |

### 3.2 Menus

Same structure as Bitcoin v25 (`RigorGUI` rename only). Console, Options, Encrypt, Backup unchanged in placement.

### 3.3 Gap vs blueprint

| Blueprint tab | Current rigor-qt |
|---------------|------------------|
| Dashboard | ≈ Overview (incomplete supply cards) |
| Wallet | ≈ File menu only (no summary page) |
| ORU | ≈ combined Overview + Send combo |
| RES | ≈ combined Overview + Send combo |
| Convergence | **Present** |
| History | ≈ Transactions |
| Explorer | **Missing** |
| Diagnostics | ≈ partial via Console only |
| About | Present (minimal protocol text) |

---

## 4. Exact additions (Bitcoin → RIGOR blueprint)

### 4.1 New top-level tabs

| Addition | Inherits from | Implementation strategy |
|----------|---------------|-------------------------|
| **Convergence** | — (greenfield) | **Already added** — `ConvergencePage` |
| **Explorer** | — | New `ExplorerPage` or embedded WebEngine; Phase D |
| **Diagnostics** | RPC Console | Split/peers from console + new wallet diag panel; Phase D |
| **Wallet** (summary) | File menu actions | New lightweight `WalletOverviewPage`; Phase A |
| **About** (enhanced) | Help → About | Extend existing dialog; Phase A |

### 4.2 Logical additions (may be sub-nav, not new tabs)

| Addition | Rationale |
|----------|-----------|
| **ORU section** | Avoid duplicating Send/Receive widgets — context switch on same pages |
| **RES section** | Same — toolbar ORU/RES sets `WalletView` context + default asset |
| **Dashboard supply cards** | Extend Overview, don't replace |

**Recommendation:** Phase A–B use **5-tab Core layout + Convergence** with **contextual ORU/RES** (blueprint sub-nav inside Overview/Send/Receive). Phase C+ optionally **split ORU/RES top-level tabs** if user testing demands it.

### 4.3 Observer-only additions

| Addition | Bitcoin equivalent |
|----------|-------------------|
| Watch Registry | Partially: import watch-only address in Core |
| Read-only Convergence | None |
| Explorer-first Dashboard | None |

---

## 5. Side-by-side comparison table

| Nav item | Bitcoin v25 | rigor-qt (now) | RIGOR blueprint | Inheritance |
|----------|-------------|----------------|-----------------|-------------|
| Overview | ✓ | ✓ (dual asset) | Dashboard | MODIFY |
| Send | ✓ | ✓ (ORU/RES) | ORU Send / RES Send | MODIFY (context) |
| Receive | ✓ | ✓ | ORU Receive / RES Receive | MODIFY (context) |
| Transactions | ✓ | ✓ | History | MODIFY |
| Convergence | — | ✓ | ✓ | ADD (done) |
| Wallet summary | — (menu) | — (menu) | ✓ tab | ADD |
| ORU hub | — | — (implicit) | ✓ tab/section | ADD (nav) |
| RES hub | — | — (implicit) | ✓ tab/section | ADD (nav) |
| Explorer | — | — | ✓ | ADD |
| Diagnostics | — (in console) | — (in console) | ✓ tab | ADD |
| About / protocol | minimal | minimal | ✓ enhanced | EXTEND |
| RPC Console | ✓ menu | ✓ menu | ✓ (Diagnostics superset) | KEEP |
| Coin control | ✓ modal | ✓ modal | ✓ modal | KEEP |
| Address book | ✓ modal | ✓ modal | ✓ modal | KEEP |
| Network / peers | ✓ console | ✓ console | ✓ console + dashboard | KEEP |

---

## 6. Keyboard shortcuts (proposed)

| Shortcut | Bitcoin v25 | RIGOR proposed |
|----------|-------------|----------------|
| Alt+1 | Overview | Dashboard |
| Alt+2 | Send | ORU *(or Send with ORU context)* |
| Alt+3 | Receive | RES *(or Receive)* |
| Alt+4 | Transactions | History |
| Alt+5 | — | Convergence *(already Alt+5 in rigor-qt)* |
| Alt+6 | — | Explorer |
| Alt+7 | — | Diagnostics |
| Ctrl+Shift+D | Debug window | Debug window (KEEP) |

**Compatibility note:** rigor-qt already uses Alt+5 for Convergence. Blueprint reorder may shift Receive/RES — document in release notes if changed.

---

## 7. Navigation migration path (no big-bang rewrite)

```
Phase A: [Overview][Send][Receive][History][Convergence]  ← current + polish
Phase B: Add ORU/RES context strip inside Send/Receive/Overview
Phase C: Optional [ORU][RES] top-level; RES mint subview
Phase D: [Explorer][Diagnostics] tabs; Dashboard rename Overview
Phase E: Branding + installer; Observer parallel nav frozen
```

This preserves Bitcoin muscle memory (first four tabs) until Phase D.

---

## 8. Output table

| Field | Status |
|-------|--------|
| Navigation Comparison | **COMPLETE** |
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Equivalence Claimed | **false** |

---

*End of RIGOR_WALLET_NAVIGATION_COMPARISON — TASK-WALLET-PROGRAM-003*

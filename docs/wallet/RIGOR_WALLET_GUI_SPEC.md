# RIGOR Wallet GUI Specification

**Task:** `TASK-WALLET-PROGRAM-008`  
**Version:** 1.0  
**Date:** 2026-06-23  
**Type:** Build-ready GUI blueprint — **no Qt implementation**

| Field | Value |
|-------|--------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

Defines **every screen** in the target RIGOR Wallet (`rigor-qt`) GUI. Maps blueprint to current Qt classes where they exist.

**Inputs:** [RIGOR_WALLET_SCREEN_MAP.md](./RIGOR_WALLET_SCREEN_MAP.md) · [RIGOR_WALLET_PRODUCT_ARCHITECTURE.md](./RIGOR_WALLET_PRODUCT_ARCHITECTURE.md) · [RIGOR_WALLET_IMPLEMENTATION_MAP.md](./RIGOR_WALLET_IMPLEMENTATION_MAP.md)

**Product scope:** **RIGOR Wallet** (signing). Observer read-only variants noted per screen.

**Implementation note:** Phase A ships with **toolbar nav** (Overview, Send, Receive, History, Convergence); blueprint sidebar is Phase D target. Behavior is identical; chrome differs.

---

## 0. Global GUI rules

| Rule | Enforcement |
|------|-------------|
| ORU and RES **never combined** in one balance hero | Separate `AssetCard` widgets |
| Convergence terminology — not "burn" | Copy lock |
| RES emission — "Mint" not "issue" | Copy lock |
| Authority labels on indexed data | `(node)` vs `(indexed)` suffix |
| No local convergence recomputation | Display RPC/explorer fields only |
| `equivalence_claimed = false` | About + compliance strip |
| Deterministic transparency | Show height, txid, source on every material action |

**Default theme:** Dark (`#0F1419`); tokens per [RIGOR_COLOR_SYSTEM.md](../branding/RIGOR_COLOR_SYSTEM.md).

---

## 1. Dashboard (Overview)

| Attribute | Value |
|-----------|--------|
| **Purpose** | System health and dual-asset summary at launch |
| **Qt target** | `OverviewPage` → evolve to Dashboard |
| **Entry** | App launch default; Alt+1 |
| **Products** | W, O (read-only actions) |

### Layout

```
┌─ WarningBanner (stack, priority) ─────────────────────────────┐
├─ SyncCard ─ NetworkCard ──────────────────────────────────────┤
├─ ORUBalancePanel ─ RESBalancePanel (equal width, side by side)│
├─ SupplyPanel (optional, explorer) ─ ExplorerStatusCard ───────┤
├─ QuickActions (W only) ───────────────────────────────────────┤
└─ RecentTransactions (5 rows) ─────────────────────────────────┘
```

### Components

| Component | Widget ID | Notes |
|-----------|-----------|-------|
| Sync progress | `SyncCard` | Height, %, plain language |
| Network | `NetworkCard` | Chain, peers, testnet badge |
| ORU summary | `ORUBalancePanel` | Link → ORU hub |
| RES summary | `RESBalancePanel` | Link → RES hub |
| Supply | `SupplyPanel` | Indexed ORU/RES totals |
| Explorer lag | `ExplorerStatusCard` | Lag blocks, last update |
| Warnings | `WarningBanner` | Offline, lag, mismatch |
| Recent txs | `TransactionListCompact` | Asset-tagged |
| Quick actions | `QuickActionBar` | Receive ORU, Send ORU, Convergence |

### Data sources

| Component | Primary | Secondary |
|-----------|---------|-----------|
| Balances | `WalletModel` / `getbalances` | — |
| Sync | `ClientModel` / `getblockchaininfo` | — |
| Peers | `ClientModel` / `getnetworkinfo` | — |
| Supply | Explorer `/api/status` | `gettxoutsetinfo`, `getconvergencestatus` |
| Explorer lag | `ExplorerClient` vs node height | — |
| Recent txs | `TransactionTableModel` | — |

### User actions

| Action | Result |
|--------|--------|
| Click ORU card | Navigate ORU Overview |
| Click RES card | Navigate RES Overview |
| Quick: Receive ORU | ORU Receive |
| Quick: Send ORU | Send (ORU context) |
| Quick: Convergence | Convergence Send |
| Click recent tx | History + tx detail |
| Click sync card | Modal overlay / Diagnostics |

### Error states

| State | Display |
|-------|---------|
| Node offline | `WarningBanner` critical; disable send/convergence |
| IBD syncing | Progress + disable spend actions |
| Explorer down | Supply panel hidden or "indexed unavailable" |
| Root mismatch | Critical banner + link Diagnostics |
| Wallet locked | Balances visible; spend actions prompt unlock |

---

## 2. Wallet overview

| Attribute | Value |
|-----------|--------|
| **Purpose** | Wallet identity, backup, encryption status |
| **Qt target** | New `WalletOverviewPage` + File menu (Phase A) |
| **Entry** | Nav "Wallet" or File menu |
| **Products** | W only |

### Layout

```
┌─ WalletHeader (name, type) ───────────────────────────────────┐
├─ EncryptionStatusCard ─ BackupStatusCard ─────────────────────┤
├─ DefaultReceiveCard (ORU address) ────────────────────────────┤
└─ Actions: Backup | Change password | Export descriptors ────┘
```

### Components

`WalletHeader`, `EncryptionStatusCard`, `BackupStatusCard`, `DefaultReceiveCard`, `DescriptorExportPanel`

### Data sources

| Component | Source |
|-----------|--------|
| Name | `WalletModel::getDisplayName()` |
| Encryption | `WalletModel::getEncryptionStatus()` |
| Backup date | Local QSettings + last `backupWallet()` |
| Descriptor | `getwalletinfo`, export RPC |

### User actions

Backup now, encrypt, change passphrase, export descriptors, open data directory

### Error states

Not backed up (warning), watch-only (no encrypt), multi-wallet tab context

---

## 3. ORU hub

### 3.1 ORU Overview

| Attribute | Value |
|-----------|--------|
| **Purpose** | ORU balances and spendable state |
| **Qt target** | Context on `OverviewPage` + ORU filter |
| **Products** | W, O |

**Layout:** `ORUBalancePanel` (confirmed / unconfirmed / immature) + spendable breakdown + link to History filter ORU.

**Data:** `getbalances.orum`, `listunspent` (asset=ORU).

**Actions:** Receive, Send, View convergence outputs.

**Errors:** Zero balance (empty state, not error); sync incomplete (banner).

### 3.2 ORU Receive

| Attribute | Value |
|-----------|--------|
| **Qt target** | `ReceiveCoinsDialog` (ORU context) |
| **Products** | W; O watch addresses |

**Layout:** QR + address list + payment request history + witness v0 copy.

**Data:** `getnewaddress`, `AddressTableModel`, recent requests.

**Actions:** New address, copy, QR fullscreen, optional amount/label request.

**Errors:** Wallet locked (new address may require unlock); HD disabled.

### 3.3 ORU Send

| Attribute | Value |
|-----------|--------|
| **Qt target** | `SendCoinsDialog` (`comboSendAsset=ORU`) |
| **Products** | W only |

**Layout:** Recipient rows, fee panel (ORU), coin control button, confirm.

**Data:** `prepareTransaction`, `sendtoaddress`, `listunspent`, fee estimation.

**Actions:** Add recipient, coin control, send, PSBT (advanced menu).

**Errors:** Insufficient ORU, invalid address, mixed asset, absurd fee, locked wallet — see [RIGOR_WALLET_ERROR_HANDLING_SPEC.md](./RIGOR_WALLET_ERROR_HANDLING_SPEC.md).

### 3.4 ORU Convergence outputs

**Layout:** Filtered tx list — type=convergence, asset=ORU.

**Data:** `TransactionTableModel` + convergence metadata in `TransactionDesc`.

**Products:** W, O (read-only).

### 3.5 ORU History

**Layout:** `TransactionView` with ORU filter preset.

---

## 4. RES hub

### 4.1 RES Overview

**Layout:** `RESBalancePanel` — confirmed, immature, pending (distinct labels per [WALLET_RES_DISPLAY_RULES.md](./WALLET_RES_DISPLAY_RULES.md)).

**Data:** `getbalances.res`.

**Never show ORU on this screen's primary balance.**

### 4.2 RES Receive

**Qt target:** `ReceiveCoinsDialog` (RES context).

**Copy:** Bech32m / witness v1 guidance for RES program addresses.

### 4.3 RES Send

**Qt target:** `SendCoinsDialog` (`comboSendAsset=RES`); fee always ORU.

**Data:** `sendres` path via `WalletModel`.

### 4.4 RES Mint history

**Layout:** `MintRecordTable` — columns per display rules.

**Data:** Explorer `/api/res/mints` primary; `listconvergencehistory` fallback.

**Mandatory columns:** mint txid, bind txid, bind height, convergence root, convergence txid, RES amount.

### 4.5 RES Mint detail

**Layout:** `MintRecordCard` full + `ExplorerLink` + tx viewer embed.

**Products:** W, O.

---

## 5. Convergence

### 5.1 Convergence Overview

| Attribute | Value |
|-----------|--------|
| **Qt target** | `ConvergencePage` (upper section) |
| **Products** | W, O |

**Layout:** Explainer text + `ConvergenceCard` (tip/prior draws) + `VerificationStatus` for `RES_PROBABILITY_TARGET`.

**Data:** `getconvergenceresult`, `getconvergencestatus`, `WalletModel::tryGetConvergenceBlockResult`.

**Errors:** RPC unavailable (show stale last-good); zero hash winner ("No winner").

### 5.2 Convergence Activity

**Layout:** Event feed table — height, winner, |F|, root.

**Data:** Explorer events + RPC; rigord wins on conflict.

### 5.3 Convergence Send

| Attribute | Value |
|-----------|--------|
| **Qt target** | `ConvergencePage` (send section) |
| **Products** | W only |

**Layout:** ORU amount field, fee estimate, confirm, help link.

**Data:** `WalletModel::sendConvergenceOru` → `createconvergence`.

**Actions:** Converge ORU, unlock if encrypted.

**Errors:** Insufficient ORU, sync incomplete, RPC failure, locked wallet.

### 5.4 Convergence Event detail / Inspector

**Layout:** Read-only round detail — lineage optional (explorer).

**Products:** W, O. No signing.

---

## 6. History (global)

| Attribute | Value |
|-----------|--------|
| **Qt target** | `TransactionView` |
| **Entry** | Alt+4 |

**Layout:** Filter bar (asset, type, date) + table + export.

**Columns:** date, type, asset, amount, confirmations, txid.

**Data:** `TransactionTableModel`, `TransactionFilterProxy`.

**Actions:** Export CSV, open tx detail, copy txid.

**Errors:** Empty wallet (empty state); rescan in progress (banner).

---

## 7. Transaction Viewer

**Entry:** History row, search, mint row, convergence event.

**Layout:** Header (txid/wtxid) + status + IO tables + witness summary + convergence block + `ExplorerLink`.

**Data:** `gettransaction`, wallet desc, explorer `/api/tx/{txid}` enrich.

**Errors:** Tx not found, pruned node (limited detail).

---

## 8. Explorer

| Attribute | Value |
|-----------|--------|
| **Qt target** | New `ExplorerPage` + `ExplorerClient` |
| **Phase** | D |

**Sub-screens:** Search home, address, block, RES mint browser, convergence browser.

**Data:** Explorer REST only — display adapter.

**Actions:** Search, open external browser, copy hash.

**Errors:** Explorer offline (graceful degrade); lag warning.

---

## 9. Settings

| Attribute | Value |
|-----------|--------|
| **Qt target** | `OptionsDialog` |

**Sections:** Network, Display, Wallet, Notifications, Explorer URL (Phase D), Advanced.

**Actions:** Save options, open datadir, reset warnings.

---

## 10. Diagnostics

| Attribute | Value |
|-----------|--------|
| **Qt target** | New `DiagnosticsPage` + `RPCConsole` |
| **Entry** | Nav or Window → Console |

**Panels:** Node health, wallet health, explorer health, peers, export bundle.

**Data:** `getblockchaininfo`, `getnetworkinfo`, `getwalletinfo`, ExplorerClient.

See Journey F in [RIGOR_WALLET_USER_JOURNEYS.md](./RIGOR_WALLET_USER_JOURNEYS.md).

---

## 11. About

| Attribute | Value |
|-----------|--------|
| **Qt target** | `UtilityDialog` |

**Content:** Version, network, `equivalence_claimed: false`, RFC links, licenses.

---

## 12. First-run wizard

| Attribute | Value |
|-----------|--------|
| **Qt target** | `Intro`, `CreateWalletDialog` |

**Steps:** Welcome → Create/Restore/Watch → Seed → Confirm → Encrypt opt → Datadir → Dashboard.

**Products:** W only.

---

## 13. Modal screens

| Modal | Trigger | Components |
|-------|---------|------------|
| Send confirm | Send / Convergence | Summary, fee, password |
| Unlock wallet | Encrypted action | `AskPassphraseDialog` |
| Backup reminder | First spend / periodic | `BackupStatusCard` CTA |
| QR fullscreen | Receive | `ReceiveRequestDialog` |
| Sync overlay | IBD | `ModalOverlay` |
| Root mismatch ack | Critical banner | Acknowledge + Diagnostics link |
| Coin control | Send → Inputs | `CoinControlDialog` |
| Address book | Send/Receive | `AddressBookPage` |

---

## 14. Screen × Qt file map (implementation reference)

| Blueprint screen | Current Qt | Target action |
|------------------|------------|---------------|
| Dashboard | `overviewpage.cpp` | MODIFY |
| ORU Send | `sendcoinsdialog.cpp` | MODIFY context |
| ORU Receive | `receivecoinsdialog.cpp` | MODIFY context |
| History | `transactionview.cpp` | EXTEND filters |
| Convergence | `convergencepage.cpp` | EXTEND |
| Wallet overview | — | ADD |
| Explorer | — | ADD |
| Diagnostics | `rpcconsole.cpp` | EXTEND |
| Settings | `optionsdialog.cpp` | EXTEND |

---

## 15. Output table

| Field | Status |
|-------|--------|
| GUI Specification | **COMPLETE** |
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Equivalence Claimed | **false** |

---

*End of RIGOR_WALLET_GUI_SPEC — TASK-WALLET-PROGRAM-008*

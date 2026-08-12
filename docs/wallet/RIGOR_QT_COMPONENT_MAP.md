# RIGOR Qt Component Map

**Task:** `TASK-WALLET-PROGRAM-004`  
**Version:** 1.0  
**Date:** 2026-06-23  
**Type:** Read-only Qt screen map — **no implementation**

| Field | Value |
|-------|--------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

Per-screen mapping: Qt files, dependencies, data sources, and estimated implementation effort.

**Effort scale:** S = ≤2 days · M = 3–5 days · L = 1–2 weeks · XL = 2+ weeks (includes integration/test)

---

## 1. Screen index

| Screen | Blueprint Name | rigor-qt Today | Phase |
|--------|------------------|----------------|-------|
| Overview | Dashboard | `OverviewPage` (partial) | A–E |
| Wallet summary | Wallet | File menu only | A |
| ORU hub | ORU | Embedded in Overview/Send | B |
| RES hub | RES | Embedded in Overview/Send | B–C |
| Convergence | Convergence | `ConvergencePage` | C–D |
| History | History | `TransactionView` | B |
| Explorer | Explorer | **Missing** | E |
| Diagnostics | Diagnostics | RPC Console partial | F |
| About | About | `UtilityDialog` | A |
| Send (modal tab) | ORU/RES Send | `SendCoinsDialog` | A–B |
| Receive (modal tab) | ORU/RES Receive | `ReceiveCoinsDialog` | A–B |
| Address book | — | `AddressBookPage` | KEEP |
| Coin control | — | `CoinControlDialog` | KEEP |
| Settings | — | `OptionsDialog` | A, E |
| RPC console | — | `RPCConsole` | KEEP |
| Network/peers | — | `RPCConsole` tabs | KEEP |

---

## 2. Primary screens

### 2.1 Overview / Dashboard

| Attribute | Value |
|-----------|--------|
| **Qt class** | `OverviewPage` |
| **Files** | `overviewpage.cpp/h`, `forms/overviewpage.ui`, `transactionoverviewwidget.cpp` |
| **Stack index** | `WalletView` widget 0 |
| **Nav shortcut** | Alt+1 (`overviewAction`) |
| **Action** | **MODIFY** → Dashboard |
| **Effort** | **M** (supply/explorer cards) + **S** (validation of existing dual balance) |

**Dependencies:**

```
OverviewPage
  ├── WalletModel (balances, recent txs)
  ├── ClientModel (sync height, peers)          [optional card]
  ├── TransactionTableModel (recent list)
  ├── BitcoinUnits (ORU/Tharn formatting)
  └── ExplorerClient (Phase E)                  [supply panel — optional P1]
```

**Data sources:**

| UI Element | Source | Priority |
|------------|--------|----------|
| ORU available / pending | `WalletModel::getCachedBalance()` → `interfaces::WalletBalances` | rigord |
| RES available / immature | Same | rigord |
| Recent transactions | `TransactionTableModel` (limit 5) | rigord |
| Sync progress | `ClientModel::getVerificationProgress()` | rigord |
| Network ORU supply | Explorer `/api/status` → `economic_snapshot.oru_indexed_utxo_total_display` | explorer (indexed) |
| RES supply | `getconvergencestatus.res_total` or explorer | rigord preferred |
| Explorer lag warning | ExplorerClient vs `ClientModel::getNumBlocks()` | derived |

**Untouched within screen:** Recent tx list widget, privacy toggle, out-of-sync click handler.

---

### 2.2 ORU hub (logical screen)

| Attribute | Value |
|-----------|--------|
| **Qt class** | *No dedicated page* — context over `OverviewPage` + `SendCoinsDialog` + `ReceiveCoinsDialog` |
| **Files** | `overviewpage.cpp`, `sendcoinsdialog.cpp`, `receivecoinsdialog.cpp`, `optionsmodel.cpp` |
| **Action** | **EXTEND** (nav context) |
| **Effort** | **M** |

**Dependencies:**

```
ORU context (AssetType = ASSET_ORU)
  ├── OptionsModel::defaultSendAsset
  ├── SendCoinsDialog::comboSendAsset (lock to ORU)
  ├── ReceiveCoinsDialog (witness v0 copy)
  └── TransactionView filter (ORU only)
```

**Data sources:**

| UI Element | Source |
|------------|--------|
| ORU balance | `WalletBalances.orum` fields via `WalletModel` |
| Send | `WalletModel::prepareTransaction` + `sendtoaddress` path |
| Receive address | Descriptor wallet P2WPKH (v0) |
| History filter | `TransactionFilterProxy` + `AssetDomainRole` |

**Optional Phase C+:** Dedicated `OruPage` wrapper — **L** if user testing requires separate tab.

---

### 2.3 RES hub (logical screen)

| Attribute | Value |
|-----------|--------|
| **Qt class** | *No dedicated page* + **`ResMintPanel`** *(new, Phase C)* |
| **Files** | Same as ORU + `resmintpanel.cpp/h` *(new)* |
| **Action** | **EXTEND** + **ADD** mint panel |
| **Effort** | **M** (context) + **L** (mint panel) |

**Dependencies:**

```
RES context
  ├── SendCoinsDialog (comboSendAsset = RES)
  ├── sendres wallet path
  ├── ReceiveCoinsDialog (witness v1 / Bech32m copy)
  ├── ResMintPanel
  │     ├── ExplorerClient → /api/res/mints
  │     └── listconvergencehistory (RPC fallback)
  └── TransactionView (RES + mint type filter)
```

**Data sources:**

| UI Element | Source |
|------------|--------|
| RES balance | `WalletBalances.res` via `WalletModel` |
| Send RES | `interfaces::Wallet` → `sendres` |
| Mint history rows | Explorer `/api/res/mints?limit=20` |
| Latest mint detail | Explorer row fields (`mint_txid`, `mint_amount`, `bind_height`) |
| RES probability (info) | `getconvergenceresult.res_probability_target` on Convergence; optional RES page link |

---

### 2.4 Convergence

| Attribute | Value |
|-----------|--------|
| **Qt class** | `ConvergencePage` |
| **Files** | `convergencepage.cpp/h`, `forms/convergencepage.ui` |
| **Stack index** | `WalletView` widget 4 |
| **Nav shortcut** | Alt+5 |
| **Action** | **EXTEND** |
| **Effort** | **M** (display gaps) + **S** (send path validation — largely done) |

**Dependencies:**

```
ConvergencePage
  ├── WalletModel::sendConvergenceOru
  ├── WalletModel::tryGetConvergenceBlockResult
  ├── ClientModel (tip height, sync gate)
  ├── interfaces::Node::getConvergenceBlockResult
  └── BitcoinAmountField / BitcoinUnits
```

**Data sources:**

| UI Element | Source | Field |
|------------|--------|-------|
| Tip/prior draw summary | `tryGetConvergenceBlockResult(height)` | winner, \|F\|, roots |
| RES total (minted) | Page refresh via node | chain convergence state |
| Converge ORU amount | User input → `sendConvergenceOru` | — |
| RES probability target | `getconvergenceresult` | `res_probability_target` **MISSING in UI** |
| Root mismatch flag | `draw_candidate_root_committed` vs `computed` | **MISSING in UI** |

**Reference spec:** [CONVERGENCE_INSPECTOR.md](../rigor-qt/CONVERGENCE_INSPECTOR.md), [FUTURE_GUI_RPC_SPEC.md](../rigor-qt/FUTURE_GUI_RPC_SPEC.md) §1.2

---

### 2.5 History / Transactions

| Attribute | Value |
|-----------|--------|
| **Qt class** | `TransactionView` (+ container widget in `walletview.cpp`) |
| **Files** | `transactionview.cpp/h`, `transactiontablemodel.cpp/h`, `transactionfilterproxy.cpp/h`, `transactiondesc.cpp`, `transactiondescdialog.cpp`, `transactionrecord.cpp/h` |
| **Stack index** | `WalletView` widget 1 |
| **Nav shortcut** | Alt+4 |
| **Action** | **EXTEND** |
| **Effort** | **M** |

**Dependencies:**

```
TransactionView
  ├── WalletModel
  ├── TransactionTableModel
  ├── TransactionFilterProxy
  ├── TransactionDescDialog
  └── CSV export (csvmodelwriter)
```

**Data sources:**

| UI Element | Source |
|------------|--------|
| Tx list | Wallet tx index (`TransactionRecord`) |
| Asset column / filter | `TransactionRecord::amount_asset`, `AssetDomainRole` |
| Convergence txs | `TransactionRecord` type + `transactiondesc` parsing |
| Export | Model dump |

---

### 2.6 Explorer

| Attribute | Value |
|-----------|--------|
| **Qt class** | `ExplorerPage` *(new)* |
| **Files** | `explorerpage.cpp/h` *(new)*, `explorerclient.cpp/h` *(new)* |
| **Stack index** | New `WalletView` widget (or modal from Dashboard) |
| **Nav shortcut** | Alt+6 *(proposed)* |
| **Action** | **ADD** |
| **Effort** | **XL** (ExplorerClient + page + options wiring) |

**Dependencies:**

```
ExplorerPage
  ├── ExplorerClient (singleton via RigorGUI)
  ├── OptionsModel (base URL, refresh)
  ├── ClientModel (height cross-check)
  └── QWebEngineView OR QListView   [architecture choice: list-first MVP]
```

**Data sources (read-only HTTP):**

| Endpoint | Use |
|----------|-----|
| `/healthz` | Liveness, lag, `critical_root_mismatch` |
| `/api/status` | Supply snapshot, integrity flags |
| `/api/res/mints` | Mint browse |
| `/api/tx/{txid}` | Tx detail drill-down |
| `/api/block/{hash}` | Block detail |

**Authority:** [EXPLORERCLIENT_ARCHITECTURE.md](../rigor-qt/EXPLORERCLIENT_ARCHITECTURE.md)

---

### 2.7 Diagnostics

| Attribute | Value |
|-----------|--------|
| **Qt class** | `DiagnosticsPage` *(new)* or extended `RPCConsole` tab |
| **Files** | `diagnosticspage.cpp/h` *(new)*, partial reuse `rpcconsole.cpp` |
| **Action** | **ADD** |
| **Effort** | **L** |

**Dependencies:**

```
DiagnosticsPage
  ├── ClientModel
  ├── WalletModel (per-wallet status)
  ├── ExplorerClient (indexer health)
  └── RPCConsole widgets (peers — optional embed)
```

**Data sources:**

| Panel | Source |
|-------|--------|
| Chain sync | `getblockchaininfo` via ClientModel |
| Wallet encryption | `WalletModel::getEncryptionStatus()` |
| Descriptor / watch-only | `getwalletinfo` |
| Explorer lag | ExplorerClient |
| Last convergence RPC error | Local log buffer |
| `equivalence_claimed` | Hard-coded **false** + About link |

---

### 2.8 Send

| Attribute | Value |
|-----------|--------|
| **Qt class** | `SendCoinsDialog`, `SendCoinsEntry` |
| **Files** | `sendcoinsdialog.cpp/h`, `sendcoinsentry.cpp/h`, `forms/sendcoinsdialog.ui` |
| **Stack index** | `WalletView` widget 3 |
| **Nav shortcut** | Alt+2 |
| **Action** | **MODIFY** (mostly done) |
| **Effort** | **S** (context defaults) + **KEEP** (core flow) |

**Dependencies:** `WalletModel`, `CoinControlDialog`, `AddressBookPage`, `AskPassphraseDialog`, `OptionsModel`, `BitcoinUnits`

**Data sources:** `prepareTransaction` / `sendCoins`; fee always ORU; asset from `comboSendAsset`.

---

### 2.9 Receive

| Attribute | Value |
|-----------|--------|
| **Qt class** | `ReceiveCoinsDialog`, `ReceiveRequestDialog` |
| **Files** | `receivecoinsdialog.cpp/h`, `receiverequestdialog.cpp/h`, `recentrequeststablemodel.cpp/h` |
| **Stack index** | `WalletView` widget 2 |
| **Nav shortcut** | Alt+3 |
| **Action** | **MODIFY** (labels) |
| **Effort** | **S** |

**Data sources:** `AddressTableModel`, QR generation, payment request history.

---

### 2.10 Wallet summary

| Attribute | Value |
|-----------|--------|
| **Qt class** | `WalletOverviewPage` *(new)* |
| **Files** | `walletoverviewpage.cpp/h` *(new)* |
| **Action** | **ADD** |
| **Effort** | **M** |

**Dependencies:** `WalletModel`, `WalletController`, `AskPassphraseDialog`

**Data sources:** Wallet name, encryption status, descriptor fingerprint, last backup timestamp (local QSettings), watch-only flag.

---

### 2.11 About / protocol

| Attribute | Value |
|-----------|--------|
| **Qt class** | About dialog in `utilitydialog.cpp` |
| **Action** | **EXTEND** |
| **Effort** | **S** |

**Data sources:** Version strings, RFC links, `equivalence_claimed = false` disclaimer.

---

## 3. Modal / menu surfaces (KEEP)

| Screen | Qt Files | Effort |
|--------|----------|--------|
| Address book | `addressbookpage.cpp`, `addresstablemodel.cpp`, `editaddressdialog.cpp` | **KEEP** — 0 |
| Coin control | `coincontroldialog.cpp`, `coincontroltreewidget.cpp` | **KEEP** — 0 |
| Encrypt / unlock | `askpassphrasedialog.cpp` | **KEEP** — 0 |
| Create wallet | `createwalletdialog.cpp`, `intro.cpp` | **MODIFY** intro — **S** |
| PSBT | `psbtoperationsdialog.cpp`, `psbt_format.cpp` | **KEEP** — 0 |
| Sign message | `signverifymessagedialog.cpp` | **KEEP** — 0 |
| Options | `optionsdialog.cpp`, `optionsmodel.cpp` | **EXTEND** — **M** |
| RPC console | `rpcconsole.cpp` | **KEEP** — 0 |
| Sync overlay | `modaloverlay.cpp` | **KEEP** — 0 |

---

## 4. Shell components

| Component | Files | Action | Effort |
|-----------|-------|--------|--------|
| Main window | `bitcoingui.cpp/h` | MODIFY nav + branding | M |
| Wallet frame | `walletframe.cpp/h` | KEEP | 0 |
| Wallet view router | `walletview.cpp/h` | EXTEND stack | S |
| Client model | `clientmodel.cpp/h` | KEEP | 0 |
| Payment URI | `paymentserver.cpp/h` | MODIFY | S |
| Splash | `splashscreen.cpp/h` | MODIFY | S |
| Chrome / theme | `guiutil.cpp` (`ApplyRigorWalletChrome`) | KEEP | 0 |

---

## 5. Effort rollup by phase

| Phase | Screens | Total Effort |
|-------|---------|--------------|
| A — Stabilization | Overview (validate), Send, Receive, About, Intro | **~1.5 weeks** |
| B — ORU | History filters, ORU context | **~1 week** |
| C — RES | RES context, ResMintPanel | **~2 weeks** |
| D — Convergence UX | ConvergencePage gaps | **~1 week** |
| E — Explorer | ExplorerClient + ExplorerPage | **~2–3 weeks** |
| F — Diagnostics | DiagnosticsPage | **~1.5 weeks** |
| G — Packaging | Icons, NSIS, deploy scripts | **~2–4 weeks** (parallel) |

**Calendar note:** Phases B–D can overlap after A; E depends on explorer-indexer-mvp availability; G runs parallel from Phase C.

---

## 6. Observer MVP screen map (non-Qt)

| Screen | Path | Data Source | Effort |
|--------|------|-------------|--------|
| Dashboard | `app/page.tsx` | `/api/observer/chain` | MODIFY — M |
| Watch Registry | `app/watch/page.tsx` | Local storage + RPC | MODIFY — M |
| ORU | `app/oru/page.tsx` | Observer API | MODIFY — S |
| RES | `app/res/page.tsx` | Observer API + mints | MODIFY — S |
| Convergence | `app/convergence/page.tsx` | `/api/observer/convergence/events` | EXTEND — M |
| Tx lookup | `app/tx/page.tsx` | `/api/observer/tx/[txid]` | KEEP — 0 |

---

## 7. Output table

| Field | Status |
|-------|--------|
| Qt Component Map | **COMPLETE** |
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Equivalence Claimed | **false** |

---

*End of RIGOR_QT_COMPONENT_MAP — TASK-WALLET-PROGRAM-004*

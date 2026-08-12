# Bitcoin Core v25 Wallet Inheritance Analysis for RIGOR

**Task:** `TASK-WALLET-PROGRAM-003`  
**Version:** 1.0  
**Date:** 2026-06-23  
**Type:** Read-only architecture study — **no code, GUI, protocol, or consensus changes**

| Field | Value |
|-------|--------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

**Baseline:** Bitcoin Core v25 Qt wallet architecture (Overview · Send · Receive · Transactions + shell menus)  
**Target:** [RIGOR Wallet Product Blueprint](./RIGOR_WALLET_PRODUCT_ARCHITECTURE.md) (TASK-WALLET-PROGRAM-002)  
**Current RIGOR tree:** `core/src/qt/` — already a Core v25 fork with dual-asset and Convergence extensions

```
equivalence_claimed = false
```

---

## 1. Executive conclusion

**Yes — RIGOR can achieve its wallet goals by extending Bitcoin Core v25 Qt, not by redesigning the wallet.**

| Metric | Estimate | Basis |
|--------|----------|--------|
| **Wallet shell + model layer unchanged** | **~70%** | WalletModel, encryption, backup, coin control, address book, PSBT, multi-wallet, RPC console |
| **Pages requiring modification only** | **~20%** | Overview, Send, Receive, Transactions (mostly done in current `rigor-qt`) |
| **Net-new surfaces (add/extend)** | **~10%** | ExplorerClient, Diagnostics, RES mint views, protocol dashboard panels, Observer app |
| **Minimum implementation delta to testnet RC** | **~25–30% of blueprint UI scope** | Phases B–C of implementation plan; much of Phase A already exists |

RIGOR must **not** rewrite wallet cryptography, UTXO selection, descriptor storage, or transaction construction. Those remain Core-derived and are already extended for ORU/RES in `core/src/wallet/`.

---

## 2. Component-by-component analysis

Legend: **KEEP** · **MODIFY** · **EXTEND** · **REMOVE** · **ADD**

---

### 2.1 Overview page

| Verdict | **MODIFY** (partially complete) |
|---------|----------------------------------|

| Bitcoin v25 | RIGOR target | Current `rigor-qt` | Action |
|-------------|--------------|-------------------|--------|
| Single BTC balance | Separate ORU + RES cards | Dual panels in `overviewpage.cpp` | **MODIFY** — add supply metrics, explorer status, warning stack per blueprint Dashboard |
| Recent transactions list | Same, asset-tagged | Asset-aware delegate | **KEEP** list widget; **EXTEND** filters |
| Sync warning banner | Plain-language sync + explorer lag | Out-of-sync warning | **EXTEND** |

**Rationale:** Overview maps to blueprint **Dashboard**. Core layout (balance + recent txs) is battle-tested. RIGOR adds second balance column and authority warnings — not a new page class.

**Files:** `overviewpage.cpp`, `forms/overviewpage.ui`

---

### 2.2 Send page

| Verdict | **MODIFY** (largely complete) |
|---------|-------------------------------|

| Bitcoin v25 | RIGOR target | Current | Action |
|-------------|--------------|---------|--------|
| Single-asset send | ORU / RES asset selector | `comboSendAsset` in `sendcoinsdialog.cpp` | **KEEP** dialog flow |
| Fee in same asset | Fee always ORU | Implemented | **KEEP** |
| Coin control integration | Asset-scoped coin control | `CCoinControl.m_asset_type` | **KEEP** |
| URI / payment requests | `rigor:` URI | Payment server adapted | **MODIFY** labels only |

**Rationale:** SendCoinsDialog is the most complex Qt wallet component. Reimplementing it would introduce regressions in fee estimation, coin selection, and PSBT flows. RIGOR already enforces asset separation per `WALLET_DESIGN_LOCK_V1`.

**Blueprint mapping:** Blueprint **ORU → Send** and **RES → Send** can be **one Send page with asset locked by entry point** (toolbar sub-nav), not two separate dialogs.

**Files:** `sendcoinsdialog.cpp`, `sendcoinsentry.cpp`, `walletmodel.cpp`

---

### 2.3 Receive page

| Verdict | **KEEP** + **MODIFY** labels |
|---------|-------------------------------|

| Bitcoin v25 | RIGOR target | Current | Action |
|-------------|--------------|---------|--------|
| Address list + QR | ORU/RES receive contexts | Standard ReceiveCoinsDialog | **KEEP** |
| Request payment history | Same | RecentRequestsTableModel | **KEEP** |
| New address generation | ORU default; RES when program active | Descriptor wallet paths | **MODIFY** — default address type hints |

**Rationale:** Receive flow is stable. Blueprint splits ORU/RES receive into sub-screens via navigation context, not new widgets.

**Files:** `receivecoinsdialog.cpp`, `recentrequeststablemodel.cpp`

---

### 2.4 Transactions page

| Verdict | **MODIFY** (partially complete) |
|---------|--------------------------------|

| Bitcoin v25 | RIGOR target | Current | Action |
|-------------|--------------|---------|--------|
| Global tx history | History + asset filter | `TransactionView` + `AssetDomainRole` | **EXTEND** — ORU/RES/convergence type filters |
| Export CSV | Same | Export button | **KEEP** |
| Tx detail dialog | + convergence metadata | `transactiondesc.cpp` | **EXTEND** |

**Rationale:** TransactionTableModel pipeline is Core-proven. Add columns/roles rather than replace.

**Blueprint mapping:** Blueprint **History** = this page; ORU/RES scoped views = **filters**, not duplicate list models.

**Files:** `transactionview.cpp`, `transactiontablemodel.cpp`, `transactiondesc.cpp`

---

### 2.5 Coin control

| Verdict | **KEEP** |
|---------|----------|

| Bitcoin v25 | RIGOR | Action |
|-------------|-------|--------|
| UTXO tree selection | Asset-filtered via `CCoinControl` | **KEEP** unchanged |
| Input list per tx | ORU fee inputs separate from transfer asset | **KEEP** — wallet layer enforces |

**Rationale:** Coin control dialog (`coincontroldialog.cpp`) is high-risk to rewrite. RIGOR already passes `m_asset_type` into coin selection.

**Files:** `coincontroldialog.cpp`, `coincontroltreewidget.cpp`

---

### 2.6 Address book

| Verdict | **KEEP** |
|---------|----------|

| Bitcoin v25 | RIGOR | Action |
|-------------|-------|--------|
| Sending / receiving tabs | Same | **KEEP** |
| Label editing | Same | **KEEP** |
| Used addresses menus | Same | **KEEP** via `bitcoingui.cpp` wallet menu |

**Rationale:** Address book is asset-agnostic at address level (RIGOR addresses are program-typed by witness version, not book entry). No structural change required.

**Files:** `addressbookpage.cpp`, `addresstablemodel.cpp`

---

### 2.7 Backup

| Verdict | **KEEP** + **EXTEND** |
|---------|----------------------|

| Bitcoin v25 | RIGOR | Action |
|-------------|-------|--------|
| wallet.dat backup | Descriptor SQLite + legacy BDB | **KEEP** `backupWallet()` |
| Seed backup (create wallet) | Same + RIGOR first-run copy | **KEEP** `intro.cpp`, `createwalletdialog.cpp` |
| Descriptor export | Blueprint Wallet screen | **EXTEND** — menu action + first-run reminder |

**Rationale:** Backup paths are safety-critical. Extend UI surfacing; do not change backup format without wallet freeze approval.

**Files:** `walletview.cpp` (backupWallet), `intro.cpp`

---

### 2.8 Encryption

| Verdict | **KEEP** |
|---------|----------|

| Bitcoin v25 | RIGOR | Action |
|-------------|-------|--------|
| Encrypt wallet | Same | **KEEP** `AskPassphraseDialog` |
| Unlock for send | Same | **KEEP** |
| Change passphrase | Same | **KEEP** |
| Lock timeout | OptionsModel | **KEEP** |

**Rationale:** Zero product reason to replace Core encryption UX.

**Files:** `askpassphrasedialog.cpp`, `walletview.cpp`

---

### 2.9 Network page

| Verdict | **KEEP** (node shell, not wallet frame) |
|---------|----------------------------------------|

| Bitcoin v25 | RIGOR | Action |
|-------------|-------|--------|
| Peers tab in debug window | Same | **KEEP** `rpcconsole.cpp` |
| Network traffic graph | Same | **KEEP** |
| Connection count in status bar | Same | **KEEP** `ClientModel` |

**Rationale:** Network is node-level in `RigorGUI`, not `WalletView`. Blueprint **Dashboard** network card reads same `ClientModel` signals.

**Files:** `rpcconsole.cpp`, `clientmodel.cpp`, `peertablemodel.cpp`

---

### 2.10 RPC console

| Verdict | **KEEP** |
|---------|----------|

| Bitcoin v25 | RIGOR | Action |
|-------------|-------|--------|
| Console + history | Same | **KEEP** |
| Wallet RPC tab | Includes `createconvergence`, `sendres` | **KEEP** — already RIGOR-extended at RPC layer |
| Peers / wallet / network tabs | Same | **KEEP** |

**Rationale:** Operator and advanced-user surface. No blueprint replacement.

**Files:** `rpcconsole.cpp`

---

### 2.11 Settings

| Verdict | **KEEP** + **EXTEND** |
|---------|----------------------|

| Bitcoin v25 | RIGOR | Action |
|-------------|-------|--------|
| Options dialog (display, network, wallet) | Same tabs | **KEEP** `optionsdialog.cpp` |
| Unit selection | ORU / Tharn units | **MODIFY** — already `BitcoinUnits::ORU` |
| Explorer URL / refresh interval | Not in Core | **ADD** options panel (Phase D) |
| Compliance display toggle | Not in Core | **ADD** About linkage |

**Rationale:** OptionsModel pattern is proven. Add RIGOR-specific page to existing dialog rather than new settings app.

**Files:** `optionsdialog.cpp`, `optionsmodel.cpp`

---

### 2.12 Wallet management

| Verdict | **KEEP** |
|---------|----------|

| Bitcoin v25 | RIGOR | Action |
|-------------|-------|--------|
| Create / load / unload wallet | Same | **KEEP** `walletcontroller.cpp` |
| Multi-wallet tabs | Same | **KEEP** `walletframe.cpp` |
| Migrate wallet | Same | **KEEP** |

**Rationale:** Multi-wallet infrastructure is expensive to rebuild. Blueprint **Wallet overview** is a new summary panel, not a replacement for WalletController.

**Files:** `walletcontroller.cpp`, `walletframe.cpp`, `createwalletdialog.cpp`

---

## 3. Components Bitcoin v25 has that RIGOR should REMOVE or hide

| Component | Verdict | Rationale |
|-----------|---------|-----------|
| BTC / satoshi unit naming | **REMOVED** (already) | Replaced by ORU / Tharn in `bitcoinunits.cpp` |
| Single-asset balance aggregation | **REMOVED** (already) | Violates dual-native design lock |
| Bitcoin-branded installer strings | **REMOVE** (packaging, not Qt) | `setup.nsi` legacy — branding task |
| Token-style combined portfolio | **NEVER ADD** | Constitution forbidden |

No Core wallet **feature** must be removed for RIGOR correctness except legacy **naming/branding** surfaces outside Qt logic.

---

## 4. Components RIGOR must ADD (not in Bitcoin v25)

| Component | Verdict | Rationale |
|-----------|---------|-----------|
| **Convergence page** | **ADD** (exists) | `convergencepage.cpp` — Core has no equivalent |
| **RES mint history view** | **ADD** | Filtered tx view + explorer enrichment |
| **ExplorerClient** | **ADD** | Read-only HTTP; GUI-10 |
| **Diagnostics panel** | **EXTEND** debug window or new tab | Blueprint operator surface |
| **Protocol / compliance panel** | **ADD** | `equivalence_claimed`, supply (About + Dashboard) |
| **RES_PROBABILITY_TARGET display** | **ADD** to Convergence | Authority read; spec required |
| **RIGOR Observer app** | **ADD** (separate product) | Not a Core fork — Next.js MVP exists |

---

## 5. Inheritance summary matrix

| Component | KEEP | MODIFY | EXTEND | REMOVE | ADD |
|-----------|------|--------|--------|--------|-----|
| Overview | 40% | 50% | 10% | 0% | 0% |
| Send | 70% | 25% | 5% | 0% | 0% |
| Receive | 85% | 15% | 0% | 0% | 0% |
| Transactions | 60% | 20% | 20% | 0% | 0% |
| Coin control | 100% | 0% | 0% | 0% | 0% |
| Address book | 100% | 0% | 0% | 0% | 0% |
| Backup | 80% | 0% | 20% | 0% | 0% |
| Encryption | 100% | 0% | 0% | 0% | 0% |
| Network | 95% | 0% | 5% | 0% | 0% |
| RPC console | 90% | 0% | 10% | 0% | 0% |
| Settings | 80% | 10% | 10% | 0% | 0% |
| Wallet management | 90% | 0% | 10% | 0% | 0% |
| Convergence | 0% | 0% | 10% | 0% | 90% |
| Explorer / Diagnostics | 0% | 0% | 0% | 0% | 100% |

**Weighted overall (by implementation effort):** ~**68% KEEP**, ~**18% MODIFY**, ~**9% EXTEND**, ~**5% ADD** (new modules only).

---

## 6. Risk of redesign vs inherit

| Approach | Risk | Recommendation |
|----------|------|----------------|
| Full wallet UI rewrite | HIGH — regression in coin selection, PSBT, encryption | **Reject** |
| Extend Core v25 Qt in place | LOW — matches current `rigor-qt` trajectory | **Adopt** |
| Observer-only product (no rigor-qt) | MEDIUM — no signing path for testnet participants | **Complement**, not replace |
| Web wallet replacement | HIGH — key material in browser | **Reject** for signing |

---

## 7. Related documents

| Document | Purpose |
|----------|---------|
| [RIGOR_WALLET_DELTA_MAP.md](./RIGOR_WALLET_DELTA_MAP.md) | Structured delta list |
| [RIGOR_WALLET_NAVIGATION_COMPARISON.md](./RIGOR_WALLET_NAVIGATION_COMPARISON.md) | Nav diff |
| [RIGOR_WALLET_IMPLEMENTATION_PLAN.md](./RIGOR_WALLET_IMPLEMENTATION_PLAN.md) | Phased delivery |

---

## 8. Output table

| Field | Status |
|-------|--------|
| Inheritance Analysis | **COMPLETE** |
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Equivalence Claimed | **false** |

---

*End of BITCOIN_V25_WALLET_INHERITANCE_ANALYSIS — TASK-WALLET-PROGRAM-003*

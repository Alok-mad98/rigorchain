# RIGOR Wallet Implementation Scope

**Task:** `TASK-WALLET-PROGRAM-009`  
**Version:** 1.0  
**Date:** 2026-06-23  
**Type:** Implementation scope estimate — **no implementation**

| Field | Value |
|-------|--------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

Estimates **LOC**, Qt components, pages, dialogs, widgets, and test requirements for full blueprint delivery vs Phase A.

**Baseline:** Current `rigor-qt` ~55–60% blueprint complete (001 audit, 009 review).

---

## 1. Summary estimates

| Metric | Already in tree | Net-new work | Total blueprint |
|--------|-----------------|--------------|-----------------|
| **C++ LOC (Qt wallet UI)** | ~48,000 inherited | ~6,000–8,000 | ~54,000–56,000 |
| **New/modified .cpp/.h files** | ~179 qt files | ~18–22 files | ~195–200 |
| **Primary pages** | 5 | 4 | 9 |
| **Modal dialogs** | 12+ (Core) | 0 new | 12+ |
| **Reusable widgets (new)** | 0 extracted | 14 | 14 |
| **Calendar weeks (1 eng)** | — | 10–14 to testnet RC | — |

---

## 2. Lines of code (LOC)

### 2.1 By phase (net-new/modified estimate)

| Phase | Modified LOC | New LOC | Total delta |
|-------|--------------|---------|-------------|
| **A** Stabilization | ~800 | ~400 | ~1,200 |
| **B** ORU | ~600 | ~500 | ~1,100 |
| **C** RES | ~400 | ~1,500 | ~1,900 |
| **D** Convergence | ~500 | ~800 | ~1,300 |
| **E** Explorer | ~300 | ~2,500 | ~2,800 |
| **F** Diagnostics | ~200 | ~1,200 | ~1,400 |
| **G** Packaging | ~100 | ~200 | ~300 (excl. brand assets) |
| **Total** | ~2,900 | ~6,900 | **~9,800** |

**Note:** ~70% of delta is **new modules** (ExplorerClient, widgets, pages); ~30% modifies existing Core-derived files.

### 2.2 LOC by component type

| Type | New LOC estimate |
|------|------------------|
| `ExplorerClient` | ~1,800 |
| Widget library (`widgets/`) | ~1,400 |
| New pages (Explorer, Diagnostics, Wallet overview, ResMint) | ~2,200 |
| Convergence/Overview/Send modifications | ~1,400 |
| Options / shell / strings | ~800 |
| Forms `.ui` + resources | ~400 (XML) |

---

## 3. Qt components

### 3.1 Pages (WalletView stack)

| Page | Class | Status | Action | Est. LOC |
|------|-------|--------|--------|----------|
| Dashboard | `OverviewPage` | Exists | MODIFY | 400 mod |
| Send | `SendCoinsDialog` | Exists | MODIFY | 300 mod |
| Receive | `ReceiveCoinsDialog` | Exists | MODIFY | 200 mod |
| History | `TransactionView` | Exists | EXTEND | 350 mod |
| Convergence | `ConvergencePage` | Exists | EXTEND | 600 mod |
| Wallet overview | `WalletOverviewPage` | **New** | ADD | 450 |
| Explorer | `ExplorerPage` | **New** | ADD | 550 |
| Diagnostics | `DiagnosticsPage` | **New** | ADD | 700 |
| RES mint | `ResMintPanel` | **New** | ADD | 650 |

### 3.2 Shell components

| Component | File | Action | Est. LOC |
|-----------|------|--------|----------|
| Main window | `bitcoingui.cpp` | MODIFY | 250 mod |
| Wallet router | `walletview.cpp` | EXTEND | 150 mod |
| Client bridge | `clientmodel.cpp` | KEEP | 0 |
| Wallet bridge | `walletmodel.cpp` | MODIFY | 200 mod |
| Explorer client | `explorerclient.cpp` | **ADD** | 1,800 |

### 3.3 Dialogs (no new modal classes)

| Dialog | Status |
|--------|--------|
| Send confirm | KEEP (Core) |
| Unlock | KEEP `AskPassphraseDialog` |
| Options | EXTEND `OptionsDialog` |
| About | EXTEND `UtilityDialog` |
| Coin control | KEEP |
| Address book | KEEP |
| Create wallet / Intro | MODIFY strings |
| QR fullscreen | KEEP |
| PSBT | KEEP |

**New dialogs:** 0 (blueprint reuses Core modals)

---

## 4. Widget library scope

| Widget | New? | Est. LOC | Phase |
|--------|------|----------|-------|
| `WarningBanner` | Yes | 180 | A |
| `ORUBalancePanel` | Yes | 120 | B |
| `RESBalancePanel` | Yes | 120 | C |
| `AssetCard` (base) | Yes | 100 | B |
| `AssetContextChip` | Yes | 80 | B |
| `ConvergenceCard` | Yes | 200 | D |
| `VerificationStatus` | Yes | 90 | D |
| `MintRecordCard` | Yes | 150 | C |
| `MintRecordTable` | Yes | 220 | C |
| `ExplorerLink` | Yes | 70 | E |
| `ExplorerStatusCard` | Yes | 130 | E |
| `SyncCard` | Yes | 100 | A |
| `NetworkCard` | Yes | 90 | A |
| `SupplyPanel` | Yes | 110 | E |
| `HashLabel` / `AmountLabel` | Yes | 160 | B |

**Widget subtotal:** ~1,920 LOC new

---

## 5. Pages count

| Category | Count |
|----------|-------|
| Primary stack pages | 9 |
| ORU/RES logical sub-screens (context, not separate stack) | 10 |
| Modal overlays | 8+ |
| First-run wizard steps | 7 |
| Settings tabs | 5 |

---

## 6. Test requirements

### 6.1 Automated (existing + recommended)

| Test type | Scope | Owner | Phase |
|-----------|-------|-------|-------|
| `make check` wallet subset | Regression | User executes | A exit |
| `feature_res_*` functional | RES RPC | CI | Exists |
| Qt unit tests (if added) | Widget smoke | Eng | B+ OPT |
| GUI-18 string grep | Forbidden terms | CI/QA | A |

**Recommendation:** Do not block Phase A on new Qt unit tests; add widget tests in Phase B if time.

### 6.2 Manual / runtime validation

| Matrix | Document | Phase | Required for |
|--------|----------|-------|--------------|
| Core wallet ops | `WALLET_RUNTIME_VALIDATION.md` | A exit | Internal |
| Journeys A–F | `RIGOR_WALLET_USER_JOURNEYS.md` | A–F | Testnet RC |
| UI checklist | `RIGOR_WALLET_RELEASE_UI_CHECKLIST.md` | G | Public RC |
| Screenshot bundle | evidence/ | G | Public RC |

### 6.3 Simulation (informative, not gate)

| Sim | Purpose |
|-----|---------|
| `python3 -m sim.wallet_sim.run_all` | Multi-client flows |
| `python3 -m sim.unified.run_full_system_sim` | Cross-subsystem |

Does **not** replace runtime validation.

### 6.4 Test effort estimate

| Activity | Person-days |
|----------|-------------|
| Phase A validation | 2–3 |
| Journeys B–D recording | 3–4 |
| Explorer/Diagnostics | 2 |
| Cross-platform RC | 5–8 |
| **Total QA** | **12–17** |

---

## 7. Scope by release gate

### 7.1 Minimum viable internal (Phase A exit)

| Deliverable | LOC |
|-------------|-----|
| About + strings + WarningBanner | ~1,200 |
| Validation PASS | QA |

### 7.2 Public testnet wallet RC

| Deliverable | Cumulative LOC delta |
|-------------|---------------------|
| Phases A–D + G packaging | ~6,500–7,500 |
| Brand assets (non-LOC) | 52 assets |
| UI checklist PASS | evidence |

### 7.3 Mainnet candidate

| Deliverable | Cumulative LOC delta |
|-------------|---------------------|
| Full A–G + sidebar | ~9,800 |
| Full icon pack | 66 SVG |
| Mainnet checklist | M-* gates |

---

## 8. Resource estimate

| Role | Phase A | Full testnet RC |
|------|---------|-----------------|
| Qt engineer | 1.5 weeks | 10–12 weeks |
| Designer | 0 (parallel logo) | 2–3 weeks |
| QA | 0.5 weeks | 3–4 weeks |
| Ops (installer) | 0 | 2–3 weeks |

---

## 9. Out of scope (explicit)

| Item | Reason |
|------|--------|
| Consensus / protocol changes | Task forbidden |
| Taproot UI | Constitution |
| Account model UI | Architecture |
| Web signing wallet | Security |
| Observer rewrite in Qt | Separate product |
| PROOF-034 execution | Separate program |

---

## 10. Output table

| Field | Status |
|-------|--------|
| Implementation Scope | **COMPLETE** |
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Equivalence Claimed | **false** |

---

*End of RIGOR_WALLET_IMPLEMENTATION_SCOPE — TASK-WALLET-PROGRAM-009*

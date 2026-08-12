# RIGOR Wallet Build Order

**Task:** `TASK-WALLET-PROGRAM-009`  
**Version:** 1.0  
**Date:** 2026-06-23  
**Type:** Implementation sequence — **no code changes**

| Field | Value |
|-------|--------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

Exact **implementation sequence** from Phase A through public testnet RC. IDs cross-reference [RIGOR_WALLET_RELEASE_UI_CHECKLIST.md](./RIGOR_WALLET_RELEASE_UI_CHECKLIST.md) UI-* gates.

**First Qt component to build (net-new extract):** `WarningBanner` (BUILD-08) — or first **file touch**: `utilitydialog.cpp` (BUILD-01).

---

## Phase A — Stabilization (Week 1–2)

| Order | ID | Work item | File(s) | UI gate | Est. |
|-------|-----|-----------|---------|---------|------|
| 1 | BUILD-01 | About: equivalence disclaimer + RFC links | `utilitydialog.cpp` | UI-04 | 0.5d |
| 2 | BUILD-02 | Intro / first-run RIGOR copy | `intro.cpp`, `createwalletdialog.cpp` | UI-72 partial | 0.5d |
| 3 | BUILD-03 | Window title + menu string rebrand | `bitcoingui.cpp`, `bitcoinstrings.cpp` | UI-01 | 0.5d |
| 4 | BUILD-04 | Locale string sync | `src/qt/locale/bitcoin_*.ts` | UI-01 | 0.5d |
| 5 | BUILD-05 | Forbidden term grep + fix | all `.ts`, `.cpp` strings | UI-56, GUI-18 | 0.5d |
| 6 | BUILD-06 | Verify Overview dual ORU/RES panels | `overviewpage.cpp` | UI-20, UI-21 | 0.5d |
| 7 | BUILD-07 | Verify Send asset combo + ORU fee | `sendcoinsdialog.cpp` | UI-31 | 0.5d |
| 8 | BUILD-08 | Extract `WarningBanner` widget | `widgets/warningbanner.cpp` *(new)* | UI-23 | 1d |
| 9 | BUILD-09 | Wire sync/offline banners to Dashboard | `overviewpage.cpp`, `bitcoingui.cpp` | UI-23, UI-34 | 1d |
| 10 | BUILD-10 | Verify Receive ORU flow | `receivecoinsdialog.cpp` | UI-30 | 0.5d |
| 11 | BUILD-11 | Verify History + recent txs asset tags | `transactionview.cpp`, `overviewpage.cpp` | UI-24 | 0.5d |
| 12 | BUILD-12 | Verify Convergence tab load + send path smoke | `convergencepage.cpp` | UI-50 | 1d |
| 13 | BUILD-13 | Encrypt / backup smoke | `askpassphrasedialog.cpp`, `walletview.cpp` | UI-70, UI-71 | 0.5d |
| 14 | BUILD-14 | Status bar sync + lock audit | `bitcoingui.cpp`, `clientmodel.cpp` | UI-11 | 0.5d |
| 15 | BUILD-15 | Phase A runtime validation handoff | QA executes matrix | UI-30–35 partial | — |

**Phase A gate:** BUILD-01 through BUILD-14 complete; BUILD-15 scheduled.

---

## Phase B — ORU completion (Week 2–3)

| Order | ID | Work item | File(s) | UI gate | Est. |
|-------|-----|-----------|---------|---------|------|
| 16 | BUILD-16 | `AssetContextChip` on Send/Receive | `sendcoinsdialog.cpp`, `receivecoinsdialog.cpp` | UI-31 | 1d |
| 17 | BUILD-17 | ORU default in OptionsModel | `optionsmodel.cpp` | — | 0.5d |
| 18 | BUILD-18 | History ORU/RES/All filter | `transactionfilterproxy.cpp`, `transactionview.cpp` | UI-61 | 1.5d |
| 19 | BUILD-19 | ORU badge in tx detail | `transactiondesc.cpp` | UI-63 | 1d |
| 20 | BUILD-20 | Extract `ORUBalancePanel` | `widgets/orubalancepanel.cpp` *(new)* | UI-20 | 1d |
| 21 | BUILD-21 | Journey B E2E validation | QA recording | UI-31–34 | — |

---

## Phase C — RES completion (Week 3–5)

| Order | ID | Work item | File(s) | UI gate | Est. |
|-------|-----|-----------|---------|---------|------|
| 22 | BUILD-22 | Extract `RESBalancePanel` | `widgets/resbalancepanel.cpp` *(new)* | UI-40 | 1d |
| 23 | BUILD-23 | RES receive copy (Bech32m) | `receivecoinsdialog.cpp`, `.ui` | UI-42 | 0.5d |
| 24 | BUILD-24 | RES send validation UX | `sendcoinsdialog.cpp` | UI-41 | 0.5d |
| 25 | BUILD-25 | `ResMintPanel` + table | `resmintpanel.cpp` *(new)* | UI-43, UI-44 | 3d |
| 26 | BUILD-26 | RES immature/pending labels | `overviewpage.cpp`, `walletmodel.cpp` | UI-45 | 1d |
| 27 | BUILD-27 | Journey D partial validation | QA | UI-40–45 | — |

---

## Phase D — Convergence UX (Week 4–6)

| Order | ID | Work item | File(s) | UI gate | Est. |
|-------|-----|-----------|---------|---------|------|
| 28 | BUILD-28 | `RES_PROBABILITY_TARGET` display | `convergencepage.cpp` | UI-52 | 1d |
| 29 | BUILD-29 | Root mismatch flag UX | `convergencepage.cpp` | UI-53 | 1d |
| 30 | BUILD-30 | Extract `ConvergenceCard` | `widgets/convergencecard.cpp` *(new)* | UI-53 | 1.5d |
| 31 | BUILD-31 | `VerificationStatus` widget | `widgets/verificationstatus.cpp` *(new)* | UI-52 | 0.5d |
| 32 | BUILD-32 | Convergence tx type in History | `transactiontablemodel.cpp`, `transactiondesc.cpp` | UI-54 | 1d |
| 33 | BUILD-33 | Convergence error modal + runbook link | `convergencepage.cpp`, `walletmodel.cpp` | UI-55 | 1d |
| 34 | BUILD-34 | Testnet createconvergence E2E | QA + ops | UI-51 | — |
| 35 | BUILD-35 | Journey C validation | QA recording | UI-50–56 | — |

---

## Phase E — ExplorerClient (Week 6–9)

| Order | ID | Work item | File(s) | UI gate | Est. |
|-------|-----|-----------|---------|---------|------|
| 36 | BUILD-36 | `ExplorerClient` module | `explorerclient.cpp/h` *(new)* | — | 5d |
| 37 | BUILD-37 | Options: explorer URL | `optionsmodel.cpp`, `optionsdialog.cpp` | — | 1d |
| 38 | BUILD-38 | `ExplorerLink` widget | `widgets/explorerlink.cpp` *(new)* | — | 0.5d |
| 39 | BUILD-39 | `ExplorerStatusCard` on Dashboard | `overviewpage.cpp` | UI-25 | 1d |
| 40 | BUILD-40 | `SupplyPanel` | `overviewpage.cpp` | UI-25 | 1d |
| 41 | BUILD-41 | `ExplorerPage` tab | `explorerpage.cpp` *(new)*, `walletview.cpp`, `bitcoingui.cpp` | UI-90 | 3d |
| 42 | BUILD-42 | Explorer offline degrade | `explorerclient.cpp` | UI-91 | 1d |

---

## Phase F — Diagnostics (Week 8–10)

| Order | ID | Work item | File(s) | UI gate | Est. |
|-------|-----|-----------|---------|---------|------|
| 43 | BUILD-43 | `DiagnosticsPage` | `diagnosticspage.cpp` *(new)* | UI-81 | 3d |
| 44 | BUILD-44 | Nav action Alt+7 | `bitcoingui.cpp` | UI-81 | 0.5d |
| 45 | BUILD-45 | Export diagnostic bundle (opt) | `diagnosticspage.cpp` | — | 2d |
| 46 | BUILD-46 | Journey F validation | QA | UI-80–83 | — |

---

## Phase G — Packaging & brand (Week 6–12, parallel)

| Order | ID | Work item | File(s) | UI gate | Est. |
|-------|-----|-----------|---------|---------|------|
| 47 | BUILD-47 | Logo sprint → app icon 1024 | `brand/app/` | UI-02, UI-07 | design |
| 48 | BUILD-48 | Run `build_app_icon.py` | `rigor.png`, `rigor.ico`, `rigor.icns` | UI-02 | 0.5d |
| 49 | BUILD-49 | Splash PNG replace | `res/images/splash-screen.png` | UI-03 | 0.5d |
| 50 | BUILD-50 | NSIS rebrand | `share/setup.nsi`, pixmaps | UI-05 | 2d |
| 51 | BUILD-51 | Linux `.desktop` + icons | `share/pixmaps/` | L2 | 1d |
| 52 | BUILD-52 | macOS bundle + DMG (opt) | `contrib/macdeploy/` | M4 | 3d |
| 53 | BUILD-53 | CI desktop release tag | `.github/workflows/` | W1, L1 | 1d |
| 54 | BUILD-54 | Public testnet RC evidence bundle | `docs/wallet/evidence/` | All PASS | QA |

---

## Parallel tracks (non-blocking order)

| Track | Items | Can start |
|-------|-------|-----------|
| Logo design | BUILD-47 | Day 1 (parallel Phase A) |
| Observer polish | UI-100–103 | After Phase A |
| Icon pack SVG | BUILD-55 *(optional)* | Phase B |
| WalletOverviewPage | BUILD-56 | Phase B |

| 55 | BUILD-55 | Navigation icon SVG set (min 8) | `brand/icons/` | UI-10 | 3d |
| 56 | BUILD-56 | `WalletOverviewPage` | `walletoverviewpage.cpp` *(new)* | — | 2d |

---

## Critical path summary

```
BUILD-01 → … → BUILD-15 (Phase A)
  → BUILD-16–21 (B) → BUILD-22–27 (C)
  → BUILD-28–35 (D)
  → BUILD-47–50 (brand, parallel from C)
  → BUILD-54 (RC evidence)
```

Explorer (BUILD-36–42) and Diagnostics (BUILD-43–46) are **P1** for testnet OPT — not on minimum critical path for signing wallet RC.

---

## First component answer

| Question | Answer |
|----------|--------|
| **First file to touch** | `utilitydialog.cpp` (BUILD-01) |
| **First net-new Qt widget** | `WarningBanner` (BUILD-08) |
| **First new page** | `ResMintPanel` (BUILD-25) or `ExplorerPage` (BUILD-41) depending on phase |

---

## Output table

| Field | Status |
|-------|--------|
| Build Order | **COMPLETE** |
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Equivalence Claimed | **false** |

---

*End of RIGOR_WALLET_BUILD_ORDER — TASK-WALLET-PROGRAM-009*

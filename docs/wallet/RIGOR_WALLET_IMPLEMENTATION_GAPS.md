# RIGOR Wallet Implementation Gaps

**Task:** `TASK-WALLET-PROGRAM-009`  
**Version:** 1.0  
**Date:** 2026-06-23  
**Type:** Read-only gap register — **no implementation**

| Field | Value |
|-------|--------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

Every remaining **unknown or incomplete item** before full wallet product delivery. Classified P0 (testnet blocker), P1 (testnet quality), P2 (mainnet / polish).

---

## 1. Gap summary

| Priority | Count | Blocks Phase A? | Blocks testnet RC? |
|----------|-------|-----------------|---------------------|
| **P0** | 14 | 0 | Yes |
| **P1** | 18 | 0 | Partial |
| **P2** | 12 | 0 | No |
| **Total** | **44** | — | — |

---

## 2. P0 — Testnet release blockers

| ID | Gap | Unknown / incomplete | Owner | Mitigation |
|----|-----|----------------------|-------|------------|
| G-P0-01 | **Public installer missing** | No downloadable testnet bundle | Ops | Phase G NSIS/AppImage |
| G-P0-02 | **Installer branding legacy** | Arda, bitcoincore.org, arda.ico | Eng + brand | `setup.nsi`, pixmaps |
| G-P0-03 | **App icon artwork** | 0/52 visual assets | Design | Logo sprint → build_app_icon.py |
| G-P0-04 | **Splash screen artwork** | Unverified Core-era splash | Design | brand/app/splash |
| G-P0-05 | **`RES_PROBABILITY_TARGET` UI** | Not in `convergencepage.cpp` | Eng | Phase C/D — spec in 008 |
| G-P0-06 | **Runtime validation not executed** | WALLET_RUNTIME_VALIDATION matrix | QA | Execute post–Phase A |
| G-P0-07 | **Testnet convergence E2E** | createconvergence failures documented | Wallet + ops | CONVERGENCE_RUNTIME_REPORT |
| G-P0-08 | **About compliance incomplete** | equivalence disclaimer partial | Eng | Phase A utilitydialog |
| G-P0-09 | **Journey A installer path** | Non-technical user cannot install | Ops | Phase G packaging |
| G-P0-10 | **Observer MVP suffix / packaging** | "MVP" naming, manual Node stack | Frontend + ops | Observer product pass |
| G-P0-11 | **GUI-18 forbidden terms gate** | Periodic grep not certified PASS | QA | String audit Phase A |
| G-P0-12 | **Unified product download page** | Wallet + Observer not bundled | Product | Phase G |
| G-P0-13 | **Evidence artifact bundle** | No screenshot/recording proof | QA | UI checklist evidence dir |
| G-P0-14 | **Mainnet gate independent** | PROOF-034 NO_GO | Program | Testnet ≠ mainnet; document |

---

## 3. P1 — Testnet quality (deferrable with documented OPT)

| ID | Gap | Notes |
|----|-----|-------|
| G-P1-01 | ExplorerClient not implemented | GUI-10; mint history enrich |
| G-P1-02 | ExplorerPage tab missing | Phase E |
| G-P1-03 | DiagnosticsPage missing | Phase F; Console partial OK |
| G-P1-04 | RES mint history in rigor-qt | Observer has; Qt missing |
| G-P1-05 | Root mismatch UX on ConvergencePage | Spec in 008; not coded |
| G-P1-06 | History asset filters | TransactionFilterProxy extend |
| G-P1-07 | WarningBanner widget extraction | Inline vs reusable |
| G-P1-08 | WalletOverviewPage | File menu only today |
| G-P1-09 | Supply panel on Dashboard | Needs explorer |
| G-P1-10 | Observer Watch Registry wiring | Partial MVP |
| G-P1-11 | Icon pack redesign (66 SVG) | Legacy Bitcoin icons |
| G-P1-12 | Testnet icon badge policy | Optional T corner |
| G-P1-13 | Convergence operator runbook (wallet) | Docs gap |
| G-P1-14 | Faucet/mining onboarding in wallet | Ops docs only |
| G-P1-15 | ORU convergence outputs view | Filter spec exists |
| G-P1-16 | Transaction viewer convergence metadata | Partial |
| G-P1-17 | Explorer offline graceful mode | Error spec exists |
| G-P1-18 | sim/wallet_sim ≠ product sign-off | Use runtime validation |

---

## 4. P2 — Mainnet / polish

| ID | Gap | Notes |
|----|-----|-------|
| G-P2-01 | Sidebar navigation (blueprint) | Phase D; toolbar OK for testnet |
| G-P2-02 | Dedicated ORU/RES top-level tabs | Context strip alternative |
| G-P2-03 | Convergence Inspector / Storyboard in Qt | Prototype exists |
| G-P2-04 | Light mode theme | Dark default sufficient testnet |
| G-P2-05 | macOS notarization | Mainnet |
| G-P2-06 | Code signing Authenticode | Mainnet |
| G-P2-07 | Trademark / legal clearance | Pre-mainnet public |
| G-P2-08 | i18n full translation | English only testnet |
| G-P2-09 | Accessibility formal audit | Mainnet checklist |
| G-P2-10 | Diagnostic export bundle | Optional RC |
| G-P2-11 | PWA / mobile observer | Out of scope testnet |
| G-P2-12 | WebEngine embedded explorer | List-first MVP OK |

---

## 5. Unknowns requiring decision (not spec gaps)

| ID | Unknown | Decision needed | Default if no decision |
|----|---------|-----------------|------------------------|
| U-01 | Sidebar vs toolbar for testnet RC | Product | Toolbar (Phase A path) |
| U-02 | Add `getresmintstatus` RPC | Wallet | Explorer-only path |
| U-03 | Testnet badge baked in icon vs Qt overlay | Brand | Qt overlay |
| U-04 | Explorer tab vs external browser only | Product | Tab Phase E; link-out Phase A OK |
| U-05 | Shire Wallet rename | Marketing | **Rejected** — use RIGOR Wallet |
| U-06 | Embedded rigord vs external node default | Ops | Embedded for public download |
| U-07 | Designer engagement timeline | Product | Parallel to Phase A |
| U-08 | Mainnet timeline | Release gates | **NO-GO** until gates clear |

---

## 6. Specification vs implementation matrix

| Spec document | Implementation status |
|---------------|----------------------|
| GUI spec (008) | ~60% exists; 40% specified not built |
| Widget library (008) | ~20% extracted; 80% to build/refactor |
| Error spec (008) | ~50% via Core; RIGOR-specific partial |
| RPC requirements (004) | Backend ~95%; Qt wiring ~70% |
| Brand assets (006) | 0% artwork |
| Logo direction (007) | 0% artwork |

---

## 7. Gap closure ownership

| Track | Closes gaps |
|-------|-------------|
| **Eng Phase A** | G-P0-08, G-P0-11, partial G-P1-07 |
| **Eng Phase B–C** | G-P1-06, G-P1-04, G-P1-15 |
| **Eng Phase D** | G-P0-05, G-P1-05, G-P0-07 partial |
| **Eng Phase E–F** | G-P1-01–03, G-P1-09 |
| **Design** | G-P0-03, G-P0-04 |
| **Ops/QA** | G-P0-01, G-P0-06, G-P0-09, G-P0-13 |
| **Observer team** | G-P0-10, G-P1-10 |

---

## 8. Output table

| Field | Status |
|-------|--------|
| Implementation Gaps | **COMPLETE** |
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Equivalence Claimed | **false** |

---

*End of RIGOR_WALLET_IMPLEMENTATION_GAPS — TASK-WALLET-PROGRAM-009*

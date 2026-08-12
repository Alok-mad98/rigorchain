# RIGOR Wallet Release UI Checklist

**Task:** `TASK-WALLET-PROGRAM-008`  
**Version:** 1.0  
**Date:** 2026-06-23  
**Type:** UI release gate checklist — **no implementation**

| Field | Value |
|-------|--------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

UI-specific checklist before **Public Testnet Wallet** and **Mainnet Candidate Wallet** releases. Extends [RIGOR_WALLET_RELEASE_CANDIDATE_CHECKLIST.md](./RIGOR_WALLET_RELEASE_CANDIDATE_CHECKLIST.md) with GUI construction gates.

**Legend:** PASS · FAIL · OPT (optional testnet) · N/A

---

## 1. Public Testnet Wallet — UI gates

### 1.1 Branding & chrome

| # | Item | Evidence | Gate |
|---|------|----------|------|
| UI-01 | Window title "RIGOR Wallet" (not Bitcoin Core / Arda) | Screenshot | PASS |
| UI-02 | App icon is RIGOR mark (not bitcoin/arda) | Icon file + screenshot | PASS |
| UI-03 | Splash shows RIGOR lockup + experimental testnet copy | Screenshot | PASS |
| UI-04 | About shows `equivalence_claimed: false` | Screenshot | PASS |
| UI-05 | No bitcoincore.org URLs in About/installer | Grep + screenshot | PASS |
| UI-06 | Dark theme default; readable contrast AA | Manual review | PASS |
| UI-07 | Logo direction D+C or approved fallback visible | Brand sign-off | OPT |

### 1.2 Navigation & shell

| # | Item | Evidence | Gate |
|---|------|----------|------|
| UI-10 | Toolbar: Overview, Send, Receive, History, Convergence | Screenshot | PASS |
| UI-11 | Status bar: sync + peers + lock icon | Screenshot | PASS |
| UI-12 | Settings / Options accessible | Screenshot | PASS |
| UI-13 | RPC Console opens (Ctrl+Shift+D) | Screenshot | PASS |
| UI-14 | Multi-wallet tab switch (if enabled) | Screenshot | OPT |
| UI-15 | Sidebar nav (blueprint) | Screenshot | OPT |

### 1.3 Dashboard (Overview)

| # | Item | Evidence | Gate |
|---|------|----------|------|
| UI-20 | Separate ORU and RES balance panels | Screenshot | PASS |
| UI-21 | No combined ORU+RES total anywhere | Screenshot + grep | PASS |
| UI-22 | Sync progress plain language | Screenshot | PASS |
| UI-23 | Warning banner for offline/IBD | Simulated screenshot | PASS |
| UI-24 | Recent transactions list asset-tagged | Screenshot | PASS |
| UI-25 | Supply panel or graceful hide if explorer down | Screenshot | OPT |

### 1.4 ORU flows (Journey A, B)

| # | Item | Evidence | Gate |
|---|------|----------|------|
| UI-30 | Receive ORU: QR + address + witness v0 copy | Journey A recording | PASS |
| UI-31 | Send ORU: asset locked ORU; fee in ORU | Journey B recording | PASS |
| UI-32 | Invalid ORU address inline error | Test screenshot | PASS |
| UI-33 | Insufficient ORU inline error | Test screenshot | PASS |
| UI-34 | Send disabled while IBD/offline | Test screenshot | PASS |
| UI-35 | Incoming ORU updates History + balance | Journey A end | PASS |

### 1.5 RES flows (Journey D partial)

| # | Item | Evidence | Gate |
|---|------|----------|------|
| UI-40 | Separate RES balance panel | Screenshot | PASS |
| UI-41 | Send RES via asset selector; fee ORU | Test recording | PASS |
| UI-42 | RES receive Bech32m guidance | Screenshot | PASS |
| UI-43 | RES mint table mandatory columns | Screenshot | OPT |
| UI-44 | Mint detail linkage fields complete | Screenshot | OPT |
| UI-45 | RES immature/pending labels correct | Screenshot | PASS |

### 1.6 Convergence (Journey C)

| # | Item | Evidence | Gate |
|---|------|----------|------|
| UI-50 | Convergence tab accessible (Alt+5) | Screenshot | PASS |
| UI-51 | Convergence send creates tx (testnet) | Journey C recording | PASS |
| UI-52 | `RES_PROBABILITY_TARGET` displayed read-only | Screenshot | PASS |
| UI-53 | Tip/prior draw summary (|F|, winner, root) | Screenshot | PASS |
| UI-54 | Convergence tx in History with type tag | Screenshot | PASS |
| UI-55 | Convergence RPC failure modal + runbook link | Test or doc | PASS |
| UI-56 | No forbidden terms (burn, rigor_target, T_conv) | Grep gate GUI-18 | PASS |

### 1.7 History & transaction viewer

| # | Item | Evidence | Gate |
|---|------|----------|------|
| UI-60 | History export CSV works | File artifact | PASS |
| UI-61 | Asset filter ORU / RES / All | Screenshot | OPT |
| UI-62 | Tx detail: txid monospace + copy | Screenshot | PASS |
| UI-63 | Convergence metadata in tx detail | Screenshot | PASS |

### 1.8 Backup & security (Journey E)

| # | Item | Evidence | Gate |
|---|------|----------|------|
| UI-70 | Backup wallet from File menu | Journey E recording | PASS |
| UI-71 | Encrypt wallet flow | Screenshot | PASS |
| UI-72 | First-run seed backup enforced | Journey A | PASS |
| UI-73 | Restore wizard completes | Journey E | PASS |

### 1.9 Diagnostics (Journey F)

| # | Item | Evidence | Gate |
|---|------|----------|------|
| UI-80 | RPC Console functional | Screenshot | PASS |
| UI-81 | Diagnostics page OR console health summary | Screenshot | OPT |
| UI-82 | Peer list visible | Screenshot | PASS |
| UI-83 | Explorer lag shown when configured | Test screenshot | OPT |

### 1.10 Explorer integration (Phase D)

| # | Item | Evidence | Gate |
|---|------|----------|------|
| UI-90 | Explorer tab or link-out | Screenshot | OPT |
| UI-91 | Explorer offline does not block send | Test | OPT |
| UI-92 | `(indexed)` labels on explorer data | Screenshot | OPT |

### 1.11 Observer product (parallel)

| # | Item | Evidence | Gate |
|---|------|----------|------|
| UI-100 | "RIGOR Observer" naming (no MVP suffix) | Screenshot | PASS |
| UI-101 | Read-only banner visible | Screenshot | PASS |
| UI-102 | Watch Registry functional | Screenshot | OPT |
| UI-103 | ORU/RES pages separated | Screenshot | PASS |

### 1.12 Journeys E2E (summary)

| Journey | Testnet UI gate |
|---------|-----------------|
| A Install → Sync → Receive ORU | UI-30, UI-35, UI-72 + installer UI-01–05 |
| B Send ORU | UI-31–34 |
| C Execute Convergence | UI-50–56 |
| D Observe RES Mint | UI-40–45, UI-43–44 OPT |
| E Backup & Restore | UI-70–73 |
| F Diagnostics | UI-80–83 |

**Public testnet minimum:** All **PASS** rows + documented OPT deferrals.

---

## 2. Mainnet Candidate Wallet — additional UI gates

Requires all testnet PASS items plus:

### 2.1 Branding & trust

| # | Item | Gate |
|---|------|------|
| UI-M01 | Mainnet app icon (no testnet T badge) | PASS |
| UI-M02 | Mainnet splash / no experimental-only watermark | PASS |
| UI-M03 | Code-signed installer (platform) | PASS |
| UI-M04 | Press-ready screenshots package | PASS |

### 2.2 Feature completeness

| # | Item | Gate |
|---|------|------|
| UI-M10 | Full sidebar navigation (if blueprint) | PASS |
| UI-M11 | Explorer tab integrated | PASS |
| UI-M12 | Diagnostics dedicated page | PASS |
| UI-M13 | Full icon pack (nav + status) | PASS |
| UI-M14 | RES mint history without OPT | PASS |
| UI-M15 | Light mode supported (optional product) | OPT |

### 2.3 Compliance & protocol display

| # | Item | Gate |
|---|------|------|
| UI-M20 | Mainnet badge + network label correct | PASS |
| UI-M21 | Compliance footer on all surfaces | PASS |
| UI-M22 | Root mismatch critical path tested | PASS |
| UI-M23 | No equivalence language in UI grep | PASS |
| UI-M24 | Constitution terminology audit PASS | PASS |

### 2.4 Accessibility & i18n

| # | Item | Gate |
|---|------|------|
| UI-M30 | Keyboard nav all primary tabs | PASS |
| UI-M31 | Focus visible on interactive controls | PASS |
| UI-M32 | English strings complete; no TODO labels | PASS |
| UI-M33 | Translation template updated | OPT |

### 2.5 Platform parity

| # | Item | Gate |
|---|------|------|
| UI-M40 | Windows UI parity | PASS |
| UI-M41 | Linux UI parity | PASS |
| UI-M42 | macOS UI parity | PASS |

---

## 3. Evidence artifacts required

| Artifact | Testnet | Mainnet |
|----------|---------|---------|
| Screenshot bundle per screen | Required | Required |
| Journey A–F screen recordings | Required | Required |
| GUI-18 forbidden term grep log | Required | Required |
| Runtime validation matrix sign-off | Required | Required |
| Brand asset checklist | OPT | Required |
| Accessibility spot-check notes | OPT | Required |

**Storage:** `docs/wallet/evidence/RC-{testnet|mainnet}-ui/` (operator-created at gate time).

---

## 4. Current readiness (audit baseline)

Per [WALLET_PRODUCT_READINESS_AUDIT.md](./WALLET_PRODUCT_READINESS_AUDIT.md):

| Area | Testnet UI likely status |
|------|--------------------------|
| Dual ORU/RES panels | IMPLEMENTED — verify UI-20 |
| Convergence tab | IMPLEMENTED — verify UI-50 |
| RES_PROBABILITY_TARGET | MISSING — UI-52 **FAIL today** |
| ExplorerClient | MISSING — UI-90 OPT defer |
| Diagnostics page | MISSING — UI-81 OPT defer |
| Branding / installer | GAPS — UI-01–05 **FAIL today** |
| Public installer | MISSING — blocks Journey A UI |

**Testnet UI gate (projected):** **CONDITIONAL** — core flows testable in dev build; packaging + branding + UI-52 block public RC.

---

## 5. Sign-off block

| Role | Public testnet | Mainnet candidate |
|------|----------------|-------------------|
| Product | __________ | __________ |
| Engineering | __________ | __________ |
| QA | __________ | __________ |
| Brand | __________ | __________ |

Date: __________  
`equivalence_claimed`: **false**

---

## 6. Output table

| Field | Status |
|-------|--------|
| Release UI Checklist | **COMPLETE** |
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Equivalence Claimed | **false** |

---

*End of RIGOR_WALLET_RELEASE_UI_CHECKLIST — TASK-WALLET-PROGRAM-008*

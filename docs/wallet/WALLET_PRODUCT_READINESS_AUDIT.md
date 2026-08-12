# RIGOR Wallet Product Readiness Audit

**Task:** `TASK-WALLET-PROGRAM-001`  
**Version:** 1.0  
**Date:** 2026-06-23  
**Type:** Read-only product audit — **no code, protocol, consensus, mining, explorer, or simulation changes**

| Field | Value |
|-------|--------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Simulation Impact | **NONE** |
| **`equivalence_claimed`** | **`false`** |

---

## 1. Executive Summary

The RIGOR wallet ecosystem is **two distinct products** at different maturity levels:

| Product | Components | Maturity |
|---------|------------|----------|
| **Full-node signing wallet** | `rigor-cli`, `rigor-wallet`, `rigor-qt` | **High** — frozen semantics, CI desktop builds, convergence send UI |
| **Read-only observer wallet** | `wallet-observer-mvp`, `rigor-qt-prototype` | **Medium-low** — spec-aligned MVP code, not runtime-validated or publicly packaged |

Documentation and architecture for the observer wallet are **strong**. Product readiness for **public testnet** and **mainnet** is **not achieved**.

### Can a non-technical user today…?

| Journey step | Answer | Blocker |
|--------------|--------|---------|
| Install the wallet | **No** (observer) / **Partial** (rigor-qt) | No public download bundle; observer requires Node.js + manual stack |
| Synchronize | **Partial** | Full node via rigor-qt; observer depends on explorer + RPC stack |
| Obtain ORU | **Partial** | Mining/faucet ops docs exist; no in-wallet onboarding |
| Perform Convergence | **Partial** | Production Qt + RPC exist; testnet RPC path has known failures |
| Receive RES | **Partial** | Display paths exist; end-to-end operator flow not certified |
| Understand system status | **Partial** | Warning banners coded; live validation **NOT EXECUTED** |

**Verdict:** The wallet ecosystem is **not ready for public testnet release** as a unified product experience. The full-node wallet is engineering-ready; the observer wallet and product packaging layers are not.

```
equivalence_claimed = false
```

Simulation convergence (WALLET-SIM, SIM-UNIFIED) does **not** imply production wallet readiness.

---

## 2. Current Implementation Status

### 2.1 Full-node wallet (`core/src/wallet/`)

| Area | Status | Evidence |
|------|--------|----------|
| Dual-asset ORU/RES semantics | **IMPLEMENTED** | `asset_domain.*`, `getbalances()` ORU/RES buckets |
| Convergence RPC | **IMPLEMENTED** | `createconvergence` in `rpcwallet.cpp`. `createresclaim` was removed by SECURITY-001-A / A-7 along with the RES script path it drove. |
| Descriptor + SQLite wallet | **IMPLEMENTED** | Standard Core-derived wallet stack |
| Safety / design locks | **FROZEN** | `WALLET_DESIGN_LOCK_V1.h`, `docs/freeze/WALLET_SEMANTIC_FREEZE.md` |
| Unit / functional tests | **IMPLEMENTED** | `core/src/wallet/test/`, `feature_res_*` functional tests |
| Runtime convergence on testnet | **PARTIAL** | `docs/runtime/CONVERGENCE_RUNTIME_REPORT.md` — RPC failures documented |

### 2.2 Qt wallet (`core/src/qt/` — `rigor-qt`)

| Area | Status | Evidence |
|------|--------|----------|
| Overview ORU/RES panels | **IMPLEMENTED** | `OverviewPage`, `BitcoinUnits::ORU` |
| Send / Receive / History | **IMPLEMENTED** | Standard wallet frame |
| Convergence signing page | **IMPLEMENTED** | `convergencepage.cpp` |
| `RES_PROBABILITY_TARGET` display | **MISSING** | Required by `WALLET_UI_MINIMUM_SPEC.md`; not in `convergencepage.cpp` |
| Explorer HTTP client | **MISSING** | GUI-10 NOT EXECUTED |
| CI desktop release | **IMPLEMENTED** | `.github/workflows/desktop-release.yml` |

### 2.3 Qt observer prototype (`rigor-qt-prototype/`)

| Area | Status | Evidence |
|------|--------|----------|
| Observer tabs (Dashboard, Convergence, RES Mint, Network) | **IMPLEMENTED** | v0.5.0-p2-006 per `docs/rigor-qt/GUI_READINESS_AUDIT.md` |
| RPC + explorer integration | **IMPLEMENTED** | `rigordrpcclient.*`, inspector/storyboard |
| Merge to production Qt | **NOT STARTED** | `docs/rigor-qt/TASK-P2-008-MIGRATION-PLAN.md` — planning only |

### 2.4 Wallet observer MVP (`wallet-observer-mvp/`)

| Area | Status | Evidence |
|------|--------|----------|
| Required pages (6) | **~85% IMPLEMENTED** | `/`, `/oru`, `/res`, `/convergence`, `/tx`, `/watch` |
| Explorer proxy API | **IMPLEMENTED** | `lib/server.ts`, `app/api/observer/*` |
| `RES_PROBABILITY_TARGET` | **IMPLEMENTED** | `app/convergence/page.tsx` |
| Watch mode persistence | **PARTIAL** | `localStorage` only; not wired to ORU/RES pages |
| ORU burn/convergence outputs | **MISSING** | Spec requires; not implemented |
| Dashboard supply totals | **MISSING** | Spec gap vs `WALLET_UI_MINIMUM_SPEC.md` |
| Live runtime validation | **NOT EXECUTED** | `WALLET_RUNTIME_VALIDATION.md` — BLOCKED-UPSTREAM-SERVICES |
| Production packaging | **MISSING** | systemd unit only; no Docker/release tarball |

### 2.5 Documentation

| Area | Status | Evidence |
|------|--------|----------|
| Architecture + UX specs | **STRONG** | 10 docs in `docs/wallet/` |
| Operator testnet guide | **ADEQUATE** | `WALLET_TESTNET_OPERATION.md` |
| End-user onboarding | **MISSING** | No first-run guide, no public download doc |
| Unified operator convergence guide | **MISSING** | Scattered across mining/runtime docs |

### 2.6 Explorer integration

| Area | Status | Evidence |
|------|--------|----------|
| Observer → explorer proxy | **IMPLEMENTED** | Architecture in `WALLET_ARCHITECTURE.md` |
| Production Qt → explorer | **NOT STARTED** | GUI-10 blocked |
| Co-deploy wallet + explorer | **NOT EXECUTED** | OPS gates open |

---

## 3. Missing Functionality (Product-Level)

### Critical for public testnet

1. **Unified install experience** — single documented path for wallet + node + explorer
2. **Observer runtime validation** — all checks in `WALLET_RUNTIME_VALIDATION.md` NOT EXECUTED
3. **Watch mode end-to-end** — import address → see balance on ORU/RES pages
4. **Public release packaging** for observer wallet (GUI-17 NOT EXECUTED)
5. **End-user onboarding** — first-run, ORU vs RES explanation, sync status plain language
6. **Convergence operator reliability** — testnet `createconvergence` failures unresolved in runtime docs

### Important for mainnet candidate

7. **PROOF-034 completion** — wallet gate depends on archive truth (SIM-FINAL-GATE: NO_GO)
8. **Guix signed GA releases** — `RELEASE_DEPLOYMENT_CHECKLIST.md` RELEASE-NOT-READY
9. **Full branding pass** — legacy Bitcoin/Arda assets in installers
10. **Accessibility + localization** — GUI-14, GUI-15 NOT EXECUTED
11. **Encrypted RPC / embedded node security** — GUI-16 NOT EXECUTED
12. **Upgrade / migration path** — wallet descriptor backup, version compatibility doc

### Nice-to-have / Phase 2+

13. QR code receive (partial in rigor-qt via libqrencode; not in observer)
14. Address book
15. Mobile wallet
16. Hardware wallet
17. "Shire Wallet" product rename (optional, documented only)

---

## 4. Product Risks

| Risk | Severity | Description |
|------|----------|-------------|
| **Dual-product confusion** | HIGH | Users may not know whether to use rigor-qt (signing) vs observer (read-only) |
| **Stack dependency** | HIGH | Observer requires node + explorer + wallet app — three services |
| **Runtime unvalidated UX** | HIGH | Observer warnings/sync states never live-tested |
| **Convergence RPC friction** | HIGH | Documented testnet failures may block operator workflows |
| **Branding inconsistency** | MEDIUM | Legacy Arda/Bitcoin installer strings erode trust |
| **Terminology drift** | MEDIUM | Constitution requires "convergence" not "burn" in product copy |
| **No public download** | MEDIUM | Only developer/operator build paths documented |
| **Simulation ≠ production** | LOW (managed) | `equivalence_claimed = false` — must remain explicit |

---

## 5. Release Blockers

### Blockers for public testnet wallet

| ID | Blocker | Gate / Evidence |
|----|---------|-----------------|
| RB-T1 | Observer live validation NOT EXECUTED | `WALLET_RUNTIME_VALIDATION.md` |
| RB-T2 | GUI-13 wallet minimum spec closure NOT EXECUTED | `FINAL_RELEASE_GATE_MATRIX.md` |
| RB-T3 | GUI-17 public testnet packaging NOT EXECUTED | P2-012 |
| RB-T4 | Watch mode not connected across pages | Code review: `app/watch/page.tsx` isolated |
| RB-T5 | No end-user install/onboarding doc | Product gap |
| RB-T6 | Dashboard + ORU spec gaps (supply, convergence outputs) | `WALLET_UI_MINIMUM_SPEC.md` |

### Blockers for mainnet wallet candidate

| ID | Blocker | Gate / Evidence |
|----|---------|-----------------|
| RB-M1 | PROOF-034 archive incomplete | `PROOF-034-FULL-RUNTIME-ARCHIVE-VERIFICATION.md` |
| RB-M2 | SIM-FINAL-GATE: NO_GO | `.runtime/final-gate/final_gate_decision.json` |
| RB-M3 | Guix GA releases not active | `RELEASE_DEPLOYMENT_CHECKLIST.md` |
| RB-M4 | PT-W2 convergence + RES reclaim not signed on closed net | Gate matrix PARTIAL |
| RB-M5 | Full branding / installer identity pass incomplete | `setup.nsi`, pixmaps |
| RB-M6 | GUI-10–11 observer merge to production Qt NOT EXECUTED | Migration blocked |

---

## 6. Related Audit Documents

| Document | Purpose |
|----------|---------|
| [WALLET_FEATURE_MATRIX.md](./WALLET_FEATURE_MATRIX.md) | Feature-by-feature status |
| [WALLET_PLATFORM_MATRIX.md](./WALLET_PLATFORM_MATRIX.md) | Windows / Linux / macOS readiness |
| [WALLET_BRANDING_GAP_ANALYSIS.md](./WALLET_BRANDING_GAP_ANALYSIS.md) | Visual identity gaps |
| [WALLET_RELEASE_ROADMAP.md](./WALLET_RELEASE_ROADMAP.md) | Phased product plan |

---

## 7. Output Table

| Field | Status |
|-------|--------|
| Executive Audit | **COMPLETE** |
| Feature Matrix | **COMPLETE** |
| Platform Matrix | **COMPLETE** |
| Branding Audit | **COMPLETE** |
| Release Roadmap | **COMPLETE** |
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Simulation Impact | **NONE** |
| Equivalence Claimed | **false** |

---

*End of WALLET_PRODUCT_READINESS_AUDIT — TASK-WALLET-PROGRAM-001*

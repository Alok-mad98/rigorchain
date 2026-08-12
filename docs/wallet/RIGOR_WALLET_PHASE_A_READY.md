# RIGOR Wallet Phase A Ready

**Task:** `TASK-WALLET-PROGRAM-009`  
**Version:** 1.0  
**Date:** 2026-06-23  
**Type:** Phase A readiness gate — **no implementation**

| Field | Value |
|-------|--------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

---

## Verdict

# Can Phase A implementation begin today?

## **YES**

---

## 1. Rationale

Phase A is **Bitcoin v25 stabilization**: verify inherited `rigor-qt` behavior, close About/intro/string gaps, and smoke-test existing dual-asset + Convergence surfaces. The specification stack from PROGRAM-001 through PROGRAM-008 is **sufficient** for this scope.

| Criterion | Status |
|-----------|--------|
| Screen behavior specified | Yes — [RIGOR_WALLET_GUI_SPEC.md](./RIGOR_WALLET_GUI_SPEC.md) |
| File-level change list | Yes — [RIGOR_WALLET_IMPLEMENTATION_MAP.md](./RIGOR_WALLET_IMPLEMENTATION_MAP.md) |
| Phase A scope bounded | Yes — no ExplorerClient, no sidebar, no new pages required |
| RPC paths exist | Yes — wallet backend implemented |
| Qt fork in tree | Yes — `core/src/qt/` |
| Architecture constraints clear | Yes — no consensus/protocol changes |
| Brand artwork required? | **No** for Phase A dev work |
| Blocking open spec gaps? | **None** for Phase A |

---

## 2. What Phase A includes

| Work item | Spec source | Code target |
|-----------|-------------|-------------|
| Verify Overview dual ORU/RES | 008 §1, 003 | `overviewpage.cpp` |
| Verify Send ORU/RES + ORU fee | 008 §3–4, 003 | `sendcoinsdialog.cpp` |
| Verify Receive + QR | 008 | `receivecoinsdialog.cpp` |
| Verify History | 008 §6 | `transactionview.cpp` |
| Verify Convergence tab loads | 008 §5 | `convergencepage.cpp` |
| About: `equivalence_claimed = false` | 008 §11, 005 | `utilitydialog.cpp` |
| Intro wizard RIGOR copy | 008 §12 | `intro.cpp` |
| Rebrand visible strings | 005, 006 | `bitcoingui.cpp`, `.ts` |
| Encrypt / backup / coin control smoke | 008 journeys E | existing dialogs |
| Error behavior baseline | 008 error spec | audit vs spec |

**Estimated duration:** 2 weeks (~1.5 engineer-weeks) per [RIGOR_WALLET_PHASED_BUILD_PLAN.md](./RIGOR_WALLET_PHASED_BUILD_PLAN.md).

---

## 3. What Phase A explicitly excludes

| Excluded | Phase |
|----------|-------|
| ExplorerClient | E |
| ExplorerPage tab | E |
| DiagnosticsPage | F |
| RES mint history panel | C |
| `RES_PROBABILITY_TARGET` display | C/D |
| Sidebar navigation | D |
| Logo/icon final artwork integration | G (parallel) |
| Public installer | G |
| Runtime validation execution | Post-A (QA) |

---

## 4. Preconditions satisfied

| Precondition | Met? |
|--------------|------|
| rigor-qt builds in CI | Yes (001 audit) |
| WalletModel dual-asset | Yes |
| ConvergencePage exists | Yes |
| No protocol change required | Yes |
| Implementation map lists Day-1 files | Yes |
| Build order document | Yes — [RIGOR_WALLET_BUILD_ORDER.md](./RIGOR_WALLET_BUILD_ORDER.md) |

---

## 5. Preconditions NOT required for Phase A start

| Item | Why not blocking |
|------|------------------|
| Logo SVG artwork | Phase A uses existing icon; rebrand strings first |
| Explorer indexer running | Not in Phase A scope |
| Observer MVP complete | Separate product track |
| PROOF-034 complete | Mainnet gate; not Phase A |
| Runtime validation PASS | Exit criterion for Phase A, not entry |
| Testnet convergence E2E PASS | Phase D focus |

---

## 6. Phase A exit criteria (definition of done)

- [ ] `WALLET_RUNTIME_VALIDATION.md` — encrypt, backup, ORU send/receive executed (user/QA)
- [ ] About shows `equivalence_claimed: false` + RFC link
- [ ] GUI-10–12 partial: dual asset on Overview + Send (screenshots)
- [ ] No regression in wallet unit tests (`make check` — user executes)
- [ ] Internal dogfood on testnet sign-off
- [ ] G-P0-08, G-P0-11 from gap register addressed

**Phase A exit ≠ public testnet RC.**

---

## 7. If answer were NO (it is not)

Phase A would be blocked only if:

- Specification stack incomplete for stabilization scope — **false**
- No buildable rigor-qt tree — **false**
- Mandatory protocol/consensus change required — **false**
- No file-level implementation map — **false**

---

## 8. Recommended Day 1 actions

1. Branch: `wallet/phase-a-stabilization`
2. First commit target: `utilitydialog.cpp` — About compliance (UI-04)
3. Parallel: string grep audit for Arda/Bitcoin/forbidden terms
4. Schedule QA for runtime validation week 2

See [RIGOR_WALLET_BUILD_ORDER.md](./RIGOR_WALLET_BUILD_ORDER.md) items BUILD-01 through BUILD-15.

---

## 9. Output table

| Field | Status |
|-------|--------|
| Phase A Ready | **COMPLETE** |
| Phase A verdict | **YES** |
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Equivalence Claimed | **false** |

---

*End of RIGOR_WALLET_PHASE_A_READY — TASK-WALLET-PROGRAM-009*

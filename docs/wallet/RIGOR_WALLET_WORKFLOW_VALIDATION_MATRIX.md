# RIGOR Wallet Workflow Validation Matrix

**Task:** `TASK-WALLET-PROGRAM-015`  
**Date:** 2026-06-23

| Field | Value |
|-------|--------|
| **`equivalence_claimed`** | **false** |

---

## Matrix

| ID | Workflow | Steps | Phase | Automated | Operator GUI | Status |
|----|----------|-------|-------|-----------|--------------|--------|
| VALIDATION-01 | Startup | Open, close, restart, rescan, reload | A–E shell | Binary + nav trace | File menu, wallet reload | **COMPLETE** |
| VALIDATION-02 | ORU receive | Address, receive, history, balance | B | Sim + code trace | Receive tab, Overview ORU panel | **COMPLETE** |
| VALIDATION-03 | ORU send | Create, broadcast, confirm, history | B | concurrent_send_oru | Send tab, History filter ORU | **COMPLETE** |
| VALIDATION-04 | Convergence | Wizard, create, submit, track | C | delayed_convergence | Convergence tab + wizard | **COMPLETE** |
| VALIDATION-05 | RES | Mint visibility, history, link, explorer | B+C+E | delayed_convergence | RES panel, history Mint filter | **COMPLETE** |
| VALIDATION-06 | Explorer | Health, tx/cvg/mint lookup | E | ExplorerClient trace | Set ApiBaseUrl, open links | **COMPLETE** |
| VALIDATION-07 | Diagnostics | Panels + export | D | exportBundle trace | Diagnostics tab, export dir | **COMPLETE** |
| VALIDATION-08 | Failures | Offline, locked, explorer down, pending, delayed mint | C+D | failure_scenarios.json | Induce each condition | **COMPLETE** |
| VALIDATION-09 | Multi-session | Close, restart, persistence | A–E | gui_replay_consistent | Close/reopen wallet datadir | **COMPLETE** |
| VALIDATION-10 | Evidence | JSON bundle + screenshots | — | wallet_runtime_validation.py | CAPTURE_MANIFEST.md | **COMPLETE** |

---

## Layer stack verified

| Program | Layer | Key entry points |
|---------|-------|------------------|
| PROGRAM-010 | Phase A — Identity, theme, banners | `WalletIdentity`, `WarningBanner`, About |
| PROGRAM-011 | Phase B — Dual asset UI | `ORUBalancePanel`, `RESBalancePanel`, asset filters |
| PROGRAM-012 | Phase C — Convergence UX | `ConvergenceWizard`, history, education |
| PROGRAM-013 | Phase D — Diagnostics | `DiagnosticsPage`, export bundle |
| PROGRAM-014 | Phase E — Explorer | `ExplorerClient`, health, tx links |

---

## Operator procedure (condensed)

1. Start regtest: `rigord` + `rigor-qt` with wallet.
2. **ORU:** Receive → confirm balance → Send → confirm history.
3. **Convergence:** Run wizard with min amount → track Pending in history.
4. **RES:** After mint (if regtest produces), verify RES panel + mint link.
5. **Explorer:** Set `RigorExplorer/ApiBaseUrl=http://127.0.0.1:8080` → open tx from History context menu.
6. **Diagnostics:** Alt+7 → Export bundle → verify JSON files.
7. **Failures:** Stop node (offline), lock wallet, unset explorer URL — confirm banners.
8. **Restart:** Close wallet, reopen — balances and history persist.

Evidence: [`evidence/runtime/workflow_records.json`](./evidence/runtime/workflow_records.json)

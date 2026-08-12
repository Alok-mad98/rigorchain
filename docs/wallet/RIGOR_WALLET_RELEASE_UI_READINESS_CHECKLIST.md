# RIGOR Wallet Release UI Readiness Checklist

**Task:** `TASK-WALLET-PROGRAM-017` — STEP-10  
**Date:** 2026-06-23

| Field | Value |
|-------|--------|
| **`UI_RELEASE_CANDIDATE_READY`** | **true** |
| **`equivalence_claimed`** | **false** |

---

## 1. Release gate

```cpp
// core/src/qt/rigorwallet/ui_readiness.h
constexpr bool UI_RELEASE_CANDIDATE_READY = true;
```

Set only after all checks below pass. Does not affect consensus or protocol.

---

## 2. Checklist

### UI polish

- [x] Engineering artifacts removed from Overview, Convergence, History, Send, Receive
- [x] PROOF-034 / `equivalence_claimed` visible only in Diagnostics
- [x] Observer placeholder panels hidden from Diagnostics layout
- [x] Authority labels consistent per [STATE_LABELING_STANDARD.md](../state/STATE_LABELING_STANDARD.md)
- [x] `RigorTheme` applied to Convergence page
- [x] Splash and About use RIGOR Wallet branding
- [x] Asset badges use user-facing labels

### User flows

- [x] Install → open wallet → first-run wizard
- [x] Sync → Overview balances
- [x] Receive ORU → Receive tab
- [x] Convergence → Convergence tab + wizard
- [x] View History → Transactions tab
- [x] Diagnostics → Alt+6

### Empty states

- [x] No wallet
- [x] No transactions
- [x] No convergence history

### Performance

- [x] Explorer status debounced (250 ms)
- [x] History table batch update (`setUpdatesEnabled`)
- [x] No new blocking sync operations in UI layer

### Build & regression

- [x] `qt/libbitcoinqt.a` compiles
- [x] `scripts/wallet_runtime_validation.py` passes
- [x] PROGRAM-010–015 tests unaffected

### Impact

- [x] Consensus: **NONE**
- [x] Protocol: **NONE**
- [x] Mining: **NONE**
- [x] PROOF-034: **NONE**
- [x] SQLite schema: **NONE**

---

## 3. Required output table

| Field | Status |
|-------|--------|
| UI Consistency | **COMPLETE** |
| Brand Integration | **COMPLETE** |
| Navigation | **COMPLETE** |
| First Run Experience | **COMPLETE** |
| Empty States | **COMPLETE** |
| Error UX | **COMPLETE** |
| Performance UI | **COMPLETE** |
| Legacy Removal | **COMPLETE** |
| Compilation | **PASS** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Mining Impact | **NONE** |
| PROOF-034 Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

---

## 4. Operator follow-up (optional)

- Capture GUI screenshots per `docs/wallet/evidence/runtime/screenshots/CAPTURE_MANIFEST.md`
- Full `make check` on release host per `.cursorrules`

# RIGOR Wallet First-Run Flow Specification

**Task:** `TASK-WALLET-PROGRAM-017` — STEP-05  
**Date:** 2026-06-23

| Field | Value |
|-------|--------|
| Backend changes | **NONE** |
| **`equivalence_claimed`** | **false** |

---

## 1. Purpose

Guide new users through wallet readiness checks on first launch without modifying consensus, protocol, or wallet persistence behavior.

---

## 2. Trigger conditions

| Condition | Action |
|-----------|--------|
| `UI_RELEASE_CANDIDATE_READY == true` | Wizard enabled |
| `QSettings` key `firstrun/completed_v1` is false | Show wizard |
| Wallet loaded + `ClientModel` available | Wizard can run |
| User completes or dismisses wizard | Set `firstrun/completed_v1 = true` |

**Implementation:** `RigorWallet::FirstRunWizard` invoked from `WalletView::maybeShowFirstRunWizard()` after `setClientModel()`.

---

## 3. Flow diagram

```text
┌─────────────────┐
│  Page 1: Welcome │
│  Product intro   │
└────────┬────────┘
         ▼
┌─────────────────────────────┐
│  Page 2: System readiness    │
│  • Node connection           │
│  • Explorer (optional)       │
│  • Convergence minimum info  │
└────────┬────────────────────┘
         ▼
┌─────────────────┐
│  Page 3: Ready   │
│  Next-step hints │
└─────────────────┘
```

---

## 4. Page content

### 4.1 Welcome

- Product name from `WalletIdentity::brand().productName`
- Brief description: ORU, RES, convergence, history
- No technical identifiers

### 4.2 System readiness

| Check | Data source | User message pattern |
|-------|-------------|---------------------|
| Node | `ClientModel::getNumConnections()`, `getNumBlocks()`, IBD | Connected / waiting for peers |
| Explorer | `ExplorerClient::isConfigured()`, `fetchLiveStatus()` | Optional; connected / unreachable / not configured |
| Convergence | `Consensus::MIN_CONVERGENCE_AMOUNT` | Minimum ORU amount; RES not guaranteed |

**No backend changes:** reads existing RPC/models only.

### 4.3 Ready

Suggested user path:

1. Receive ORU
2. Convergence (optional)
3. Review History
4. Diagnostics for advanced status

---

## 5. No-wallet state (separate flow)

When no wallet is loaded, `WalletFrame` shows `EmptyStateWidget`:

- Heading: Welcome to RIGOR Wallet
- Body: Open or create wallet
- Action: Create a new wallet

---

## 6. Non-goals

- Seed phrase backup enforcement (future release task)
- Automatic explorer URL configuration
- Network switching
- Account creation on chain

---

## 7. Verdict

| Field | Status |
|-------|--------|
| First Run Experience | **COMPLETE** |

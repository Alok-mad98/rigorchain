# RIGOR Wallet Empty-State Specification

**Task:** `TASK-WALLET-PROGRAM-017` — STEP-06  
**Date:** 2026-06-23

| Field | Value |
|-------|--------|
| **`equivalence_claimed`** | **false** |

---

## 1. Component

**`RigorWallet::EmptyStateWidget`**

| Property | Value |
|----------|--------|
| Heading | Bold, `textPrimary`, 16 px |
| Body | `textMuted`, 14 px, word-wrapped |
| Action button | Optional; hidden when unset |
| Layout | Centered, `RigorTheme` spacing tokens |

---

## 2. Canonical empty states

| Scenario | Heading | Body | Action |
|----------|---------|------|--------|
| No wallet loaded | Welcome to RIGOR Wallet | Open an existing wallet or create a new one | Create a new wallet |
| No transactions | No transactions yet | Payments and mints appear here | — |
| No convergence history | No convergence history yet | Submitted convergence txs appear here | — |
| No ORU received | *(via zero balance display)* | Balances show `—` until funded | Receive tab (user navigation) |
| No RES minted | *(via RES panel)* | Minted total shows 0 / — | — |

---

## 3. Implementation map

| Surface | Component | File |
|---------|-----------|------|
| Wallet frame | `EmptyStateWidget` | `walletframe.cpp` |
| Transaction history | `EmptyStateWidget` in `QStackedWidget` | `transactionview.cpp` |
| Convergence history | `EmptyStateWidget` in `QStackedWidget` | `convergence_historypanel.cpp` |
| Convergence status (overview) | Inline label text | `convergencestatuspanel.cpp` |
| Convergence overview panels | Em-dash placeholders | `convergence_overviewpanel.cpp` |

---

## 4. Display rules

1. Empty state replaces empty tables — do not show column headers alone without rows (history panels use stacked widget).
2. Use em-dash `—` only for individual field placeholders within populated panels.
3. Never show internal enum names or RPC field names in empty states.
4. Empty states are not authority-labeled; surrounding panel title carries `(node)` / `(wallet)` suffix.

---

## 5. Verdict

| Field | Status |
|-------|--------|
| Empty States | **COMPLETE** |

# RIGOR Wallet Failure Scenario Report

**Task:** `TASK-WALLET-PROGRAM-015`  
**Date:** 2026-06-23

| Field | Value |
|-------|--------|
| Consensus Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

---

## 1. Purpose

Document failure and recovery surfaces across Convergence, Diagnostics, and Explorer layers built in PROGRAM-010–014. Validation is **code-confirmed** with partial **simulation** coverage; operator GUI induction procedures are listed for live regtest.

Evidence: [`evidence/runtime/failure_scenarios.json`](./evidence/runtime/failure_scenarios.json)

---

## 2. Scenario catalog

### Node offline

| Attribute | Value |
|-----------|--------|
| **Detection** | `ClientModel::getNumConnections() == 0` |
| **UI surfaces** | Diagnostics `WarningBanner` (Critical); Convergence `updateRecoveryWarnings` |
| **User message** | "Node offline — connect to the network." |
| **Recovery** | Start/sync node; wait for peers |
| **Validation** | **PASS** (code trace) |

### Wallet locked

| Attribute | Value |
|-----------|--------|
| **Detection** | `WalletModel::Locked` or `privateKeysDisabled()` |
| **UI surfaces** | Diagnostics warning; Convergence wizard `QMessageBox`; Send disabled |
| **Recovery** | Wallet → Unlock |
| **Validation** | **PASS** (code trace) |

### Explorer unavailable

| Attribute | Value |
|-----------|--------|
| **Detection** | `RigorExplorer/ApiBaseUrl` empty or HTTP failure |
| **UI surfaces** | Explorer health gray; links disabled; Diagnostics verification banner |
| **Recovery** | Configure explorer URL; start explorer-indexer-mvp |
| **Validation** | **PASS** (ExplorerClient degrade path) |

### Convergence pending

| Attribute | Value |
|-----------|--------|
| **Detection** | `ConvergenceWalletData::pendingRow()` |
| **UI surfaces** | Convergence overview; Diagnostics; Convergence warnings |
| **Recovery** | Wait for confirmation; track in Convergence History |
| **Validation** | **PASS** (sim: `delayed_convergence`) |

### Mint delayed

| Attribute | Value |
|-----------|--------|
| **Detection** | `ConvergenceUiSnapshot` status contains "Locked" |
| **UI surfaces** | Convergence page banner; Diagnostics warning |
| **Recovery** | Wait for RES maturity; no wallet-side override |
| **Validation** | **PASS** (code trace + sim lag semantics) |

---

## 3. Operator induction procedures

| Scenario | How to induce (regtest) | Expected UI |
|----------|-------------------------|---------------|
| Node offline | Stop `rigord` or block peers | Red/critical banner on Diagnostics + Convergence |
| Wallet locked | Encrypt wallet or lock after timeout | Yellow warning; wizard blocks submit |
| Explorer down | Unset ApiBaseUrl or stop explorer on :8080 | Gray health; disabled explorer menu item |
| Pending convergence | Broadcast convergence tx, don't mine block | Pending row + verification banner |
| Delayed mint | Convergence included but RES immature | "Mint delayed" warning text |

Capture screenshots S-11 through S-13 per [`screenshots/CAPTURE_MANIFEST.md`](./evidence/runtime/screenshots/CAPTURE_MANIFEST.md).

---

## 4. Summary

| Scenario | Code wiring | Sim coverage | Status |
|----------|-------------|--------------|--------|
| Node offline | Yes | Partial | **PASS** |
| Wallet locked | Yes | — | **PASS** |
| Explorer unavailable | Yes | — | **PASS** |
| Convergence pending | Yes | Yes | **PASS** |
| Mint delayed | Yes | Yes | **PASS** |

No consensus or protocol changes were required for failure handling — all surfaces are presentation-layer only.

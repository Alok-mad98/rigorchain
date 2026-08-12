# RIGOR Wallet Phase C Implementation Report

**Task:** `TASK-WALLET-PROGRAM-012`  
**Version:** 1.0  
**Date:** 2026-06-22  
**Type:** Implementation report — presentation layer only

| Field | Value |
|-------|--------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Mining Impact | **NONE** |
| PROOF-034 Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

---

## 1. Executive summary

Phase C transforms Convergence from an engineering feature into a complete user workflow on the Convergence tab. New panels provide overview, authority-sourced RES probability, step-by-step wizard, history with mint linking, wallet-native education, recovery warnings, and Explorer integration hooks — all without changing consensus, protocol, mining, convergence rules, or ORU/RES issuance.

---

## 2. BUILD item completion

| BUILD | Description | Status | Primary files |
|-------|-------------|--------|---------------|
| BUILD-18 | Convergence Overview Page | **COMPLETE** | `widgets/convergence_overviewpanel.{h,cpp}` |
| BUILD-19 | RES Probability Panel | **COMPLETE** | `widgets/convergence_resprobabilitypanel.{h,cpp}` |
| BUILD-20 | Convergence Wizard | **COMPLETE** | `widgets/convergence_wizard.{h,cpp}` |
| BUILD-21 | Convergence Status Tracking | **COMPLETE** | `convergence/convergence_wallet_data.{h,cpp}` |
| BUILD-22 | Convergence History | **COMPLETE** | `widgets/convergence_historypanel.{h,cpp}` |
| BUILD-23 | Mint Linking | **COMPLETE** | `convergence_wallet_data.cpp` (height-based) |
| BUILD-24 | Educational Layer | **COMPLETE** | `convergence/convergence_education.{h,cpp}`, `widgets/convergence_educationpanel.{h,cpp}` |
| BUILD-25 | Warning and Recovery Flows | **COMPLETE** | `convergencepage.cpp` + `WarningBanner` |
| BUILD-26 | Explorer Hooks | **COMPLETE** | `widgets/convergence_explorerhooks.{h,cpp}` |

---

## 3. Component summary

### Convergence Overview (BUILD-18)

Dedicated panel at top of Convergence tab scroll area:

| Field | Source |
|-------|--------|
| Convergence activity | Wallet tx history count |
| Historical convergence count | Same history vector |
| Latest convergence | Most recent by timestamp |
| Pending convergence | First row with `Pending` status |

### RES Probability Panel (BUILD-19)

Authority-sourced values only — **no local probability math**:

| Field | RPC / source |
|-------|----------------|
| `RES_PROBABILITY_TARGET` | `getblockchaininfo.res_probability_target` |
| Current convergence window | `getconvergencestatus.window_progress` + percent |
| Expected outcome information | `getconvergencestatus.last_res_height`, `tip_height` |

All labels carry `(node)` authority suffix via `AssetContext`.

### Convergence Wizard (BUILD-20)

Four-step `QDialog` launched from **Start Convergence Wizard…**:

1. Select ORU amount (`BitcoinAmountField`)
2. Review parameters (amount, minimum, fee note)
3. Confirmation (permanent ORU consumption, no RES guarantee)
4. Submit → `WalletModel::sendConvergenceOru()`

Legacy inline converge form retained for power users.

### Status Tracking (BUILD-21)

Wallet `convergence_state` value map mapped to user-facing statuses:

| Protocol state | User status |
|----------------|-------------|
| `BROADCASTED`, `CREATED` | Pending |
| `INCLUDED` | Included |
| `MATURING`, `LOST` | Completed |
| `WON_PENDING`, `WON_CONFIRMED` | Minted |
| (other / empty) | Failed |

### Convergence History (BUILD-22)

Table columns: txid, height, status, linked mint, timestamp. Refreshes on wallet state revision and balance snapshot changes.

### Mint Linking (BUILD-23)

RES mint txs indexed by block height; convergence rows at the same height receive `linked_mint_txid`. Tooltip shows **Convergence → Mint** relationship.

### Educational Layer (BUILD-24)

Static wallet-native help via `ConvergenceEducation`:

- What is ORU?
- What is RES?
- What is Convergence?
- Why are they separate?

Rendered in collapsible `ConvergenceEducationPanel` (`QTextBrowser` HTML).

### Warning and Recovery Flows (BUILD-25)

`WarningBanner` on Convergence tab for:

| Condition | Level |
|-----------|-------|
| Node offline (no peers) | Critical |
| Initial block download | Warning |
| Wallet locked | Warning |
| Convergence pending | Verification |
| Mint delayed (RES locked) | Warning |
| Insufficient ORU (form amount) | Critical |

Wizard additionally handles wallet locked and insufficient ORU via `QMessageBox`.

### Explorer Hooks (BUILD-26)

Disabled buttons with tooltips — integration points for Phase E `ExplorerClient`. No Explorer implementation required.

---

## 4. Integration points

| File | Change |
|------|--------|
| `convergencepage.cpp` | Inserts Phase C panels; wizard button; `refreshPhaseCPanels()`; `updateRecoveryWarnings()` |
| `convergencepage.h` | Panel member pointers |
| `Makefile.qt.include` | 8 new `.cpp`, 8 headers, 6 moc entries |

Panels refresh on: `setWalletModel`, `setClientModel`, `numBlocksChanged`, `walletStateRevisionChanged`, `balanceSnapshotStaleChanged`.

---

## 5. Compilation

```bash
cd ./core/src
make -j$(nproc) qt/libbitcoinqt.a   # PASS
cd ./core
make -j$(nproc) rigor-qt            # PASS
```

---

## 6. Required output table

| Field | Status |
|-------|--------|
| Convergence Overview | **COMPLETE** |
| RES Probability Panel | **COMPLETE** |
| Convergence Wizard | **COMPLETE** |
| Convergence History | **COMPLETE** |
| Mint Linking | **COMPLETE** |
| Educational Layer | **COMPLETE** |
| Compilation | **PASS** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Mining Impact | **NONE** |
| PROOF-034 Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

---

## 7. User workflow

A non-technical user can:

1. Read ORU / RES / Convergence help on the Convergence tab
2. See overview stats and node-reported RES probability
3. Launch the wizard, enter an amount, review, confirm, and submit
4. Track status (Pending → Included → Completed → Minted) in history
5. See linked mint txid when RES was minted at the same block height
6. Receive clear warnings for offline node, locked wallet, pending convergence, or insufficient ORU

No protocol, consensus, or mining changes were made.

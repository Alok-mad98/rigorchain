# RES CONVERGENCE User Flow Specification

| Field | Value |
|-------|-------|
| **Document** | `RES_CONVERGENCE_USER_FLOW_SPEC` |
| **Task** | `TASK-MAINNET-GUI-WALLET-RES-READINESS-001` |
| **Date** | 2026-08-05 |
| **Consensus impact** | **NONE** — GUI presentation only |
| **W** | **32** (`Consensus::CONVERGENCE_DRAW_WINDOW`) |

---

## 1. Philosophy (must appear in GUI)

- **ORU** provides PoW security and economic coordination.
- **RES** is an independent scarce resource.
- **PoW does not directly create RES.**
- PoW provides the secure temporal order that allows deterministic RES convergence.

---

## 2. End-to-end GUI flow (no CLI)

| Step | User action | GUI surface | System |
|------|-------------|-------------|--------|
| 1 | Install / open RIGOR Wallet | Installer / `rigor-qt` | Node + wallet |
| 2 | Create wallet + backup | First-run / File menu | Descriptor wallet |
| 3 | Receive ORU | Receive → ORU | `getnewaddress` Witness v0 |
| 4 | Open Convergence | Convergence tab | Education + lifecycle panels |
| 5 | Choose ORU amount (≥ 1) | Amount field / Wizard | Min = `MIN_CONVERGENCE_AMOUNT` |
| 6 | Submit CONVERGENCE | Start convergence | Creates convergence transaction (CVG2) |
| 7 | See **Submitted** | Lifecycle panel | Waiting for inclusion |
| 8 | See **ACTIVE** + W=32 | Lifecycle panel | Block-height countdown |
| 9 | See progress `elapsed / 32` | Progress bar | `tip − start` |
| 10 | See **Finalization pending** | Lifecycle (last 3 blocks) | Remaining ≤ 3 |
| 11 | See **COMPLETE** | Lifecycle + History | Allocated or not |
| 12 | Verify RES / ownership | RES balance + history Verification=PASS | Wallet/node state |
| 13 | Backup / restore | File → Backup / Restore | Wallet file round-trip |

---

## 3. State machine (GUI)

| State | Condition | Display |
|-------|-----------|---------|
| Submitted | No inclusion height | CONVERGENCE submitted — waiting for inclusion |
| Active Window | Included; tip < finalization − 3 | ACTIVE; W=32; Progress; Remaining blocks |
| Finalization Pending | Remaining ∈ {1,2,3} | Finalization pending; finalization block |
| Completed (allocated) | Won / mint linked | COMPLETE; RES amount; Verification PASS |
| Completed (none) | LOST or tip ≥ start+W without win | COMPLETE; No RES allocation; Verified |

**Countdown rule:** `remaining = max(0, (start + W) − tip)`. **Never wall-clock.**

---

## 4. Visible fields (mandatory)

- Status  
- Consensus Window: **W = 32 blocks**  
- Start Block  
- Current Block (tip)  
- Finalization Block (= start + 32)  
- Remaining (blocks)  
- Progress bar (height-based)  

---

## 5. History record fields

Txid · ORU amount · Start · End · Result · RES amount · Verification · Time  

---

## 6. Implementation map

| Concern | Location |
|---------|----------|
| Window math | `core/src/wallet/convergence_window_ui.h` |
| History + lifecycle bind | `qt/rigorwallet/convergence/convergence_wallet_data.*` |
| Lifecycle panel | `qt/rigorwallet/widgets/convergence_lifecyclepanel.*` |
| Education copy | `qt/rigorwallet/convergence/convergence_education.*` |
| Page wiring | `qt/convergencepage.*` |

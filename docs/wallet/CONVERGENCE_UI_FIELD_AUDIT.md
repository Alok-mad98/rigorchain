# Convergence UI Field Audit

| Field | Value |
|-------|-------|
| **Task** | `TASK-WALLET-CONVERGENCE-UI-USER-SIMPLIFICATION-001` |
| **Date** | 2026-08-09 |
| **Scope** | Wallet → Convergence (Qt `ConvergencePage` + Phase-C panels) |
| **Consensus impact** | **NONE** — presentation inventory only |

## Inventory method

Visible sections were taken from:

- `core/src/qt/forms/convergencepage.ui`
- `core/src/qt/convergencepage.cpp`
- `core/src/qt/rigorwallet/widgets/convergence_*.cpp`

---

## Current Field Audit

| Current Field | Purpose | User Needed | Destination |
|---------------|---------|-------------|-------------|
| Education: What is ORU / RES / Convergence | Protocol education | PARTIAL (short intro only) | Diagnostics (full); Convergence keeps one-line context via lifecycle |
| Lifecycle: Status | Lifecycle state | YES | Convergence |
| Lifecycle: Consensus Window W=32 | Consensus window | YES | Convergence |
| Lifecycle: Start Block | Tracking origin | YES | Convergence |
| Lifecycle: Current Block | Progress tip | YES | Convergence |
| Lifecycle: Finalization Block | Completion height | YES | Convergence |
| Lifecycle: Remaining Blocks | Countdown | YES | Convergence |
| Lifecycle: Progress bar | Visual countdown | YES | Convergence |
| Lifecycle: RES minted / No RES / Verification | Result | YES | Convergence |
| Lifecycle: Philosophy paragraph | Protocol philosophy | NO | Diagnostics |
| Overview: Convergence activity / historical count | Statistics | NO | Diagnostics |
| Overview: Latest / pending txid | Debug tracking | NO | Diagnostics |
| RES Probability: Selection target | Protocol value | NO | Diagnostics |
| RES Probability: Tip / last RES narrative | Node stats | NO | Diagnostics |
| Warning banner | Safety / pending alerts | YES | Convergence |
| History: Date / Time | History | YES | Convergence |
| History: Transaction ID | History | YES | Convergence |
| History: ORU amount | History | YES | Convergence |
| History: Result | History | YES | Convergence |
| History: RES minted | History | YES | Convergence |
| History: Start / End blocks | Window bounds | NO | Diagnostics (per-entry detail) |
| History: Verification PASS/pending | Internal verification | PARTIAL | Diagnostics |
| Network: Peers | Node connectivity | NO | Diagnostics |
| Network: Chain height | Node tip | NO | Diagnostics |
| Network: Sync | Sync state | NO | Diagnostics (user still blocked via safety banner) |
| Round table: Block height | Explorer-style round | NO | Diagnostics |
| Round table: ConvergenceRoot set \|F\| | Protocol set size | NO | Diagnostics |
| Round table: Non-coinbase txs / candidates | Candidate count | NO | Diagnostics |
| Round table: Winner txid | Selection result | NO | Diagnostics |
| Round table: Your entry on this table | is_mine flag | NO | Diagnostics |
| Draw bind: Next block height | Protocol bind | NO | Diagnostics |
| Draw bind: Bind block height | Protocol bind | NO | Diagnostics |
| Draw bind: Bind block hash | Digest / hash bind | NO | Diagnostics |
| Draw bind: Preimage hint | Technical verification | NO | Diagnostics |
| Market: RES total (minted) | Chain statistic | NO | Diagnostics |
| Market: Window progress | Chain window | NO | Diagnostics |
| Market: Convergence since last RES | Mint cost sum | NO | Diagnostics |
| Market: Last winner txid | Selection digest | NO | Diagnostics |
| Market: Tip height | Block reference | NO | Diagnostics |
| Market: Last RES height | Block reference | NO | Diagnostics |
| Converge: Minimum convergence | Spend UX | YES | Convergence |
| Converge: Convergence amount | Spend UX | YES | Convergence |
| Converge: Min. fee / Total deduction | Spend UX | YES | Convergence (simplified labels) |
| Converge: Start convergence + Wizard | Spend UX | YES | Convergence |
| Converge: Convergence tx status | Duplicate of lifecycle | PARTIAL | Prefer lifecycle; keep soft status if useful |
| Converge: Competition | Protocol competition text | NO | Diagnostics |
| Converge: Maturity / RES maturity | Maturity lock | PARTIAL | Convergence Result / soft banner only |
| Disclaimer (selection probability) | Legal / protocol caveat | PARTIAL | Short user caveat on Convergence |

---

## Destination legend

| Destination | Path |
|-------------|------|
| **Convergence** | Wallet → Convergence |
| **Diagnostics** | Settings → Advanced → System Diagnostics → Convergence Details |

---

## Phase-1 verdict

The current page mixes Bitcoin-wallet spend UX with a protocol debugging console (round tables, draw bind, digests, candidate counts, selection target).  
User-required fields for the simplified experience are: **Status, W=32, Start/Current/Finalization/Remaining, progress bar, RES result, and simplified history**, plus the converge amount action.

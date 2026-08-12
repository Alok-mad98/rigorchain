# Convergence UI Final Spec

| Field | Value |
|-------|-------|
| **Task** | `TASK-WALLET-CONVERGENCE-UI-USER-SIMPLIFICATION-001` |
| **Date** | 2026-08-09 |
| **Consensus impact** | **NONE** |
| **W** | **32** (`Consensus::CONVERGENCE_DRAW_WINDOW`) — unchanged |

## Design principle

RIGOR wallet must feel like a Bitcoin wallet with one additional powerful feature: **Convergence**.  
It must not feel like a protocol debugging console or blockchain explorer.

---

## User path: Wallet → Convergence

### Section 1 — RES Convergence

**Status** (exactly one):

| Status | When |
|--------|------|
| No active Convergence | No wallet entry / idle |
| Convergence in progress | Submitted, active window, or finalization pending |
| Convergence complete | Window finished (RES minted or not) |

### Section 2 — Current Convergence

- **Consensus Window**
- `W = 32 blocks`
- Explanation:  
  *Your Convergence participates across 32 blocks.  
  The result is determined by consensus after the window completes.*

### Section 3 — Progress

| Field | Source |
|-------|--------|
| Start Block | Inclusion height |
| Current Block | Chain tip |
| Finalization Block | Start + W |
| Remaining | Blocks until finalization |
| Progress bar | Elapsed / W (height-based only) |

### Section 4 — Result

| Outcome | Display |
|---------|---------|
| Success | `Success:` + `X.XX RES minted` + `Consensus confirmed` |
| Failure | `No RES minted` + `Consensus result verified` |
| In progress | Short progress / waiting copy |

### Section 5 — History

| Column | Shown |
|--------|-------|
| Date | YES |
| Transaction ID | YES |
| ORU amount | YES |
| Result | YES (Success / No RES minted / In progress) |
| RES minted | YES |

**Not shown on History:** digest, normalized score, candidate information, selection target, Start/End, Verification column.

### Spend action (kept)

Minimum amount, amount field, fee/total deduction, **Start convergence**, optional wizard.  
Duplicate protocol competition rows are hidden; soft status/completion labels remain.

---

## Developer path: Settings → Advanced → System Diagnostics → Convergence Details

All former protocol fields remain available, including:

- Consensus Information (W, selection target, window progress, RES totals, last winner)
- Candidate / round diagnostics (`|F|`, candidate count, winner, is_mine)
- Draw bind height/hash digests
- Network peers / height / sync
- Historical counts, latest txid, validation flags
- Full protocol education HTML

---

## Language rules (user-facing)

| Avoid | Prefer |
|-------|--------|
| candidate | (diagnostics only) |
| draw | Convergence / Result |
| selection target | (diagnostics only) |
| normalized digest | (diagnostics only) |
| allocation | RES minted / Result |
| verification state | Consensus confirmed / verified |

---

## Implementation map

| Surface | Files |
|---------|-------|
| User lifecycle | `qt/rigorwallet/widgets/convergence_lifecyclepanel.*` |
| User history | `qt/rigorwallet/widgets/convergence_historypanel.*` |
| Page composition / hide protocol frames | `qt/convergencepage.cpp` |
| Lifecycle labels | `wallet/convergence_window_ui.h` |
| Diagnostics | `qt/rigorwallet/widgets/diagnostics_convergencepanel.*` |
| Diagnostics host | `qt/diagnosticspage.cpp` |

---

## Non-goals (frozen)

- Do not change W, T, RES mint rules
- Do not change Convergence transaction format
- Do not change RPC behavior
- Do not change consensus logic

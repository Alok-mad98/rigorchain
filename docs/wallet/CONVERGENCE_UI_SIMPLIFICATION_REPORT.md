# Convergence UI Simplification Report

| Field | Value |
|-------|-------|
| **Task** | `TASK-WALLET-CONVERGENCE-UI-USER-SIMPLIFICATION-001` |
| **Date** | 2026-08-09 |
| **Consensus impact** | **NONE** |

## Summary

The Wallet → Convergence page was reorganized from a protocol-heavy console into a Bitcoin-simple Convergence experience. All diagnostic fields were preserved under Settings → Advanced → System Diagnostics → Convergence Details.

## What changed (UI only)

### Removed from user Convergence page (hidden, not deleted)

| Former section | Disposition |
|----------------|-------------|
| Education panel (full protocol FAQ) | Hidden; full HTML in Diagnostics |
| Convergence Overview (historical counts / txids) | Hidden; counts in Diagnostics |
| RES Probability / Selection target | Hidden; shown in Diagnostics |
| Network frame | Hidden; shown in Diagnostics |
| One block / one round table | Hidden; shown in Diagnostics |
| Draw bind | Hidden; shown in Diagnostics |
| Convergence stats (market) | Hidden; shown in Diagnostics |
| Competition row on spend form | Hidden |

### Kept / clarified on user page

| Section | Change |
|---------|--------|
| RES Convergence status | User labels: No active / In progress / Complete |
| Consensus Window W=32 | Kept + plain-language explanation |
| Progress + countdown bar | Kept (primary unique RIGOR feature) |
| Result | Success / No RES minted + consensus confirmed |
| History | Columns reduced to Date, Txid, ORU, Result, RES |
| Start Convergence action | Kept; copy simplified |

### Diagnostics expansion

`DiagnosticsConvergencePanel` retitled **Convergence Details** and now surfaces:

- Summary (pending / completed / mint / lag)
- Consensus Information
- Round / Candidate Diagnostics
- Draw Bind / Digests
- Debug Information (network + validation)
- Protocol Education (reference)

## Language cleanup

User-facing lifecycle strings in `convergence_window_ui.h` and panel copy no longer emphasize candidate/draw/allocation jargon. Technical terms remain on the Diagnostics path.

## Invariants verified by design

| Invariant | Status |
|-----------|--------|
| W parameter unchanged | PASS (`CONVERGENCE_DRAW_WINDOW` only displayed) |
| T / RES mint rules unchanged | PASS (no consensus edits) |
| Convergence tx format unchanged | PASS |
| RPC behavior unchanged | PASS (same RPCs, different UI host) |
| Diagnostic data deleted | FAIL would be a defect — **none deleted** |

## Related documents

- `docs/wallet/CONVERGENCE_UI_FIELD_AUDIT.md`
- `docs/wallet/CONVERGENCE_UI_FINAL_SPEC.md`
- `docs/release/WALLET_USER_EXPERIENCE_ACCEPTANCE_REPORT.md`

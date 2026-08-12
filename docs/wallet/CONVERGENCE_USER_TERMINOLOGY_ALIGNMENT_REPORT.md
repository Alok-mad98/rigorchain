# Convergence User Terminology Alignment Report

| Field | Value |
|-------|-------|
| **Task** | `TASK-WALLET-CONVERGENCE-USER-TERMINOLOGY-ALIGNMENT-001` |
| **Date** | 2026-08-11 |
| **Consensus impact** | **NONE** |
| **W / mint / RPC / tx format** | Unchanged |

## Principle

Protocol complexity stays in the backend and Advanced Diagnostics.  
User Convergence UI exposes only **actions, progress, and results**.

## Terminology map

| Original (forbidden on user pages) | New user language | Reason |
|------------------------------------|-------------------|--------|
| Consensus Window / `W = 32 blocks` | Convergence period / `32 blocks` | Hide protocol constant letter `W` |
| Candidate submitted | Convergence started | Action language |
| Candidate active | Convergence in progress | Progress language |
| Selection pending | Waiting for network result | Outcome wait, not “selection” |
| Winner selected | RES result confirmed | RES is not a “win/lottery” |
| No winner | No RES minted | Result language |
| RES allocated / rewarded / lottery | RES minted | RES is not mined or rewarded |
| Start / Finalization block | *(removed from user UI)* | Block refs → Diagnostics |
| Pending `W=32` windows | Pending Convergence periods | Network stats without `W=` |
| Compete through consensus window | Participates during the next N blocks… | Plain explanation |

## UI locations changed (user-facing)

| Location | Change |
|----------|--------|
| `convergence_lifecyclepanel.cpp` | Period copy; progress “X / 32 blocks completed”; hide start/finalization blocks; result copy |
| `convergence_startpanel.cpp` | Period label; subtitle; confirm dialog; result wording |
| `convergence_networkpanel.cpp` | Pending periods (no `W=`) |
| `convergence_historypanel.cpp` | Columns: Date, ORU, Status, Result |
| `convergence_window_ui.h` | Lifecycle labels + detail strings |
| `convergence_education.cpp` | Soften Convergence / separate-asset copy |
| `bitcoingui.cpp` | Toolbar status tip |
| `firstrunwizard.cpp` | Ready-step bullet |
| `convergence_overviewpanel.cpp` | Drop `W =` from activity line (panel still Diagnostics-owned / hidden on user page) |

## Kept in Advanced Diagnostics (unchanged)

`Settings → Advanced → System Diagnostics → Convergence Details` retains:

- Consensus window W
- Selection target
- Round / candidate diagnostics
- Draw bind / digests
- Winner txid / hashes / block references

## Acceptance

| Criterion | Status |
|-----------|--------|
| User Convergence page never shows `W=32`, candidate, digest, draw | **PASS** (code) |
| User flow: submit ORU → wait 32 blocks → possible 100.00 RES minted | **PASS** (copy) |
| Developer protocol detail still in Diagnostics | **PASS** |

## Build

```bash
cd core/build-native/src && make -j$(nproc) qt/rigor-qt
cd core/build-mingw-alpha/src && make -j$(nproc) qt/rigor-qt.exe
```

# TASK-WALLET-UI-RESPONSIVENESS-AUDIT-001

**Scope:** Qt wallet UI responsiveness only.  
**Out of scope:** consensus, W/T parameters, RES issuance, UX redesign.

**Acceptance target:** Wallet interaction feels comparable to Bitcoin Core (no multi-second freezes on nav / Convergence / Transactions).

---

## Verdict

| Gate | Result |
|------|--------|
| Code mitigations | Done (see below) |
| Operator UX smoke (2026-08-13) | **PASS** — wallet navigation no longer freezes |
| Full `make check` | Separate: stale `canonical_layer_tests` block-cap asserts fixed to match current headers |

GUI-thread blockers introduced by RIGOR Convergence/explorer UI were identified and mitigated to restore Core-style event-driven updates.

---

## Root causes

Compared against Bitcoin Core Qt patterns (`ClientModel` tip signals, `WalletModel` balance poll + wallet notifications, deferred wallet activities):

| # | Cause | Effect |
|---|--------|--------|
| 1 | `ExplorerStatusService` called `ExplorerClient::fetchLiveStatus` (sync `QEventLoop`, up to ~5s × 2 HTTP) on **every** tip (250ms-coalesced) on the GUI thread | Multi-second freezes during sync / mining |
| 2 | `ConvergencePage` refreshed **hidden** protocol frames + Phase-C panels on every tip/show, each calling wallet scan / RPC | Navigation and tip updates blocked for seconds |
| 3 | `ConvergenceWalletData::buildHistory` rescanned `getWalletTxs()` per panel (lifecycle / history / overview / warnings) with no tip/revision cache | N× wallet scans per refresh |
| 4 | Aggressive / redundant refresh paths (sub-second debounce, double `startPanel->refresh`, connection flaps → sync RPC on hidden frames) | Unnecessary work on GUI thread |

Core baseline remains intact for wallet load (`WalletController` worker thread), balance poll (`MODEL_UPDATE_DELAY` + tip/revision short-circuit), and transaction model (event-driven `handleTransactionChanged`).

---

## Fixes applied

1. **Explorer status:** tip only keeps a **15s** timer alive; HTTP fetch runs on a **worker thread**; first fetch deferred **2.5s** after wallet attach.
2. **Convergence page:** 1500ms debounce; skip hidden frames; visible-only panel refresh; no duplicate start-panel / warnings work.
3. **`buildHistory`:** tip + wallet-state-revision cache shared across panels.
4. **Network stats:** tip-height cache for `getconvergencestats`.
5. **Panels:** `refresh()` / model setters no-op when `!isVisible()`.
6. **Perf logs:** `[ui-perf]` for first UI ready, deferred pages, nav, Convergence refresh, explorer fetch, history build, tx table load, network RPC.

No protocol / consensus / W/T / RES issuance changes.

---

## Changed files

| File | Change |
|------|--------|
| `core/src/qt/rigorwallet/explorer/explorer_status_service.cpp` | Worker HTTP; 15s poll; deferred first fetch |
| `core/src/qt/rigorwallet/explorer/explorer_status_service.h` | `applyFetchResult` slot |
| `core/src/qt/convergencepage.cpp` | Debounce, visible-only refresh, tip-aware history, connection-flap guard |
| `core/src/qt/rigorwallet/convergence/convergence_wallet_data.cpp` | Tip/revision cache + `[ui-perf]` |
| `core/src/qt/rigorwallet/widgets/convergence_{lifecycle,history,overview,network,start,resprobability}panel.cpp` | Skip work when not visible; network tip cache |
| `core/src/qt/bitcoin.cpp` | `[ui-perf] first UI ready` |
| `core/src/qt/bitcoingui.cpp` | Nav latency `[ui-perf]` |
| `core/src/qt/walletview.cpp` | Deferred-pages / page-switch `[ui-perf]` |
| `core/src/qt/transactiontablemodel.cpp` | `refreshWallet` `[ui-perf]` |

---

## Timer / refresh policy (after)

| Path | Before | After |
|------|--------|-------|
| Explorer HTTP | Every tip (~250ms coalesce), GUI-blocking | 15s periodic, worker thread; tip only arms timer |
| Convergence UI | Often immediate / 400ms; hidden frames too | 1500ms debounce; visible panels only |
| Balance / peers / tx | Core `MODEL_UPDATE_DELAY` + signals | Unchanged (Core pattern) |

---

## Performance logging

Filter wallet logs for `[ui-perf]`:

- `first UI ready (post-splash show) …ms`
- `WalletView constructed…` / `deferred pages ready …ms`
- `nav Overview|Transactions|Convergence …ms`
- `WalletView.gotoHistoryPage|gotoConvergencePage …ms`
- `ConvergencePage.refreshVisibleUi …ms`
- `ConvergenceWalletData.buildHistory …ms` (cache miss only)
- `ExplorerStatusService.fetchLiveStatus …ms (worker)`
- `TransactionTableModel.refreshWallet …ms`
- `ConvergenceNetworkPanel.getconvergencestats …ms`

### Before / after (expected)

| Metric | Before (observed) | After (target) |
|--------|-------------------|----------------|
| Cold start → interactive UI | Slow; explorer/Convergence work contended with paint | Core-like: splash then paint; heavy work deferred / off-GUI |
| Nav Wallet / Transactions / Convergence | Multi-second freeze | No visible freeze; `[ui-perf] nav …` typically low ms once pages built |
| Convergence tip refresh | Full RPC + N× history scans on GUI | Debounced; cached history; network RPC once per tip |
| Explorer during sync | GUI blocked up to ~10s per tip storm | Worker; ≤ one in-flight; 15s cadence |

Exact numbers depend on machine/wallet size — capture from `[ui-perf]` after rebuild.

---

## Operator verification (required)

Agent does **not** run full `make`. From `core/` (or usual build tree):

```bash
# Target name is rigor-qt (not bitcoin-qt). Binary: core/src/qt/rigor-qt
cd ./core
# If Makefile/config.status missing (e.g. after distclean), configure first:
./autogen.sh   # if needed
./configure --with-gui=qt5   # plus your usual flags
make -j$(nproc) rigor-qt
```

Then cold-start the wallet with logging visible (e.g. console / debug.log), and check:

1. **Cold start** — window paints; no multi-second “Not Responding”.
2. **Nav** — Overview → Convergence → Transactions: no freeze.
3. **Convergence** — page stays responsive while tip advances.
4. **Transactions** — list scrolls/filters without stalling.

Grep logs:

```bash
grep '\[ui-perf\]' ~/.rigor/*/debug.log   # adjust datadir
```

**PASS** (UX): operator confirmed no stutter after rebuild (2026-08-13).

---

## Residual notes

- `ExplorerClient::httpGet` still uses a local `QEventLoop` — acceptable **only** on the worker used by `ExplorerStatusService`. Call sites that still invoke `fetchLiveStatus` on the GUI (e.g. first-run wizard, diagnostics) are infrequent modal/tools paths, not tip-driven.
- First open of Transactions / Convergence before deferred construction finishes may still do one-time page build (same class of cost as Core first wallet attach); deferred `WalletView` construction minimizes this.
- Diagnostics page retains denser RPC for advanced users; not on the primary nav path.

# rigor-qt Prototype GUI Readiness Audit

**Task:** `TASK-P2-007-GUI-READINESS-AUDIT`  
**Date:** 2026-06-09  
**Scope:** `rigor-qt-prototype/` (read-only audit)  
**Baseline:** Prototype version `0.5.0-p2-006` (P2-001 through P2-006)  
**Comparison target:** Production `core/src/qt/` (`rigor-qt`) per [`FUTURE_GUI_RPC_SPEC.md`](./FUTURE_GUI_RPC_SPEC.md) and [`WALLET_UI_MINIMUM_SPEC.md`](../wallet/WALLET_UI_MINIMUM_SPEC.md)

**Audit rule:** No code changes. Documentation only.

---

## Executive summary

`rigor-qt-prototype` is a **successful read-only observer shell** that validates layout, RIGOR terminology, explorer-backed convergence surfaces, and dual-asset display policy. It is **not** a substitute for production `rigor-qt`, which already ships full wallet, convergence send, and embedded-node integration.

The prototype is suitable as a **reference implementation** for observer-only tabs and explorer HTTP clients. Migration into `core/src/qt/` requires a model-layer refactor, RPC surface completion, wallet security boundaries, and retirement of duplicate fetch logic — not a direct file copy.

---

## Section A — Feature inventory

### A.1 Tabs and panels

| Surface | File(s) | Data source | Implemented capability |
|---------|---------|-------------|------------------------|
| **Dashboard** | `src/views/dashboardview.{h,cpp}` | `getblockchaininfo` | Height, headers, verification %, best block hash, chain name/network |
| **Convergence** | `src/views/convergenceview.{h,cpp}` | `getconvergencestatus` | Active flag, `last_res_height`, `last_winner_txid`; candidate count and root show `—` (not in RPC) |
| **Convergence Inspector** | `src/convergenceinspector.{h,cpp}` | Explorer only | Summary, lineage tree, PCORB table, sigma, mint linkage; copy + open-in-browser |
| **Protocol Storyboard** | `src/protocolstoryboard.{h,cpp}` | Explorer only | Six narrative sections, 6-field PCORB catalog (A-7), visual timeline, explainer boxes |
| **RES Mint** | `src/resmintpanel.{h,cpp}` | `getresmintstatus` (soft-fail) + `/api/res/mints` | Latest mint card, ORU context card, proof card, 20-row history table, copy/open |
| **Network** | `src/views/networkview.{h,cpp}` | `getnetworkinfo` | Version, protocol version, connections, relay fee, warnings |
| **Logs** | `src/views/logsview.{h,cpp}` | Local ring buffer | Timestamp / level / message table; RPC + panel events appended |
| **Settings** | `src/settingsdialog.{h,cpp}`, `src/rpcsettings.{h,cpp}` | `QSettings` | rigord URL, RPC user/password, explorer base URL |

### A.2 Shell and infrastructure

| Component | File(s) | Notes |
|-----------|---------|-------|
| Main window | `src/mainwindow.{h,cpp}` | 7-tab `QTabWidget`, RPC banner, footer compliance text, 30s `QTimer`, manual Refresh |
| RPC client | `src/rigordrpcclient.{h,cpp}` | Parallel HTTP JSON-RPC batch: `getblockchaininfo`, `getnetworkinfo`, `getconvergencestatus` |
| View utilities | `src/viewutil.{h,cpp}` | Shared form labels, monospace hash styling |
| CLI | `src/main.cpp` | `--screenshot-dir` headless capture (7 PNGs) |
| Build | `rigor-qt-prototype.pro` | Standalone `qmake`; Qt Widgets + Network |

### A.3 RPC integration (rigord)

| Method | Used by | Behavior |
|--------|---------|----------|
| `getblockchaininfo` | Dashboard, RES Mint (tip height) | Required; batch failure clears RPC-backed tabs |
| `getnetworkinfo` | Network | Required; same batch |
| `getconvergencestatus` | Convergence | Required; same batch |
| `getresmintstatus` | RES Mint | Optional; `-32601` soft-fail → explorer-only path |

**Not called (specified in P2-002 but absent):** `getconvergenceresult`, `listconvergencehistory`, `getblock`, `getrawtransaction`, `gettxoutsetinfo`.

### A.4 Explorer integration

| Endpoint | Consumers |
|----------|-----------|
| `GET /api/status` | Inspector, Storyboard |
| `GET /api/res/mints` | Inspector (`limit=1`), Storyboard (`limit=1`), RES Mint (`limit=20`) |
| `GET /api/block/{hash}` | Inspector, Storyboard |
| `GET /api/lineage/{txid}` | Inspector, Storyboard |
| `GET /api/tx/{txid}` | Inspector, Storyboard |

**Not used:** `/healthz`, `/chain/tip`, `/api/res/events`, explorer lag / `critical_root_mismatch` banners.

### A.5 Compliance surfaces (frozen policy)

- Global banner and footer: `equivalence_claimed: false`
- No `rigor_target`, `res_draw_target`, or `T_conv` in UI strings (verified by source review)
- ORU / RES visually separated on RES Mint tab
- No send, sign, broadcast, or `submitblock` calls
- Inspector / Storyboard explicitly disclaim local verification

### A.6 Legacy / dead artifacts (not in build)

| Artifact | Status |
|----------|--------|
| `src/mockdata.{h,cpp}` | Orphaned from P2-001; not linked in `.pro` |
| `src/views/resmintview.{h,cpp}` | Superseded by `resmintpanel`; not in `.pro` |
| `ARCHITECTURE.md`, `TEST_PLAN.md`, `README.md` tabs table | Partially stale (still describe P2-001 mock scope in places) |

---

## Section B — Production gap analysis

Comparison: prototype observer shell vs. production `core/src/qt/` (wallet-enabled `rigor-qt`) and [`WALLET_UI_MINIMUM_SPEC.md`](../wallet/WALLET_UI_MINIMUM_SPEC.md).

### B.1 Observer / chain surfaces

| Area | vs. P2-002 / wallet spec | Classification | Gap detail |
|------|--------------------------|----------------|------------|
| Dashboard — chain height / tip | Partially covered | **PARTIAL** | Missing ORU supply, RES supply, convergence event metrics, peer count on dashboard, explorer sync status |
| Dashboard — dual-asset totals | Required | **MISSING** | No `res_total`, no indexed ORU from `/api/status` |
| Convergence — \|F\|, root, sigma | Required via `getconvergenceresult` | **PARTIAL** | Tab uses `getconvergencestatus` only; \|F\| and root always `—`; no sigma |
| Convergence — `RES_PROBABILITY_TARGET` | Required display | **MISSING** | Not shown on any tab |
| Convergence — root mismatch detection | Specified | **MISSING** | No `committed` vs `computed` compare |
| RES Mint — latest + history | Required | **PARTIAL** | Explorer history works; RPC fallback path thin; status machine simplified |
| Transaction viewer | Required page | **MISSING** | No txid lookup tab (only deep links from RES Mint / Inspector) |
| Network — sync state machine | Specified in P2-002 §4.2 | **PARTIAL** | No IBD / stalled / progress labels; `getblockchaininfo` sync fields unused |
| Network — RPC endpoint display | Mock mapped | **MISSING** | Settings URL not echoed on Network tab |
| Explorer — health / lag warnings | Specified | **MISSING** | No `/healthz`, no height lag banner, no `critical_root_mismatch` surfacing |
| Logs — `debug.log` tail | Optional in spec | **MISSING** | In-app buffer only |
| Inspector + Storyboard | Beyond minimum wallet spec | **READY** | Fit for observer/education scope when explorer is up |
| Settings | External RPC config | **PARTIAL** | Works for standalone prototype; not `OptionsModel` / datadir integration |

### B.2 Full production wallet features

| Feature | Production `rigor-qt` | Prototype | Classification |
|---------|----------------------|-----------|----------------|
| **Wallet** | `WalletModel`, balances, encryption | None | **MISSING** |
| **Address management** | Address book, labels | None | **MISSING** |
| **Transaction history** | `TransactionView` / models | None | **MISSING** |
| **Receive** | `ReceiveCoinsDialog` | None | **MISSING** |
| **Send** | `SendCoinsDialog` | None | **MISSING** |
| **Node control** | Embedded `rigord`, IBD UI | External RPC URL only | **MISSING** |
| **Mempool** | Via node / RPC console | None | **MISSING** |
| **Mining** | Not primary GUI; agent logs optional | None | **MISSING** |
| **Notifications** | `Notificator`, tray | None | **MISSING** |
| **Multi-window** | Wallet frames, dialogs | Single main window | **MISSING** |
| **Localization** | `.ts` / `bitcoin_en.ts` chain | `tr()` only, no translations | **MISSING** |
| **Accessibility** | Bitcoin Core baseline (partial) | Plain widgets, no AT enhancements | **MISSING** |

### B.3 Production assets already ahead of prototype

Production `core/src/qt/` already includes:

- `ConvergencePage` with wallet-linked convergence send (`onSendConvergenceClicked`)
- `ClientModel::getConvergenceBlockResult` (C++ node interface, not HTTP JSON-RPC in GUI)
- Full `WalletFrame` navigation (Overview, Send, Receive, History, Convergence)
- `RpcConsole`, peer table, options dialog, splash, payment server

**Implication:** Migration is a **selective port** of P2-004..006 observer panels, not a replacement of production GUI.

---

## Section C — Architecture review

### C.1 Qt architecture

| Aspect | Prototype | Production target (P2-002 §3.4) | Assessment |
|--------|-----------|-----------------------------------|------------|
| Pattern | View-centric widgets + ad hoc fetch | `ClientModel` + dedicated `*Model` QObject layer | **Gap — refactor required** |
| UI toolkit | Qt Widgets | Qt Widgets (+ `.ui` forms) | Compatible |
| Threading | All network on GUI thread | Same in Core GUI (node thread behind interfaces) | Acceptable for prototype; production uses `interfaces::Node` |
| Build | Isolated `qmake` project | Autotools / `core` integration | Merge needs build-system wiring |

### C.2 Widget layout

- Flat `QTabWidget` in `MainWindow` — appropriate for prototype.
- Production uses `RigorGUI` + `WalletFrame` + stacked pages; observer tabs would live under a **non-wallet** or **Tools** section, or augment existing Convergence / Overview surfaces.
- Inspector and Storyboard overlap ~70% in data chain; production should share one `ExplorerClient` and diverge only in presentation.

### C.3 Data flow

```text
Prototype today:
  MainWindow → RigordRpcClient (3 RPCs) → Dashboard / Convergence / Network
  MainWindow → ResMintPanel (own NAM + RPC + HTTP)
  MainWindow → Inspector / Storyboard (own NAM + HTTP chain each)

Production target:
  Views → Models → ClientModel / ExplorerClient → rigord / explorer
```

**Issues:**

1. **Four separate `QNetworkAccessManager` instances** — duplicate connection config, no shared cache.
2. **No rigord-wins merge policy** — explorer and RPC never cross-validated on Dashboard.
3. **No `getconvergenceresult`** — Convergence tab cannot meet wallet minimum spec.
4. **Refresh is timer-only** — no tip-change signal; 30s fixed interval for all surfaces.

### C.4 RPC integration

- HTTP JSON-RPC to user-configured URL works for **remote observer** deployments.
- Production embedded wallet uses in-process `interfaces::Node::executeRpc` — different transport, same methods.
- Batch semantics: **all-or-nothing** — one failed method clears Dashboard, Convergence, Network, RES Mint RPC fields even if partial data could display.

### C.5 Explorer integration

- Sequential fetch chains in Inspector and Storyboard are **correct and constitution-compliant** (display-only).
- Missing operational guards from spec: `/healthz`, indexer lag, `equivalence_claimed` read from `/api/status`, `critical_root_mismatch` banner.
- Storyboard labels explorer lineage authoritative for educational copy; global footer still says “rigord authoritative” — **mild messaging tension** on explorer-only tabs.

### C.6 Refresh model

| Mechanism | Present | Spec (P2-002) |
|-----------|---------|---------------|
| 30s `QTimer` | Yes | Yes (default) |
| Manual Refresh | Yes | Yes |
| Faster poll during IBD | No | Required (5s) |
| Tip-change invalidation | No | Required |
| Per-tab TTL / stale badge | No | Required |
| Explorer refresh when rigord down | Yes (Inspector, Storyboard) | Partial match |

### C.7 State handling

- Binary states: RPC banner OK / error; per-panel placeholder vs content.
- No formal state machine for sync, explorer lag, or RES mint presentation status (P2-002 §4.1–4.3).
- No last-good snapshot cache on RPC failure.

### C.8 Error handling

| Scenario | Behavior |
|----------|----------|
| RPC down | Red banner; `clearAllRpcFields()`; explorer tabs keep trying |
| Explorer down | Gray placeholder on Inspector, Storyboard, RES Mint (if no RPC mint data) |
| `getresmintstatus` missing | WARN log; explorer path continues |
| Partial RPC success | Not supported — entire batch fails |

### C.9 Offline behavior

- No cached snapshots with timestamps (spec §5.3 offline panel).
- Logs tab always works (local).
- No mock fallback on live tabs — **correct** for post-P2-003 behavior.

---

## Section D — Security review

### D.1 RPC credential storage

| Risk | Severity | Detail |
|------|----------|--------|
| Plaintext password in `QSettings` | **Medium** | `rigord/password` stored unencrypted in platform registry/ini |
| HTTP RPC URL default | **Medium** | Default `http://127.0.0.1:8332` — credentials sent in cleartext if not localhost TLS |
| No credential isolation | Low | Same settings object passed to all RPC consumers |

Production should use OS keychain, cookie auth, or embedded node with no remote RPC password in GUI settings.

### D.2 Explorer trust boundary

| Risk | Severity | Detail |
|------|----------|--------|
| Explorer as display authority | **Medium** | Inspector / Storyboard show indexed fields without lag or mismatch warnings |
| Open-in-browser links | Low | `QDesktopServices::openUrl` to user-configured explorer — acceptable if explorer is trusted |
| No TLS validation guidance | Low | User can point to arbitrary HTTP URL |

Constitution compliance: GUI does not **verify** proofs locally — correct. Risk is **user belief** that explorer data equals consensus truth without cross-check banners.

### D.3 Read-only guarantees

| Check | Result |
|-------|--------|
| No wallet private keys | Pass |
| No `send*`, `sign*`, `submitblock` RPC | Pass |
| No local winner / sigma recompute | Pass |
| Convergence send | N/A in prototype (production ConvergencePage **does** send — out of prototype scope) |

### D.4 Attack surfaces

1. **Malicious explorer URL** — could serve crafted JSON; impact is mis-display only (no signing).
2. **Malicious RPC endpoint** — could return fake chain info; observer misled, no funds at risk in prototype (no wallet).
3. **MITM on HTTP RPC** — credential theft if remote RPC used.

### D.5 User confusion risks

| Risk | Mitigation today | Recommended for production |
|------|------------------|---------------------------|
| Convergence tab shows `—` for root/\|F\| while Inspector shows values | Footnote on Convergence tab | Unify via `getconvergenceresult` or label tab “Status summary” |
| “rigord authoritative” footer on explorer-only tabs | Partial | Per-tab authority label (RPC vs indexer) |
| Sigma naming | Storyboard explains; RES Mint says “Proof / sigma” | Align labels with P2-002 “Winner selection digest (h)”. A-7 removed the proof signature, so sigma is the only Ed25519-adjacent artifact left to name |
| Dashboard does not match README mock field list | Docs drift | Update docs or complete dashboard fields |
| Prototype vs `rigor-qt` binary name | User may confuse binaries | Clear branding / “Observer Prototype” in title bar |

---

## Section E — Migration plan

### E.1 Principles

1. **Do not fork wallet logic** — port observer panels only; wallet stays in existing `WalletModel` path.
2. **Introduce models before merging widgets** — extract `ExplorerClient` and extend `ClientModel` per P2-002 §3.4.
3. **One explorer fetch pipeline** — deduplicate Inspector and Storyboard chains.
4. **Keep `equivalence_claimed: false` frozen** in all release builds.
5. **Prototype remains buildable** until production parity is verified, then gate behind `RIGOR_QT_PROTOTYPE` or retire.

### E.2 P2-008 — Foundation merge prep (core/src/qt/)

**Goal:** Production-ready data plane without changing tab UX yet.

| Work item | Deliverable |
|-----------|-------------|
| `ExplorerClient` QObject | Shared HTTP client: `/healthz`, `/api/status`, mints, block, lineage, tx |
| Extend `ClientModel` refresh batch | Add `getconvergenceresult(tip)` to convergence refresh path |
| Rigord-wins merge helper | Height lag, root mismatch, stale TTL per P2-002 §5 |
| Feature flags | `gui.explorer.enabled`, `gui.explorer.url` |
| CI grep gate | Forbid `rigor_target`, `res_draw_target`, `T_conv` in `core/src/qt/` |

**Exit criteria:** Unit-testable models; no new tabs; `equivalence_claimed` remains false.

### E.3 P2-009 — Observer tab port

**Goal:** Bring prototype observer UX into production shell.

| Work item | Source prototype | Production host |
|-----------|------------------|-----------------|
| Dashboard enrichment | `dashboardview` + P2-002 field table | `OverviewPage` or new observer section |
| Convergence status panel | `convergenceview` + `getconvergenceresult` | Extend `ConvergencePage` read-only region or split observer sub-tab |
| RES Mint observer panel | `resmintpanel` | New page or augment RES assets view per wallet spec |
| Convergence Inspector | `convergenceinspector` | Tools → Inspector (modal or tab) |
| Protocol Storyboard | `protocolstoryboard` | Help → Protocol Storyboard or education menu |
| Settings | Explorer URL in `OptionsModel` | Replace `QSettings` prototype store |
| Logging | `logsview` pattern | Integrate with `QMessageLogContext` / existing debug log viewer |

**Exit criteria:** Parity with prototype capabilities on testnet; explorer lag banners; no duplicate NAM.

### E.4 P2-010 — Production hardening & wallet spec closure

**Goal:** Meet `WALLET_UI_MINIMUM_SPEC.md` remaining pages.

| Work item | Classification |
|-----------|----------------|
| ORU Assets page (balance, history, burn) | Wallet — likely exists partially; align labels |
| RES Assets page (balance, mint history, bind height) | Wallet + explorer enrichment |
| Transaction viewer (txid lookup, witness summary) | New — prototype lacks entirely |
| `RES_PROBABILITY_TARGET` display on Convergence | Required field — add to `ConvergencePage` |
| Localization pass | `.ts` update for new strings |
| Accessibility review | Keyboard nav, contrast, screen reader labels on new tables |
| Security | Encrypted RPC storage or deprecate remote RPC in favor of embedded node |
| Retire `rigor-qt-prototype` binary | After feature parity sign-off |

**Exit criteria:** Wallet minimum spec checklist green; security review; no architecture constitution violations.

### E.5 Future milestones (post P2-010)

| Milestone | Description |
|-----------|-------------|
| P2-011 | Miner agent read-only panel (log tail per P2-002 §2.4) |
| P2-012 | Public testnet release packaging (`rigor-qt` + optional explorer bundle) |
| P2-013 | Equivalence / formal verification UI boundaries audit (still `equivalence_claimed: false`) |

### E.6 Preconditions before merging into `core/src/qt/`

- [ ] `ExplorerClient` shared and tested against frozen explorer API schemas
- [ ] `getconvergenceresult` wired; Convergence tab no longer shows permanent `—` for \|F\| / root
- [ ] Dashboard dual-asset rows per wallet spec
- [ ] Rigord-wins merge and explorer lag banners implemented
- [ ] Wallet team sign-off that Inspector/Storyboard do not imply proof verification
- [ ] Autotools / CI build includes new sources; `make check` / Qt tests updated
- [ ] Prototype `ARCHITECTURE.md` / `TEST_PLAN.md` archived or marked historical
- [ ] No plaintext RPC password persistence in release configuration

---

## Section F — Readiness verdict

### F.1 Prototype (`rigor-qt-prototype`)

**Verdict: PARTIAL**

| Strength | Weakness |
|----------|----------|
| Working observer shell with live RPC + explorer | Incomplete vs P2-002 dashboard/convergence field map |
| Constitution-compliant read-only stance | No model layer; duplicated HTTP clients |
| Inspector + Storyboard are strong education tools | Stale internal docs; orphan mock sources |
| Builds standalone; screenshot automation | No tests updated since P2-001 |

Suitable for: **internal layout review, explorer integration proof, testnet demos with external rigord + explorer.**

### F.2 Public testnet GUI

**Verdict: CONDITIONAL GO**

**Conditions:**

1. Ship as **“RIGOR Observer”** or clearly labeled prototype — not as full wallet.
2. Require documented rigord + explorer endpoints; show RPC error banner when node down.
3. Do not claim wallet send/receive, mempool, or mining.
4. Publish known gaps: Convergence tab partial fields, no `RES_PROBABILITY_TARGET` on dashboard path.
5. `equivalence_claimed: false` visible in release notes and UI.

### F.3 Production `rigor-qt` (merge into `core/src/qt/`)

**Verdict: NOT READY**

Production wallet GUI already exists and exceeds prototype for wallet operations. Observer features from P2-004..006 are **not yet ported**, and prototype architecture does not meet production model, security, localization, or wallet minimum spec requirements. Merge after P2-008..010.

---

## Required output

| Field | Status |
|-------|--------|
| Feature Inventory | **COMPLETE** |
| Gap Analysis | **COMPLETE** |
| Architecture Review | **COMPLETE** |
| Security Review | **COMPLETE** |
| Migration Plan | **COMPLETE** |
| Readiness Verdict | **COMPLETE** |
| Architecture Impact | **NONE** (audit only) |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Freeze Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## References

- `rigor-qt-prototype/README.md` — build and tab overview
- `docs/rigor-qt/FUTURE_GUI_RPC_SPEC.md` — production binding spec (P2-002)
- `docs/rigor-qt/CONVERGENCE_INSPECTOR.md` — P2-005
- `docs/rigor-qt/PROTOCOL_STORYBOARD.md` — P2-006
- `docs/wallet/WALLET_UI_MINIMUM_SPEC.md` — production page requirements
- `core/src/qt/convergencepage.cpp` — production convergence + wallet send
- `core/src/qt/clientmodel.h` — integration target for chain/convergence data

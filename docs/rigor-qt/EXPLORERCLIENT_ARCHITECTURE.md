# ExplorerClient Architecture — Production rigor-qt

**Task:** `TASK-P2-008A-EXPLORERCLIENT-ARCHITECTURE`  
**Status:** Architecture specification only — **no implementation**  
**Planned location:** `core/src/qt/explorerclient.{h,cpp}` (future task)  
**Authority:** [`FUTURE_GUI_RPC_SPEC.md`](./FUTURE_GUI_RPC_SPEC.md), [`TASK-P2-008-MIGRATION-PLAN.md`](./TASK-P2-008-MIGRATION-PLAN.md), ``RIGOR_EXPLORER_AI_EXECUTION_CONSTITUTION.md``

---

## 0. Architectural mandate

**Rule:** Future observer pages **must never** call `QNetworkAccessManager`, construct explorer URLs, or parse explorer JSON directly.

All explorer HTTP communication passes through a **single shared `ExplorerClient`** instance owned by `RigorGUI` (one client per application process).

```text
┌─────────────────────────────────────────────────────────────┐
│  Observer pages (views only)                                 │
│  Dashboard · RES Mint · Inspector · Storyboard · future …   │
└───────────────────────────┬─────────────────────────────────┘
                            │ subscribe / read cache
                            ▼
┌─────────────────────────────────────────────────────────────┐
│  ObserverDataModel (optional facade)                         │
│  rigord-wins merge · per-consumer error slices               │
└───────────────────────────┬─────────────────────────────────┘
                            │
              ┌─────────────┴─────────────┐
              ▼                           ▼
┌─────────────────────────┐   ┌─────────────────────────┐
│  ClientModel             │   │  ExplorerClient          │
│  interfaces::Node        │   │  HTTP · cache · schedule  │
│  (consensus authority)   │   │  (indexed display only)   │
└─────────────────────────┘   └─────────────────────────┘
```

**Constitutional boundary:** `ExplorerClient` is a **display adapter** for the explorer-indexer-mvp REST surface. It does not implement consensus, proof verification, or wallet logic.

---

## 1. Responsibilities

### 1.1 In scope

| Responsibility | Description |
|----------------|-------------|
| **Endpoint access** | Build URLs from configured base; issue GET requests for documented paths only |
| **Request scheduling** | Timer-driven refresh, tip-triggered invalidation, manual refresh, priority queues |
| **Response parsing** | JSON parse to typed value objects; schema-tolerant additive fields |
| **Caching** | Fast/slow tiers, TTL, last-good retention, manual bypass |
| **Error propagation** | Typed errors per endpoint domain; never blanket-clear unrelated panels |
| **Lag detection** | Compare indexer height (from `/healthz` or `/api/status`) to `ClientModel::getNumBlocks()` |
| **Mismatch detection** | Surface `critical_root_mismatch` from `/healthz` and `/api/status`; optional per-block root flags from mint rows |

### 1.2 Explicit non-responsibilities

| Non-responsibility | Rationale |
|--------------------|-----------|
| Protocol verification | rigord validates scripts; GUI displays indexed fields only |
| Winner calculation | Forbidden by explorer constitution (CP-06) |
| Convergence replay | No local eligibility or draw recomputation |
| Mint validation | RES mint legality is node consensus; explorer rows are informational |
| Wallet signing / broadcast | Out of scope for observer layer |
| Equivalence claims | Client may **read** `equivalence_claimed`; must never set or imply `true` |

### 1.3 Authority policy

On conflict between explorer cache and `ClientModel` / `interfaces::Node`:

1. **rigord wins** for height, winner, root, and supply figures shown as authoritative.
2. Explorer values may appear with suffix **“(indexed)”** or stale badge when lagging.
3. Explorer-only tabs (Inspector, Storyboard) show indexed data with footer: explorer authoritative **for display**; rigord authoritative **for consensus**.

---

## 2. Supported endpoints

Base URL: `{gui.explorer.url}` (default `http://127.0.0.1:8080`), no trailing slash.

All paths below are also valid under the `/api` prefix where noted.

### 2.1 Current (STABLE integrator surface)

| Endpoint | Method | Cache tier | Primary consumers |
|----------|--------|------------|-------------------|
| `/healthz` | GET | Fast | Global banner, lag, recovery state |
| `/api/status` | GET | Fast | Dashboard econ snapshot, integrity flags, height cross-check |
| `/api/res/mints` | GET | Slow | RES Mint history; bundle seed (`?limit=N`) |
| `/api/block/{hash}` | GET | Slow | Inspector, Storyboard, block panels |
| `/api/lineage/{txid}` | GET | Slow | Inspector, Storyboard |
| `/api/tx/{txid}` | GET | Slow | Inspector, Storyboard, RES Mint proof panel |

#### `/healthz` — parsed fields

| Field path | Use |
|------------|-----|
| `ok` | Liveness |
| `chain.last_height` | Indexer tip for lag |
| `chain.last_block_hash` | Cache key invalidation |
| `chain.critical_root_mismatch` | Mismatch detection |
| `chain.updated_at` | Stale detection |
| `indexer_recovery.state` | Recovery banner (`idle`, `stalled`, …) |

#### `/api/status` — parsed fields

| Field path | Use |
|------------|-----|
| `chain_identity.equivalence_claimed` | Must log/warn if not `false`; display frozen `false` |
| `block_health.current_height_node` | Cross-check vs `ClientModel` |
| `block_health.current_height_indexer` | Lag vs node |
| `block_health.best_block_hash` | Bundle fallback when mint row lacks hash |
| `economic_snapshot.oru_indexed_utxo_total_display` | Dashboard ORU (indexed) |
| `economic_snapshot.res_minted_total_display` | Dashboard RES cross-check |
| `system_integrity.critical_root_mismatch` | Mismatch banner |
| `convergence_health.*` | Optional dashboard event counts |

#### `/api/res/mints` — query parameters

| Param | Default | Use |
|-------|---------|-----|
| `limit` | `20` for history; `1` for convergence bundle | Row count |

#### `/api/block/{hash}` — parsed fields

| Field path | Use |
|------------|-----|
| `height` | Convergence height |
| `convergence_panel.candidate_set_size` | \|F\| |
| `convergence_panel.convergence_root` | Root display |
| `convergence_panel.winner_txid` | Winner fallback |

#### `/api/lineage/{txid}` — parsed fields

| Field path | Use |
|------------|-----|
| `f_set_size` | \|F\| fallback |
| `convergence_root` | Root fallback |
| `candidate_set[]` | Candidate listing |
| `sigma.selection_digest_h` | Winner selection digest (h) |
| `lineage_graph` | Inspector tree |

#### `/api/tx/{txid}` — parsed fields

| Field path | Use |
|------------|-----|
| `converge_proof_panel.pcorb_witness[]` | PCORB table (6 items — SECURITY-001-A / A-7) |
| `converge_proof_panel.res_mint_linkage.*` | Mint linkage |

`converge_proof_panel.ed25519.*` is **retired**: A-7 removed the proof signature and its key from the
witness, so there is no signature field to consume.

### 2.2 Future endpoints (reserved interface)

Not implemented in v1 client; API surface reserved for forward compatibility.

| Endpoint | Purpose | Version gate |
|----------|---------|--------------|
| `/api/convergence` | Paginated convergence event feed | `ExplorerCapabilities::convergenceFeed` |
| `/api/metrics` | Indexer operational metrics (lag, queue depth) | `ExplorerCapabilities::metrics` |

**Version gate behavior:** If response includes unknown `indexer_api_version`, emit `ExplorerVersionMismatch` at **warn** severity; continue serving known endpoints from cache where possible.

### 2.3 Forbidden calls (client must reject at compile/review time)

- Any mutating HTTP method (POST, PUT, DELETE)
- Internal audit RPC routed through explorer
- Endpoints not listed in STABLE matrix without feature flag

---

## 3. Cache model

### 3.1 Cache tiers

```text
ExplorerClient
├── ExplorerFastCache          TTL 30s (configurable)
│   ├── healthz
│   └── api/status
├── ExplorerSlowCache          TTL 60s (configurable)
│   ├── res/mints?limit=*
│   ├── block/{hash}
│   ├── lineage/{txid}
│   └── tx/{txid}
├── ExplorerBundleCache        TTL 60s; key = (block_hash, winner_txid, mint_txid)
│   └── materialized ConvergenceBundle (5-step chain)
└── ExplorerManualSnapshot     no TTL until next manual refresh
    └── user-triggered full refresh snapshot
```

#### Fast cache

| Property | Value |
|----------|-------|
| **Purpose** | Liveness, lag, econ snapshot, integrity flags |
| **Default TTL** | 30 seconds |
| **Invalidation** | Manual refresh; `healthz.chain.last_height` change; node tip change if lag was non-zero |
| **Stale policy** | Serve stale with `(stale)` metadata until refresh completes or TTL expires |

#### Slow cache

| Property | Value |
|----------|-------|
| **Purpose** | Block, tx, lineage, mint list payloads (large / chain-dependent) |
| **Default TTL** | 60 seconds |
| **Keying** | Path + query string (e.g. `mints:limit=20` separate from `mints:limit=1`) |
| **Invalidation** | Tip change; mint list head change; manual refresh; bundle key change |

#### Manual refresh

| Property | Value |
|----------|-------|
| **Trigger** | User **Refresh** action; optional per-page “Reload from explorer” |
| **Behavior** | Bypass TTL for requested domains; coalesce duplicate manual requests within 500 ms |
| **Snapshot** | On success, update `ExplorerManualSnapshot` timestamp shown in offline panel |
| **Failure** | Retain last-good entries; emit domain-specific error; do not clear other domains |

### 3.2 TTL strategy

| Data class | Default TTL | Fast poll (IBD) | Invalidation events |
|------------|-------------|-----------------|---------------------|
| `/healthz` | 30s | 10s | Recovery state change |
| `/api/status` | 30s | 10s | Tip change; integrity flag change |
| `/api/res/mints?limit=1` | 60s | 30s | New indexer height |
| `/api/res/mints?limit=20` | 60s | 30s | Head mint txid change |
| `/api/block/{hash}` | 60s | 30s | Bundle key change |
| `/api/lineage/{txid}` | 60s | 30s | Winner txid change |
| `/api/tx/{txid}` | 60s | 30s | Mint/winner txid change |
| Convergence bundle | 60s | 30s | Any step key change |

**IBD fast poll:** When `ClientModel` reports `initialblockdownload == true`, halve TTLs (floor 10s) until synced.

### 3.3 Coalescing strategy

#### In-flight deduplication

| Pattern | Rule |
|---------|------|
| Same URL in flight | Second subscriber attaches to existing `QNetworkReply`; one parse, many signal emissions |
| `fetchConvergenceBundle()` | Only one 5-step chain runs globally; concurrent callers receive same completion signal |
| Manual + timer overlap | Manual refresh supersedes timer tick for same domain; cancel redundant timer job if not started |

#### Convergence bundle pipeline (coalesced)

```text
Step 1: GET /api/status          ─┐
Step 2: GET /api/res/mints?limit=1 │
Step 3: GET /api/block/{hash}      ├── single in-flight "bundle job"
Step 4: GET /api/lineage/{winner}  │
Step 5: GET /api/tx/{mint|winner} ─┘
         │
         ▼
   ExplorerBundleCache (complete | partial)
```

**Partial bundle:** Steps 1–2 success but step 4 fails → cache `partial=true`, emit `lineageUpdated` with error slice; Inspector/Storyboard render degraded sections.

#### Request priority

| Priority | Requests |
|----------|----------|
| P0 (immediate) | Manual refresh, user-opened txid not in cache |
| P1 (tip change) | `/healthz`, `/api/status`, bundle step 1–2 |
| P2 (background) | Full mint history `limit=20`, bundle steps 3–5 |
| P3 (idle) | Future `/api/metrics` |

Max concurrent requests: **4** (configurable); queue P2/P3 when at cap.

---

## 4. Error model

### 4.1 Error taxonomy

| Code | Enum | Meaning | User-visible label |
|------|------|---------|-------------------|
| `Unavailable` | `ExplorerError::Unavailable` | Explorer disabled, connection refused, DNS failure | “Explorer unavailable” |
| `Timeout` | `ExplorerError::Timeout` | HTTP or connect timeout (>10s default) | “Explorer timeout” |
| `Malformed` | `ExplorerError::Malformed` | Non-JSON, schema missing required keys | “Explorer response invalid” |
| `IndexerLag` | `ExplorerError::IndexerLag` | Indexer height < node height − threshold | “Explorer lagging N blocks” |
| `RootMismatch` | `ExplorerError::RootMismatch` | `critical_root_mismatch == true` | “Explorer integrity warning — verify on node” |
| `ExplorerVersionMismatch` | `ExplorerError::VersionMismatch` | Unsupported `indexer_api_version` | “Explorer version incompatible” |

**Severity:**

| Severity | Codes | Banner |
|----------|-------|--------|
| Info | IndexerLag (≤3 blocks) | Yellow status bar |
| Warning | IndexerLag (>3), VersionMismatch | Yellow banner |
| Critical | RootMismatch | Red/yellow banner; rigord fields still shown |

### 4.2 Per-domain error isolation

Errors are scoped to **endpoint domain**, not global GUI state.

| Domain ID | Endpoints | Failure effect |
|-----------|-----------|----------------|
| `health` | `/healthz` | Lag/mismatch banners unknown; other domains unaffected |
| `status` | `/api/status` | Dashboard indexed econ `—`; bundle may abort step 1 |
| `mints` | `/api/res/mints` | RES Mint history empty; bundle uses status fallback hash |
| `block` | `/api/block/{hash}` | Inspector/Storyboard \|F\|/root from lineage only |
| `lineage` | `/api/lineage/{txid}` | Lineage tree empty; sigma `—` |
| `tx` | `/api/tx/{txid}` | PCORB / mint linkage `—` |
| `bundle` | Composite | `bundle.complete == false`; pages use partial data |

**Invariant:** Failure in domain `lineage` **must not** clear cached `mints` or rigord-backed `ClientModel` fields.

### 4.3 Error propagation API

```cpp
// Planned types (documentation only)

struct ExplorerErrorInfo {
    ExplorerError code;
    QString domain;       // "health" | "status" | "mints" | ...
    QString message;      // technical detail for logs
    QString userMessage;  // translated short label
    QDateTime occurredAt;
    bool recoverable{true};
};

struct ExplorerDomainState {
    bool available{false};
    bool stale{false};
    QDateTime lastSuccess;
    std::optional<ExplorerErrorInfo> lastError;
};
```

Views query `ExplorerClient::domainState("mints")` rather than interpreting raw HTTP codes.

### 4.4 Lag detection algorithm

```text
node_height  := ClientModel::getNumBlocks()
index_height := healthz.chain.last_height OR status.block_health.current_height_indexer
lag          := node_height - index_height

if lag > LAG_WARN_THRESHOLD (default 3):
    emit errorRaised(IndexerLag, domain="health", severity=Warning)
elif lag > 0:
    emit errorRaised(IndexerLag, domain="health", severity=Info)
else:
    clear IndexerLag for domain health
```

### 4.5 Mismatch detection algorithm

```text
mismatch := healthz.chain.critical_root_mismatch
         OR status.system_integrity.critical_root_mismatch
         OR (optional) latest mint row committed_root != computed_root

if mismatch:
    emit errorRaised(RootMismatch, domain="status", severity=Critical)
    // Do NOT hide rigord convergence data on ConvergencePage
```

---

## 5. Signal design

All signals emitted on **GUI thread**. Payloads use **immutable snapshot structs** (copy-on-emit) so views do not hold references into cache internals.

### 5.1 Primary signals (required)

```cpp
class ExplorerClient : public QObject {
    Q_OBJECT

Q_SIGNALS:
    // Fast cache domains
    void healthUpdated(const ExplorerHealthSnapshot& health);
    void statusUpdated(const ExplorerStatusSnapshot& status);

    // Slow cache domains
    void mintsUpdated(const ExplorerMintsSnapshot& mints);
    void lineageUpdated(const ExplorerLineageSnapshot& lineage);

    // Composite
    void convergenceBundleUpdated(const ExplorerConvergenceBundle& bundle);

    // Errors — always includes domain for per-tab isolation
    void errorRaised(const ExplorerErrorInfo& error);
    void errorCleared(const QString& domain);
};
```

### 5.2 Supplementary signals (recommended)

| Signal | Purpose |
|--------|---------|
| `blockUpdated(const ExplorerBlockSnapshot&)` | Single-block subscribers without full bundle |
| `txUpdated(const ExplorerTxSnapshot&)` | On-demand tx lookup (future transaction viewer) |
| `enabledChanged(bool)` | Feature flag `gui.explorer.enabled` toggled |
| `refreshStarted()` / `refreshFinished()` | Global progress indicator |

### 5.3 Snapshot struct summaries

| Struct | Key fields |
|--------|------------|
| `ExplorerHealthSnapshot` | `ok`, `indexerHeight`, `lagBlocks`, `criticalRootMismatch`, `recoveryState`, `fetchedAt`, `stale` |
| `ExplorerStatusSnapshot` | `equivalenceClaimed` (read-only), `nodeHeight`, `indexerHeight`, `oruSupplyDisplay`, `resSupplyDisplay`, `criticalRootMismatch`, `stale` |
| `ExplorerMintsSnapshot` | `limit`, `rows[]`, `headMintTxid`, `stale`, `domainState` |
| `ExplorerLineageSnapshot` | `winnerTxid`, `fSetSize`, `root`, `sigmaH`, `candidates[]`, `graph`, `complete`, `stale` |
| `ExplorerConvergenceBundle` | All above slices + `block`, `tx`, `complete`, `failedDomain` |

### 5.4 Subscription pattern for views

Views **connect signals once** in constructor; they **never** call HTTP.

```cpp
// Pattern (future Observer page)
connect(explorerClient, &ExplorerClient::mintsUpdated,
        this, &ResMintObserverPage::onMintsUpdated);
connect(explorerClient, &ExplorerClient::errorRaised,
        this, [this](const ExplorerErrorInfo& e) {
            if (e.domain == QLatin1String("mints"))
                showMintsError(e.userMessage);
        });
```

Optional: `ExplorerClient::subscribe(ExplorerTopic topic)` with refcount so coordinator knows which domains to poll when page visible.

---

## 6. Observer page mapping

### 6.1 Consumer matrix

| Future page | ExplorerClient data | Signals | rigord complement |
|-------------|---------------------|---------|-------------------|
| **Dashboard** (chain observer strip) | `statusUpdated`, `healthUpdated` | econ snapshot, lag, mismatch banners | `ClientModel` height, peers, `getconvergencestatus` |
| **RES Mint** (`ResMintObserverPage`) | `mintsUpdated`, `txUpdated`, `errorRaised(mints\|tx)` | history table, proof panel | `getresmintstatus` soft-fail; tip height from `ClientModel` |
| **Convergence Inspector** | `convergenceBundleUpdated`, `lineageUpdated`, `errorRaised(bundle\|*)` | summary, tree, PCORB | Optional cross-check labels from rigord (read-only) |
| **Protocol Storyboard** | `convergenceBundleUpdated`, `statusUpdated` | sections 1–6, timeline | Explainer text only; no extra fetch |
| **ConvergencePage** (read-only enrich) | `healthUpdated`, `statusUpdated` | lag/mismatch banners only | Primary data from `getConvergenceBlockResult` / RPC |
| **Future: Transaction viewer** | `txUpdated`, on-demand `fetchTx(txid)` | witness summary | `getrawtransaction` via node |
| **Future: Convergence feed** | `/api/convergence` (v2) | paginated model | rigord wins on conflict |
| **Future: Metrics panel** | `/api/metrics` (v2) | ops dashboard | none |

### 6.2 Bundle sharing (Inspector + Storyboard)

Both pages subscribe to **`convergenceBundleUpdated` only**. Neither page triggers the 5-step chain directly.

```text
ObserverRefreshCoordinator
        │
        ▼
ExplorerClient::scheduleConvergenceBundle()
        │
        ├──► ConvergenceInspectorPage::onBundleUpdated()
        └──► ProtocolStoryboardPage::onBundleUpdated()
```

### 6.3 Placeholder behavior (explorer down)

| Page | When `Unavailable` for bundle domain |
|------|--------------------------------------|
| Inspector | Gray placeholder; rigord banner unchanged |
| Storyboard | Same |
| RES Mint | Gray placeholder if `mints` also failed; partial if mints OK |
| Dashboard | Hide indexed ORU row; show rigord-only fields |

No mock fallback in production.

---

## 7. Production integration

### 7.1 Lifecycle and ownership

```text
RigorGUI
  ├── ClientModel*              (existing; embedded node)
  ├── OptionsModel*             (existing; add explorer settings)
  ├── ExplorerClient*           (NEW; child of RigorGUI)
  └── ObserverRefreshCoordinator* (NEW; wires timers + tip signals)
```

Construction order: `OptionsModel` → `ClientModel` → `ExplorerClient(options, clientModel)` → observer pages.

### 7.2 `ClientModel` integration

| Integration point | Use |
|-------------------|-----|
| `getNumBlocks()` | Lag detection numerator |
| `getBestBlockHash()` | Optional bundle hash cross-check |
| `numBlocksChanged` | Invalidate slow cache + schedule bundle refresh |
| `getStatusBarWarnings()` | Merge with explorer mismatch banner (rigord warnings take precedence in text order) |
| `node().executeRpc` | **Not called from ExplorerClient** — remains in `ObserverDataModel` |

`ExplorerClient` holds **`ClientModel*`** (observer pointer, not owner) for height/lag only.

### 7.3 `interfaces::Node` integration

**No direct use** inside `ExplorerClient`. Node access stays in `ClientModel` / `ObserverDataModel` to preserve layering:

```text
interfaces::Node  →  ClientModel  →  ObserverDataModel  →  views
                              ↘
                        ExplorerClient  →  views (indexed only)
```

### 7.4 `OptionsModel` integration

| Setting key | Type | Default | ExplorerClient use |
|-------------|------|---------|-------------------|
| `gui.explorer.enabled` | bool | `false` | Master switch; when false, emit `Unavailable` once, skip HTTP |
| `gui.explorer.url` | string | `http://127.0.0.1:8080` | Base URL |
| `gui.explorer.lagWarnBlocks` | int | `3` | Lag threshold |
| `gui.explorer.timeoutMs` | int | `10000` | Timeout → `Timeout` error |
| `gui.observer.equivalence_claimed` | bool | **`false` frozen** | Assert in release builds |

On `OptionsModel` settings change: `ExplorerClient::reconfigure()` clears caches and schedules refresh.

### 7.5 `ConvergencePage` integration

`ConvergencePage` **continues** to load convergence draw data via `ClientModel::getConvergenceBlockResult` and `executeRpc("getconvergencestatus")`.

ExplorerClient provides **supplementary** signals only:

- Connect `healthUpdated` / `errorRaised(RootMismatch|IndexerLag)` → `m_convergenceSafetyLabel` or global banner
- Do **not** replace round table data with explorer lineage

Wallet send path on `ConvergencePage` is unchanged and unrelated to `ExplorerClient`.

### 7.6 `ObserverToolsContainer` integration

Proposed host widget (future P2-009):

```text
ObserverToolsContainer (QTabWidget or QStackedWidget)
  ├── ConvergenceInspectorPage   ── subscribe: convergenceBundleUpdated
  └── ProtocolStoryboardPage     ── subscribe: convergenceBundleUpdated
```

Container connects to `ObserverRefreshCoordinator::setPageVisible()` so bundle polling pauses when no observer tab visible (optional power optimization).

### 7.7 `ObserverDataModel` (facade — recommended)

Thin QObject between views and dual sources:

| Role | Detail |
|------|--------|
| Merge | Dashboard RES: rigord `res_total` wins; show explorer `res_minted_total_display` as “(indexed cross-check)” |
| Slice errors | Maps `ExplorerErrorInfo` to per-page view state |
| RPC batch | Owns rigord refresh; does not duplicate explorer HTTP |

Views depend on **`ObserverDataModel`** where both rigord and explorer fields appear; explorer-only pages may connect directly to `ExplorerClient`.

---

## 8. Threading, security, and compliance

| Topic | Rule |
|-------|------|
| Threading | GUI thread only; `QNetworkAccessManager` owned by `ExplorerClient` |
| TLS | Support `https://` base URL; no certificate pinning in v1 |
| Input trust | Treat all JSON as untrusted; validate hex length; no `eval` |
| Open URL | Views open browser; `ExplorerClient` returns URLs only |
| Logging | Log domain errors at `WARN`; never log RPC passwords |
| Constitution | Read-only GET; `equivalence_claimed: false`; no winner math |

---

## 9. Future file layout (implementation reference)

```text
core/src/qt/
  explorerclient.h
  explorerclient.cpp
  explorercache.h          // Fast/Slow/Bundle cache templates
  explorer_types.h         // Snapshots, errors, enums
  observerrefreshcoordinator.h/cpp
  observerdatamodel.h/cpp  // optional facade
```

**Not in scope for P2-008A:** `.cpp` files, Makefile.am edits, tests.

---

## 10. Required verdict

| Field | Status |
|-------|--------|
| Architecture Impact | **NONE** (documentation only) |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Freeze Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## References

- [`TASK-P2-008-MIGRATION-PLAN.md`](./TASK-P2-008-MIGRATION-PLAN.md)
- [`FUTURE_GUI_RPC_SPEC.md`](./FUTURE_GUI_RPC_SPEC.md) §2.3, §5
- [`GUI_READINESS_AUDIT.md`](./GUI_READINESS_AUDIT.md)
- `core/src/qt/clientmodel.h`
- `rigor-qt-prototype/src/convergenceinspector.cpp` — reference fetch chain (to be retired)

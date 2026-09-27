# RIGOR Official Site — Frontend Specification

**Task:** ``
**Parent:** [ARCHITECTURE_OVERVIEW.md](./ARCHITECTURE_OVERVIEW.md)

| Field | Value |
|-------|-------|
| `equivalence_claimed` | **false** |

---

## §3 — Frontend architecture

### Recommended stack (spec-only)

| Layer | Choice | Rationale |
|-------|--------|-----------|
| Framework | **React 18** | Component model aligns with design system |
| Build | **Vite** | Deterministic static export; fast CI |
| Language | **TypeScript** | Typed adapters for RPC/explorer |
| Styling | **Tailwind CSS** + CSS variables from [COMPONENT_DESIGN_SYSTEM.md](./COMPONENT_DESIGN_SYSTEM.md) | Utility grid + token-based status colors |
| Routing | **React Router** (SPA) | GitHub Pages compatible with `404.html` fallback |
| Math | **KaTeX** + `react-katex` | Axiom page rendering |
| Markdown | `react-markdown` + build-synced content | Publication corpus |
| State | **Lightweight derived state** — React Context + `useReducer` | No Redux/Zustand required |

**Alternative:** Next.js `output: 'export'` — acceptable if static export only; no SSR backend.

### Explicit exclusions

- No wallet libraries
- No Stratum/mining clients
- No `ethers` / Web3 signing
- No server-side API routes in v1.0

---

## Page specifications

### `/` — Landing

| Zone | Content |
|------|---------|
| Hero | Deterministic dual-asset protocol (ORU · RES) |
| Live strip | `SystemStatusBar` summary |
| Three pillars | Protocol · Axioms · Lab (links) |
| Pipeline diagram | DurinHash → Block → FORGE → RES mint → Archive |
| CTA | Explorer, Verification (no investment CTAs) |

Bindings: `getblockchaininfo`, `/api/status` — see [API_BINDING_LAYER.md](./API_BINDING_LAYER.md).

---

### `/protocol`

| Section | Component |
|---------|-----------|
| Overview | `ProtocolCard` variant `overview` |
| ORU | Conservation + flat subsidy 128 (`h ≥ 1`; no halving) |
| RES | Conservation + mint cap |
| FORGE | Destroy-to-mint at `destroy_cost(h)` → RES path |
| Consensus | DurinHash PoW, 80-byte header |
| UTXO | Script-as-compliance |

Static MD from `content/protocol.md` + optional `LiveProtocolStrip`.

---

### `/axioms`

| Section | Component |
|---------|-----------|
| Invariant grid | 6 panels (determinism, conservation, E=246 mint cost, 125, non-equivalence) |
| Full document | Rendered `AXIOMS_v1.0` / publication §2 |

No live RPC required.

---

### `/lab`

| Section | Component |
|---------|-----------|
| Interpretation | State-machine + financial/computational hybrid |
| Layer diagram | Protocol ≠ Proof ≠ Runtime ≠ Visualization |
| Experiment tags | PROOF-034, runtime soak, visualization |
| Positioning quote | Lab statement from publication §3.5 |

---

### `/explorer`

| Tab | Components |
|-----|------------|
| Chain | `ProtocolCard`, height/hash |
| Convergence | `ConvergencePanel` |
| RES | `MintPanel` |
| Block / Tx / Lineage | Explorer API tables + detail |

Per-tab state isolation; 30s refresh — [API_BINDING_LAYER.md](./API_BINDING_LAYER.md).

---

### `/network`

| Widget | Component |
|--------|-----------|
| Peers | `NetworkPanel` — `getnetworkinfo` |
| Chain sync | IBD flag, verification progress |
| RPC health | Latency, last error |
| Explorer health | `/api/status`, `/healthz` |
| Mining observability | Read-only: chain work, difficulty — **not** miner control |

**Note:** Displays network/mining **health metrics** only; no pool or worker UI.

---

### `/verification`

| Widget | Component |
|--------|-----------|
| PROOF-034 | `VerificationPanel` — milestones, height progress |
| P03 | Certification volume status (static baseline + optional manifest) |
| GV system | 12 golden vector families checklist |
| Evidence grades | E0–E4 legend |
| Release tiers | CT GO / PT CONDITIONAL / M NO-GO |

---

## §6 — State management model

### `SystemState` (read-only, derived)

```typescript
type ConnectionStatus = 'online' | 'degraded' | 'offline' | 'syncing';

type SystemState = {
  chain_height: number | null;
  best_block_hash: string | null;
  initial_block_download: boolean | null;
  convergence_status: Record<string, unknown> | null;
  convergence_result: Record<string, unknown> | null; // optional soft-fail
  rpc_status: {
    reachable: boolean;
    latency_ms: number | null;
    last_error: string | null;
    fetched_at: string | null;
  };
  explorer_status: {
    reachable: boolean;
    last_error: string | null;
    fetched_at: string | null;
    equivalence_claimed: boolean | null; // expect false
  };
  mint_state: {
    res_mints: unknown[] | null; // from explorer
    res_mint_status: Record<string, unknown> | null; // RPC soft-fail
  };
  verification_state: {
    proof034: VerificationManifest | null; // static or fetched manifest URL
    p03_label: 'P03_PARTIAL' | 'P03_CERTIFIED' | null;
    gv_materialized: number | null;
    gv_total: 12;
  };
  global_status: ConnectionStatus;
};
```

### Derivation rules

```text
global_status =
  IF rpc unreachable → offline
  ELSE IF ibd → syncing
  ELSE IF explorer down OR equivalence_claimed === true OR soft-fail → degraded
  ELSE → online
```

### Store architecture

```text
SystemStateProvider (Context)
  ├── usePolling(30_000) → refreshCore()
  ├── adapters/rpc.ts
  ├── adapters/explorer.ts
  └── adapters/verification.ts (optional manifest fetch)
```

**Rules:**

- All state is **derived** from fetches; no user mutations
- No write operations
- No wallet interaction
- Page-level hooks select slices; explorer tabs keep local `TabState` per public-site spec

---

## Application bootstrap

```text
main.tsx
  → SystemStateProvider
  → Router
    → AppShell (nav, status, banners)
      → <Outlet /> per route
```

### Environment (build-time)

| Variable | Purpose |
|----------|---------|
| `VITE_RPC_PROXY_URL` | Read-only rigord proxy |
| `VITE_EXPLORER_API_URL` | Public explorer base |
| `VITE_VERIFICATION_MANIFEST_URL` | Optional PROOF-034 JSON |
| `VITE_SITE_VERSION` | Git tag / SHA injected in CI |

---

## Error handling (frontend)

| Event | UI |
|-------|-----|
| RPC failure | `ErrorBanner` code `RPC_OFFLINE` |
| Explorer failure | `DegradedBanner`; RPC-only mode label |
| Partial field | `DegradedField` per component |
| Stale data | Amber "stale" if `fetched_at` > 60s |

Never silent fallback — [API_BINDING_LAYER.md](./API_BINDING_LAYER.md) §9.

---

## Build output

| Output | Path |
|--------|------|
| Static bundle | `dist/` |
| Asset hashing | Content-hashed JS/CSS |
| `index.html` | SPA entry |
| `404.html` | Copy of `index.html` for GH Pages SPA |

---

## References

- [ARCHITECTURE_OVERVIEW.md](./ARCHITECTURE_OVERVIEW.md)
- [API_BINDING_LAYER.md](./API_BINDING_LAYER.md)
- [COMPONENT_DESIGN_SYSTEM.md](./COMPONENT_DESIGN_SYSTEM.md)
- [../public-site/ROUTE_AND_DATA_BINDING_SPEC.md](../public-site/ROUTE_AND_DATA_BINDING_SPEC.md)

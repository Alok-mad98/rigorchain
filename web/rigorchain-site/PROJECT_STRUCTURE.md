# RIGOR Official Site — Project Structure

**Task:** TASK-PUBLICATION-004  
**Domain:** https://www.rigorchain.com  
**Repository:** https://github.com/rigorchain/rigorchain  
**equivalence_claimed:** `false`

## Overview

Static SPA (React 18 + TypeScript + Vite). Read-only observer: no backend, no SSR, no CI/CD YAML. Data from rigord JSON-RPC (proxy) and explorer HTTP API only.

## Directory layout

```
web/rigorchain-site/
├── public/
│   └── CNAME                    # www.rigorchain.com (GitHub Pages / static host)
├── lib/
│   └── rigordRpcClient.ts       # JSON-RPC client (HTTP, timeout, no retry masking)
├── src/
│   ├── App.tsx                  # Router + SystemStateProvider
│   ├── main.tsx                 # React entry
│   ├── index.css                # Deterministic design system (no animation libs)
│   ├── vite-env.d.ts
│   ├── components/
│   │   ├── AppShell.tsx         # Status bar, nav, error banners
│   │   ├── ProtocolCard.tsx     # ORU / RES / FORGE / live observables
    │   │   ├── ConvergencePanel.tsx # RFC-020 same-tx mint explainer
│   │   └── RESMintPanel.tsx     # RES mint table (explorer)
│   ├── context/
│   │   └── SystemStateContext.tsx  # Derived SystemState, 30s poll
│   ├── lib/
│   │   ├── explorerClient.ts    # GET /api/* explorer client (public inputs only)
│   │   └── zeroTrust/           # Independent RIGOR verifier (WebCrypto, Uint8Array)
│   └── pages/
│       ├── LandingPage.tsx
│       ├── DownloadPage.tsx
│       ├── WalletPage.tsx
│       ├── MiningPage.tsx
│       ├── ExplorerPortalPage.tsx
│       ├── DocumentationPage.tsx
│       ├── ZeroTrustVerificationPage.tsx
│       ├── ProtocolPage.tsx
│       ├── LabPage.tsx
│       ├── VerificationPage.tsx
│       └── ExplorerPage.tsx     # Legacy live panels (not the verifier)
├── index.html
├── package.json
├── vite.config.ts
├── tsconfig.json
├── PROJECT_STRUCTURE.md         # this file
├── DEPLOYMENT_MANUAL.md         # manual build + upload (no Actions)
└── dist/                        # build output (gitignored) → upload to host
```

## Routes (TASK-RELEASE-004)

| Path | Page | Purpose |
|------|------|---------|
| `/` | LandingPage | Official overview + three-layer model |
| `/download` | DownloadPage | Official download entry |
| `/wallet` | WalletPage | Wallet TXID sources |
| `/mining` | MiningPage | Miner / `getblockhash` sources |
| `/explorer` | ExplorerPortalPage | Viewer portal (not a validator) |
| `/documentation` | DocumentationPage | Docs + lab links |
| `/zero-trust-verification` | ZeroTrustVerificationPage | Independent RIGOR verifier ⭐ |
| `/protocol` | ProtocolPage | Frozen protocol rules (Layer 1) |
| `/science` | SciencePage | Deterministic state machine (Layer 2) |
| `/lab` | LabPage | Experimental lab framing, PROOF-034 running |
| `/status` | StatusPage | Live verification status (read-only) |
| `/experiment` | ExperimentPage | PROOF-034 explanation |
| `/verification` | → redirect `/status` | Legacy PROOF-034 alias |
| `/verify` | → redirect `/zero-trust-verification` | Short alias |

## State model

Fully derived in `SystemStateContext` — no user mutation, no hidden transitions.

```typescript
SystemState {
  chain_height: number | null
  best_block_hash: string | null
  initial_block_download: boolean | null
  network_info: NetworkInfo | null
  convergence_status: Record<string, unknown> | null
  convergence_result: Record<string, unknown> | null
  rpc_status: { reachable, latency_ms, last_error, fetched_at }
  explorer_status: { reachable, last_error, fetched_at, equivalence_claimed }
  mint_state: { res_mints, res_mint_status, mint_error }
  verification_state: { proof034_phase, p03_label, gv_materialized, gv_total }
  global_status: "online" | "degraded" | "offline" | "syncing"
  refreshing: boolean
}
```

## Data flow

```
Explorer API ──► explorerClient.ts ──┐
                                     ├──► SystemStateContext ──► pages / components
rigord RPC ────► rigordRpcClient.ts ─┘
```

Rules: no mock data, no silent fallback substitution, all failures visible in UI.

## Error model

| Failure | UI |
|---------|-----|
| RPC failure | Red banner (`banner-error`) |
| Explorer failure | Gray panel (`banner-offline`) |
| Partial data | `—` placeholder |
| Timeout | `global_status: degraded` + amber badge |

## Environment variables

| Variable | Required | Purpose |
|----------|----------|---------|
| `VITE_RPC_PROXY_URL` | For live chain | HTTPS JSON-RPC proxy to rigord |
| `VITE_EXPLORER_API_URL` | For mint index | Explorer base URL |
| `VITE_VERIFICATION_MANIFEST_URL` | Optional | Live GV manifest JSON (`established` count) |
| `VITE_SITE_VERSION` | Optional | Footer build label |

## Build

```bash
cd web/rigorchain-site
npm install
npm run build
```

Output: `dist/` → manual upload to www.rigorchain.com (see `DEPLOYMENT_MANUAL.md`).

## Validation table (TASK-PUBLICATION-004)

| Field | Status |
|-------|--------|
| Landing Page | COMPLETE |
| Protocol Page | COMPLETE |
| Lab Page | COMPLETE |
| Verification Page | COMPLETE |
| API Layer | COMPLETE |
| Error Handling | COMPLETE |
| State Model | COMPLETE |
| Deployment Manual | COMPLETE |
| Freeze Compliance | COMPLETE |
| equivalence_claimed | false |

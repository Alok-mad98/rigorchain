# RIGOR Wallet Product Architecture

**Task:** `TASK-WALLET-PROGRAM-002`  
**Version:** 1.0  
**Date:** 2026-06-23  
**Type:** Read-only architecture — **no implementation, consensus, protocol, mining, or explorer changes**

| Field | Value |
|-------|--------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Implementation Impact | **NONE** |
| **`equivalence_claimed`** | **`false`** |

**Parent audit:** [WALLET_PRODUCT_READINESS_AUDIT.md](./WALLET_PRODUCT_READINESS_AUDIT.md) (TASK-WALLET-PROGRAM-001)

This document defines the **target product architecture** for the public-testnet wallet. It is a blueprint for future implementation, not a description of current code.

```
equivalence_claimed = false
```

---

## 1. Product Family Overview

The public-testnet wallet product consists of **two complementary applications** under one brand:

| Product | Name | User | Capabilities |
|---------|------|------|--------------|
| **Signing wallet** | **RIGOR Wallet** | Participants who hold keys | Send/receive ORU/RES, convergence, backup/restore, full node sync |
| **Observer** | **RIGOR Observer** | Watchers, auditors, educators | Read-only ORU/RES/convergence visibility; no keys |

Both products share:

- Dual-native asset presentation (ORU ≠ RES)
- Authority-derived convergence data (no local eligibility computation)
- Explorer integration for indexed history and enrichment
- Identical terminology (`OpuHash`, convergence, Mint)
- `equivalence_claimed: false` compliance surface

---

## 2. Full-Node Wallet Architecture

### 2.1 Layer model

```text
┌─────────────────────────────────────────────────────────────────┐
│  RIGOR Wallet UI (rigor-qt)                                      │
│  Screens: Dashboard · Wallet · ORU · RES · Convergence · …      │
├─────────────────────────────────────────────────────────────────┤
│  Presentation layer (Qt views, no consensus logic)               │
├─────────────────────────────────────────────────────────────────┤
│  Wallet model (WalletModel, wallet RPC client)                   │
├─────────────────────────────────────────────────────────────────┤
│  rigord JSON-RPC  │  Explorer HTTP client (read-only enrich)    │
├─────────────────────────────────────────────────────────────────┤
│  rigord (full node)          │  explorer-indexer-mvp (optional) │
├─────────────────────────────────────────────────────────────────┤
│  Local chain state · UTXO set · wallet.dat / descriptors         │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 Components

| Component | Role | Authority |
|-----------|------|-----------|
| **rigor-qt** | Primary desktop UI | Presentation only |
| **WalletModel** | Balance, tx, address state | Wallet RPC |
| **Node RPC bridge** | Chain sync, broadcast, convergence RPC | rigord |
| **ExplorerClient** | Mint history, convergence events, tx enrichment | Explorer API (read-only) |
| **rigord** | Consensus, mempool, block relay | Protocol (unchanged) |
| **Embedded node option** | Single-install bundle spawns rigord child process | Ops packaging |

### 2.3 Wallet modes (target)

| Mode | Description | Default for |
|------|-------------|-------------|
| **Full node** | rigor-qt + local rigord; wallet RPC to local node | Public testnet download |
| **Thin + remote node** | rigor-qt connects to user-configured RPC (advanced) | Power users |
| **Watch-only wallet** | Descriptors imported; no signing keys | Recovery / audit |

Observer mode is **not** inside rigor-qt — it is the separate **RIGOR Observer** product (web or future lite shell).

### 2.4 Data authority rules

| Data type | Primary source | Secondary (enrichment) |
|-----------|----------------|------------------------|
| ORU/RES balances | `getbalances`, `listunspent` | Explorer address view |
| Tx confirmation | Wallet + node | Explorer `/tx/:txid` |
| Convergence eligibility | `getconvergenceresult` | Explorer convergence tables |
| RES mint linkage | RPC + wallet tx | Explorer `/res/mints` |
| `RES_PROBABILITY_TARGET` | Explorer / RPC compliance | Must not be locally computed |
| Chain sync status | `getblockchaininfo` | Explorer `/chain/tip` lag compare |

**Hard rule:** UI never recomputes convergence eligibility, draw outcomes, or RES supply.

### 2.5 Security boundaries

- Private keys remain in wallet process only
- Explorer client is **read-only HTTP** — no signed payloads from explorer
- RPC credentials stored in OS keychain where platform supports (target)
- `equivalence_claimed` displayed from compliance endpoint; never user-toggleable

---

## 3. Observer Wallet Architecture

### 3.1 Layer model

```text
┌─────────────────────────────────────────────────────────────────┐
│  RIGOR Observer UI (Next.js or future Qt read-only shell)        │
├─────────────────────────────────────────────────────────────────┤
│  Server proxy / API routes (no secrets in browser)               │
├─────────────────────────────────────────────────────────────────┤
│  explorer-indexer-mvp API  │  rigord RPC (authority overlay)    │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 Deployment topologies

| Topology | Use case |
|----------|----------|
| **Local operator** | Developer machine: explorer :8080 + RPC :18443 + observer :3001 |
| **Docker Compose** | Single-command testnet stack for non-developers |
| **Hosted observer** | Public read-only site pointing at public explorer + public RPC read endpoints |

### 3.3 Watch registry

Central **Watch Registry** (client-side, persisted):

- ORU addresses
- RES addresses  
- txids
- convergence txids

All asset pages consume the registry — not isolated per-page state (fixes Program-001 gap).

---

## 4. Explorer Integration Architecture

### 4.1 Integration pattern

```text
Wallet UI ──► ExplorerClient ──► GET /chain/tip
              │                  GET /address/:addr
              │                  GET /tx/:txid
              │                  GET /res/mints
              │                  GET /convergence/events
              └──► Compare with node RPC ──► warn on lag / root mismatch
```

### 4.2 ExplorerClient contract (target interface)

| Method | Explorer endpoint | Purpose |
|--------|-------------------|---------|
| `chainTip()` | `/chain/tip` | Height, hash, sync freshness |
| `addressSummary(addr)` | `/address/:addr` | Indexed balances, tx list |
| `transaction(txid)` | `/tx/:txid` | IO details, witness summary |
| `resMints(filter)` | `/res/mints` | Mint history with linkage fields |
| `convergenceEvents(range)` | `/convergence/events` | Event timeline |
| `convergenceResult(id)` | `/convergence/result/:id` | Winner, target, eligibility |
| `compliance()` | `/compliance` or observer proxy | `equivalence_claimed`, policy flags |

### 4.3 Warning engine

| Condition | User-facing warning |
|-----------|---------------------|
| Explorer tip lag > N blocks vs node | "Indexer behind node" |
| Tip stale > 2× refresh interval | "Chain data may be stale" |
| `critical_root_mismatch` | "Critical root mismatch — do not trust displayed state" |
| Explorer unreachable | "Explorer offline — showing node data only" |
| Rate limited | "Indexer busy — retrying" |

---

## 5. Multi-Platform Architecture

### 5.1 Platform matrix (target state)

| Platform | RIGOR Wallet delivery | RIGOR Observer delivery |
|----------|----------------------|---------------------------|
| **Windows** | Signed NSIS installer: rigor-qt + rigord + optional explorer bundle | MSI or portable zip + Docker option |
| **Linux** | `.deb` / AppImage: rigor-qt + rigord | systemd + nginx template OR Docker Compose |
| **macOS** | Signed DMG: rigor-qt + rigord | Docker Compose; native menu-bar app (Phase 2+) |

### 5.2 Install bundle contents (public testnet target)

**RIGOR Wallet (full install):**

```
RIGOR-Wallet-{version}-{platform}/
├── bin/rigor-qt
├── bin/rigord
├── bin/rigor-cli
├── share/rigor.conf.example
├── share/icons/
└── README-first-run.pdf (or in-app onboarding)
```

**RIGOR Observer (operator stack):**

```
RIGOR-Observer-Stack-{version}/
├── docker-compose.yml    # rigord + explorer + observer
├── .env.example
├── nginx/                # optional TLS front
└── README-operator.md
```

### 5.3 Update architecture

| Mechanism | Wallet | Observer |
|-----------|--------|----------|
| Version check | rigor-qt About → release API | Package tag / compose pull |
| Data migration | Wallet descriptor format stable per freeze | Watch registry export/import JSON |
| Node upgrade | rigord binary replace; chain dir preserved | Container image bump |

---

## 6. Product Unification Roadmap (architecture only)

| Phase | Architecture state |
|-------|-------------------|
| **Now** | Split: rigor-qt (signing) + wallet-observer-mvp (web) |
| **Testnet RC** | rigor-qt gains ExplorerClient; observer web remains parallel |
| **Post-testnet** | Optional: read-only tab set ported from prototype into rigor-qt |
| **Never** | Observer must not gain signing paths; wallet must not hide dual-asset separation |

---

## 7. Related documents

| Document | Purpose |
|----------|---------|
| [RIGOR_WALLET_UX_SPEC.md](./RIGOR_WALLET_UX_SPEC.md) | User journeys |
| [RIGOR_WALLET_SCREEN_MAP.md](./RIGOR_WALLET_SCREEN_MAP.md) | All screens |
| [RIGOR_WALLET_INFORMATION_ARCHITECTURE.md](./RIGOR_WALLET_INFORMATION_ARCHITECTURE.md) | Navigation & data hierarchy |
| [RIGOR_WALLET_BRANDING_SYSTEM.md](./RIGOR_WALLET_BRANDING_SYSTEM.md) | Visual identity |
| [RIGOR_WALLET_RELEASE_CANDIDATE_CHECKLIST.md](./RIGOR_WALLET_RELEASE_CANDIDATE_CHECKLIST.md) | RC gates |

---

## 8. Output table

| Field | Status |
|-------|--------|
| Wallet Product Architecture | **COMPLETE** |
| UX Specification | See UX spec |
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Implementation Impact | **NONE** |
| Equivalence Claimed | **false** |

---

*End of RIGOR_WALLET_PRODUCT_ARCHITECTURE — TASK-WALLET-PROGRAM-002*

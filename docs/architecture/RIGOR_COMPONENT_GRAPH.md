# RIGOR Component Interaction Graph

**Task:** `TASK-ARCH-001` (deliverable)  
**Version:** 1.0  
**Date:** 2026-06-20  
**Type:** Textual component graph — read-only analysis

| Field | Value |
|-------|-------|
| Consensus Impact | **NONE** |
| `equivalence_claimed` | **false** |

This document provides a textual component interaction graph: module boundaries, dependency directions, interface contracts, and cross-layer coupling rules for the RIGOR system.

---

## 1. Graph Legend

```
[A] ──RPC──► [B]     Remote procedure call (client → server)
[A] ──read──► [B]    Read-only data consumption
[A] ──mirror► [B]    Non-authoritative logic copy
[A] ──write─X [B]    Forbidden interaction
[A] ⊂ [B]            A is module inside B
```

**Layer tags:** L1 Protocol · L2 Execution · L3 Observer · L4 Experimental

---

## 2. Top-Level System Graph

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ L1 PROTOCOL                                                                  │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ rigord (core/)                                                       │    │
│  │  validation · consensus · script · rpc · net · wallet · qt         │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────────────┘
         ▲ RPC                           ▲ RPC (read)
         │ submitblock/GBT               │
┌────────┴────────────┐         ┌────────┴────────────────────────────────────┐
│ L2 EXECUTION        │         │ L3 OBSERVER                                  │
│  pool/ (Rust)       │         │  explorer-indexer-mvp                        │
│  pilot · gbt ·      │         │  wallet-observer-mvp                         │
│  assembler ·        │         │  rigor-qt-prototype                          │
│  stratum · winner · │         │  web/rigorchain-site                         │
│  mint · miner-agent │         │  runtime/proof034/observer                   │
└────────┬────────────┘         └──────────────────────────────────────────────┘
         │ JSONL telemetry                    ▲ artifacts (read)
         ▼                                      │
┌─────────────────────────────────────────────────────────────────────────────┐
│ L4 EXPERIMENTAL                                                              │
│  sim/ · ci/replay/ · gui/replay_viewer/ · simulation/ · docs/formal/        │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Layer 1 — Protocol Components

### 3.1 rigord Internal Graph

```
┌──────────────┐
│  bitcoind    │  entry
└──────┬───────┘
       │
       ├─► net/          P2P block/tx propagation
       ├─► validation.cpp ◄─── CENTRAL: ConnectBlock
       │       │
       │       ├── consensus/convergence_root.*
       │       ├── consensus/convergence_logic.*  ──► GetBlockWinner
       │       ├── consensus/rigor_res.*
       │       ├── consensus/res_proof.*
       │       ├── consensus/tx_check.cpp
       │       ├── consensus/tx_verify.cpp
       │       └── script/interpreter.cpp
       │
       ├─► node/miner.cpp     BlockAssembler → GBT source
       ├─► rpc/
       │     ├── mining.cpp   getblocktemplate, submitblock
       │     └── blockchain.cpp  getconvergenceresult, getblockconvergence
       │
       ├─► chain.h            CBlockIndex caches
       ├─► txmempool.cpp
       └─► wallet/ (optional, non-consensus for signing)
```

### 3.2 L1 Module Dependency Table

| Module | Depends on | Provides to |
|--------|------------|-------------|
| `validation.cpp` | consensus/*, script/*, primitives/* | chainstate, RPC |
| `convergence_logic.cpp` | convergence_root, sigma_hash, rigor_math | validation, RPC |
| `rigor_res.cpp` | res_proof, statetag | validation |
| `miner.cpp` | validation hooks, mempool | rpc/mining.cpp |
| `rpc/blockchain.cpp` | convergence_logic (read) | L2, L3 clients |
| `rigor_consensus_lock.h` | amount.h | all consensus modules |

### 3.3 L1 External Interfaces

**P2P (Bitcoin-compatible + Rigor blocks):**

| Message | Direction | Purpose |
|---------|-----------|---------|
| `version` / `verack` | bidirectional | Handshake |
| `inv` / `getdata` | bidirectional | Block/tx inventory |
| `block` / `tx` | bidirectional | Payload delivery |
| `headers` / `getheaders` | bidirectional | Header sync |

**JSON-RPC (port configurable; regtest default 28761):**

| Method | Layer consumer | Mutates chain? |
|--------|----------------|----------------|
| `getblockchaininfo` | L2, L3, L4 | No |
| `getblocktemplate` | L2 | No |
| `submitblock` | L2 | **Yes** |
| `getblock` | L3 | No |
| `getrawtransaction` | L3 | No |
| `getconvergenceresult` | L2, L3 | No |
| `getconvergencestatus` | L3, L4 | No |
| `getblockconvergence` | L3 | No |
| `generate` / `generatetoaddress` | L4 (archive) | **Yes** (regtest) |

Extended `getblock` fields: `rigor_winner`, `rigor_res_minted`, `cumulative_convergence`.

---

## 4. Layer 2 — Execution Components

### 4.1 Pool Stack Graph

```
                    ┌─────────────┐
                    │   pilot     │  orchestrator
                    └──────┬──────┘
                           │
     ┌─────────────────────┼─────────────────────┐
     ▼                     ▼                     ▼
┌─────────┐         ┌─────────────┐       ┌──────────────┐
│rigor-gbt│         │block-       │       │stratum-      │
│         │──tpl──►│assembler    │──job─►│server        │
└────┬────┘         └──────┬──────┘       └──────┬───────┘
     │ RPC                 │                     │ Stratum
     ▼                     │                     ▼
┌─────────┐                │              ┌──────────────┐
│ rigord  │                │              │rigor-miner-  │
└─────────┘                │              │agent         │
                           │              └──────────────┘
              ┌────────────┼────────────┐
              ▼            ▼            ▼
        ┌──────────┐ ┌───────────┐ ┌─────────────┐
        │ winner-  │ │ share-    │ │ mint-       │
        │ engine   │ │ validator │ │ engine      │
        └────┬─────┘ └───────────┘ └──────┬──────┘
             │                            │ submitblock
             └──────────┬─────────────────┘
                        ▼
                   ┌─────────┐
                   │ rigord  │
                   └─────────┘

        ┌──────────────────────┐
        │ rigor-convergence-root│  shared library (C root in Rust)
        └──────────────────────┘
              ▲            ▲
              │            │
        block-assembler  winner-engine
```

### 4.2 Pool Crate Interaction Matrix

| From → To | rigor-gbt | block-assembler | winner-engine | stratum-server | mint-engine | share-validator | miner-agent |
|-----------|-----------|-----------------|---------------|----------------|-------------|-----------------|-------------|
| **rigor-gbt** | — | TemplateRecord | — | — | — | — | — |
| **block-assembler** | reads cache | — | job snapshot | PublishedJob | — | — | — |
| **winner-engine** | — | preview on publish | — | winner envelope | authorize token | — | — |
| **stratum-server** | — | — | confirm/invalidate | — | trigger mint | validate share | notify/submit |
| **mint-engine** | — | — | reads auth | — | — | — | — |
| **share-validator** | — | root check | — | pass/fail | — | — | — |
| **miner-agent** | — | — | — | Stratum client | — | — | — |

### 4.3 Stratum Protocol Surface

**Transport:** TCP line-delimited JSON-RPC 2.0 (standard Stratum mining).

| Method | Direction | Handler | Notes |
|--------|-----------|---------|-------|
| `mining.subscribe` | C→S | stratum-server | Requires `opushash` in agent string |
| `mining.authorize` | C→S | stratum-server | Worker credentials |
| `mining.submit` | C→S | share-validator | job_id, extranonce2, ntime, nonce |
| `mining.notify` | S→C | stratum-server | Job push |
| `mining.set_difficulty` | S→C | stratum-server | Share target |
| `mining.notify.rigor` | S→C | *defined, not emitted* | miner-agent only; preview metadata |

**RIGOR-specific validation (share-validator):**

- OpuHash proof hash vs share/block target
- Convergence root equality with template
- CVGE commitment wire match
- RES mint placeholder integrity when `mint_required`

### 4.4 Optional Persistence (`rigor-pool-pg`)

When `DATABASE_URL` set:

```
stratum-server ──► share store
block-assembler ──► job_snapshot store
winner-engine ──► winner_job, authorization store
mint-engine ──► mint_job store
rigor-gbt ──► gbt_record store
pilot ──► recovery orchestration
```

Default devtest: in-memory only.

---

## 5. Layer 3 — Observer Components

### 5.1 Observer Graph

```
┌─────────┐
│ rigord  │
└────┬────┘
     │ RPC import
     ▼
┌─────────────────────┐
│ explorer-indexer-mvp │
│  Express + Postgres  │
└─────────┬───────────┘
          │ REST /api/*
     ┌────┴────┬────────────┐
     ▼         ▼            ▼
┌─────────┐ ┌──────────┐ ┌────────────────┐
│ wallet- │ │ rigor-qt │ │ rigorchain-site│
│observer │ │ prototype│ │ /status /api   │
└─────────┘ └──────────┘ └────────────────┘
```

### 5.2 Explorer API Surface

Base: `explorer-indexer-mvp` — routes from `registerRoutes.ts`

| Endpoint | Method | Response domain |
|----------|--------|-----------------|
| `/healthz` | GET | Liveness |
| `/api/status` | GET | Sync + `equivalence_claimed` |
| `/api/chain/tip` | GET | Tip height, convergence totals |
| `/api/blocks` | GET | Paginated block list |
| `/api/block/:hash` | GET | Block + convergence panel |
| `/api/tx/:txid` | GET | Tx + lineage embed |
| `/api/lineage/:txid` | GET | Convergence graph |
| `/api/address/:addr` | GET | Address history |
| `/api/res/mints` | GET | RES mint catalog |
| `/api/res/events` | GET | RES events filter |
| `/search/resolve?q=` | GET | Hash/height/address search |

HTML routes: `/blocks`, `/block/:hash`, `/tx/:txid`, `/address/:addr`, `/status`, `/search`.

### 5.3 Wallet Observer Proxy API

Next.js routes (`wallet-observer-mvp/app/api/observer/`):

| Route | Upstream |
|-------|----------|
| `/api/observer/chain` | Explorer or RPC |
| `/api/observer/compliance` | Static compliance metadata |
| `/api/observer/address/[address]` | Explorer |
| `/api/observer/tx/[txid]` | Explorer |
| `/api/observer/res/mints` | Explorer |
| `/api/observer/convergence/events` | Explorer |
| `/api/observer/convergence/result/[hashOrHeight]` | Explorer / RPC |

Env: `EXPLORER_API_BASE`, `RIGOR_RPC_URL`.

### 5.4 GUI Observer APIs

**rigor-qt-prototype** (direct):

| Source | Methods / paths |
|--------|-----------------|
| rigord RPC | `getblockchaininfo`, `getnetworkinfo`, `getconvergencestatus`, `getresmintstatus` |
| Explorer HTTP | `/api/blocks`, `/api/block/:hash`, `/api/tx/:txid`, `/api/res/mints`, `/api/status` |

Tabs: Dashboard, Convergence, RES Mint, Inspector, Storyboard, Network, Logs.

**web/rigorchain-site** (proxied):

| Binding | Env var |
|---------|---------|
| RPC proxy | `VITE_RPC_PROXY_URL` |
| Explorer | `VITE_EXPLORER_API_URL` |
| Verification manifest | `VITE_VERIFICATION_MANIFEST_URL` |

Client: `src/lib/explorerClient.ts`, state: `SystemStateContext.tsx`.

---

## 6. Layer 4 — Experimental Components

### 6.1 PROOF-034 Graph

```
┌─────────┐     mine      ┌──────────────────────────┐
│ rigord  │──────────────►│ regtest archive datadir   │
│ regtest │               │ h → 2,100,001             │
└────┬────┘               └────────────┬─────────────┘
     │                                 │
     │                      golden_vector_materializer.sh
     │                                 │
     │                                 ▼
     │                      archive/proof-034/*.json
     │                                 │
     │                      golden_vector_verify.sh
     │                                 │
     ▼                                 ▼
runtime/proof034/observer ◄─── harness report
     │
     ▼
.runtime/proof034/proof034_observation.json
```

### 6.2 Simulation Graph

```
.runtime/pool-metrics/*.jsonl
         │
         ▼
    sim/core (SIM-004)
         │
         ▼
    sim/emulator (SIM-005)
         │
         ├─► sim/proof_validator (SIM-006) ◄── fixtures/archive/
         │
         ▼
    ci/replay/pipeline.py (SIM-008)
         │
         ├─► sim/unification (SIM-009)
         │
         └─► gui/replay_viewer (SIM-007)
```

### 6.3 Formal Proof (Reference Only)

```
docs/formal/phase4_coq/
    │
    └── PROOF-023 … PROOF-033 (ESTABLISHED)
            │
            └── Informs spec; no runtime edge to rigord
```

---

## 7. Cross-Layer Interaction Rules

### 7.1 Permitted Edges

| From | To | Edge type | Condition |
|------|-----|-----------|-----------|
| L2 pool | L1 rigord | RPC | GBT read, submitblock write |
| L3 explorer | L1 rigord | RPC | Import poll read-only |
| L3 GUI | L3 explorer | HTTP | REST proxy |
| L3 GUI | L1 rigord | RPC | Allowlisted read |
| L4 PROOF-034 | L1 rigord | RPC + regtest mine | Designated datadir |
| L4 SIM-006 | L4 archive | File read | Fixture copy |
| L4 SIM-004 | L2 metrics | JSONL read | Telemetry export |

### 7.2 Forbidden Edges

| From | To | Reason |
|------|-----|--------|
| L4 simulation | L1 rigord | IV-01 — no live injection |
| L3 observer | L1 rigord | Mutating RPC forbidden |
| L3 observer | L4 PROOF-034 | IV-02 — no harness alteration |
| L2 pool | L1 rigord | Alternate validation path forbidden |
| L4 simulation | L4 PROOF-034 | No verdict substitution |

### 7.3 Mirror Edges (Non-Authoritative)

| From | To | Certification |
|------|-----|---------------|
| winner-engine | convergence_logic.cpp | 28/28 golden + 122/122 root |
| rigor-convergence-root | convergence_root.cpp | Golden CR tests |
| share-validator | ConnectBlock layout | Structural checks only |

---

## 8. Component Inventory (Major Modules)

| ID | Component | Layer | Path | Maturity |
|----|-----------|-------|------|----------|
| C-01 | rigord | L1 | `core/src/` | Production baseline |
| C-02 | ConnectBlock pipeline | L1 | `core/src/validation.cpp` | Frozen |
| C-03 | Convergence engine | L1 | `core/src/consensus/convergence_*` | Frozen |
| C-04 | RES / PCORB | L1 | `core/src/consensus/rigor_res.*` | Frozen |
| C-05 | rigor-gbt | L2 | `pool/rigor-gbt/` | CT PASS |
| C-06 | block-assembler | L2 | `pool/block-assembler/` | CT PASS |
| C-07 | winner-engine | L2 | `pool/winner-engine/` | CT PASS |
| C-08 | mint-engine | L2 | `pool/mint-engine/` | CT PASS |
| C-09 | stratum-server | L2 | `pool/stratum-server/` | CT PASS |
| C-10 | rigor-miner-agent | L2 | `pool/rigor-miner-agent/` | CT PASS |
| C-11 | pool pilot | L2 | `pool/pilot/` | CT PASS |
| C-12 | explorer-indexer | L3 | `explorer-indexer-mvp/` | CT PASS |
| C-13 | wallet-observer | L3 | `wallet-observer-mvp/` | CT PASS |
| C-14 | rigor-qt-prototype | L3 | `rigor-qt-prototype/` | PARTIAL |
| C-15 | rigorchain-site | L3 | `web/rigorchain-site/` | Active |
| C-16 | PROOF-034 tooling | L4 | `core/contrib/tools/golden_vector_*` | Procedure only |
| C-17 | sim core | L4 | `sim/core/` | Implemented |
| C-18 | replay CI | L4 | `ci/replay/` | Complete |
| C-19 | GUI replay viewer | L4 | `gui/replay_viewer/` | Implemented |
| C-20 | Coq formal corpus | L4 | `docs/formal/phase4_coq/` | Terminal |

---

## 9. Interface Contract Summary

### 9.1 GBT TemplateRecord (L1 → L2)

Critical fields consumed by pool:

| Field | Consumer | Validation |
|-------|----------|------------|
| `height` | all pool crates | V-04 |
| `previousblockhash` | assembler, validator | V-01 |
| `bits` | PoW only | Not for draw |
| `prev_nbits` (RIGOR ext) | winner-engine σ | FVR-M |
| `coinbasevalue` | assembler ORU | V-07 |
| `default_convergence_commitment` | assembler CVGE | V-09 |
| `transactions[]` | F extraction | V-03 |

### 9.2 PublishedJobSnapshot (L2 internal)

| Field | Consumer |
|-------|----------|
| `job_id` | stratum, miner-agent |
| `convergence_root` | share-validator |
| `mint_required` | mint-engine, validator |
| `preview_winner_txid` | stratum envelope, mint-engine |
| `merkle_branch` | miner-agent |

### 9.3 Explorer Block Record (L3 derived)

| Field | Source |
|-------|--------|
| `hash`, `height`, `time` | rigord getblock |
| `rigor_winner` | rigord extended field |
| `rigor_res_minted` | rigord extended field |
| `convergence_panel` | getconvergenceresult or indexed |
| `equivalence_claimed` | Hardcoded false in status |

---

## 10. Deployment Topology (Reference)

### 10.1 Closed Testnet (Current GO)

```
NODE-A/B/C (rigord mesh)
        │
        ├── pool pilot (single operator)
        │     ├── stratum-server :3333
        │     └── rigor-miner-agent × N
        │
        ├── explorer-indexer-mvp
        └── rigor-qt-prototype / wallet-observer
```

### 10.2 PROOF-034 Archive (Planned)

```
Single regtest rigord (archive datadir)
        │
        ├── golden_vector_materializer (background mine)
        ├── golden_vector_verify (milestone checks)
        └── runtime/proof034/observer (passive monitor)
```

### 10.3 Public Testnet (Conditional)

Adds: DNS seeds, public Stratum, TLS explorer, hardened rate limits — several gates PARTIAL/NOT EXECUTED.

---

## 11. Coupling Heat Map

| Coupling | Strength | Risk |
|----------|----------|------|
| pool ↔ rigord GBT schema | **High** | Template drift breaks mining |
| winner-engine ↔ consensus draw | **High** | Must stay golden-synced |
| explorer ↔ RPC schema | Medium | Extended fields dependency |
| website ↔ explorer API | Medium | Stale status if indexer lags |
| SIM ↔ JSONL schema | Medium | Telemetry format versioning |
| Coq ↔ runtime | **Low** | Documentation reference only |
| GUI ↔ convergence recompute | **Forbidden** | V-07 violation if implemented |

---

## 12. Related Documents

| Document | Path |
|----------|------|
| Global architecture report | [RIGOR_GLOBAL_ARCHITECTURE_REPORT.md](./RIGOR_GLOBAL_ARCHITECTURE_REPORT.md) |
| Execution model | [RIGOR_EXECUTION_MODEL.md](./RIGOR_EXECUTION_MODEL.md) |
| Determinism map | [RIGOR_DETERMINISM_MAP.md](./RIGOR_DETERMINISM_MAP.md) |
| Pool stratum architecture | [../../pool/stratum-server/docs/ARCHITECTURE.md](../../pool/stratum-server/docs/ARCHITECTURE.md) |
| Winner engine architecture | [../../pool/winner-engine/ARCHITECTURE.md](../../pool/winner-engine/ARCHITECTURE.md) |
| Explorer architecture | [../explorer/](../explorer) |

---

*End of RIGOR Component Interaction Graph*

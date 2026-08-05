# RIGOR Explorer / Indexer Architecture

**Task:** `TASK-EXPLORER-BOOTSTRAP-001`  
**Mode:** INDEXER / EXPLORER / READ-ONLY CHAIN INTERPRETATION  
**Protocol baseline:** commit `4437597197aa713396a123673cc36606346ad523`, tag `mainnet-freeze-rfc014-2026-05-25`  
**Scope:** Closed testnet foundation only — no public deployment in this task.

---

## 1. Purpose

Provide a **protocol-correct**, **read-only** indexing and visibility layer for the RIGOR `test` chain during closed testnet operation. The stack MUST mirror node-reported state; it MUST NOT implement alternate consensus rules, probability models, or asset inference.

**In scope:** ORU, RES, convergence, convergence lineage, replay-field display, witness-v1 RES mint interpretation.  
**Out of scope:** Production UI polish, exchange APIs, analytics dashboards, account models, NFT/token layers.

---

## 2. Design principles

| Principle | Explorer obligation |
|-----------|---------------------|
| Script-as-asset | Classify ORU/RES from `scriptPubKey` / RPC decode — never from a parallel `asset` column in UTXO storage |
| Node is truth | All indexed facts derivable from STABLE RPC + frozen decode rules |
| txid identity | Convergence, draw, F-set, convergence lineage keyed by **txid** |
| No consensus in API | REST/API layer is presentation + query only |
| Deterministic ordering | API sorts by `(height ASC, txid ASC)` — never hash-map iteration order |

---

## 3. Indexer topology

```
┌──────────────────────────────────────────────────────────────────┐
│  Operator browser (VPN / SSH tunnel during closed phase)          │
└────────────────────────────┬─────────────────────────────────────┘
                             │ HTTPS (optional static UI)
┌────────────────────────────▼─────────────────────────────────────┐
│  Explorer UI (minimal pages — see EXPLORER_UI_MINIMUM_SPEC.md)   │
└────────────────────────────┬─────────────────────────────────────┘
                             │ REST (read-only)
┌────────────────────────────▼─────────────────────────────────────┐
│  Indexer API service                                              │
│  - block importer                                                   │
│  - tx decoder cache                                                 │
│  - convergence enricher (getconvergenceresult)                      │
│  - convergence lineage linker                                       │
│  - mismatch alerter (root committed vs computed)                    │
└────────────────────────────┬─────────────────────────────────────┘
                             │ JSON-RPC 1.1 (loopback)
┌────────────────────────────▼─────────────────────────────────────┐
│  rigord (NODE-C pattern)                                            │
│  -testnet, txindex=1, no wallet, rpcbind=127.0.0.1                  │
└────────────────────────────────────────────────────────────────────┘
```

### 3.1 Component responsibilities

| Component | Responsibility | MUST NOT |
|-----------|----------------|----------|
| **rigord** | Full validation, chainstate, STABLE RPC | Be replaced by indexer for validity |
| **Importer** | Persist blocks/headers/txs from RPC | Recompute draw winner independently of node |
| **Enricher** | Attach convergence rows per block | Invent eligibility without `getconvergenceresult` |
| **API** | Serve sorted, labeled ORU/RES views | Expose `invalidateblock` or wallet keys |
| **UI** | Display indexed fields + disclaimers | Merge ORU+RES balances |

### 3.2 Deployment placement (closed testnet)

Per closed topology: **NODE-C** hosts `rigord` + indexer + API. NODE-A holds spendable keys; NODE-B mines. Explorer MUST NOT share a process with NODE-A wallet.

---

## 4. RPC ingestion model

### 4.1 Primary ingestion paths

| Phase | RPC methods | Notes |
|-------|-------------|-------|
| **Initial sync** | `getblockhash`, `getblock` (verbosity 2), `getblockheader` | Height-ordered import genesis → tip |
| **Convergence backfill** | `getconvergenceresult` | One call per block (or height); store full candidate array |
| **Incremental** | `getbestblockhash`, `getblock` | On new tip; detect hash change at same height → reorg |
| **Tx detail** | `getrawtransaction` (verbose) | Requires `txindex=1` |
| **RES history** | `listconvergencehistory` | Cross-check mint block list |
| **Chain status** | `getblockchaininfo`, `getconvergencestatus` | Tip metrics, cumulative convergence |

### 4.2 Forbidden ingestion

- `getconvergeinfo` (UNSTABLE stub)
- Retired fields: `rigor_target`, `res_draw_target`, `res_target_ctx`
- Nonexistent RPC: `getconvergencedrawcandidates`
- Wallet RPC on indexer node (no wallet loaded)

### 4.3 Decode path

Use `decoderawtransaction` / verbose `getrawtransaction` for script and witness display. Asset labels MUST follow the same rules as `AssetTypeFromScriptPubKey` (witness v0 → ORU; v1 CVG2 → ORU convergence; other v1 32-byte → RES).

---

## 5. Polling vs ZMQ

| Mechanism | Recommendation (closed testnet) | Rationale |
|-----------|----------------------------------|-----------|
| **HTTP polling** | **Default** — 30 s tip poll + immediate import on hash change | Simple, auditable, no extra ports; sufficient for 60 s block spacing |
| **ZMQ** | **Optional** later (`hashblock`, `hashtx`) | Reduces latency; requires `zmqpub*` config and firewall rules; not required for foundation |

**Foundation rule:** Implement correctness on polling first. ZMQ is an optimization only if it does not change import ordering or introduce race-driven nondeterminism in the API (serialize tip updates behind a single importer mutex).

---

## 6. Database architecture

### 6.1 Logical schemas

| Store | Contents | Primary keys |
|-------|----------|--------------|
| `blocks` | Header fields + Rigor extensions | `block_hash` |
| `transactions` | Raw hex, txid, wtxid, block link | `txid` |
| `outputs` | Per-vout script, value, inferred asset | `(txid, vout)` |
| `convergence_results` | Full `getconvergenceresult` JSON per block | `block_hash` |
| `convergence_candidates` | Normalized candidate rows | `(block_hash, txid)` |
| `convergence_events` | ORU convergence txs (CVG2) | `(block_hash, convergence_txid)` |
| `res_mints` | Links winner convergence → RES mint tx | `(mint_txid, mint_vout_index)` |
| `chain_meta` | Importer tip, last good height | singleton |

### 6.2 Engine choice (summary)

| Engine | Closed testnet | Public testnet |
|--------|----------------|----------------|
| **SQLite** | Acceptable (< ~50k blocks) | Risk under concurrent API load |
| **PostgreSQL** | Recommended if MON-1 + API co-located | Preferred for HA read replicas |

See `CLOSED_TESTNET_EXPLORER_DEPLOYMENT.md` for sizing.

### 6.3 Immutability

Blocks at height `h` are immutable **until reorg**. On reorg, delete rows with `height > fork_point` (see `EXPLORER_REORG_STRATEGY.md`) — never UPDATE in place without audit log.

---

## 7. txid vs wtxid policy

| Use case | Identifier | Rule |
|----------|------------|------|
| Primary keys, URLs, convergence set F | **txid** | Mandatory |
| Draw winner, candidate list | **txid** | Must match `getconvergenceresult.candidates[].txid` |
| Convergence lineage edges | **txid** | Convergence tx identity |
| Mempool display (if added) | wtxid optional secondary | Label as “witness id”; never primary |
| Foreign keys / JOINs | **txid** only | wtxid display-only column |

**Regression lock (P3):** For a known RES block, indexed eligible txid set MUST equal RPC `candidates[].txid` where `draw_eligible=true`.

---

## 8. Reorg handling model (summary)

| Parameter | Value / policy |
|-----------|----------------|
| Bind depth (consensus) | `k = 10` (`CONVERGENCE_DRAW_BIND_DEPTH`) |
| Closed-testnet reorg test target | Up to **6 blocks** (ops runbook); indexer MUST handle **≥ 10** rollback depth |
| Detection | `getbestblockhash` change or same-height hash mismatch |
| Action | Truncate DB above fork; re-import; re-run `getconvergenceresult` for affected range |

Full procedure: `EXPLORER_REORG_STRATEGY.md`.

---

## 9. Cross-references

| Document | Topic |
|----------|-------|
| `RIGOR_CHAIN_DATA_MODEL.md` | Canonical entities |
| `EXPLORER_RPC_COMPATIBILITY_MATRIX.md` | RPC tiers |
| `RES_INDEXING_RULES.md` | RES/ORU/convergence indexing |
| `WITNESS_V1_DISPLAY_RULES.md` | PCORB display |
| `EXPLORER_SECURITY_BOUNDARIES.md` | Forbidden behaviors |
| `EXPLORER_GO_NO_GO.md` | Launch gates |

**Upstream (repo root):** `EXPLORER_RPC_STABILITY_MATRIX.md`, `CLOSED_TESTNET_EXPLORER_BOOTSTRAP.md`, `CLOSED_TESTNET_NODE_TOPOLOGY.md`.

---

## 10. Document metadata

| Field | Value |
|-------|--------|
| Risk classification | **MEDIUM** (mis-indexing); **HIGH** if wtxid used as identity |
| Explorer classification | **FOUNDATION-ARCHITECTURE** |
| Consensus impact | **NONE** |
| Replay impact | **INDIRECT** — wrong display mimics consensus failure |
| Determinism impact | **INDIRECT** — API must use explicit sorts |
| Indexing risk | **MEDIUM** |
| Minimal safe recommendation | Poll NODE-C; index `getconvergenceresult`; txid-only keys; alert on root mismatch |

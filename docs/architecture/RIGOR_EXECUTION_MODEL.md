# RIGOR System Execution Model

**Task:** `TASK-ARCH-001` (deliverable)  
**Version:** 1.0  
**Date:** 2026-06-20  
**Type:** Read-only analysis — **no code changes**

| Field | Value |
|-------|-------|
| Consensus Impact | **NONE** |
| `equivalence_claimed` | **false** |

This document defines how RIGOR executes work end-to-end: state transitions, process boundaries, and the forward/reverse data paths through the stack. All descriptions align with implemented code in `core/` and `pool/`.

---

## 1. Execution Philosophy

RIGOR execution follows three rules:

1. **UTXO-only state** — every asset movement is spend-old / create-new; no account table is authoritative.
2. **Script law** — validation interprets `scriptPubKey` + witness; no parallel asset storage in `CTxOut`.
3. **Single winner authority** — `GetBlockWinner` in `convergence_logic.cpp` is the only consensus draw API; pool replay is operational mirror only.

Execution splits into **consensus execution** (Layer 1, inside `ConnectBlock`) and **operational execution** (Layer 2, mining/pool/orchestration). Observers (Layer 3) and lab systems (Layer 4) never participate in state transition.

---

## 2. Consensus State Machine (Layer 1)

### 2.1 Block Connection (`ConnectBlock`)

Authoritative path: `core/src/validation.cpp::ConnectBlock`.

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. Header / PoW (OpuHash + ASERT)                               │
├─────────────────────────────────────────────────────────────────┤
│ 2. Build view_at_block_entry (UTXO at block entry)              │
├─────────────────────────────────────────────────────────────────┤
│ 3. F ← ListConvergenceRootCommitmentTxids(view)                 │
│    weights[i] ← GetConvergenceAmount(tx_i)                      │
├─────────────────────────────────────────────────────────────────┤
│ 4. C ← ComputeConvergenceRootFromEligibilityTxids(F)            │
├─────────────────────────────────────────────────────────────────┤
│ 5. Validate coinbase scriptSig binds C (BIP34 height + root)    │
├─────────────────────────────────────────────────────────────────┤
│ 6. bind ← B_{h-10} or RFC000 bootstrap                          │
│    target_prev ← pindexPrev->nBits                              │
│    σ_i ← ComputeSigmaHash(txid_i, C, bind, target_prev, h)     │
├─────────────────────────────────────────────────────────────────┤
│ 7. eligible[i] ← σ_i < T · f_i   (T = 2^240)                   │
│    winner ← argmin eligible (uint512 + txid tie-break)          │
├─────────────────────────────────────────────────────────────────┤
│ 8. For each tx in block:                                        │
│      CheckTxInputs · script verify · ConnectEvaluateResMintTx     │
├─────────────────────────────────────────────────────────────────┤
│ 9. EnforceResMintUnique · VerifyCoinbaseConvergenceAndResLayout │
├─────────────────────────────────────────────────────────────────┤
│ 10. Supply update:                                              │
│       S_ORU += subsidy - Σconverge                              │
│       S_RES += M(h) · RES_REWARD_AMOUNT                         │
│       RES cap check                                             │
├─────────────────────────────────────────────────────────────────┤
│ 11. UpdateCoins (skip convergence CVG2 outputs)                │
└─────────────────────────────────────────────────────────────────┘
```

Canonical ordering is enforced by `core/doc/rigor_consensus_pipeline.md`. Steps MUST NOT be reordered.

### 2.2 Transaction Classification (StateTag)

Before script paths execute, `statetag.cpp` classifies each transaction:

| Tag | Meaning | Domain |
|-----|---------|--------|
| ORU transfer | Standard witness v0 spend/create | ORU |
| Convergence | CVG2 burn | ORU |
| RES mint | ORU inputs + RES output + PCORB | ORU → RES creation |
| RES spend | RES inputs + PCORB claim | RES |

Classification is rule lookup, not EVM-style execution.

### 2.3 UTXO Update Semantics

| Output type | UTXO behavior |
|-------------|---------------|
| Witness v0 ORU | Created on connect; removed on spend |
| Witness v1 RES | Created on mint/spend path; standard UTXO |
| CVG2 convergence | **Never** inserted into UTXO set |
| OP_RETURN | Never in UTXO set; value must be 0 |

### 2.4 Reorg Handling

On disconnect (`DisconnectBlock`):

- Per-block caches on `CBlockIndex` reset (`m_cumulative_convergence`, RES mint flags)
- UTXO rolled back via undo data
- Winner recomputed from ancestors on reconnect

Pool Layer 2 mirrors reorg: `handle_tip_change` invalidates templates, jobs, winner provisional state, mint authorization.

---

## 3. Operational Execution Model (Layer 2)

### 3.1 Process Topology

```
┌──────────────┐     RPC      ┌─────────────┐
│   rigord     │◄────────────►│  rigor-gbt  │
│  (C++ node)  │              │  (Rust)     │
└──────┬───────┘              └──────┬──────┘
       │                             │
       │ GBT TemplateRecord          │
       ▼                             ▼
┌──────────────┐              ┌─────────────┐
│   mempool    │              │block-assembl│
│  (in-node)   │              │    er       │
└──────────────┘              └──────┬──────┘
                                     │ PublishedJobSnapshot
                                     ▼
                              ┌─────────────┐
                              │  winner-    │
                              │  engine     │
                              └──────┬──────┘
                                     │ preview + authorize
                                     ▼
                              ┌─────────────┐     Stratum    ┌──────────────┐
                              │  stratum-   │◄────────────►│ rigor-miner- │
                              │  server     │              │    agent     │
                              └──────┬──────┘              └──────────────┘
                                     │ block candidate
                                     ▼
                              ┌─────────────┐
                              │ mint-engine │
                              └──────┬──────┘
                                     │ submitblock
                                     ▼
                              ┌──────────────┐
                              │   rigord     │
                              │ ConnectBlock │
                              └──────────────┘
```

Orchestration entry: `pool/pilot/src/lib.rs` — `PoolPilot::poll_once`.

### 3.2 rigord Block Template Production

`BlockAssembler::CreateNewBlock` (`core/src/node/miner.cpp`):

1. Compute ORU subsidy via `GetBlockSubsidy`
2. Collect mempool txs (ORU-domain fees to coinbase)
3. Build coinbase: ORU `vout[0]`, CVGE OP_RETURN commitment, witness commitment
4. Embed convergence root placeholder in coinbase `scriptSig` (updated when body txs known)
5. Expose via `getblocktemplate` with Rigor extensions:
   - `default_witness_commitment`
   - `default_convergence_commitment`
   - `coinbasevalue` = ORU subsidy + ORU fees only

### 3.3 GBT Adapter Cycle

`rigor-gbt` (`pool/rigor-gbt/src/adapter.rs`):

| Step | Action |
|------|--------|
| Poll | `getblockchaininfo` — skip if `initialblockdownload=true` |
| Fetch | `getblocktemplate {"rules":["segwit"]}` |
| Validate | V-01..V-10 structural checks |
| Cache | Immutable `TemplateRecord` in `TemplateCache` |
| Tip watch | `getbestblockhash` — invalidate on reorg |

### 3.4 Block Assembler Job Publication

`block-assembler` (`pool/block-assembler/src/publisher.rs`):

1. Parse template convergence candidates into **F** mirror
2. Compute C via `rigor-convergence-root` crate
3. Assemble coinbase: BIP34 height, CVGE script, RIGOR draw-target tag + `prev_nbits`
4. Reserve body-mint placeholder slot when `mint_required`
5. Compute merkle branches → `PublishedJobSnapshot`
6. Publish to stratum-server job cache

### 3.5 Winner Engine Lifecycle

`winner-engine` state machine:

```
PROVISIONAL → AUTHORIZED → CONFIRMED
     │              │
     └─ invalidate ─┴─ revoke (reorg)
```

| Phase | Function | Output |
|-------|----------|--------|
| Preview | `preview_winner` / `replay_convergence_draw` | Winner txid, eligibility vector |
| Register | `register_published_job` | Job-bound draw context |
| Authorize | `authorize_mint_for_block` | `MintAuthorizationToken` (AUTH-01..15) |
| Confirm | `confirm_winner_for_block` | Post-submit validation |
| Invalidate | `invalidate_jobs` on reorg | Revoke provisional/authorized |

Draw replay uses **identical** F, C, σ, inequality, and argmin logic as rigord (golden-tested 28/28 + 122/122).

### 3.6 Stratum Share → Block Path

1. Miner receives `mining.notify` job (extranonce, merkle branch, nbits)
2. Miner submits `mining.submit` (nonce, ntime, extranonce2)
3. `share-validator` checks:
   - OpuHash share/block target
   - Replay guard (duplicate share rejection)
   - Convergence root match vs template
   - CVGE / RIGOR commitment integrity
   - RES mint placeholder slot integrity
4. On block target hit → `handle_block_candidate`

### 3.7 Mint Engine Block Assembly

When block found and winner authorized:

1. `mint_for_authorized_winner` builds Witness v1 RES tx (100 RES = 10,000 atoms)
2. Inserts PCORB witness binding to winner convergence txid
3. Assembles full block with mint in reserved slot
4. `submitblock` to rigord
5. rigord runs ConnectBlock — **final validity decision**

Mint state: `Built → Submitted | Revoked | Orphaned`

### 3.8 Mining Work Proof (OpuHash)

PoW uses OpuHash (not SHA256d). Miner agent declares `opushash` capability at `mining.subscribe`. Share difficulty set via `mining.set_difficulty`.

---

## 4. Observer Execution Model (Layer 3)

Observers execute **read pipelines only**. No mutation of consensus or verification state.

### 4.1 Explorer Indexer Pipeline

```
rigord RPC                    PostgreSQL              HTTP clients
    │                              │                      │
    ├─ getblock ──────────────────►│ blocks table         │
    ├─ getrawtransaction ─────────►│ txs table            │
    ├─ getconvergenceresult ──────►│ convergence_panels   │
    └─ periodic tip poll ─────────►│ chain_state          │
                                   │                      │
                                   └──── REST /api/* ────►│
```

Indexer: `explorer-indexer-mvp/` — Express server, import loop, rate limits.

### 4.2 GUI / Website Read Pipeline

```
User browser
    │
    ├─► wallet-observer-mvp /api/observer/* ──► explorer API
    │
    ├─► rigor-qt-prototype ──► RPC + explorer HTTP
    │
    └─► web/rigorchain-site ──► VITE_RPC_PROXY + VITE_EXPLORER_API
                                      │
                                      ▼
                               rigord (read-only allowlist)
                               explorer-indexer-mvp
```

RPC allowlist (website): `getblockchaininfo`, `getnetworkinfo`, `getconvergencestatus`, `getconvergenceresult`, `getresmintstatus`.

**Forbidden RPCs for observers:** signing, wallet send, `generate`, `submitblock`, mining control.

### 4.3 PROOF-034 Passive Observer

`runtime/proof034/observer/` — Python process:

- Polls `getblockcount`, `getconvergencestatus`
- Runs golden vector harness against archive fixtures
- Writes `.runtime/proof034/proof034_observation.json`
- **Never** controls mining; fails silently on RPC loss

---

## 5. Experimental Execution Model (Layer 4)

### 5.1 PROOF-034 Runtime Archive

Execution domain B — uses normal frozen consensus on designated regtest datadir:

```
rigord -regtest (archive datadir)
    │
    ▼
golden_vector_materializer.sh  ──► mine to milestone heights
    │
    ▼
golden_vector_verify.sh --require-full  ──► harness report
    │
    ▼
archive/proof-034/  (SHA256 manifest, JSON artifacts)
```

Target: h ≥ **2,100,001** (2.1M+1). Current partial state: 9/12 vectors at h ≤ 501.

### 5.2 Simulation Pipeline (SIM-004 → SIM-008)

Execution domain C — **no live node connection**:

```
JSONL telemetry (pool metrics)
    │
    ▼
sim/core ReplayKernel  (SIM-004)
    │
    ▼
sim/emulator SystemEmulator  (SIM-005)
    │
    ▼
sim/proof_validator  (SIM-006)  ← fixtures from archive
    │
    ▼
ci/replay/pipeline.py  (SIM-008)  → pinned output hashes
```

SIM-006 validates milestone vectors offline (`live_execution_required: false`).

### 5.3 GUI Replay Viewer (SIM-007)

Loads SIM-004/005/006 output artifacts only. No rigord connection. Presentation domain.

---

## 6. Forward Path Trace (Complete)

| Step | Component | Action | Authority |
|------|-----------|--------|-----------|
| 1 | rigord | Maintains chain + mempool | Consensus |
| 2 | rigord | `getblocktemplate` | Consensus |
| 3 | rigor-gbt | Poll, validate, cache template | Operational |
| 4 | block-assembler | Build job, compute C, reserve mint slot | Operational |
| 5 | winner-engine | Preview draw, attach to job envelope | Operational (mirror) |
| 6 | stratum-server | Push `mining.notify` | Operational |
| 7 | rigor-miner-agent | PoW scan, `mining.submit` | Operational |
| 8 | share-validator | Validate share/block | Operational |
| 9 | winner-engine | Confirm winner for block | Operational |
| 10 | mint-engine | Build RES mint tx + block | Operational |
| 11 | rigord | `submitblock` → ConnectBlock | **Consensus** |
| 12 | rigord | Update UTXO, chain index, supply caches | Consensus |
| 13 | explorer-indexer | Import block, index convergence | Observer |
| 14 | GUI / website | Display block, winner, RES mint | Observer |

**Consensus boundary:** Step 11. All prior steps are candidate construction; rigord may reject.

---

## 7. Reverse Path Trace (Complete)

| Step | Component | Action | Authority |
|------|-----------|--------|-----------|
| 1 | User | Opens GUI / website / wallet observer | — |
| 2 | Presentation | HTTP request to observer API or RPC proxy | Observer |
| 3 | Explorer API | Query PostgreSQL cache | Observer (derived) |
| 3a | RPC proxy | Forward allowlisted method to rigord | Observer (live) |
| 4 | rigord | Return chain tip, block, convergence result | Consensus (read) |
| 5 | Explorer | Enrich with lineage, RES events, panels | Observer (derived) |
| 6 | GUI | Render dashboard, convergence tab, inspector | Observer |

**Staleness rule:** Indexed data may lag tip; live RPC is authoritative for tip height and convergence status. Presentation MUST label provenance per Execution Domain Model §5.6.

---

## 8. Runtime State Stores

### 8.1 Consensus (rigord)

| Store | Mutability | Scope |
|-------|------------|-------|
| UTXO set (`CCoinsViewCache`) | Per ConnectBlock | Global chain |
| Mempool | Pre-block | Node-local |
| `CBlockIndex` caches | Per block | Convergence sum, RES flags |
| Chain tip | Atomic advance | Global |

No global mutable draw state — winner recomputed from block content + parent context.

### 8.2 Pool (operational)

| Store | Location | Reorg behavior |
|-------|----------|----------------|
| `TemplateCache` | rigor-gbt | Invalidate ACTIVE/STALE |
| `JobCache` | block-assembler | Clear active list |
| `WinnerRuntime` | winner-engine | `invalidate_jobs` |
| `MintStateStore` | mint-engine | `revoke_for_jobs` |
| `ReplayGuard` | share-validator | Reject stale job shares |
| `SessionManager` | stratum-server | Per-connection |
| PostgreSQL (optional) | rigor-pool-pg | Recovery persistence |

### 8.3 Observer

| Store | Location | Authority |
|-------|----------|-----------|
| PostgreSQL explorer DB | explorer-indexer-mvp | Derived index |
| Browser session state | GUI / website | Ephemeral |
| `.runtime/proof034/` | PROOF-034 observer | Verification artifacts |

---

## 9. Failure Modes & Recovery

| Failure | Layer | Recovery |
|---------|-------|----------|
| Invalid block submit | 1 | rigord rejects; mint-engine marks Orphaned |
| Reorg | 1+2 | DisconnectBlock + pool invalidate cascade |
| GBT stale | 2 | rigor-gbt poll catches tip change |
| Draw preview mismatch | 2 | Block rejected at share-validator; no submit |
| RPC unavailable | 3 | Observer degrades; PROOF-034 observer silent fail |
| Archive mining interrupt | 4 | Resume from datadir; materializer idempotent |

---

## 10. Execution Invariants

| ID | Invariant |
|----|-----------|
| EX-01 | ConnectBlock order fixed per rigor_consensus_pipeline.md |
| EX-02 | Winner computed once via GetBlockWinner API |
| EX-03 | Convergence outputs never enter UTXO |
| EX-04 | Pool submitblock does not bypass validation |
| EX-05 | Observers never call mutating RPC |
| EX-06 | Simulation never connects to live consensus datadir |
| EX-07 | PROOF-034 uses frozen consensus, not alternate rules |
| EX-08 | `equivalence_claimed = false` at all execution boundaries |

---

*End of RIGOR System Execution Model*

# RIGOR Global Architecture Report

**Task:** `TASK-ARCH-001`  
**Version:** 1.0  
**Date:** 2026-06-20  
**Type:** Read-only system analysis — **no code, consensus, or protocol changes**  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`

| Field | Value |
|-------|-------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| `equivalence_claimed` | **false** |

This document is the authoritative system-level architecture review of the RIGOR stack, derived strictly from existing source code, tests, and documentation. It serves as the foundation for Phase-3 full system simulation (SIM-004 → SIM-010), PROOF-034 final interpretation, official GitHub release cleanup, public website technical narrative, and long-term mainnet readiness evaluation.

---

## 1. Executive Summary

RIGOR is a **script-native, self-proving cryptoeconomic state machine** built on a Bitcoin Core–derived node (`rigord`). Its architecture rests on a single constitutional principle:

> **Script-as-Asset** — asset type is inferred from `scriptPubKey` at parse time; `CTxOut` storage is unchanged.

The system implements a closed dual-asset economy:

| Asset | Role | Script container |
|-------|------|------------------|
| **ORU** (Operational Resource Unit) | PoW subsidy, fees, convergence (burn) domain | Witness v0; CVG2 witness v1 for convergence |
| **RES** (Reward / Settlement) | Selection-derived issuance, capped supply | Witness v1 (32-byte program, no CVG2 prefix) |

**Convergence** (implementation term; replaces whitepaper FORGE/BURN semantics) destroys ORU via CVG2 outputs. A deterministic draw over eligible convergence transactions selects a winner; a **PCORB** witness authorizes exactly one RES mint per block when predicates pass.

The stack separates into four layers (§2). Consensus truth lives exclusively in Layer 1. Layer 2 operationalizes mining and pool infrastructure as a **non-consensus mirror** of frozen rules. Layers 3 and 4 observe and experiment without authority.

**Critical invariant (global):** Formal proof (Coq), runtime archive verification (PROOF-034), and simulation (SIM-004+) are **linked but not equivalent** verification layers. `equivalence_claimed` remains **false** across all surfaces.

---

## 2. System Layer Model

### 2.1 Layer 1 — Protocol Layer (Frozen Consensus Rules)

**Authority:** Sole source of on-chain truth.  
**Status:** **STRICTLY FROZEN** under RFC-000 … RFC-015 and `rigor_consensus_lock.h`.

| Concern | Implementation |
|---------|----------------|
| Block validation | `core/src/validation.cpp` — `ConnectBlock` 15-step state machine |
| Script / asset inference | `core/src/primitives/transaction.cpp`, `core/src/script/standard.cpp` |
| Convergence set **F**, root **C**, σ, winner | `core/src/consensus/convergence_root.*`, `convergence_logic.*`, `selection.*` |
| RES mint / PCORB | `core/src/consensus/rigor_res.*`, `res_proof.*` |
| Supply equations | RFC-002; enforced in ConnectBlock; cached on `CBlockIndex` |
| PoW | OpuHash + ASERT (`CheckProofOfWork`) |
| Frozen constants | `core/src/consensus/rigor_consensus_lock.h` |

**Constitutional constraints (non-negotiable):**

1. `CTxOut = (nValue, scriptPubKey)` — no persisted asset field.
2. Only Witness v0, Witness v1 (32-byte), and OP_RETURN outputs permitted.
3. No Taproot/BIP341 semantics; StateTag classification, not script VM execution.
4. Convergence outputs excluded from UTXO set (burn on connect).
5. At most one scoring RES mint per block; RES never from coinbase body alone.
6. Integer-only arithmetic; `RES_PROBABILITY_TARGET = 2^240` for draw inequality.

**Normative references:** `docs/rfcs/RFC-000-consensus-constitution.md`, `RFC-000A-rigor-implementation-bible.md`, `RFC-001-deterministic-selection.md`, `RFC-002-supply-evolution.md`, `core/doc/rigor_consensus_pipeline.md`, `core/doc/convergence-consensus-v1.md`.

### 2.2 Layer 2 — Execution Layer (rigord, Mining, ORU/RES Logic)

**Authority:** Operationalizes frozen rules; does not define them.  
**Status:** Closed testnet **GO**; public testnet **CONDITIONAL GO** (see release gate matrix).

| Subsystem | Location | Role |
|-----------|----------|------|
| Node daemon | `core/src/bitcoind.cpp` → `rigord` | Validation, mempool, GBT, RPC |
| Block assembly | `core/src/node/miner.cpp` | Coinbase ORU subsidy, CVGE commitment, convergence root binding |
| Pool GBT adapter | `pool/rigor-gbt/` | Polls `getblocktemplate`, validates V-01..V-10, caches templates |
| Block assembler | `pool/block-assembler/` | Job construction, convergence root, mint placeholder |
| Stratum server | `pool/stratum-server/` | Work distribution, share validation orchestration |
| Winner engine | `pool/winner-engine/` | Off-chain draw replay, mint authorization (AUTH-01..15) |
| Mint engine | `pool/mint-engine/` | RES body tx build, block package, `submitblock` |
| Miner agent | `pool/rigor-miner-agent/` | Stratum client, OpuHash share submission |
| Pool pilot | `pool/pilot/` | End-to-end closed-testnet harness |
| Shared root lib | `pool/rigor-convergence-root/` | Rust port of convergence root computation |

Layer 2 explicitly declares **`equivalence_claimed: false`** until golden certification completes. Pool draw replay mirrors rigord logic for preview and authorization; **rigord `GetBlockWinner` is the only consensus authority**.

### 2.3 Layer 3 — Observer Layer (GUI, Explorer, Dashboard)

**Authority:** Display and index only; no consensus or verification mutation.  
**Status:** Closed testnet MVP complete; public hardening partial.

| Component | Path | Data source |
|-----------|------|-------------|
| Explorer indexer | `explorer-indexer-mvp/` | rigord RPC → PostgreSQL → REST |
| Wallet observer | `wallet-observer-mvp/` | Explorer API proxy + optional RPC |
| Qt GUI prototype | `rigor-qt-prototype/` | RPC + explorer HTTP |
| Production Qt | `core/src/qt/` | Full node wallet (separate from observer prototype) |
| Public website | `web/rigorchain-site/` | RPC proxy + explorer API + verification manifest |
| PROOF-034 passive observer | `runtime/proof034/observer/` | RPC height, golden vector harness, archive SHA256 |
| Ops dashboards | `contrib/testing/ops/` | JSONL telemetry renderers |

All observer surfaces enforce read-only RPC allowlists (no signing, mining, or broadcast). Explorer/indexer MUST NOT recompute convergence as source of truth (Domain D violation V-07).

### 2.4 Layer 4 — Experimental Layer (PROOF-034, Simulation, Lab Tools)

**Authority:** Non-authoritative for consensus; produces evidence and analysis only.

| Track | Location | Status |
|-------|----------|--------|
| PROOF-034 runtime archive | `core/contrib/tools/golden_vector_*.sh`, `docs/proof/PROOF-034-*.md` | Procedure established; **run not yet executed** to h ≥ 2,100,001 |
| SIM-004 replay kernel | `sim/core/` | Implemented |
| SIM-005 system emulator | `sim/emulator/` | Implemented |
| SIM-006 PROOF-034 validator | `sim/proof_validator/` | Implemented (offline fixtures) |
| SIM-007 GUI replay viewer | `gui/replay_viewer/` | Implemented |
| SIM-008 replay CI | `ci/replay/` | Complete (GitHub workflow) |
| SIM-009 unification | `sim/unification/` | Implemented |
| SIM-010 equivalence framework | `docs/sim/FULL_SYSTEM_EQUIVALENCE_FRAMEWORK.md` | Spec only — theoretical lattice |
| Economic simulator | `simulation/` | Offline ORU/RES lifecycle stress |
| Formal proof (Coq) | `docs/formal/phase4_coq/` | Phase-5 terminal (PROOF-032) |

Layer 4 MUST NOT write to Layer 1 consensus state or alter Layer 3 verification execution (Domain C isolation).

### 2.5 Layer ↔ Execution Domain Mapping

The [Execution Domain Model](../constitution/EXECUTION_DOMAIN_MODEL.md) uses domains A–D. Mapping to this task's layers:

| Task layer | Domain | Overlap notes |
|------------|--------|---------------|
| Layer 1 Protocol | **A** Consensus | Identical |
| Layer 2 Execution | **A** (mining/pool as consensus execution) + operational infra | Pool preview/authorization is non-consensus sub-layer |
| Layer 3 Observer | **D** Presentation | Read-only display |
| Layer 4 Experimental | **B** Verification + **C** Simulation | PROOF-034 = Domain B; SIM-* = Domain C |

---

## 3. Asset System

### 3.1 ORU (Operational Resource Unit)

| Property | Value | Source |
|----------|-------|--------|
| Base unit | 1 ORU = `COIN` (10⁸ base units) | `rigor_consensus_lock.h` |
| Initial block subsidy | 1000 ORU | RFC-001 §3.3 |
| Halving interval | 210,000 blocks | RFC-001 |
| Floor subsidy | 125 ORU (max 3 halvings) | `GetBlockSubsidy` |
| Script inference | Witness v0 → ORU | `AssetTypeFromScriptPubKey` |
| Transfer | Standard UTXO spend; ORU-domain inputs only for ORU outputs | `tx_verify.cpp` |

**Issuance:** PoW coinbase only (`ConnectBlock` subsidy check).  
**Destruction (convergence):** CVG2 witness v1 outputs; amount ≥ `MIN_CONVERGENCE_AMOUNT` (1 ORU); excluded from UTXO on connect.

### 3.2 RES (Reward / Settlement Asset)

| Property | Value | Source |
|----------|-------|--------|
| Atom | 1 atom = 0.01 RES | `RES_COIN = 100` atoms per RES |
| Mint reward | 10,000 atoms = 100.00 RES per event | `RES_REWARD_AMOUNT` |
| Max supply | 21,000,000 RES | `RES_MAX_SUPPLY` |
| Script inference | Witness v1 without CVG2 prefix | `AssetTypeFromScriptPubKey` |
| Mint mode | ORU-domain inputs + PCORB witness | `rigor_res.cpp` |
| ~~Claim mode~~ | **Removed (SECURITY-001-A / A-7).** RES-domain inputs can no longer carry a PCORB witness; the script path that accepted one is gone, leaving the 2-item Ed25519 key path as the only way to spend RES. | `rigor_res.cpp` |

**Issuance rules (consensus):**

1. `|F| ≥ CONVERGENCE_DRAW_MIN_TXS_FOR_RES` (1)
2. At least one eligible candidate passes inequality `σ_i < T · f_i` where `T = 2^240`
3. Winner exists (argmin cross-multiply + txid tie-break)
4. Exactly one scoring RES mint body tx with valid `ConvergeProof`
5. `M(h) ∈ {0,1}` — at most one mint per block
6. `S_RES(h) + RES_REWARD_AMOUNT ≤ RES_MAX_SUPPLY`

### 3.3 Convergence (FORGE/BURN Replacement)

| Legacy term | RIGOR implementation |
|-------------|---------------------|
| FORGE | Convergence transaction (`IsConvergenceTx`) |
| BURN / MTH burn | ORU sent to CVG2 witness v1 output |
| MIN_FORGE | `MIN_CONVERGENCE_AMOUNT` = 1 ORU |
| PCORB | `ConvergeProof` witness stack in RES tx |

**Important doc-code note:** RFC-001 §5.2 originally specified OP_RETURN converge destruction. **Implementation uses CVG2 witness burn outputs**, not OP_RETURN destruction. OP_RETURN with `RIGOR` tag carries draw-target commitment only (zero value).

### 3.4 Observer-Only Fields

These exist for indexing, RPC enrichment, and supply mirroring. They **MUST NOT** be read by ConnectBlock validation predicates:

| Field / struct | Location | Purpose |
|----------------|----------|---------|
| `RuntimeSupplyState` | `runtime_supply_state.h` | RFC-002 mirror; observational |
| `m_block_converge_sum`, `m_chain_oru_total`, `m_chain_res_mint_total` | `chain.h` | Per-block / chain caches |
| `coq_supply_field_hook` | `coq_supply_field_hook.h` | Formal verification export; read-only |
| Explorer DB columns | `explorer-indexer-mvp/` | Indexed convergence panels, lineage |
| Pool preview metadata | `winner-engine` envelopes | Stratum job enrichment (non-consensus) |

### 3.5 Determinism Guarantees (Asset Layer)

- Asset type is a pure function of `scriptPubKey` bytes.
- Input domain (`ORU` vs `RES`) derived from prevout script inference only.
- No account balances; all state transitions are UTXO spend/create.
- Supply invariants checked with integer arithmetic at ConnectBlock.

---

## 4. Convergence System

### 4.1 Set F (Eligibility)

A non-coinbase transaction enters **F** iff all hold (`ListConvergenceRootCommitmentTxids`):

1. Exactly one positive CVG2 convergence output (`IsConvergenceTx`)
2. Convergence amount ≥ `MIN_CONVERGENCE_AMOUNT`
3. `CheckTxInputs` succeeds at block-entry view
4. Transaction fee > 0

### 4.2 Convergence Root C

```
if |F| < 2  →  C = 0³² (all-zero uint256)
else        →  C = Hash256( concat( sort(F, memcmp) ) )
```

Committed in coinbase `scriptSig`: BIP34 height + 32-byte push of C.

### 4.3 Sigma (σ) Computation

100-byte preimage (Sigma-CR-01):

```
bind_commit = Hash256( B_{h-10} || LE32(target_prev) )
σ_i         = Hash256( txid_i || C || bind_commit || LE32(height) )
```

- `k = BLOCK_OFFSET_K = 10`; heights 1..9 use frozen bootstrap bind hash.
- `target_prev = pindexPrev->nBits` — **never** current block target (FVR-M).

### 4.4 PCORB Structure

`ConvergeProof` (`res_proof.h`):

| Field | Role |
|-------|------|
| `convergence_txid` | Must equal draw winner txid |
| `convergence_amount_sat` | Winner weight f_i |
| `bind_hash` | bind_commit at height |
| `height`, `target_prev` | Replay domain separation |
| `convergence_root` | Must equal C |
| `sig`, `pubkey` | Ed25519 over σ digest |

Verified by `VerifyConvergeProof` during `ConnectEvaluateResMintTx`.

### 4.5 Winner Selection

```
Eligible iff:  σ_i < RES_PROBABILITY_TARGET · f_i    (T = 2^240)
Winner:        argmin_i (σ_i · f_j < σ_j · f_i)       (uint512)
Tie-break:     smaller txid (memcmp)
```

Single API: `GetBlockWinner` → `CollectConvergenceWinnerTxid`.

### 4.6 RES Mint Triggering

RES mint is permitted when ConnectBlock confirms:

1. Winner txid non-zero
2. Winner ∈ F and passes inequality
3. Body RES mint tx with valid PCORB binding to winner
4. `EnforceResMintUnique` — one scoring mint
5. RES cap not exceeded
6. CVGE coinbase commitment matches computed winner

Mint may appear in coinbase output after validation or as body tx inserted by mint-engine before `submitblock`.

---

## 5. Runtime vs Formal vs Simulation Separation

These three verification tracks are **NOT equivalent**. They form linked layers of evidence with distinct authority scopes.

| Track | What it proves | What it does NOT prove | Authority |
|-------|----------------|------------------------|-----------|
| **Formal proof (Coq)** | Frozen model properties, gate baselines, draw impossibility, CPP-01 non-closability | That regtest mirrors exist at h=630k/2.1M/2.1M+1 | Mathematical specification layer |
| **Runtime archive (PROOF-034)** | 12 golden vectors materialized, harness 9 PASS + 3 PARTIAL + 0 FAIL at h ≥ 2,100,001 | Equivalence, consensus correctness, draw closure | Empirical runtime evidence (Domain B) |
| **Simulation (SIM-004+)** | Deterministic replay of telemetry/fixtures; offline milestone vector checks | Live chain behavior; archive substitution | Lab analysis (Domain C) |

```
Formal (Coq)          Runtime (PROOF-034)         Simulation (SIM)
     │                        │                          │
     │  spec reference        │  live regtest mining     │  JSONL / fixtures
     ▼                        ▼                          ▼
  Phase-5 terminal         9/12 vectors @ h≤501      SIM-004→006 CI green
  PROOF-032/033            RUN NOT EXECUTED to 2.1M+1  equivalence_claimed=false
```

**Explicit non-equivalence:** Pool winner-engine golden replay (122/122 ROOT_EQUIVALENT, 28/28 draw vectors) certifies **operational mirror accuracy**, not consensus equivalence. FVR-M permanently prohibits draw closure claims.

---

## 6. External Interfaces (Summary)

Detailed surfaces documented in [RIGOR_COMPONENT_GRAPH.md](./RIGOR_COMPONENT_GRAPH.md).

| Interface | Protocol | Consumer |
|-----------|----------|----------|
| rigord JSON-RPC | Bitcoin-compatible + Rigor extensions | Pool, explorer, GUI, PROOF-034 |
| Explorer REST | `/api/*` HTTP | Wallet observer, website, Qt prototype |
| Stratum | `mining.*` methods + OpuHash agent | Miner agent |
| GUI observer proxy | Next.js `/api/observer/*` | Wallet observer MVP |
| Website bindings | `VITE_RPC_PROXY_URL`, `VITE_EXPLORER_API_URL` | Public status/explorer client |

Rigor-specific RPCs: `getblockconvergence`, `getconvergenceresult`, `getconvergencestatus`, extended `getblock` fields (`rigor_winner`, `rigor_res_minted`).

---

## 7. End-to-End Execution Flow (Summary)

Full trace in [RIGOR_EXECUTION_MODEL.md](./RIGOR_EXECUTION_MODEL.md).

**Forward (block production):**

```
rigord (GBT) → rigor-gbt → block-assembler → stratum-server → rigor-miner-agent
                    ↓              ↓                ↓
            winner-engine ← convergence root ← share-validator
                    ↓
              mint-engine → submitblock → rigord ConnectBlock
                    ↓
         explorer-indexer → wallet-observer / Qt / website
```

**Reverse (observer read path):**

```
GUI / website → explorer API or RPC proxy → rigord / PostgreSQL cache
```

---

## 8. System Risks & Gaps

### 8.1 Critical Gaps

| Gap | Impact | Evidence |
|-----|--------|----------|
| PROOF-034 archive not executed to h=2,100,001 | Blocks public release push gate | `PROOF-034-FULL-RUNTIME-ARCHIVE-VERIFICATION.md` |
| 3/12 golden vectors NOT_EXECUTED (630k, 2.1M, 2.1M+1) | Milestone supply/draw boundaries unverified at runtime | PROOF-034 §3 |
| Mainnet tier **NO-GO** | No production mainnet launch | `FINAL_RELEASE_GATE_MATRIX.md` |

### 8.2 Partial Subsystems

| Subsystem | Gap |
|-----------|-----|
| Qt GUI prototype | Convergence tab incomplete (CT-11 PARTIAL) |
| Public explorer on website | `/explorer` redirects to `/status`; no embedded block UI |
| `mining.notify.rigor` | Defined on miner-agent; not yet emitted by stratum-server |
| rigor-miner-agent PoW | OpuHash traits stubbed in 022A for some paths |
| PostgreSQL pool persistence | Optional; in-memory default for devtest |

### 8.3 Bottlenecks

| Bottleneck | Description |
|------------|-------------|
| PROOF-034 mining duration | Regtest to h=2,100,001 is compute/time intensive |
| Single-threaded ConnectBlock draw | Full F enumeration per block at validation |
| Explorer indexer sync | RPC polling lag on long chains |
| Template cache churn | Reorg invalidates GBT + job + winner + mint state cascade |

### 8.4 Missing Observability

| Area | Gap |
|------|-----|
| Cross-layer trace correlation | No unified trace ID from share → block → ConnectBlock |
| Real-time draw preview in Stratum | Preview data in internal envelopes only |
| Public RES cap proximity alert | Website status shows milestones; no live cap gauge at scale |
| Simulation ↔ runtime diff dashboard | SIM-009 unification exists; not wired to live ops |

### 8.5 Unreconciled Modules / Doc-Code Divergence

| Item | Issue |
|------|-------|
| Whitepaper FORGE/BURN | Public docs use legacy terms; code uses Convergence/CVG2 |
| RFC-001 OP_RETURN converge | Superseded by CVG2 witness burn in implementation |
| `rigor.cpp::PrepareContext` | Diagnostic path; not aligned with ConnectBlock F set |
| ARCHITECTURE.md L2 "Assets" | Simplified layer model; consensus lives in `core/src/consensus/` not only `consensus_rigor/` |
| EXECUTION_DOMAIN_MODEL §4.7 | Says simulation "NOT YET ACTIVE"; SIM-004–009 are implemented (offline) |

### 8.6 Devtest vs Production Divergence

| Dimension | Devtest | Production target |
|-----------|---------|-------------------|
| Network | Regtest / closed testnet | Public testnet → mainnet |
| Pool persistence | In-memory caches | PostgreSQL (`rigor-pool-pg`) |
| GUI | Observer prototype | Packaged `rigor-qt` + public site |
| Verification | Partial h≤501 archive | Full h≥2,100,001 archive |
| Equivalence claims | Explicitly false everywhere | Must remain false (FVR-M) |

---

## 9. Simulation Readiness Assessment

| Phase-3 task | Readiness | Blocker |
|--------------|-----------|---------|
| SIM-004 replay kernel | **Ready** | None — CI integrated |
| SIM-005 system emulator | **Ready** | None |
| SIM-006 PROOF-034 validator | **Ready** | Offline fixtures; 3 vectors await archive |
| SIM-007 GUI replay viewer | **Ready** | Presentation only |
| SIM-008 replay CI | **Ready** | FAST/FULL/PROOF_REPLAY modes |
| SIM-009 unification | **Ready** | Analytical only; `equivalence_claimed=false` |
| SIM-010 equivalence framework | **Spec only** | No executable proof engine |
| Full system sim (SIM-004→010 integrated) | **Conditional** | PROOF-034 archive completion improves fixture fidelity |

Simulation can proceed in parallel with PROOF-034 archive execution. Simulation output MUST NOT substitute for runtime archive claims.

---

## 10. Major Module Coverage Index

| Module | Layer | Key path | Covered |
|--------|-------|----------|---------|
| ConnectBlock / validation | 1 | `core/src/validation.cpp` | ✓ |
| Convergence root / logic | 1 | `core/src/consensus/convergence_*.cpp` | ✓ |
| RES / PCORB | 1 | `core/src/consensus/rigor_res.*`, `res_proof.*` | ✓ |
| Script / StateTag | 1 | `core/src/script/`, `statetag.*` | ✓ |
| rigord RPC | 1/2 | `core/src/rpc/` | ✓ |
| Block assembler (node) | 2 | `core/src/node/miner.cpp` | ✓ |
| Pool stack | 2 | `pool/*` | ✓ |
| Explorer indexer | 3 | `explorer-indexer-mvp/` | ✓ |
| Wallet observer | 3 | `wallet-observer-mvp/` | ✓ |
| Qt prototype | 3 | `rigor-qt-prototype/` | ✓ |
| Public website | 3 | `web/rigorchain-site/` | ✓ |
| PROOF-034 tooling | 4 | `core/contrib/tools/golden_vector_*.sh` | ✓ |
| Simulation core | 4 | `sim/`, `ci/replay/` | ✓ |
| Formal Coq corpus | 4 | `docs/formal/phase4_coq/` | ✓ |

---

## 11. Success Criteria Verdict

| Field | Requirement | Status |
|-------|-------------|--------|
| Architecture coverage | 100% of major modules | **MET** |
| Determinism model | Explicitly defined | **MET** — see [RIGOR_DETERMINISM_MAP.md](./RIGOR_DETERMINISM_MAP.md) |
| Execution flow | Fully traceable | **MET** — see [RIGOR_EXECUTION_MODEL.md](./RIGOR_EXECUTION_MODEL.md) |
| Layer separation | Clearly defined | **MET** — §2 |
| Simulation readiness | Assessed | **MET** — §9 |
| Protocol impact | NONE | **CONFIRMED** |
| Consensus impact | NONE | **CONFIRMED** |
| Equivalence claim | MUST remain false | **CONFIRMED** |

---

## 12. Related Documents

| Document | Path |
|----------|------|
| Execution model | [RIGOR_EXECUTION_MODEL.md](./RIGOR_EXECUTION_MODEL.md) |
| Determinism map | [RIGOR_DETERMINISM_MAP.md](./RIGOR_DETERMINISM_MAP.md) |
| Component graph | [RIGOR_COMPONENT_GRAPH.md](./RIGOR_COMPONENT_GRAPH.md) |
| Execution domain model | [../constitution/EXECUTION_DOMAIN_MODEL.md](../constitution/EXECUTION_DOMAIN_MODEL.md) |
| PROOF-034 procedure | [../proof/PROOF-034-FULL-RUNTIME-ARCHIVE-VERIFICATION.md](../protocol/proof/PROOF-034-FULL-RUNTIME-ARCHIVE-VERIFICATION.md) |
| Release gate matrix | `../release/FINAL_RELEASE_GATE_MATRIX.md` |
| Consensus pipeline checklist | [../../core/doc/rigor_consensus_pipeline.md](../../core/doc/rigor_consensus_pipeline.md) |

---

*End of RIGOR Global Architecture Report — TASK-ARCH-001*

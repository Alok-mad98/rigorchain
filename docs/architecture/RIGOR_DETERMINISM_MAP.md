# RIGOR Data Flow & Determinism Map

**Task:** `TASK-ARCH-001` (deliverable)  
**Version:** 1.0  
**Date:** 2026-06-20  
**Type:** Read-only analysis — **no code changes**

| Field | Value |
|-------|-------|
| Consensus Impact | **NONE** |
| `equivalence_claimed` | **false** |

This document maps data flows across RIGOR layers and defines the determinism model: state transition rules, absence of global mutable consensus state, replayability conditions, hash-consistency assumptions, and convergence invariants.

---

## 1. Determinism Definition

RIGOR consensus satisfies:

```
∀ block B at height h with parent context P:
  evaluate(B, P) → (accept | reject, state_delta)
```

For accepted blocks, `state_delta` (UTXO set, supply tallies, index caches) is **identical** across:

- Platforms (Linux x86_64, etc.)
- Compilers (given same source revision)
- Execution environments (rigord node)

As stated in `ARCHITECTURE.md`:

> same input → same bytes output

This applies to **Layer 1 only**. Layer 2 pool replay targets bitwise-identical draw results but is not consensus-authoritative. Layers 3–4 may use derived caches with explicit non-equivalence.

---

## 2. State Transition Determinism Rules

### 2.1 Block-Level Transition Function

```
T(S_{h-1}, B_h) → S_h
```

Where `S_h` includes:

| Component | Deterministic source |
|-----------|---------------------|
| UTXO set | `UpdateCoins` after per-tx validation |
| ORU supply | `S_ORU(h) = S_ORU(h-1) + R(h) - C(h)` |
| RES supply | `S_RES(h) = S_RES(h-1) + M(h) · RES_REWARD` |
| Chain index metadata | Cumulative convergence, RES mint flags |

**Inputs frozen at evaluation time:**

- Block tx list (order matters for ConnectBlock loop)
- Parent block hash chain back to genesis
- Parent `nBits` (`target_prev`)
- Block at height `h-10` hash (`bind_commit`)
- UTXO view at block entry

### 2.2 Per-Transaction Determinism

For each non-coinbase tx `tx` in block:

```
1. domain ← GetTransactionInputAsset(prevouts)
2. reject if illegal cross-domain outputs
3. CheckTxInputs(view, tx, domain)
4. VerifyScript (witness paths)
5. if RES mint path: ConnectEvaluateResMintTx(proof, winner, C, F)
6. if convergence: skip UTXO insert for CVG2 vout
```

StateTag classification (`statetag.cpp`) is a pure function of tx structure + input domain.

### 2.3 Draw Sub-Transition (Convergence)

Draw evaluation is a pure function:

```
draw(F, C, h, B_{h-10}, target_prev, T=RES_PROBABILITY_TARGET) → (eligible[], winner_txid | ∅)
```

Shipping `T` is Deterministic Reserve Issuance windowed `RES_PROBABILITY_TARGET` at **`E = 246`** (`C = 1024` ORU per mint). **Historical RFC-014 / retired design:** `T = 2^240`.

No randomness external to block content. σ is `Hash256` of deterministic preimage.

### 2.4 Integer Arithmetic Domain

| Operation | Type | Rule |
|-----------|------|------|
| Inequality check | uint256 × uint64 vs uint256 | Promoted to uint512 cross-multiply |
| Argmin | uint512 comparison | Strict `<`; tie → memcmp(txid) |
| Supply arithmetic | int64 CAmount | Saturating checks; reject on overflow/underflow |
| Sort F for root | memcmp on 32-byte txids | Platform-independent byte order |

No floating point in consensus path.

---

## 3. Absence of Global Mutable State

### 3.1 What Does NOT Exist (Consensus)

| Forbidden pattern | RIGOR status |
|-------------------|--------------|
| Global account balance table | **Absent** — UTXO only |
| Per-address nonce | **Absent** |
| Draw RNG seed store | **Absent** — σ from hash of block data |
| External oracle feed | **Absent** |
| Admin override flag | **Absent** |
| Persisted asset tag on CTxOut | **Absent** — script inference only |

### 3.2 Observational Mirrors (Non-Consensus)

These are updated during ConnectBlock but **MUST NOT** feed back into validation predicates:

| Mirror | File | Read by ConnectBlock? |
|--------|------|----------------------|
| `RuntimeSupplyState` | `runtime_supply_state.h` | **No** (explicit comment) |
| `coq_supply_field_hook` | `coq_supply_field_hook.h` | **No** |
| `CBlockIndex` supply caches | `chain.h` | Used for RPC/export; recomputed on connect |

### 3.3 Node-Local Non-Deterministic State

These affect **ordering** or **availability** but not validity of an accepted block:

| State | Scope | Impact |
|-------|-------|--------|
| Mempool tx order | Node-local | Which txs miner includes (block must still be valid) |
| P2P peer set | Node-local | Block propagation timing |
| Template refresh timing | Pool | Job staleness, not consensus |

Two nodes accepting the same block at the same height **must** reach identical UTXO and supply state.

---

## 4. Replayability Conditions

### 4.1 Chain Replay (Full Node)

Full replay from genesis requires:

| Input | Description |
|-------|-------------|
| Block files / raw blocks | Complete chain segment |
| Consensus params | `chainparams` for network (regtest/testnet/mainnet) |
| Frozen rule version | Source revision at freeze baseline |

Replay procedure: sequential `ConnectBlock` without mempool. Output: identical chainstate hash (UTXO snapshot hash) for same block sequence.

### 4.2 Draw Replay (Single Block)

Minimum inputs to reproduce winner:

```json
{
  "height": "h",
  "F": ["txid_0", "txid_1", "..."],
  "weights": ["f_0", "f_1", "..."],
  "convergence_root": "C",
  "bind_block_hash": "B_{h-10}",
  "target_prev": "nBits from parent header",
  "T": "RES_PROBABILITY_TARGET (Deterministic Reserve Issuance E=246, windowed; C=1024 ORU)"
}
```

Implemented identically in:

- `core/src/consensus/convergence_logic.cpp`
- `pool/winner-engine/src/draw.rs`
- `pool/rigor-convergence-root/src/root.rs`

Golden tests: `convergence_consensus_tests.cpp`, `pool/winner-engine/tests/golden_vectors.rs`, `pool/rigor-convergence-root/tests/golden_cr.rs`.

### 4.3 Pool Telemetry Replay (SIM-004)

Requires:

| Input | Format |
|-------|--------|
| `miner_shares.jsonl` | Timestamped share events |
| `pool_job_lifecycle.jsonl` | Job publish/invalidate |
| `winner_authorization.jsonl` | Preview/authorize/revoke |
| `mint_engine.jsonl` | Mint build/submit |

Determinism guarantee: identical logs → identical `output_hash` (SHA-256 of timeline).

**Not** chain replay — operational mirror only.

### 4.4 PROOF-034 Golden Vector Replay

Requires:

| Input | Source |
|-------|--------|
| Archive datadir blocks | Regtest mining output |
| Vector catalog | 12 golden vectors at defined heights |
| Harness expected JSON | `golden_vector_fixtures.json` |

SIM-006 replays offline without live node when fixtures present.

---

## 5. Hash-Consistency Assumptions

### 5.1 Hash Functions

| Function | Usage |
|----------|-------|
| `Hash256(x) = SHA256(SHA256(x))` | Convergence root, σ, bind_commit, merkle |
| OpuHash | PoW header hash |
| SHA-256 (single) | Simulation output_hash, archive manifests |
| Ed25519 | PCORB signature over σ digest |

### 5.2 Critical Preimage Layouts

**Convergence root C** (|F| ≥ 2):

```
preimage = txid_0 || txid_1 || ... || txid_{n-1}   (sorted by memcmp)
C = Hash256(preimage)
```

**Sigma σ_i** (100 bytes, Sigma-CR-01):

```
preimage = txid_i (32) || C (32) || bind_commit (32) || LE32(height) (4)
σ_i = Hash256(preimage)
```

**Bind commit:**

```
bind_commit = Hash256( B_{h-10} (32) || LE32(target_prev) (4) )
```

**CVGE commitment** (coinbase OP_RETURN):

```
"CVGE" wire tag + cumulative_convergence + draw_candidates_root
```

Any change to preimage byte layout is a **hard-forking** change (guarded by `RIGOR_CONSENSUS_LOCK_VERSION`).

### 5.3 Endianness

| Field | Encoding |
|-------|----------|
| Height in σ | LE32 |
| target_prev in bind | LE32 (compact bits) |
| Amounts | CAmount int64 LE on wire |
| txids | Internal uint256 (Bitcoin convention) |

### 5.4 Cross-Implementation Consistency

| Pair | Verified by |
|------|-------------|
| C++ rigord ↔ Rust pool root | 122/122 ROOT_EQUIVALENT |
| C++ draw ↔ Rust winner-engine | 28/28 golden vectors |
| Runtime ↔ SIM-006 fixtures | Offline validator (partial until archive complete) |

---

## 6. Convergence Invariants

### 6.1 Set F Invariants

| ID | Invariant |
|----|-----------|
| F-01 | Every tx in F is non-coinbase |
| F-02 | Every tx in F has exactly one `OP_CONVERGE` output |
| F-03 | ∀ tx ∈ F: convergence_amount ≥ MIN_CONVERGENCE_AMOUNT |
| F-04 | ∀ tx ∈ F: CheckTxInputs passes at block entry |
| F-05 | ∀ tx ∈ F: txfee > 0 |
| F-06 | F order for root = memcmp sort on txid (not block order) |

### 6.2 Root C Invariants

| ID | Invariant |
|----|-----------|
| C-01 | |F| < 2 ⟹ C = 0³² |
| C-02 | |F| ≥ 2 ⟹ C = Hash256(sorted txids) |
| C-03 | Coinbase scriptSig commits C exactly |
| C-04 | CVGE commitment draw root consistent with F |

### 6.3 Draw Invariants

| ID | Invariant |
|----|-----------|
| D-01 | T = RES_PROBABILITY_TARGET at Deterministic Reserve Issuance `E = 246` (windowed; never from nBits). Historical RFC-014 / retired design: `T = 2^240`. |
| D-02 | target_prev used only in bind_commit, not in T |
| D-03 | f_i = convergence_amount(tx_i) — no β transform |
| D-04 | Eligible ⟺ σ_i < T · f_i (uint512) |
| D-05 | Winner = unique argmin or txid tie-break |
| D-06 | |F| = 0 ⟹ no winner, no RES mint |
| D-07 | Winner ∉ F or ¬eligible ⟹ no valid RES mint |

### 6.4 RES Mint Invariants

| ID | Invariant |
|----|-----------|
| M-01 | At most one scoring RES mint per block |
| M-02 | PCORB.convergence_txid = winner |
| M-03 | PCORB.convergence_root = C |
| M-04 | VerifyConvergeProof passes (sig + inequality) |
| M-05 | M(h) ∈ {0,1} |
| M-06 | S_RES never exceeds RES_MAX_SUPPLY |
| M-07 | RES mint never from coinbase-only path without body validation |

### 6.5 Supply Invariants (RFC-002)

```
S_ORU(h) = 1000·COIN·Σ halvings - Σ all convergence burns
S_RES(h) = Σ_{i=1..h} GetResMintReward(i) · M(i)
S_RES(h) ≤ RES_MAX_SUPPLY
```

Observed at ConnectBlock; mirrors in `RuntimeSupplyState` for export only.

---

## 7. Data Flow Map (Cross-Layer)

### 7.1 Consensus Data Plane

```
                    ┌─────────────────────────────────────┐
                    │           Block B_h                  │
                    │  header │ coinbase │ body txs        │
                    └──────────────┬──────────────────────┘
                                   │
         ┌─────────────────────────┼─────────────────────────┐
         ▼                         ▼                         ▼
   ┌───────────┐           ┌─────────────┐          ┌────────────┐
   │ Parse     │           │ Build F, C  │          │ PoW check  │
   │ scripts   │           │ σ, winner   │          │ OpuHash    │
   └─────┬─────┘           └──────┬──────┘          └────────────┘
         │                        │
         ▼                        ▼
   AssetType              ConvergenceDraw
   inference              Result
         │                        │
         └──────────┬─────────────┘
                    ▼
              CheckTxInputs
              VerifyScript
              RES / PCORB
                    │
                    ▼
              UpdateCoins
              Supply update
                    │
                    ▼
              S_h (chainstate)
```

### 7.2 Operational Data Plane (Pool)

```
getblocktemplate JSON
        │
        ├─► TemplateRecord (immutable)
        │         │
        │         ├─► F candidates + weights
        │         ├─► convergence_root C
        │         ├─► prev_nbits (draw domain)
        │         └─► mint_required flag
        │
        ▼
PublishedJobSnapshot
        │
        ├─► Stratum job (merkle, extranonce)
        ├─► Winner preview envelope
        └─► Mint placeholder metadata
        │
        ▼
Block candidate (hex)
        │
        ▼
submitblock → ConnectBlock
```

**Data that MUST NOT cross pool→consensus with alteration:**

- Convergence root C (must match recomputation)
- Coinbase CVGE bytes
- Winner txid for PCORB binding
- RES mint amount (fixed RES_REWARD)

### 7.3 Observer Data Plane

```
ConnectBlock completes
        │
        ├─► RPC: getblock (+ rigor_winner, rigor_res_minted)
        ├─► RPC: getconvergenceresult
        └─► RPC: getconvergencestatus
                │
                ▼
        Explorer indexer import
                │
                ├─► PostgreSQL normalized tables
                └─► REST /api/* enrichment
                        │
                        ▼
                GUI / website render
```

Observer data is **eventually consistent** with chain tip. Convergence panels SHOULD be sourced from rigord RPC fields or recomputation flagged as derived.

### 7.4 Experimental Data Plane

```
                    ┌──────────────────┐
                    │  Domain A chain   │
                    └────────┬─────────┘
                             │ read-only
              ┌──────────────┼──────────────┐
              ▼              ▼              ▼
        PROOF-034       SIM-006         SIM-004
        archive         fixtures        JSONL
              │              │              │
              ▼              ▼              ▼
        harness JSON    validator      replay hash
              │              │              │
              └──────────────┴──────────────┘
                             │
                             ▼
                    SIM-009 unification
                    (compare only, no write)
```

---

## 8. Determinism Boundary Matrix

| Operation | Layer | Deterministic? | Authoritative? |
|-----------|-------|----------------|----------------|
| ConnectBlock | 1 | **Yes** | **Yes** |
| GetBlockWinner | 1 | **Yes** | **Yes** |
| Mempool admission | 1 | Yes (given same mempool) | No (policy) |
| GBT tx selection | 2 | No (miner heuristic) | No |
| Draw preview | 2 | **Yes** (given template) | No |
| Share validation | 2 | **Yes** | No |
| Explorer indexing | 3 | Yes (given RPC snapshot) | No |
| SIM-004 replay | 4 | **Yes** (given JSONL) | No |
| PROOF-034 harness | 4 | **Yes** (given archive) | Empirical only |

---

## 9. Known Non-Determinism Boundaries (Explicit)

These are **outside** consensus determinism claims:

| Boundary | Description |
|----------|-------------|
| Mempool tx ordering | Miners may choose different valid tx subsets |
| Block timestamp | Within consensus-permitted drift |
| P2P message timing | Network layer |
| Pool job publish timing | Operational scheduling |
| Explorer import lag | Observer staleness |
| GUI refresh interval | Presentation |

---

## 10. Verification Layer Determinism (Non-Equivalent)

| Layer | Deterministic artifact | Equivalence to Layer 1? |
|-------|------------------------|-------------------------|
| Formal Coq | Proof terms, Qed status | **No** — specification model |
| PROOF-034 archive | Golden vector JSON + block hashes | **No** — empirical sample |
| SIM-004 replay | output_hash from JSONL | **No** — operational telemetry |
| SIM-006 validator | PASS/PARTIAL/FAIL per vector | **No** — fixture check |
| Pool golden replay | Match count 122/122 | **No** — mirror certification only |

Global flag: **`equivalence_claimed = false`**.

---

## 11. Replay Checklist (Engineering)

Before claiming replay success at any layer:

- [ ] Rule version pinned to freeze baseline
- [ ] Height and parent context specified
- [ ] F set extracted at block entry view (not mid-block)
- [ ] target_prev from parent, not current header
- [ ] Bootstrap bind hash used for h ≤ 9
- [ ] T = Deterministic Reserve Issuance windowed `RES_PROBABILITY_TARGET` (`E = 246`) confirmed (not compact target; Historical RFC-014 / retired design: `2^240`)
- [ ] Cross-multiply uses uint512
- [ ] Convergence outputs excluded from UTXO replay
- [ ] Observer/provenance labeled if not Layer 1

---

*End of RIGOR Data Flow & Determinism Map*

# RES Indexing Rules (Explorer Interpretation)

**Task:** `TASK-EXPLORER-BOOTSTRAP-001`  
**Baseline:** `mainnet-freeze-rfc014-2026-05-25`  
**Normative:** RFC-001, RFC-002, RFC-014, RFC-008 erratum, `core/src/consensus/`, `getconvergenceresult`.

---

## 1. Purpose

Exact **read-only** rules for how the indexer classifies and stores RES-related chain facts. These rules MUST match node RPC output; the indexer MUST NOT reimplement consensus selection math for production truth.

---

## 2. Asset separation (ORU vs RES)

| Signal | ORU | RES |
|--------|-----|-----|
| Witness version | **0** (P2WPKH/P2WSH) | **1** (32-byte program) |
| `OP_CONVERGE` (`OP_PUSHBYTES_20 \|\| HASH160 \|\| 0xbd`) | Convergence burn / draw-entry — **ORU**, unspendable | N/A |
| Native / RES2 v1 program | N/A | **RES** transfer or mint output |
| Historical RFC-014 Witness v1 `CVG2` | **Retired** — not a live asset (indexer classifies `NONE`) | N/A |
| `listunspent` / decode `asset` | `"ORU"` | `"RES"` |
| Display decimals | 8 (base units per `COIN`) | 6 (`RES_COIN = 1_000_000` atoms per 1.000000 RES) |

**Forbidden:** Single “balance” column summing ORU + RES.

---

## 3. RES mint detection

A transaction is indexed as **RES mint** when ALL hold:

1. It is the **coinbase** at height `H`.
2. One output is canonical `OP_RES_MINT` (`<height> <20-byte HASH160> 0xbb`).
3. Block RPC `rigor_res_minted == 1` is a cross-check (`CBlockIndex::has_res`). A missing coinbase mint on a `has_res=1` block is a mismatch, not a body-tx fallback.
4. The paid winner is the **parent** draw (`getconvergenceresult(parent).draw_winner_txid`). This block's `rigor_winner` is the next window, not this mint.

**Indexer MUST NOT:**

- Infer mint from probability heuristics.
- Mark mint from mempool alone.
- Treat a body-tx PCORB witness as issuance. Live mint has no funding input and no 6-field proof stack.

### 3.1 Mint amount

| Rule | Value |
|------|-------|
| Consensus mint increment | `GetResMintReward(H)` = RFC-017 `W(k)` at epoch `k = floor(H / 210000)`; genesis `W(0)` = 40.000000 RES (`RES_REWARD_GENESIS`). Historical RFC-015: constant 100 RES. |
| Supply cap | `MAX_RES_SUPPLY` — display as static constant |

---

## 4. ORU vs RES separation in outputs

For each `vout`:

```
IF script is OP_CONVERGE → asset = ORU (unspendable burn / draw-entry)
ELSE IF script is witness v0 → asset = ORU
ELSE IF witness v1 AND Historical RFC-014 CVG2 prefix → asset = NONE (retired; not a third asset)
ELSE IF witness v1 AND 32-byte RES program → asset = RES
ELSE IF OP_RETURN → asset = NONE (not in supply)
ELSE → asset = NONE (unknown / nonstandard)
```

Match `AssetTypeFromScriptPubKey` semantics in `core/src/primitives/transaction.cpp`.

Development databases created before script alignment may contain retired
classifications. Public network launch will rebuild the explorer view and
indexed observations from genesis using current consensus classification
rules. No production
CVG2 migration is required. Historical CVG2 conversion is not a consensus
concern. `OP_CONVERGE` is the only live Convergence classification.

Policy: [`../archive/migration/MIGRATION_NOTES.md`](../archive/migration/MIGRATION_NOTES.md).

---

## 5. Convergence eligibility (indexer view)

Eligibility for RES draw is **only** taken from `getconvergenceresult`:

| Field | Indexer action |
|-------|----------------|
| `candidates[].draw_eligible` | Store verbatim |
| `candidates[].convergence_amount` | Store; require ≥ `min_convergence` when eligible |
| `candidates[].h` | Store for audit display |
| `convergence_root_set_size` | Store \|F\| |
| `res_probability_target` | Verify constant on every block |

**Inequality (informational display only):**

```
H_i < RES_PROBABILITY_TARGET · f_i   (RFC-017)
T = windowed RES_PROBABILITY_TARGET at E = 246 (C = 1024 ORU per mint)
  hex WINDOWED: 000000000055e63b88c230e77e7ee106959b5d3e1e9ea69ebac0b0b4be383479
Historical RFC-014: T = 2^240 (hex 00010000…00 — not shipping)
f_i = convergence_amount (ORU)
```

**Forbidden indexer logic:**

- Recompute eligibility using `bits` or `SetCompact(target_prev)` as T.
- Reimplement `RunSelection` as authority — node RPC is authority for closed phase.

### 5.1 Coupled mint rule

| Condition | Display |
|-----------|---------|
| \|F\| < 2 | No coupled RES mint; `draw_eligible` may be false for all |
| \|F\| ≥ 2 | Mint-capable block; show candidate set |

---

## 6. Convergence tracking (ORU convergence)

### 6.1 What counts as convergence activity

| Event | Detection |
|-------|-----------|
| Convergence tx | `convergence_type == v1_cvg2` in `getconvergenceresult` OR `IsConvergenceTx` equivalent via decode |
| Amount | `convergence_amount` on candidate row (convergence fee) |

### 6.2 Convergence lineage index

Build edges:

1. **Inclusion:** `convergence_txid` → `block_hash` at height `H`.
2. **F-set:** `convergence_txid` in committed root set when `convergence_amount > 0` and tx appears in candidates with `convergence_type=v1_cvg2`.
3. **Winner:** `draw_winner_txid == convergence_txid`.
4. **Mint reference:** On RES mint tx, parse witness item 0; if equals `convergence_txid`, link `convergence_txid` → `mint_txid`.

### 6.3 Cumulative convergence

Use `getblockchaininfo.cumulative_convergence` / per-block header field for chain-level ORU convergence counter — do not sum convergence fees locally unless cross-checking.

---

## 7. Replay-safe asset tracing

Fields that MUST be stored for any RES mint display (there is no longer a proof-bearing RES spend: A-7 removed the script path and the `createresclaim` RPC):

| Field | Why |
|-------|-----|
| `bind_hash` | Sigma-CR-01 domain |
| `bind_height` / `bind_blockhash` | Ancestor at `H - 10` (or bootstrap) |
| `target_prev` | Parent `nBits` at mint — witness item 4 |
| `convergence_root` | F-set commitment at mint block |
| `convergence_txid` | Winning convergence reference |
| `res_probability_target` | Constant inequality RHS |

**Not sufficient for replay display:** wtxid, fee rate, mempool time.

### 7.1 txid vs wtxid (RES domain)

| Operation | Identifier |
|-----------|------------|
| `convergence_txid` in proof | **txid** |
| F-set membership | **txid** sorted |
| `draw_winner_txid` | **txid** |
| Mempool relay dedup | wtxid (display only) |

Test reference: `convergence_consensus_tests.cpp` — `txid_vs_wtxid_locked`.

---

## 8. RES indexing edge cases

| Case | Rule |
|------|------|
| Bootstrap heights `1..9` | `bind_height = -1`; frozen zero bind preimage — show RPC text, do not invent ancestor |
| Block without undo on historical query | `draw_candidate_root_computed` may be zero; `draw_eligible` false — label “undo unavailable” |
| Empty F (`convergence_root_set_size = 0`) | Valid; not an error |
| Committed ≠ computed root | **CRITICAL alert** — possible consensus divergence |
| Multiple `OP_CONVERGE` outputs in one tx | Consensus rejects — indexer may show decode error from node |
| Non-zero reserved witness | Post F-WX-08: invalid tx — do not index as valid mint |
| `getconvergenceresult` vs local recompute mismatch | Trust RPC; file bug against node |

---

## 9. Verification hooks (closed testnet)

| ID | Check |
|----|-------|
| P1 | `draw_candidate_root_committed == draw_candidate_root_computed` for sample ≥ 20 mint-capable blocks |
| P2 | 100% headers `res_probability_target` == shipping windowed hex `000000000055e63b88c230e77e7ee106959b5d3e1e9ea69ebac0b0b4be383479` (`E = 246`). Historical RFC-014 `00010000…00` (`2^240`) MUST NOT match. |
| P3 | Eligible txid set equals `getconvergenceresult` for known RES block |

---

## 10. Document metadata

| Field | Value |
|-------|--------|
| Risk classification | **MEDIUM** (edge cases); **CRITICAL** on root mismatch |
| Explorer classification | **INDEXING-RULES** |
| Consensus impact | **NONE** (indexer); **INDIRECT** if rules diverge from node |
| Replay impact | **INDIRECT** — missing bind fields breaks audit |
| Determinism impact | **NONE** if RPC-authoritative |
| Indexing risk | **HIGH** (wtxid, `rigor_target`, local selection) |
| Minimal safe recommendation | RPC-authoritative eligibility; txid keys; store full bind bundle per mint |

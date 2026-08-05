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
| CVG2 prefix (`CVG2` + BE amount) | Convergence output — **ORU convergence fee** | N/A |
| Native / RES2 v1 program | N/A | **RES** transfer or mint output |
| `listunspent` / decode `asset` | `"ORU"` | `"RES"` |
| Display decimals | 8 (base units per `COIN`) | 2 display / 100 atoms per 1.00 RES |

**Forbidden:** Single “balance” column summing ORU + RES.

---

## 3. RES mint detection

A transaction is indexed as **RES mint** when ALL hold:

1. Included in block body (non-coinbase) at height `H`.
2. Block header/RPC: `rigor_res_minted == 1` for that block (cross-check).
3. Spends/create pattern matches node RES mint path: **ORU-domain** inputs creating a witness v1 RES output, with the PCORB proof on `vin[0]`.
4. `vin[0]` witness carries the **6-item** PCORB proof (SECURITY-001-A / A-7). Any push beyond index 5 is funding-input material, not proof — for a devtest P2WSH-funded mint it is the redeem script, giving 7 items. There are no reserved-zero items to check: former items 9–10 were removed with the RES script path.
5. `listconvergencehistory` includes block at `H` (cross-check optional).

**Indexer MUST NOT:**

- Infer mint from probability heuristics.
- Mark mint from mempool alone.
- Use coinbase txid-bound mint programs (consensus does not use that model).

### 3.1 Mint amount

| Rule | Value |
|------|-------|
| Consensus mint increment | `RES_REWARD_AMOUNT` = 1.00 RES (100 atoms) per successful mint |
| Supply cap | `MAX_RES_SUPPLY` — display as static constant |

---

## 4. ORU vs RES separation in outputs

For each `vout`:

```
IF script is witness v0 → asset = ORU
ELSE IF witness v1 AND CVG2 convergence program → asset = ORU (convergence fee)
ELSE IF witness v1 AND 32-byte RES program → asset = RES
ELSE IF OP_RETURN → asset = NONE (not in supply)
ELSE → asset = ORU (legacy non-witness; rare on testnet)
```

Match `AssetTypeFromScriptPubKey` semantics in `core/src/primitives/transaction.cpp`.

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
H_i < RES_PROBABILITY_TARGET · f_i   (RFC-014)
T = 2^240 (constant)
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
| Multiple CVG2 outputs in one tx | Consensus rejects — indexer may show decode error from node |
| Non-zero reserved witness | Post F-WX-08: invalid tx — do not index as valid mint |
| `getconvergenceresult` vs local recompute mismatch | Trust RPC; file bug against node |

---

## 9. Verification hooks (closed testnet)

| ID | Check |
|----|-------|
| P1 | `draw_candidate_root_committed == draw_candidate_root_computed` for sample ≥ 20 mint-capable blocks |
| P2 | 100% headers `res_probability_target == 00010000…00` |
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

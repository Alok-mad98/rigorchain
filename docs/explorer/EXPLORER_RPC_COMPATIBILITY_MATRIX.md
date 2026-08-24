# Explorer RPC Compatibility Matrix

**Task:** `TASK-EXPLORER-BOOTSTRAP-001`  
**Baseline:** commit `4437597197aa713396a123673cc36606346ad523`, tag `mainnet-freeze-rfc014-2026-05-25`  
**Source of truth:** `core/src/rpc/blockchain.cpp`, `core/src/rpc/rawtransaction.cpp`, `EXPLORER_RPC_STABILITY_MATRIX.md` (repo root).

---

## 1. Classification legend

| Tier | Explorer may depend? | Change policy |
|------|----------------------|---------------|
| **STABLE** | Yes — indexer + public API | Breaking change requires ops RFC + version bump |
| **INTERNAL** | Closed net diagnostics only | No product surface |
| **UNSAFE** | No — grep CI must fail if referenced | Misleading or removed |
| **FORBIDDEN** | Never call from indexer | Admin / chain mutation |

---

## 2. Tier definitions (explorer-specific)

- **STABLE:** Field semantics frozen for testnet/mainnet integrators.  
- **INTERNAL:** Determinism audits (`getdeterministicstatehash`, replay snapshots).  
- **UNSAFE:** Retired names, wrong physics (`rigor_target`), or nonexistent methods.  
- **FORBIDDEN:** Methods that alter chain view or bypass validation.

---

## 3. Blockchain / indexing RPC

| Method | Tier | Indexer use |
|--------|------|-------------|
| `getblock` (verbosity 0–2) | **STABLE** | Block import, tx list |
| `getblockheader` | **STABLE** | Light refresh |
| `getblockhash` | **STABLE** | Height → hash |
| `getbestblockhash` | **STABLE** | Tip watch |
| `getrawtransaction` | **STABLE** | Tx detail (`txindex=1` required) |
| `getblockchaininfo` | **STABLE** | Chain, cumulative convergence |
| `getblockstats` | **STABLE** | Optional stats pages |
| `gettxout` | **STABLE** | UTXO lookup (no wallet) |
| `gettxoutsetinfo` | **STABLE** | Supply audit (heavy) |
| `scantxoutset` | **STABLE** | Address scans (rate-limit) |
| `getblockconvergence` | **STABLE** | CVGE cross-check |
| **`getconvergenceresult`** | **STABLE** | **Primary draw audit** |
| `getconvergencestatus` | **STABLE** | RES window / tip cadence |
| `listconvergencehistory` | **STABLE** | RES mint block list |
| `getassetbalance` | **STABLE** | Address ORU/RES scan |
| `getmempoolinfo` | **STABLE** | Mempool cumulative convergence |
| `getrawmempool` (verbose) | **STABLE** | Pending convergence object |
| `getmempoolentry` | **STABLE** | Single mempool convergence |
| `getconvergeinfo` | **UNSAFE** | Stub — height/hash only |
| `getdeploymentinfo` | **UNSAFE** | REST-only drift |
| `getdeterministicstatehash` | **INTERNAL** | Replay audits |
| `getreplaysnapshot` | **INTERNAL** | Large binary |
| `getchainstatesummary` | **INTERNAL** | Diff tooling |
| `diffchainstatesummaries` | **INTERNAL** | |
| `invalidateblock` | **FORBIDDEN** | Alters active chain |
| `reconsiderblock` | **FORBIDDEN** | |
| `submitblock` | **FORBIDDEN** | Miner-only |
| `preciousblock` | **FORBIDDEN** | |

---

## 4. Required methods — expanded notes

### 4.1 `getblock`

| Field | Tier | Explorer rule |
|-------|------|---------------|
| `res_probability_target` | **STABLE** | Display; verify == shipping windowed hex `000000000055e63b88c230e77e7ee106959b5d3e1e9ea69ebac0b0b4be383479` (RFC-017 `E = 246`). Historical RFC-014 `00010000…00` (`2^240`) is not live. |
| `draw_candidate_root` | **STABLE** | Link to convergence page |
| `rigor_res_minted` | **STABLE** | RES badge |
| `rigor_winner` | **STABLE** | Winner **txid** |
| `cumulative_convergence` | **STABLE** | ORU cumulative convergence index |
| `bits` | **STABLE** | PoW only — label “PoW target” |
| `rigor_target` | **UNSAFE** | **Removed** — do not parse |
| `res_draw_target` | **UNSAFE** | **Removed** |
| `res_target_ctx` | **UNSAFE** | **Removed** |

### 4.2 `getrawtransaction`

| Aspect | Tier | Rule |
|--------|------|------|
| `txid` | **STABLE** | Primary key |
| `hash` (wtxid) | **STABLE** | Secondary display column |
| `vin[].txinwitness` | **STABLE** | PCORB stack display |
| `vout[].value` | **STABLE** | Asset inference via script |

### 4.3 `gettxout`

| Aspect | Tier | Rule |
|--------|------|------|
| `value`, `scriptPubKey` | **STABLE** | UTXO pages |
| Asset label | Derived | From script — not an RPC field |

### 4.4 `getconvergenceresult` (mandatory)

| Field | Tier | Indexer column |
|-------|------|----------------|
| `hash`, `height` | **STABLE** | |
| `bind_height`, `bind_blockhash`, `bind_hash` | **STABLE** | Replay binding |
| `res_probability_target` | **STABLE** | Constant check |
| `min_convergence` | **STABLE** | 1.00 ORU |
| `convergence_root_set_size` | **STABLE** | \|F\| |
| `draw_candidate_root_committed` | **STABLE** | Alert if ≠ computed |
| `draw_candidate_root_computed` | **STABLE** | |
| `draw_winner_txid` | **STABLE** | |
| `candidates[].txid` | **STABLE** | **Not wtxid** |
| `candidates[].h` | **STABLE** | |
| `candidates[].draw_eligible` | **STABLE** | |
| `candidates[].convergence_type` | **STABLE** | `v1_cvg2` \| `none` |

### 4.5 `res_probability_target`

| Surface | Tier | Semantics |
|---------|------|-----------|
| `getblock` / `getblockheader` | **STABLE** | Constitutional RHS = RFC-017 windowed `T` at `E = 246` (not Historical RFC-014 `2^240`) |
| `getconvergenceresult` | **STABLE** | Same constant — **not** PoW |
| `bits` / `target_prev` | **STABLE** (distinct) | PoW / replay bind — **never** RES inequality |

### 4.6 Deprecated `rigor_target` paths

| Name | Tier | Action |
|------|------|--------|
| `rigor_target` | **UNSAFE** | Remove from indexer code; CI grep |
| `res_draw_target` | **UNSAFE** | Same |
| `res_target_ctx` | **UNSAFE** | Same |
| `getconvergencedrawcandidates` | **UNSAFE** | RPC does not exist — use `getconvergenceresult` |

---

## 5. Raw transaction / decode RPC

| Method | Tier | Notes |
|--------|------|-------|
| `decoderawtransaction` | **STABLE** | Offline decode |
| `decodescript` | **STABLE** | |
| `createrawtransaction` | **FORBIDDEN** on indexer | Write path |
| `signrawtransactionwithkey` | **FORBIDDEN** | |

---

## 6. Wallet RPC (indexer node: not loaded)

| Method | Tier | Closed explorer |
|--------|------|-----------------|
| `createconvergence` | **STABLE** (wallet) | **FORBIDDEN** on NODE-C |
| `createresclaim` | **REMOVED** — unregistered by SECURITY-001-A / A-7 with the RES script path | **N/A** — method does not exist |
| `sendres` | **STABLE** (wallet) | **FORBIDDEN** on NODE-C |
| `getbalances` | **STABLE** | **FORBIDDEN** on NODE-C |

Wallet RPC remains **STABLE** for ecosystem wallets on NODE-A only.

---

## 7. Mempool RPC

| Method | Field | Tier |
|--------|-------|------|
| `getmempoolentry` | `convergence` | **STABLE** |
| `getrawmempool` | nested `convergence` | **STABLE** |
| `getmempoolinfo` | `cumulative_convergence` | **STABLE** |

Mempool views are **non-final** — label “unconfirmed”; never treat as minted RES.

---

## 8. CI / integrator grep gates

```bash
# MUST return no hits in explorer/indexer code:
rg 'rigor_target|res_draw_target|res_target_ctx|getconvergencedrawcandidates' indexer/
# MUST return hits only in docs/comments in core:
rg 'getconvergenceresult|res_probability_target' indexer/
```

---

## 9. Document metadata

| Field | Value |
|-------|--------|
| Risk classification | **LOW** (STABLE path); **HIGH** if retired fields parsed |
| Explorer classification | **RPC-CONTRACT** |
| Consensus impact | **NONE** |
| Replay impact | **INDIRECT** — dropping bind fields breaks audit trails |
| Determinism impact | **NONE** |
| Indexing risk | **HIGH** if `rigor_target` or wtxid used |
| Minimal safe recommendation | Whitelist STABLE table; forbid FORBIDDEN; grep UNSAFE names |

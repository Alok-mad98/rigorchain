# RFC-018 Explorer Fee-Burn Statistics — Implementation Report

**Task:** `TASK-RIGOR-RFC018-EXPLORER-FEE-BURN-STATISTICS-IMPLEMENTATION-P1-001`  
**Date:** 2026-08-20  
**Design:** [RIGOR_RFC018_FEE_BURN_STATISTICS_DESIGN.md](./RIGOR_RFC018_FEE_BURN_STATISTICS_DESIGN.md) Option C  
**Consensus impact:** **NONE**  
**Push / merge / tag:** **NONE** (local commit only)  
**Honesty:**

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

This report records explorer/indexer statistics only. It does not claim C++ ConnectBlock equivalence, does not add a chainstate accumulator, and does not change wallet spendability.

---

## 1. Decision implemented

Cumulative RFC-018 ORU fee burn is **explorer/indexer-only**.

Not modified:

- consensus
- chainstate
- `ConnectBlock`
- `m_chain_oru_total`
- `m_block_converge_sum`
- wallet consensus logic

Formula (indexer, presentation):

```
destroyed_fee_oru(block) = Σ floor(eligible_ORU_fee / 2)
cumulative_fee_burn_oru  = Σ destroyed_fee_oru(block)  along the active indexed chain
```

Eligibility matches the RFC-018 class used by the explorer: RES-input domain (`asset_kind = RES` on a prevout), not coinbase, not the ORU-funded mint tx. ORU fee is the ORU leg `ORU_in − ORU_out`. Missing prevouts yield burn `0` for that tx (undercount, never invent). NUM=1 DEN=2. Coins: `1 ORU = 100000000` atoms, `1 RES = 1000000` atoms. No 100-atom assumptions.

---

## 2. Files changed

| Path | Role |
|------|------|
| `explorer/src/rfc018FeeBurn.ts` | Pure burn math and API view object |
| `explorer/src/rfc018FeeBurn.test.ts` | Unit tests (burn / no-burn / multi-tx / cumulative / reorg / separation) |
| `explorer/src/rfc018FeeBurnStore.ts` | SQL upsert, tip/height reads, stored-block recompute, backfill, prefix recompute |
| `explorer/migrations/005_rfc018_fee_burn_stats.sql` | `rfc018_fee_burn_stats` table |
| `explorer/src/db.ts` | Migration list; `listBlocks` / `getBlockByHash` attach `rfc018_fee_burn` |
| `explorer/src/importer.ts` | Per-block burn at import; backfill tick |
| `explorer/src/api/status.ts` | `economic_snapshot.rfc018_fee_burn` and `supply_chart` |
| `explorer/src/registerRoutes.ts` | `/api/chain/tip` nested `rfc018_fee_burn` |
| `explorer/src/uiSsr.ts` | Status S4 four-series labels; block-page burn rows |
| `explorer/src/uiSsr.test.ts` | SSR labels stay unmerged |
| `explorer/public/status.js` | Dashboard S4 labels |
| `explorer/public/oru.html` | Supply chart rows |
| `explorer/package.json` | `test` / `test:unit` include `dist/rfc018FeeBurn.test.js` |
| `docs/security/RIGOR_RFC018_EXPLORER_FEE_BURN_IMPLEMENTATION_REPORT.md` | This report |

No files under `core/src/consensus`, `core/src/validation.cpp`, or wallet consensus paths.

---

## 3. Database schema

Migration `005_rfc018_fee_burn_stats.sql`, applied by `ensureSchema` after `004_consensus_fingerprint.sql`.

```sql
CREATE TABLE IF NOT EXISTS rfc018_fee_burn_stats (
  height BIGINT PRIMARY KEY,
  block_hash TEXT NOT NULL REFERENCES blocks(block_hash) ON DELETE CASCADE,
  burned_oru BIGINT NOT NULL DEFAULT 0,
  cumulative_burned_oru BIGINT NOT NULL DEFAULT 0
);
```

Index: `rfc018_fee_burn_stats_block_hash_idx` on `block_hash`.

`burned_oru` is that block’s `destroyed_fee_oru`. `cumulative_burned_oru` is the prefix sum on the currently indexed chain. Values are **ORU atoms** (1e8).

---

## 4. API changes

Optional statistics. `equivalence_claimed` remains `false`.

### `GET /api/status` (`economic_snapshot`)

```json
{
  "rfc018_fee_burn": {
    "block_burned_oru": "...",
    "cumulative_burned_oru": "...",
    "block_burned_oru_display": "... ORU",
    "cumulative_burned_oru_display": "... ORU",
    "equivalence_claimed": false,
    "indexed": true,
    "height": "..."
  },
  "supply_chart": {
    "total_issued_oru": { "display": "...", "source": "..." },
    "cumulative_convergence": { "display": "...", "rpc": "...", "source": "..." },
    "rfc018_fee_burn": { "block_burned_oru": "...", "cumulative_burned_oru": "...", "source": "..." },
    "oru_circulating_spendable": { "atoms": "...", "display": "...", "source": "..." },
    "never_merged": true,
    "equivalence_claimed": false
  }
}
```

Legacy `oru_burned_lifetime_display` still means **Convergence destroyed** (`getblockchaininfo.cumulative_convergence`), not RFC-018 fee burn. UI labels say “Convergence destroyed ORU”.

### `GET /api/block/:hash`

Top-level `rfc018_fee_burn` with this block’s burn and chain prefix cumulative.

### `GET /api/blocks`

Each row includes nested `rfc018_fee_burn` (from the LEFT JOIN).

### `GET /api/chain/tip`

Nested `rfc018_fee_burn` `{ block_burned_oru, cumulative_burned_oru, indexed }`.

No node RPC was added. `getruntimesupply.supply_semantics.rfc018_fee_burn_cumulative` remains `not_persisted_on_block_index`.

---

## 5. Reorg behavior

Explorer already disconnects with:

```sql
DELETE FROM blocks WHERE height >= $1
```

`rfc018_fee_burn_stats.block_hash` references `blocks(block_hash) ON DELETE CASCADE`, so disconnected heights disappear with the blocks. Remaining prefix `cumulative_burned_oru` stays valid. Re-import of the new branch upserts new rows from parent cumulative.

No consensus undo, no chainstate field, no `DisconnectBlock` change.

After gap backfill, `recomputeRfc018CumulativePrefix` rebuilds `SUM(burned_oru) OVER (ORDER BY height)` so a filled hole cannot leave a stale suffix.

---

## 6. Supply charts (never merged)

Explorer displays four separate series:

| Series | Source | Must not be |
|--------|--------|-------------|
| Total issued ORU | `getruntimesupply.oru_subsidy_cumulative_derived` (Σ R) | RFC-002 ledger `S_ORU` relabeled as circulating |
| Convergence destroyed ORU | `cumulative_convergence` | RFC-018 fee burn |
| RFC-018 fee burned ORU | `rfc018_fee_burn_stats.cumulative_burned_oru` | Convergence, UTXO, or `S_ORU − UTXO` residual |
| Circulating spendable ORU | indexed `oru_utxos` with `CIRCULATING_ORU_SQL_PREDICATE` | issued − destroyed (that identity is not claimed here) |

`supply_chart.never_merged = true`. Circulating remains the UTXO SUM. Fee burn never entered the UTXO set, so it is not subtracted from circulating in the explorer.

---

## 7. Tests

`explorer/src/rfc018FeeBurn.test.ts` (run via `npm run test:unit`):

| Case | Assertion |
|------|-----------|
| Block with fee burn | RES-transfer `floor(1000/2) = 500` |
| Block without fee burn | Pure ORU fee, coinbase, ORU-funded mint → `0` |
| Multiple transactions | Two RES-transfers sum `2000 + 500` |
| Cumulative correctness | `0 + 500 + 0 + 1500 = 2000` |
| Reorg rollback | Abandoned tip burn is not added to the new branch |
| Precision | `ORU_COIN = 1e8`, `RES_COIN = 1e6`; share of 1 atom is 0 |
| Separation | View object has no circulating / Convergence / issued keys |

`explorer/src/uiSsr.test.ts` asserts S4 and block HTML keep Convergence destroyed, RFC-018 fee burn, issued, and circulating as distinct labels.

Importer/SQL reorg is the existing `truncateFromHeight` CASCADE path; unit tests cover the cumulative rollback identity the importer relies on (`parent + new_branch`, not `parent + old + new`).

---

## 8. Limits (honest)

- Indexer burn can lag the tip and can undercount a tx whose prevouts are not yet in `outputs`.
- Explorer cumulative is **not** a consensus scalar and is **not** fed to wallets.
- `S_ORU − circulating_UTXO` is still a mixed residual (fee burn + underclaim + any index gap). It is not labeled RFC-018 burn.
- `equivalence_claimed = false`: this indexer walk is not a Coq/C++ refinement of `ConnectBlock`.

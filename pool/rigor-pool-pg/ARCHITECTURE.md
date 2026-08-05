# rigor-pool-pg Architecture

**Task:** TASK-MINING-017  
**Authority:** `POSTGRESQL_PERSISTENCE_ARCHITECTURE.md`

## Layering

```
┌─────────────────────────────────────────────────────────┐
│  pool crates (rigor-gbt, block-assembler, …)            │
│  pg_adapter.rs — domain wiring + in-memory fallback     │
└──────────────────────────┬──────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────┐
│  winner-engine/persist/pg.rs — WinnerRepository traits    │
└──────────────────────────┬──────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────┐
│  rigor-pool-pg — PgPool, schema, SQL stores, recovery     │
└──────────────────────────┬──────────────────────────────┘
                           │
                    PostgreSQL
```

## Entity flow

`miners → workers → shares → candidate_blocks → winner_jobs → mint_jobs → treasury_utxos → payout_rounds / payout_lines → reorg_events → audit_events`

## Atomic transactions

`PgMintStore::reserve_mint_with_utxo` inserts `mint_jobs` and marks `treasury_utxos` **RESERVED** in a single database transaction.

## Recovery

`RecoveryEngine::check_tip_and_recover`:

1. Load `pool_chain_cursor`
2. Compare with chain tip; on mismatch insert `reorg_events` and run §5.5 orphan handler
3. Rehydrate counts for active candidates, inflight mints, replay share window

Winner invalidation uses `WinnerEngine::handle_invalidation` → `invalidate_winner()` / `revoke_authorization()` in the owning service.

## Observability

`PersistenceMetrics` tracks `pg_inserts`, `pg_updates`, `pg_reorgs`, `memory_fallback_ops`. Exposed via `PoolBackend::metrics_snapshot()`.

## Constraints

- No `CTxOut` or consensus changes
- Job snapshots immutable (`snapshot_jsonb` INSERT-only; status column UPDATE-only)
- `equivalence_claimed = false` until P-03 PostgreSQL soak

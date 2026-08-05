# rigor-pool-pg

PostgreSQL persistence infrastructure for the RIGOR Rust pool stack (**TASK-MINING-017**).

Authority: `docs/mining/POSTGRESQL_PERSISTENCE_ARCHITECTURE.md`

## Role

This crate provides:

- Connection pooling (`PgPool`) and schema migration (`migrations/001_schema.sql`)
- SQL store primitives (shares, job snapshots, mint/treasury, audit, recovery)
- `PoolBackend` — uses PostgreSQL when `DATABASE_URL` is set, otherwise callers fall back to in-memory state

Domain trait implementations (e.g. `WinnerRepository`) live in `winner-engine/src/persist/pg.rs`. Each pool crate exposes a thin `pg_adapter.rs` wrapper.

## Usage

```bash
# In-memory fallback (default in tests)
cargo test -p rigor-pool-pg

# Live PostgreSQL
export DATABASE_URL=postgres://user:pass@localhost/rigor_pool
cargo test -p winner-engine --features postgres -- --ignored
```

Build pool crates with PostgreSQL support:

```bash
cargo build -p rigor-pool-pilot --features postgres
```

## Crate adapters

| Crate | `pg_adapter.rs` | Persisted domain |
|-------|-----------------|------------------|
| `rigor-gbt` | template archive, longpollid |
| `block-assembler` | `PublishedJobSnapshot`, job serial |
| `share-validator` | share replay set |
| `stratum-server` | shares, PPLNS, workers, audit |
| `mint-engine` | mint_jobs + treasury (atomic TX) |
| `winner-engine` | winner_jobs, candidates, auth tokens |
| `pilot` | status snapshots, recovery orchestration |

## Impact

| Field | Value |
|-------|-------|
| Consensus | **NONE** |
| Protocol | **NONE** |
| Freeze | **NONE** |
| `equivalence_claimed` | **false** |

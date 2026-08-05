# winner-engine

Production Winner Engine for the RIGOR pool stack (**TASK-MINING-016**, wired into
Stratum + Mint Engine in **TASK-MINING-018**).

Implements `docs/mining/WINNER_ENGINE_PRODUCTION_SPEC.md`: convergence candidate
extraction, canonical convergence root, full draw replay, mint authorization, and
in-memory persistence with PostgreSQL adapters via `rigor-pool-pg` (TASK-MINING-017).

## Status

| Field | Value |
|-------|-------|
| `equivalence_claimed` | **false** (P-03 certification pending) |
| Consensus impact | **NONE** |
| Protocol impact | **NONE** |

## Quick start (pool integration)

```rust
use winner_engine::WinnerRuntime;

let mut rt = WinnerRuntime::open()?; // DATABASE_URL → PostgreSQL; else in-memory

// Stratum publish path
let preview = rt.preview_winner(&template, &job)?;
let _cand_id = rt.register_published_job(&template, &job)?;

// Block-found path
if let Some(token) = rt.authorize_mint_for_block(&template, &job, true, false)? {
    // mint-engine consumes token via mint_for_authorized_winner
}

// Reorg
rt.invalidate_job(&job, winner_engine::RevocationReason::Reorg)?;
```

Lower-level draw replay (tests / golden vectors):

```rust
use winner_engine::{
    extract_convergence_candidates, replay_draw_from_candidate_set,
    RES_PROBABILITY_TARGET, GOLDEN_TARGET_PREV,
};
let set = extract_convergence_candidates(&template)?;
let draw = replay_draw_from_candidate_set(&set, bind_block_hash, GOLDEN_TARGET_PREV, RES_PROBABILITY_TARGET);
```

## PostgreSQL persistence

```rust
use winner_engine::{WinnerPgBackend, persist_golden_draw};

let mut backend = WinnerPgBackend::open()?;
// Uses PostgreSQL when DATABASE_URL is set; otherwise InMemoryWinnerStore.
```

Trait implementation: `src/persist/pg.rs` (feature `postgres`).  
Thin adapter: `src/pg_adapter.rs`.

## Tests

```bash
cd pool/winner-engine
cargo test
# Live PostgreSQL golden soak:
# export DATABASE_URL=postgres://...
# cargo test --features postgres --test golden_persist -- --ignored
```

Golden vectors load `formal_verification/phase3_golden_vectors.json` (ConvergenceRoot,
SigmaCR01, Inequality, RunSelection).

## Encoding note

- **GBT path:** `convergence_txids` use Bitcoin display hex → `display_hex_to_native`.
- **Golden / hash outputs:** forward byte hex → `hash_hex_to_native`.

## See also

- `ARCHITECTURE.md` — module map and state machine
- `TEST_PLAN.md` — validation matrix

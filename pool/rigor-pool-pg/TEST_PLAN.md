# rigor-pool-pg Test Plan

**Task:**

## Automated tests

| ID | Test | Location | Status |
|----|------|----------|--------|
| PG-01 | Memory backend open | `tests/backend_memory.rs` | PASS |
| PG-02 | Recovery noop (memory) | `tests/backend_memory.rs` | PASS |
| PG-03 | `EQUIVALENCE_CLAIMED == false` | `tests/backend_memory.rs` | PASS |
| PG-04 | Golden persist memory roundtrip | `winner-engine/tests/golden_persist.rs` | PASS |
| PG-05 | Golden persist PostgreSQL | `winner-engine/tests/golden_persist.rs` | IGNORE (needs `DATABASE_URL`) |

## Manual PostgreSQL soak

```bash
createdb rigor_pool_test
export DATABASE_URL=postgres://localhost/rigor_pool_test
cargo test -p winner-engine --features postgres --test golden_persist -- --ignored
```

## Golden test report (memory runtime)

| Vector module | Persist roundtrip | Winner auth fields |
|---------------|-------------------|-------------------|
| RunSelection WIN-000..006 | PASS (memory) | convergence_root, mint_required, eligible_count |

PostgreSQL runtime report: pending live `DATABASE_URL` soak (PG-05).

## Regression

```bash
cargo test -p rigor-pool-pg
cargo test -p winner-engine
cargo test -p block-assembler
cargo test -p share-validator
cargo test -p stratum-server
```

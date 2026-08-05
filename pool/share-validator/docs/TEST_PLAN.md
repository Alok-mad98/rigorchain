# share-validator Test Plan

**Run:** `cd pool/share-validator && cargo test`

## Unit tests (6)

| Module | Test | Covers |
|--------|------|--------|
| `opuhash` | `reference_vector_base_0x01` | Golden vector vs `pow_tests.cpp` |
| `opuhash` | `reference_vector_base_all_ff` | Golden vector |
| `opuhash` | `verify_share_rejects_high_pow` | Impossible target |
| `replay` | `rejects_duplicate_share_id` | ERR_REPLAY_SHARE |
| `replay` | `rejects_stale_job` | ERR_STALE_JOB |
| `convergence` | `rejects_convergence_txid_not_in_mempool` | ERR_CONVERGENCE_MISMATCH |

## Integration tests (7)

| Test | Covers |
|------|--------|
| `assembler_validator_convergence_consistency` | Assembler ↔ validator C binding |
| `accepts_share_when_pow_and_bindings_pass` | Valid share acceptance |
| `rejects_convergence_root_mismatch` | Root recomputation |
| `rejects_invalid_opuhash_on_impossible_target` | ERR_INVALID_OPOUHASH |
| `rejects_stale_job_after_reorg` | Reorg invalidation |
| `rejects_replay_share_id` | Replay protection |
| `opuhash_golden_vectors_match_rigord` | Cross-crate golden check |

## Deferred

- Live `rigord submitblock` soak
- SIMD OpuHash performance benchmarks
- DB-backed replay persistence

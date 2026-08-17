# share-validator Test Plan

**Run:** `cd pool/share-validator && cargo test`

## Unit tests

| Module | Test | Covers |
|--------|------|--------|
| `opuhash` | `reference_vector_base_0x01` | Golden vector vs `pow_tests.cpp` |
| `opuhash` | `reference_vector_base_all_ff` | Golden vector |
| `opuhash` | `verify_share_rejects_high_pow` | Impossible target |
| `replay` | `rejects_duplicate_share_id` | ERR_REPLAY_SHARE |
| `replay` | `rejects_stale_job` | ERR_STALE_JOB |
| `convergence` | `accepts_historical_window_txid_not_in_current_transactions` | A-9-C window (no body membership) |
| `convergence` | `accepts_two_window_txids_absent_from_template_body` | C over buried F + GBT root |
| `convergence` | `rejects_malformed_convergence_txid` | ERR_CONVERGENCE_MISMATCH |
| `convergence` | `rejects_gbt_published_root_drift` | GBT root ≠ C(list) |
| `convergence` | `committed_window_drops_fallen_block_and_paid_winner` | A-9-C slide + paid exclusion |
| `convergence` | `rejects_gbt_list_that_drifts_from_parent_window` | GBT vs on-chain window |
| `convergence` | `accepts_gbt_list_matching_parent_window` | GBT vs on-chain window |
| `job_binding` | `accepts_matching_job_and_template` | height / prevhash / version |
| `job_binding` | `rejects_height_mismatch` | ERR_STALE_JOB |
| `job_binding` | `rejects_prevhash_mismatch` | ERR_STALE_JOB |
| `job_binding` | `rejects_template_version_mismatch` | ERR_STALE_JOB |
| `job_binding` | `rejects_non_active_template` | ERR_STALE_JOB |
| `res_mint` | `accepts_node_automint_with_historical_window` | node mint + buried F |
| `res_mint` | `rejects_wrong_mint_amount` | not 100.00 RES |
| `res_mint` | `rejects_wrong_recipient` | res_program ≠ RESO |
| `res_mint` | `rejects_missing_mint_when_winner` | mint required |
| `res_mint` | `rejects_extra_mint` | exactly one |
| `res_mint` | `rejects_mint_without_winner` | no mint if no winner |
| `res_mint` | `placeholder_still_forbids_pool_built_mint` | pool must not construct RES |
| `res_mint` | `block_must_carry_the_gbt_automint` | submitblock cannot replace mint |

## Integration tests

| Test | Covers |
|------|--------|
| `assembler_validator_convergence_consistency` | Assembler ↔ validator C binding |
| `accepts_share_when_pow_and_bindings_pass` | Valid share acceptance |
| `accepts_share_with_historical_window_txid_and_opaque_mint` | A-9-C buried F + opaque GBT body (no winner) |
| `rejects_share_when_winner_present_but_mint_missing` | ERR_RES_MINT_INVALID |
| `rejects_convergence_root_mismatch` | Root recomputation |
| `rejects_invalid_opuhash_on_impossible_target` | ERR_INVALID_OPOUHASH |
| `rejects_stale_job_after_reorg` | Reorg invalidation |
| `rejects_replay_share_id` | Replay protection |
| `opuhash_golden_vectors_match_rigord` | Cross-crate golden check |

## Deferred

- Live `rigord submitblock` soak
- SIMD OpuHash performance benchmarks
- DB-backed replay persistence
- Production `CommittedWindowSource` wired to `getconvergenceresult(parent)`

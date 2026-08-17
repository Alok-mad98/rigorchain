# share-validator

Share Validation Layer for the RIGOR Pool Stack (TASK-MINING-008).

Enforcement boundary between miner submissions, Block Assembler jobs, and `rigord` consensus rules — without modifying consensus, RPC, or protocol code.

## Dependencies

| Crate | Role |
|-------|------|
| [`rigor-gbt`](../rigor-gbt) | Read-only `TemplateRecord` |
| [`block-assembler`](../block-assembler) | Read-only `JobSnapshot` |

## Validation pipeline (ordered)

1. JobSnapshot existence + ACTIVE state
2. TemplateRecord FK match (record_id, height, previousblockhash, template_version, tip_hash)
3. OpuHash proof (`verify_opuhash_share` / `verify_opuhash_block`)
4. `convergence_txids` well-formed 32-byte ids (A-9-C: **not** required in this block's `transactions` / mempool)
5. `convergence_root` recomputation (C over the window list; match coinbase + GBT published root)
6. Optional: GBT `convergence_txids` vs the parent's on-chain W=32 window (`CommittedWindowSource`)
7. CVGE + RIGOR consistency
8. RES: pool must not construct a mint; node GBT automint is positively checked (count, 100.00 RES, winner `res_program`, bootstrap spend, PCORB fields)
9. Duplicate `share_id` replay protection

## API

```rust
use share_validator::{ShareValidator, ShareSubmission, BlockSubmission};

let validator = ShareValidator::with_defaults(jobs, templates);
let result = validator.validate_share(share);
let block_result = validator.validate_block(block);
let state = validator.get_validation_state(&job_id);
```

## OpuHash

Implements `GetOpusHashFromBaseHash` per `core/src/pow_opus.cpp`:

- 64 KiB ChaCha20 buffer
- 256 mix rounds
- Golden vectors from `pow_tests.cpp::OpuHash_v3plus_reference_vectors`

## Error codes

| Code | Meaning |
|------|---------|
| `ERR_INVALID_OPOUHASH` | PoW above target |
| `ERR_CONVERGENCE_MISMATCH` | C / CVGE / RIGOR drift |
| `ERR_REPLAY_SHARE` | Duplicate `share_id` |
| `ERR_STALE_JOB` | Non-ACTIVE job, or height / prevhash / template_version drift |
| `ERR_RES_MINT_INVALID` | Pool-built mint, or node automint missing/wrong amount/recipient/funding |
| `ERR_TEMPLATE_MISMATCH` | `record_id` / template FK failure |

## Build & test

```bash
cd pool/share-validator
cargo test
```

## Governance

| Field | Value |
|-------|-------|
| Consensus impact | **NONE** |
| Protocol impact | **NONE** |
| Freeze impact | **NONE** |
| `equivalence_claimed` | **false** |

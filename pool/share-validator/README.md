# share-validator

Share Validation Layer for the RIGOR Pool Stack (TASK-MINING-008).

Enforcement boundary between miner submissions, Block Assembler jobs, and `rigord` consensus rules — without modifying consensus, RPC, or protocol code.

## Dependencies

| Crate | Role |
|-------|------|
| [`rigor-gbt`](../rigor-gbt) | Read-only `TemplateRecord` |
| [`block-assembler`](../block-assembler) | Read-only `JobSnapshot` |

## Validation pipeline (ordered)

1. JobSnapshot existence
2. TemplateRecord match
3. OpuHash proof (`verify_opuhash_share` / `verify_opuhash_block`)
4. `convergence_txids` membership
5. `convergence_root` recomputation
6. CVGE + RIGOR consistency
7. RES body-mint placeholder safety
8. Duplicate `share_id` replay protection

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
| `ERR_STALE_JOB` | Non-ACTIVE job |
| `ERR_RES_MINT_INVALID` | Unauthorized body mint |
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

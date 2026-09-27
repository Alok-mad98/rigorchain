# block-assembler Test Plan

**Task:**
**Run:** `cd pool/block-assembler && cargo test`

## Unit tests

| Module | Test | Verifies |
|--------|------|----------|
| `convergence` | `zero_root_when_fewer_than_two_txids` | C = 0 when \|F\| < 2 |
| `convergence` | `root_is_order_independent` | Sort before Hash256 |
| `script` | `bip34_small_height_uses_opcodes` | OP_1..OP_16 encoding |
| `script` | `rigor_script_is_ten_bytes` | RIGR + prev_nbits LE32 |
| `coinbase` | `coinbase_contains_height_and_convergence_root` | scriptSig + outputs |
| `merkle` | `single_tx_merkle_is_tx_hash` | Merkle degenerate case |
| `placeholder` | `slot_index_follows_gbt_package` | vtx[k+1] reservation |
| `placeholder` | `placeholder_txid_is_stable` | Deterministic seed hash |
| `cache` | `job_and_candidate_ids_are_unique_and_paired` | UK-02, UK-05 |
| `cache` | `publish_stales_previous_active_on_same_tip` | STALE transition |
| `cache` | `tip_change_invalidates_jobs` | INVALIDATED on reorg |
| `publisher` | `publish_assigns_unique_job_and_candidate_ids` | End-to-end publish |

## Integration tests (`tests/integration.rs`)

| Test | Verifies |
|------|----------|
| `end_to_end_template_cache_to_job_publish` | rigor-gbt ingest → publish → read API |
| `reorg_invalidates_jobs_and_allows_new_publish` | Tip change + new job_id |
| `body_mint_placeholder_is_immutable_and_reserved` | Placeholder slot + per-record stability |

## Manual verification (optional, against rigord)

1. Run `rigor-gbt` poll loop against testnet `rigord`
2. Pass ACTIVE `TemplateRecord` to `JobPublisher::publish_from_template`
3. Inspect `coinbase.scriptsig_hex` contains height push + 32-byte C
4. Inspect `commitments.rigor_script_hex` uses `prev_nbits` not `bits`
5. Confirm `body_mint.reserved == true` and `body_mint_txid == None`

## Not in scope (deferred)

- Golden `assembly_hash` vectors against `ConnectBlock`
- Live RES body-mint vtx insertion
- Stratum wire `mining.notify` encoding
- Database persistence of `candidate_block_id`

## Expected CI gate

All **15** automated tests pass (`12` unit + `3` integration).

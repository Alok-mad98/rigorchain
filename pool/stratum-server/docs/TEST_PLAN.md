# stratum-server Test Plan

**Task:** ,
**Run:** `cd pool/stratum-server && cargo test`

## Unit tests (8)

| Module | Test |
|--------|------|
| `protocol` | subscribe parse, opushash agent gate |
| `difficulty` | higher difficulty → lower target |
| `payout` | PPLNS ORU distribution |
| `winner` | authoritative preview (not min-txid sort); root mismatch rejection |
| `res_mint` | 100 RES atoms; `winner_context_from_token`; preview mapping |

## Integration tests (6)

| Test | Covers |
|------|--------|
| `subscribe_authorize_flow` | Stratum handshake |
| `rejects_non_opushash_agent` | User-agent gate |
| `full_pipeline_publish_notify_submit` | GBT → job → notify envelope winner fields |
| `res_mint_revoked_on_reorg` | Winner invalidate + mint revoke on tip change |
| `reorg_orphans_payout_shares` | Reorg orphan handling |
| `job_expiry_invalidates_downstream` | STALE on supersede |

## Cross-crate verification 

| Path | Crates | Status |
|------|--------|--------|
| Stratum → Winner → Mint → Assembler | `stratum-server`, `winner-engine`, `mint-engine`, `block-assembler` | PASS (integration + unit) |
| P-03 golden replay | `winner-engine/tests/golden_vectors.rs` | PASS |
| PostgreSQL soak | `DATABASE_URL` + `--features postgres` | optional `#[ignore]` |

## Manual soak (optional)

```bash
# Terminal 1: start rigord + ingest loop (user-operated)
# Terminal 2: run stratum TCP server binding 0.0.0.0:3333
```

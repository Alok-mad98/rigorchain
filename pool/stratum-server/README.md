# stratum-server

Production Stratum v1 server and full miner submission pipeline (TASK-MINING-009).
Winner Engine wiring for authoritative draw preview and mint authorization
(TASK-MINING-018).

## Stack integration

| Crate | Role |
|-------|------|
| `rigor-gbt` | `TemplateRecord` source |
| `block-assembler` | `JobSnapshot` publish |
| `winner-engine` | `WinnerRuntime` via `SharedWinnerService` — preview, authorize, invalidate |
| `mint-engine` | `mint_for_authorized_winner` — RES body mint only with token |
| `share-validator` | Share / block validation |

## Stratum v1 methods

| Method | Support |
|--------|---------|
| `mining.subscribe` | ✅ (requires `opushash` user-agent) |
| `mining.authorize` | ✅ |
| `mining.notify` | ✅ (server push) |
| `mining.set_difficulty` | ✅ (via session) |
| `mining.submit` | ✅ → share-validator |

## Pipeline

```text
GBT ingest → TemplateRecord → JobPublisher
  → WinnerRuntime::register_published_job + preview_winner
  → mining.notify (preview_winner_txid, mint_required)
mining.submit → ShareValidator → PPLNS credit
  → (block?) WinnerRuntime::authorize_mint_for_block
  → MintEngine::mint_for_authorized_winner
tip change / reorg → invalidate_jobs + mint revoke
```

## Winner Engine

- `SharedWinnerService::memory()` — default for tests
- `SharedWinnerService::open()` — PostgreSQL when `DATABASE_URL` is set (feature `postgres`)
- `StratumJobEnvelope.preview_winner_txid` — authoritative draw winner (replaces min-txid heuristic)

## Build & test

```bash
cd pool/stratum-server
cargo test
```

## Governance

| Field | Value |
|-------|-------|
| Consensus impact | **NONE** |
| Protocol impact | **NONE** (Stratum v1 wire unchanged) |
| Freeze impact | **NONE** |
| `equivalence_claimed` | **false** |

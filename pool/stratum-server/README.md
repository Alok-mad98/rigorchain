# stratum-server

ORU-only Stratum v1 server and miner submission pipeline (TASK-MINING-009).
Option A (`TASK-POOL-RES-BOUNDARY-IMPLEMENT-003`): no RES mint construction.

## Stack integration

| Crate | Role |
|-------|------|
| `rigor-gbt` | `TemplateRecord` source (opaque txs) |
| `block-assembler` | `JobSnapshot` publish + `assemble_solved_block` |
| `winner-engine` | Optional draw preview telemetry only (not mint authorize) |
| `share-validator` | Share / block validation |

`mint-engine` is **not** a runtime dependency.

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
  → mining.notify (ORU job; mint_required=false)
  → mining.submit → ShareValidator
  → (block candidate) assemble_solved_block → submitblock
  → PPLNS ORU credit after node accepts
```

## Impact

| Field | Value |
|-------|-------|
| Consensus | NONE |
| Protocol wire | Stratum v1 unchanged |
| RES issuance | NONE (node automint / wallet Convergence only) |
| equivalence_claimed | false |

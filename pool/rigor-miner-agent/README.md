# rigor-miner-agent

Production CPU miner client for the RIGOR pool Stratum stack (TASK-MINING-022A / **023**).

Stratum v1 client, job cache, **CPU SHA3-256 RGRPOW01 mining loop** (`share-validator` port), multi-thread nonce scanning, convergence metadata display, metrics, and CLI. No consensus logic, winner selection, mint, or convergence replay.

## Stack position

```text
rigor-miner-agent  ──TCP Stratum v1──►  stratum-server
                                              │
                    block-assembler ◄───────────┘
                    winner-engine (preview only — display)
                    share-validator
                    mint-engine (pool only)
```

## Features

| Component | Status |
|-----------|--------|
| Stratum client | `connect`, `subscribe`, `authorize`, `read_event`, `submit_share` |
| Job manager | `JobSnapshot` cache, stale detection, statistics |
| CPU mining | `CpuOpuHashWorker`, `CpuNonceScanner`, `MiningController` (`--threads N`) |
| Convergence metadata | Display-only (`convergence_root_hex`, `preview_winner_txid`, `mint_required`) |
| Metrics | hashrate, hashes, shares found/submitted/accepted/rejected, stale jobs, uptime |
| CLI | `rigor-miner-agent --pool --worker --password --threads --log-interval-secs --stats` |

## Build & test

```bash
cd pool/rigor-miner-agent
cargo test
cargo run -- --pool 127.0.0.1:3333 --worker test.worker --password x --threads 2
cargo run -- --pool 127.0.0.1:3333 --worker test.worker --password x --stats
```

## Constraints

- Agent string **must** contain `rgrpow01` (default: `rigor-miner/1.0-rgrpow01`)
- Miner **never** performs draw selection, winner validation, or mint locally
- No pool or protocol modifications in this task

## Governance

| Field | Value |
|-------|-------|
| Architecture impact | **NONE** (new miner crate) |
| Consensus impact | **NONE** |
| Protocol impact | **NONE** |
| Freeze impact | **NONE** |
| `equivalence_claimed` | **false** |

## See also

- `docs/mining/TASK-MINING-022-CPU-MINER-AGENT-SPEC.md`
- `ARCHITECTURE.md`
- `TEST_PLAN.md`

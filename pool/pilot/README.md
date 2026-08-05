# rigor-pool-pilot

Closed testnet pilot binary for TASK-MINING-010. Wires the in-memory pool stack:

`rigor-gbt` → `block-assembler` → `share-validator` → `stratum-server` → `mint-engine`

## Environment

| Variable | Default | Description |
|----------|---------|-------------|
| `RIGOR_RPC_URL` | `http://127.0.0.1:8332/` | `rigord` JSON-RPC |
| `RIGOR_RPC_USER` / `RIGOR_RPC_PASSWORD` | — | RPC auth |
| `STRATUM_LISTEN` | `127.0.0.1:3333` | Stratum v1 bind address |
| `GBT_POLL_INTERVAL_MS` | `2000` | GBT poll interval |
| `MINT_AUTO_SUBMIT` | `false` | Submit assembled blocks via RPC |
| `PPLNS_WINDOW` | `1` | PPLNS round window |
| `PILOT_STATUS_FILE` | `.runtime/pool-pilot/status.json` | Periodic metrics dump |

PostgreSQL / Redis are **not required** — the Rust pool stack uses in-memory state only.

## Build

```bash
cd pool/pilot
cargo build --release
```

## Run (against live `rigord`)

```bash
export RIGOR_RPC_URL=http://127.0.0.1:48761/
export RIGOR_RPC_USER=devtest
export RIGOR_RPC_PASSWORD=devtestpass
export MINT_AUTO_SUBMIT=true
./target/release/rigor-pool-pilot
```

Full pilot procedure: [docs/mining/POOL_CLOSED_TESTNET_PILOT.md](../../docs/mining/POOL_CLOSED_TESTNET_PILOT.md)

## Tests

```bash
cargo test
```

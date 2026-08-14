# rigor-pool (Alpha / ORU-only)

Product binary for the RIGOR Stratum pool. Packaging installs a compatibility copy
named `rigor-pool-pilot` (same binary; no second Cargo `[[bin]]`).

```text
rigor-gbt → block-assembler → share-validator → stratum-server
                                              → ORU PPLNS payout
```

**No mint-engine.** RES involvement remains **NONE** on the pool path.

## Commands

```bash
# Local Alpha / regtest (recommended first run)
rigor-pool --config pool/config/example.devtest.toml run
rigor-pool --config pool/config/example.devtest.toml status

# Production-shaped (requires secrets in TOML or env)
rigor-pool --config pool/config/example.toml run
```

## Configuration

| File | Profile | Starts without secrets? |
|------|---------|-------------------------|
| `pool/config/example.devtest.toml` | `devtest` | Yes (password `x`) |
| `pool/config/example.toml` | `production` | No — needs payout script + worker hashes |

Environment variables still override TOML keys:

| Variable | Description |
|----------|-------------|
| `RIGOR_RPC_URL` / `RIGOR_RPC_USER` / `RIGOR_RPC_PASSWORD` | Node JSON-RPC |
| `STRATUM_LISTEN` | `host:port` Stratum bind |
| `ORU_FEE_PERCENT` | Pool fee % of coinbase ORU |
| `PAYOUT_INTERVAL` | Ops settlement cadence (seconds) |
| `PPLNS_WINDOW` | PPLNS window |
| `RIGOR_POOL_PROFILE` | `production` or `devtest` |
| `RIGOR_POOL_PAYOUT_SCRIPT_HEX` | Coinbase ORU script (production) |
| `RIGOR_POOL_WORKER_CREDENTIALS` | `worker=hash;…` (production) |
| `DATABASE_URL` | Optional Postgres |
| `PILOT_STATUS_INTERVAL_SECS` / `PILOT_STATUS_FILE` | Status JSON |

## Build

```bash
cd pool/pilot
cargo build --release --bin rigor-pool
# output: target/release/rigor-pool
# packaging copies it to rigor-pool-pilot for script compatibility
```

Miner agent: `pool/rigor-miner-agent`.  
Guide: `docs/pool/MINER_CONNECTION_GUIDE.md`.

## Tests

```bash
cargo test
```

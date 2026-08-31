# RIGOR Pool (Alpha / ORU-only)

> **SOLO quarantine (P0-014 M-3).** This directory is **not** part of the shipped
> SOLO miner / `rigord` release. Do not add Stratum extranonce, a pool payout
> layer, or a second reward path to `core/`. `scriptSig` has zero free bytes.

**Status:** Internal Alpha mining product  
**Authority:** `docs/pool/RIGOR_POOL_PRODUCT_POSITION.md` (Option A)  
**Consensus / W / T / RES issuance:** Unchanged — this tree does not modify them.

## What this pool is

- Stratum mining coordination (`opushash`)
- Share validation
- Block assembly + `submitblock`
- **ORU** payout accounting (PPLNS + optional fee)

## What this pool is not

- Does **not** create, trigger, allocate, or simulate **RES**
- Does **not** depend on `mint-engine` at runtime
- Does **not** use the legacy yiimp stack

## Active binaries (Rust)

| Binary | Crate | Role |
|--------|-------|------|
| `rigor-pool` / `rigor-pool-pilot` | `pool/pilot` | Pool server |
| `rigor-miner-agent` | `pool/rigor-miner-agent` | CPU Stratum miner |

Config examples:
- Local Alpha: [`config/example.devtest.toml`](./config/example.devtest.toml)
- Production-shaped: [`config/example.toml`](./config/example.toml)

Miner guide: [`../docs/pool/MINER_CONNECTION_GUIDE.md`](../docs/pool/MINER_CONNECTION_GUIDE.md)  
Alpha package: `dist/alpha/pool/`

```bash
# Build
cd pool/pilot && cargo build --release --bin rigor-pool
cd ../rigor-miner-agent && cargo build --release

# Local Alpha (devtest — starts without production secrets)
../pilot/target/release/rigor-pool --config ../config/example.devtest.toml run
# or: dist/alpha/pool/linux/bin/rigor-pool --config pool/config/example.devtest.toml run

# Status (pool must already be running)
rigor-pool --config pool/config/example.devtest.toml status
```

`example.toml` is profile=production and will refuse to start until `payout_script_hex` + `worker_credentials` are set.

---

## LEGACY / ABANDONED — do not operate for RIGOR

### Abandoned yiimp vendor tree

The following paths are **upstream yiimp leftovers**. They are **not** the RIGOR Alpha pool.
Do not install, cron, or point miners at them for RIGOR:

| Path | Notes |
|------|-------|
| `pool/stratum/` | C++ multi-algo stratum |
| `pool/web/` | PHP yaamp UI |
| `pool/sql/` | MySQL dumps |
| `pool/bin/` | yiimp CLI helpers |
| `pool/blocknotify*` | Daemon hooks for yiimp |

Historical yiimp install prose that used to live in this README has been removed to avoid operator confusion.

### Legacy mint-engine

`pool/mint-engine/` is **LEGACY / audit-only**. It is **not** linked into `rigor-pool` or `stratum-server`.  
RES body mint (when required) is assembled by `rigord` automint / wallet Convergence — never by the pool product.

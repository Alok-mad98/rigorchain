# RIGOR Pool

**Node RPC (mainnet):** `8761`.  
**Network label:** `mainnet` (not an HRP string; HRP is `rg`).

> **SOLO quarantine.** This directory is **not** part of the shipped SOLO
> miner / `rigord` release. Do not add Stratum extranonce, a pool payout
> layer, or a second reward path to `core/`.

The pool is an **ORU-only** mining coordination product. It polls
`getblocktemplate` from `rigord`, validates templates, and may run a
Stratum listener for workers. It does **not** mint RES.

## What this pool is

- GBT client against `rigord` (`pool/rigor-gbt`, default RPC `http://127.0.0.1:8761`)
- Share validation / ORU payout accounting (PPLNS + optional fee)
- Block submission via node `submitblock`
- Template format is the node GBT (canonical coinbase copy-bytes)

## What this pool is not

- Does **not** create, trigger, allocate, or simulate **RES**
- Does **not** replace node coinbase assembly
- Does **not** use the leftover yiimp / yaamp tree
- SOLO miners must talk to `rigord` GBT, not to this pool, unless they
  intentionally join the pool product

## Active binaries (Rust)

| Binary | Crate | Role |
|--------|-------|------|
| `rigor-pool` / `rigor-pool-pilot` | `pool/pilot` | Pool server |
| `rigor-miner-agent` | `pool/rigor-miner-agent` | CPU Stratum miner |
| `rigor-gbt` | `pool/rigor-gbt` | GBT adapter + template cache |

Config:

- Production-shaped: [`config/example.toml`](./config/example.toml) (`network = "mainnet"`, `rpc_port = 8761`)
- Local/devtest: [`config/example.devtest.toml`](./config/example.devtest.toml) (`rpc_port = 48761`)

Miner guide: [`../docs/pool/MINER_CONNECTION_GUIDE.md`](../docs/pool/MINER_CONNECTION_GUIDE.md)

```bash
cd pool/pilot && cargo build --release --bin rigor-pool
cd ../rigor-miner-agent && cargo build --release
```

`example.toml` is profile=production and will refuse to start until
`payout_script_hex` + `worker_credentials` are set.

PoW digest used by the **node** and by pool share validation is SHA3-256
`RGRPOW01`. Production flow: [`../docs/pool/PRODUCTION_MINING_FLOW.md`](../docs/pool/PRODUCTION_MINING_FLOW.md).

---

## Historical / abandoned — do not operate

The following paths are **upstream yiimp leftovers**. They are not the
RIGOR pool:

| Path | Notes |
|------|-------|
| `pool/stratum/` | C++ multi-algo stratum |
| `pool/web/` | PHP yaamp UI |
| `pool/sql/` | MySQL dumps |
| `pool/bin/` | yiimp CLI helpers |
| `pool/blocknotify*` | Daemon hooks for yiimp |

Lottery `winner-engine` / `mint-engine` crates were removed. RES mint is a
node body transaction priced by `destroy_cost(h)`, never by the pool.

Alpha / AlphaPlus pool packaging names are historical.

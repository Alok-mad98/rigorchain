# RIGOR Windows Miner Dashboard

**Document status:** Production documentation

| Field | Value |
|-------|-------|
| **Surface** | `http://127.0.0.1:18083/` |
| **Mode** | Read-only visualization |
| **Mining path** | SOLO GBT |
| **Consensus / kernel / PoW / validation / RES rules / wallet accounting** | **Unchanged** |

This dashboard is the Windows miner visualization layer for the shipped SOLO path. It is not a backend operator console, not a pool payout UI, and not a consensus authority.

---

## User workflow

A Windows miner should be able to:

1. Start Wallet
2. Start local `rigord`
3. Start `rigor-miner`
4. Mine ORU via GBT copy-bytes + `submitblock`
5. Watch height and miner status
6. See maturity
7. See Convergence
8. Understand that RES is not mined
9. Open Explorer

without reading backend logs.

```
Windows User
   ↓
Wallet (rg1q…)
   ↓
Local rigord (RPC 8761)
   ↓
rigor-miner  getblocktemplate → copy coinbase → nonce grind → submitblock
   ↓
ORU Maturity
   ↓
Convergence
   ↓
RES result (not mined)
```

The quarantined pool observer is secondary. It is not the SOLO mining path.

---

## Architecture

```
Windows browser  →  127.0.0.1:18083  (this dashboard, read-only)
                         |
         +---------------+----------------+
         |               |                |
   Explorer :18081   Local rigord :8761  Wallet RPC (opt-in)
   Observability :18082  Optional pool status.json
```

Implementation lives in `mining/windows-miner-dashboard/`:

| Path | Role |
|------|------|
| `server.py` | Loopback HTTP, GET only, EventSource `/api/stream` |
| `aggregator.py` | Maps existing results into user-facing JSON |
| `public/` | Miner account UI |
| `installer/windows/start-miner-dashboard.cmd` | Windows launcher |

The dashboard does **not** submit shares, assemble blocks, sign, or call consensus code.

---

## API

All routes are **GET**, localhost, no private data.

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/` | Miner dashboard HTML |
| GET | `/healthz` | Liveness |
| GET | `/api/status` | Full snapshot |
| GET | `/api/stream` | Same snapshot as Server-Sent Events |
| GET | `/api/miner/status` | Wallet, miner, node, height, lifecycle |
| GET | `/api/miner/workers` | Miner instance table |
| GET | `/api/oru/convergence` | WAITING / WINNER + contribution counts |
| GET | `/api/wallet/mining` | Immature ORU, Trusted ORU, spendable |
| GET | `/api/res/status` | Current RES, pending, last winner, events |
| GET | `/api/pool/status` | Quarantined pool observer |

POST / PUT / DELETE return `405`. Non-loopback bind is refused.

Optional payout address is pasted in the UI (`rg1q…` only) or set as `MINING_ADDRESS`.
The dashboard stores it in the browser and requests `?address=` on GET APIs.

Windows package (miner installer / zip, not the wallet installer):

```bash
bash scripts/alpha/package_windows_miner.sh
```

Artifacts: `dist/alpha/windows/` miner zip/exe (names follow the
current `RIGOR_RELEASE_VERSION`). Launch `start-miner-dashboard.cmd` from that package.

---

## Security

| Rule | Result |
|------|--------|
| Localhost default (`127.0.0.1:18083`) | Yes |
| Read-only | Yes (GET only) |
| No wallet signing | Yes |
| No private key access | Yes |
| No RPC password exposure | Secret keys stripped |

Never expose: `rpcuser`, `rpcpassword`, wallet seed, private keys.

---

## Start

```bash
cd mining/windows-miner-dashboard
python3 server.py
```

Windows:

```text
installer\windows\start-miner-dashboard.cmd
```

Then open `http://127.0.0.1:18083/` in the Windows browser. Explorer remains `http://127.0.0.1:18081/`.

---

## Tests

```bash
cd mining/windows-miner-dashboard
python3 -m unittest tests.test_dashboard -v
```

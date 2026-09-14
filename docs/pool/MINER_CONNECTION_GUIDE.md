# RIGOR Pool Miner Connection Guide (Alpha / ORU-only)

**Audience:** Internal Alpha miners connecting to a RIGOR Stratum pool  
**Product rule:** Pool pays **ORU** only. The pool does **not** mint, allocate, or pay **RES**.  
**Consensus / W / T / RES issuance:** Unchanged — RES remains Convergence + node automint.

Canonical product position: [`RIGOR_POOL_PRODUCT_POSITION.md`](./RIGOR_POOL_PRODUCT_POSITION.md)

---

## 1. What you earn

| Asset | Source |
|-------|--------|
| **ORU** | Proof-of-work shares on this pool (PPLNS accounting). Consensus coinbase subsidy is RFC-020: **0** at genesis, **128 ORU** per block forever, no halving. `coinbasevalue` is that subsidy plus claimable ORU fees. |
| **RES** | **Not from the pool.** Optional later via wallet Convergence (body-tx destroy at `destroy_cost(h)`). |

Do not configure the miner “for RES.” There is no pool RES endpoint.

---

## 2. Wallet creation (ORU receive address)

1. Install / run the Alpha wallet or CLI (`rigor-wallet` / `rigor-cli`) against the same network as the pool node.
2. Create or unlock a wallet.
3. Generate a **native SegWit ORU receive address** (Witness v0 P2WPKH / P2WSH).  
   Example CLI pattern:
   ```bash
   rigor-cli -rpcuser=… -rpcpassword=… getnewaddress "" "bech32"
   ```
4. Keep the corresponding private keys / seed offline backups. Pool operators never need your seed.

Alpha operators typically register your ORU address off-band for settlement (ledger credits are keyed by **worker name**; treasury send uses the registered ORU address).

---

## 3. ORU address setup for the pool

Tell the pool operator:

- Your **worker name** (see below)
- Your **ORU payout address** (from §2)

The pool’s coinbase goes to the **operator** payout script; miner ORU is credited in the pool ledger and settled per `[payout]` policy (`ORU_fee_percent`, `payout_interval`).

---

## 4. Miner software

Use the Alpha Stratum agent (not legacy yiimp / not solo `rigor-miner` GBT unless you intend solo):

```text
rigor-miner-agent
```

Packaged under `dist/alpha/pool/` (Linux; Windows when cross-built).

Build from source:

```bash
cd pool/rigor-miner-agent
cargo build --release
```

---

## 5. Miner configuration

### Required flags

| Flag | Meaning | Example |
|------|---------|---------|
| `--pool` | Stratum `host:port` | `pool.example:3333` |
| `--worker` | `mining.authorize` worker name | `alice.rig1` |
| `--password` | Worker password (operator-issued) | `(secret)` |
| `--threads` | CPU mining threads | `4` |

### Example

```bash
./rigor-miner-agent \
  --pool 127.0.0.1:3333 \
  --worker alice.rig1 \
  --password 'your-worker-password' \
  --threads 4
```

Optional:

```bash
./rigor-miner-agent … --stats          # handshake + metrics JSON, then exit
./rigor-miner-agent … --log-interval-secs 15
```

---

## 6. Stratum URL

Stratum v1 TCP (no TLS in Alpha packaging):

```text
stratum+tcp://<host>:<port>
```

Equivalent agent form (host:port only):

```text
--pool <host>:<port>
```

Default Alpha listen (see `pool/config/example.toml`):

```text
0.0.0.0:3333
```

Algo / user-agent expectation: **rgrpow01** (SHA3-256 `RGRPOW01`).
Historical OpuHash agents are rejected. Stock altcoin miners that do not
speak this path will not work.

---

## 7. Worker name

Format recommended:

```text
<account>.<rig>
```

Examples: `alice.rig1`, `lab.miner02`

Rules:

- Must match a credential entry configured by the operator (`worker_credentials` / `RIGOR_POOL_WORKER_CREDENTIALS`).
- Share credit identity is the **authorized session worker**, not a free-form submit field.

---

## 8. Password

- Production pools use **per-worker hashed credentials** (not the shared `"x"`).
- Devtest / local Alpha may use password `x` when `RIGOR_POOL_PROFILE=devtest`.
- Never reuse your wallet seed as the Stratum password.

---

## 9. Verify you are connected

On the miner:

- Log line `handshake OK`
- Periodic `shares_accepted` increments

On the pool operator host:

```bash
rigor-pool --config /path/to/pool.toml status
```

Expect:

- `miners connected` ≥ 1  
- `shares accepted` increasing  
- `RES involvement: NONE` / `res_mint_events: 0`

---

## 10. What not to use

| Path | Status |
|------|--------|
| `pool/stratum`, `pool/web`, `pool/sql` (yiimp) | **Abandoned** for RIGOR — do not connect miners here |
| `pool/mint-engine` | **Legacy** — not in the Alpha pool runtime |
| Solo `rigor-miner` / GBT | Solo mining only; different product surface |

---

## 11. Troubleshooting

| Symptom | Check |
|---------|--------|
| Auth fails | Worker name + password; operator credential map |
| No jobs | Pool ↔ `rigord` RPC (`[node]` in config); IBD finished |
| Shares rejected | Wrong algo / stale job / difficulty |
| Expecting RES from pool | Misconfigured expectation — use wallet Convergence instead |

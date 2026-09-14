# Production mining flow

**PoW (node SSOT):** `SHA3-256("RGRPOW01" || header_80)`  
**Mainnet RPC:** `8761` (never Bitcoin `8332`)  
**Network label:** `mainnet` (HRP remains `rg`)

```
Miner
  ↓
Pool
  ↓
GBT
  ↓
RGRPOW01
  ↓
rigord
```

## SOLO

```
minerd  --  getblocktemplate  --  SHA3-256(RGRPOW01 || header_80)  --  submitblock  --  rigord
```

The node builds the RFC-020 ORU subsidy (0 at height 0; **128 ORU** for
every `h ≥ 1`; no halving) and the canonical coinbase. RES is not a
coinbase prize. The miner copies `default_coinbase_tx` and grinds nonce
(and GBT `curtime` as supplied).

## Pool

```
rigor-miner-agent  --  Stratum v1 (agent rgrpow01)
        ↓
rigor-pool  (rigor-gbt + block-assembler + share-validator)
        ↓
getblocktemplate / submitblock  --  rigord :8761
```

- Share and block candidates are scored with **SHA3-256 RGRPOW01**, not OpuHash.
- Merkle / txid remain SHA256d.
- Pool pays **ORU only**. PoW does not create RES.
- Historical `opushash` Stratum agents are rejected.

Config: [`../../pool/config/example.toml`](../../pool/config/example.toml)
(`network = "mainnet"`, `rpc_port = 8761`).

Miner connection: [`MINER_CONNECTION_GUIDE.md`](./MINER_CONNECTION_GUIDE.md).

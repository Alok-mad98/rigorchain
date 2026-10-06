# RIGOR SOLO miner PoW

Official SOLO miner (`minerd`) searches:

```
SHA3-256("RGRPOW01" || header_80)
```

This is the same digest as `rigord` `CheckProofOfWork` /
`rigor_kernel::GetRigorPowHash`.

## Production mining flow

```
minerd
  ↓
GBT (getblocktemplate — node-authoritative coinbase)
  ↓
RGRPOW01  SHA3-256("RGRPOW01" || header_80)
  ↓
rigord  submitblock → CheckProofOfWork → ConnectBlock
```

SOLO: `minerd` → GBT → RGRPOW01 → `submitblock` → `rigord`.
Coinbase subsidy is RFC-020: 0 at genesis, **128 ORU** thereafter, no
halving. The miner copies `coinbasevalue`; it does not recompute it.
Stratum is not a SOLO URL. Default node RPC is `8761`.

Operator guide: [`../../miner/README_MINER.md`](../../miner/README_MINER.md).

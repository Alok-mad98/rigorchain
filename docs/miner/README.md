# RIGOR SOLO miner PoW

Official SOLO miner (`minerd`) searches:

```
SHA3-256("RGRPOW01" || header_80)
```

This is the same digest as `rigord` `CheckProofOfWork` /
`rigor_kernel::GetRigorPowHash`.

## Production mining flow

```
Miner
  ↓
Pool (optional Stratum coordination; ORU-only)
  ↓
GBT (getblocktemplate — node-authoritative coinbase)
  ↓
RGRPOW01  SHA3-256("RGRPOW01" || header_80)
  ↓
rigord  submitblock → CheckProofOfWork → ConnectBlock
```

SOLO (no pool): `minerd` → GBT → RGRPOW01 → `submitblock` → `rigord`.
Coinbase subsidy is RFC-020: 0 at genesis, **128 ORU** thereafter, no
halving. The miner copies `coinbasevalue`; it does not recompute it.

Pool: `rigor-miner-agent` → Stratum (`rgrpow01`) → pool share-validator
(RGRPOW01) → `submitblock` → `rigord`.

Operator guide: [`../../miner/README_MINER.md`](../../miner/README_MINER.md).  
Pool flow: [`../pool/PRODUCTION_MINING_FLOW.md`](../pool/PRODUCTION_MINING_FLOW.md).  
P0-098: [`../security/RIGOR_SOLO_MINER_POW_ALIGNMENT_P0_098.md`](../security/RIGOR_SOLO_MINER_POW_ALIGNMENT_P0_098.md).  
P0-099: [`../security/RIGOR_PRODUCTION_MINING_PATH_AUDIT_P0_099.md`](../security/RIGOR_PRODUCTION_MINING_PATH_AUDIT_P0_099.md).

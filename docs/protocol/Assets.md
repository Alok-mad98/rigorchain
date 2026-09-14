# Assets

**Status:** Active index. Not a new economic model.  
**Issuance SSOT:** [`RFC-020.md`](./RFC-020.md)  
**Circulation sink:** [`RFC-018.md`](./RFC-018.md)

```
equivalence_claimed = false
documentation_consensus_authority = false
```

RIGOR is a dual-asset UTXO protocol. Asset class is inferred from
`scriptPubKey`. This page does not invent parameters. Read RFC-020 and
the code.

## Native assets

| Asset | Role | Where specified |
|-------|------|-----------------|
| **ORU** | Fees, miner subsidy, Convergence destroy | RFC-020 §1–2; `GetBlockSubsidy` |
| **RES** | Reserve. Minted only by a body-tx that destroys ORU | RFC-020; `CheckResMintArithmetic` |

PoW does not create RES.

## Current documents

| Document | Role |
|----------|------|
| [`RFC-020.md`](./RFC-020.md) | Destroy-cost function, flat ORU emission, RES cap |
| [`RFC-018.md`](./RFC-018.md) | Half of the ORU fee on a RES-transfer is unclaimable |
| [`../specification/SPEC-002-ECONOMIC-MODEL.md`](../specification/SPEC-002-ECONOMIC-MODEL.md) | Economic model aligned to shipping code |
| [`../whitepaper/chapters/02-two-assets.md`](../whitepaper/chapters/02-two-assets.md) | Public narrative of the two assets |
| [`../SCRIPT_STRUCTURE.md`](../SCRIPT_STRUCTURE.md) | Script templates / address surface |

Numeric SSOT for RFC-020: `tools/res_continuous_price_schedule.py` and
`core/src/consensus/res_issuance.h`.

RFC-019 epoch / ORU-halving numbers are historical. See
[`../../archive/rfc/`](../../archive/rfc/).

# Mining

**Document status:** Production documentation

Production mining is SHA3-256 over a domain-tagged 80-byte header:

```
digest = SHA3-256("RGRPOW01" || header_80)
```

The node is the only consensus assembler. A SOLO miner copies canonical
coinbase bytes from `getblocktemplate`, searches nonce (and allowed `nTime`),
then calls `submitblock`. Stratum is not a SOLO URL. Proof-of-Work does not
create RES. RFC-020 coinbase subsidy is **0** at genesis and **128 ORU**
for every height ≥ 1. ORU does not halve.

| Document | Role |
|----------|------|
| [SOLO_MINING.md](../mining/SOLO_MINING.md) | SOLO local-node connectivity |
| [RIGOR_GBT_ADAPTER_SPEC.md](../mining/RIGOR_GBT_ADAPTER_SPEC.md) | `getblocktemplate` adapter |
| [TEMPLATE_CACHE_AND_ASSEMBLER_SPEC.md](../mining/TEMPLATE_CACHE_AND_ASSEMBLER_SPEC.md) | Template cache and assembler |
| [CONVERGENCE_ROOT_CANONICAL_SPEC.md](../mining/CONVERGENCE_ROOT_CANONICAL_SPEC.md) | Convergence root in the template |
| [RIGOR_POOL_STACK_ARCHITECTURE.md](../mining/RIGOR_POOL_STACK_ARCHITECTURE.md) | Pool stack (non-consensus) |
| [RIGOR_WINDOWS_MINER_DASHBOARD.md](../mining/RIGOR_WINDOWS_MINER_DASHBOARD.md) | Local Windows miner visualization (`:18083`) |

Miner binary: [`../../miner/README_MINER.md`](../../miner/README_MINER.md).

Historical P0 mining notes are in `archive/` and are not operator SSOT.

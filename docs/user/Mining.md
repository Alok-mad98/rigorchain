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
| [MINING_SCOPE.md](../mining/MINING_SCOPE.md) | What the project provides; no official pool |
| [CONVERGENCE_ROOT_CANONICAL_SPEC.md](../mining/CONVERGENCE_ROOT_CANONICAL_SPEC.md) | Convergence root in the template |
| [RIGOR_WINDOWS_MINER_DASHBOARD.md](../mining/RIGOR_WINDOWS_MINER_DASHBOARD.md) | Local Windows miner visualization (`:18083`) |

Miner binary: [`../../miner/README_MINER.md`](../../miner/README_MINER.md).

Mine only after the node has at least one peer and `getblockchaininfo`
shows chain `main` with the published genesis. A single peer is not proof
of the public chain: compare block 0 and the tip with that peer.
`getblocktemplate` returns "not connected" when the peer list is empty.
Do not set `solomining=1` to bypass that. The flag skips template guards.
It does not publish a private chain. See
[`MINER-MAINNET-README.txt`](./MINER-MAINNET-README.txt).

Historical P0 mining notes are in `archive/` and are not operator SSOT.

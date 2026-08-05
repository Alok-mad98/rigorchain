# Rigor documentation (root entry)

## Terminology (Middle-earth naming)

| Name | Role in consensus / narrative |
|------|--------------------------------|
| **OpuHash** | Consensus proof-of-work: compute-heavy digest from the block header id (see `core/src/pow_opus.cpp`). Immovable, auditable work that anchors the chain. |
| **ConvergenceDrawCandidates** | The **reconvergenced sword**: the protocol object carrying the convergence **result** — in code, `struct ConvergenceDrawCandidates` with `root` (the **hilt**: candidate Merkle root committed in CVGE) and `hash` (the **blade**: per-tx digest where used off-chain). Metaphor: shards of intent tempered into one edge the network can wield. |
| **Reconvergence** | The function that hashes `(txid, bind, height, target_prev)` into an **ConvergenceDrawCandidatesResult** — “reconvergence the shards” rather than “run a weighted selection”; semantics are deterministic from chain context, not a wager. |

**Dual-native asset protocol (ORU + RES)** — witness versions, asset isolation, RES mint vs transfer:

- **[Protocol design → `core/doc/protocol_design.md`](../core/doc/protocol_design.md)**

Related:

- [convergence / RES consensus (v3)](../core/doc/convergence-consensus-v1.md)
- [Mining pools: GBT, coinbase, RES mint binding](../core/doc/mining-pool-gbt.md)

The full developer doc index lives under [`core/doc/README.md`](../core/doc/README.md).

---

## Mainnet node RPC hardening

Any **internet-adjacent or production-leaning** mainnet node should start from the hardened template:

- **``core/contrib/ops/rigor-rpc-production.conf.example``** — `server=1`, loopback-only `rpcbind` / `rpcallowip`, and `rpcauth` guidance. Copy to your datadir (as `bitcoin.conf` / `rigor.conf`) or pass `-conf=`. Keys under **`[main]`** apply only to mainnet; the file also includes **`[regtest]`** so the same layout can be smoke-tested locally with `-regtest`.

**Generate `rpcauth`** (from the `core/` tree):

```bash
cd /path/to/ring/core
python3 share/rpcauth/rpcauth.py YOUR_RPC_USER
```

Paste the printed `rpcauth=...` line into the config (under the correct `[chain]` section). More detail: [`core/share/rpcauth/README.md`](../core/share/rpcauth/README.md).

Testnet / ops scripts: ``core/contrib/ops/README.md``.

## Release checklist: checkpoints and chainwork

Before tagging a **mainnet** release after the chain has grown:

1. **Checkpoints** — In [`core/src/kernel/chainparams.cpp`](../core/src/kernel/chainparams.cpp) (`CMainParams::checkpointData`), add `{height, uint256S("0x...")}` rows using `rigor-cli getblockhash <height>` on a synced honest node. Keep heights monotonic; wrong hashes fork new installs off the network.
2. **`nMinimumChainWork` / `defaultAssumeValid`** — Bump from `getblockchaininfo` / `getblockhash` per the comments next to those fields in the same file (must not set `nMinimumChainWork` above the real tip’s chainwork).

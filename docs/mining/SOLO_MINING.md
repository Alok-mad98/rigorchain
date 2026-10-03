# RIGOR SOLO Mining — Local Node Architecture

**Document status:** Operational documentation

This page describes how a SOLO miner connects to its own node. It does not define, modify, or reinterpret any consensus rule. RPC reachability is an operational choice. It is not part of RIGOR consensus.

## Miner-owned node

RIGOR SOLO mining is designed around a miner-owned node. The miner communicates with its own node through a local RPC interface, while the node participates in the RIGOR network through P2P. Therefore, SOLO mining does not require a publicly reachable mining RPC endpoint.

A SOLO miner runs against the miner's own RIGOR full node. The miner obtains block templates through that node's local RPC interface.

## Recommended configuration

```text
RIGOR Miner
    |
    | RPC: 127.0.0.1:8761
    v
RIGOR Node
    |
    | P2P
    v
RIGOR Network
```

`127.0.0.1:8761` is a local RPC endpoint. It does not need to be publicly reachable.

The node's P2P connectivity is what connects the miner's RIGOR node to the RIGOR network.

## Local RPC

The RPC interface is a local coordination interface between the miner and its node. It is not a centralized mining gateway or a required public service operated by RIGOR.

RIGOR does not require an official centralized mining RPC gateway.

A public RPC endpoint may exist for specialized infrastructure. It is not a requirement for SOLO mining.

In the normal SOLO configuration, the RIGOR Miner should not depend on a remote project-operated RPC endpoint.

## Example

Mainnet SOLO mining against the local node:

```bash
rigor-miner -a rgrpow01 \
  -o http://127.0.0.1:8761 \
  -u <rpcuser> \
  -p <rpcpassword> \
  --coinbase-addr=<your-mainnet-address> \
  -t <threads>
```

`<rpcuser>`, `<rpcpassword>`, and `<your-mainnet-address>` are placeholders. Algorithm identifier: `rgrpow01`.

## Related

| Document | Role |
|----------|------|
| [Mining scope](./MINING_SCOPE.md) | What the project provides, and what stays independent |
| [Mining](../user/Mining.md) | SOLO mining overview |
| [rigor miner](../../miner/README_MINER.md) | Reference miner |

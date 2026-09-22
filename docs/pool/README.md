# Pool

**Document status:** Production documentation

The pool is ORU-only mining coordination. It consumes `getblocktemplate` from
`rigord` on RPC **8761**. It pays **ORU only**. It does not mint RES. Block
acceptance remains the node.

| Document | Role |
|----------|------|
| [RIGOR_MINING_POOL_INTEGRATION_GUIDE_v1.0.md](./RIGOR_MINING_POOL_INTEGRATION_GUIDE_v1.0.md) | Third-party pool integration spec (`rgrpow01`) |
| [../RGRPOW01_COMPATIBILITY.md](../RGRPOW01_COMPATIBILITY.md) | How to verify a rgrpow01 implementation |
| [PRODUCTION_MINING_FLOW.md](./PRODUCTION_MINING_FLOW.md) | Production mining flow |
| [MINER_CONNECTION_GUIDE.md](./MINER_CONNECTION_GUIDE.md) | Miner connection |
| [RIGOR_POOL_PRODUCT_POSITION.md](./RIGOR_POOL_PRODUCT_POSITION.md) | Product position |
| [POOL_RES_BOUNDARY_ARCHITECTURE_DECISION.md](./POOL_RES_BOUNDARY_ARCHITECTURE_DECISION.md) | RES stays off the pool critical path |
| [POOL_RES_BOUNDARY_SECURITY_AUDIT.md](./POOL_RES_BOUNDARY_SECURITY_AUDIT.md) | RES boundary security audit |
| [POOL_MINER_NODE_FLOW_AUDIT.md](./POOL_MINER_NODE_FLOW_AUDIT.md) | Miner / node / pool flow |

Implementation: [`../../pool/README.md`](../../pool/README.md).  
Yaamp/yiimp paths under `pool/` are quarantined leftovers, not the production
product.

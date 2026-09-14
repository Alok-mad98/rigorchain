# Explorer

Independent observation product. Not consensus. Not a ledger of record.

**Mainnet genesis:** `f8aceb50d974f3157c7bc2dfffab28dee0c97d99e4e3ec931b84e06862fdd717`  
**HRP:** `rg` · **RPC:** `8761` · **Network:** `mainnet`

Asset labels must follow `AssetTypeFromScriptPubKey` (witness v0 → ORU;
`0xbd` converge → ORU destroy; native witness v1 → RES).

| Document | Role |
|----------|------|
| [EXPLORER_ARCHITECTURE.md](./EXPLORER_ARCHITECTURE.md) | Explorer implementation |
| [RES_INDEXING_RULES.md](./RES_INDEXING_RULES.md) | RES indexing |
| [WITNESS_V1_DISPLAY_RULES.md](./WITNESS_V1_DISPLAY_RULES.md) | Witness v1 display |
| [EXPLORER_UI_MINIMUM_SPEC.md](./EXPLORER_UI_MINIMUM_SPEC.md) | UI minimum |
| [EXPLORER_RPC_COMPATIBILITY_MATRIX.md](./EXPLORER_RPC_COMPATIBILITY_MATRIX.md) | RPC compatibility |
| [EXPLORER_SECURITY_BOUNDARIES.md](./EXPLORER_SECURITY_BOUNDARIES.md) | Security boundaries |
| [EXPLORER_PRODUCTION_GATE.md](./EXPLORER_PRODUCTION_GATE.md) | Production gate (startup, accounting, pagination, deploy, testnet) |

Protocol identity: [`../whitepaper/RIGOR_PROTOCOL_IDENTITY.md`](../whitepaper/RIGOR_PROTOCOL_IDENTITY.md).
Code facts: [`../developer/CODE_SSOT_PROTOCOL_FACTS.md`](../developer/CODE_SSOT_PROTOCOL_FACTS.md).

```
Consensus defines reality.
Explorer displays consensus-derived data.
database_reset ≠ monetary_reset
```

# RPC

**Status:** Active index.

```
documentation_consensus_authority = false
```

JSON-RPC is an operator interface. It is not consensus. Do not expose
it to the public Internet.

## Transport

| Document | Role |
|----------|------|
| [`../../core/doc/JSON-RPC-interface.md`](../../core/doc/JSON-RPC-interface.md) | Transport / auth notes (inherited Core doc) |
| [`../wallet/README.md`](../wallet/README.md) | Wallet RPC entry |
| [`../user/Wallet.md`](../user/Wallet.md) | User workflows that call RPC |

Default ports: mainnet P2P **8760**, RPC **8761**.

Wallet construction RPCs live in `core/`. Consensus still decides
Convergence and RES mint. The wallet does not mine.

Code facts: [`CODE_SSOT_PROTOCOL_FACTS.md`](./CODE_SSOT_PROTOCOL_FACTS.md).

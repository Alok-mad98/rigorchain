# Wallet documentation

Assets: **ORU** / **RES**.

The wallet is a descriptor SQLite wallet in `core/`. It constructs
transactions. Consensus decides Convergence outcomes and RES mints.
The wallet does not mine.

| Document | Role |
|----------|------|
| [../user/Wallet.md](../user/Wallet.md) | Create, backup, send/receive ORU & RES, Convergence |
| [WALLET_CONVERGENCE_HEIGHT_PAD.md](./WALLET_CONVERGENCE_HEIGHT_PAD.md) | 144-block confirmation margin for Convergence construction |
| [RIGOR_COLD_STORAGE_GUIDE.md](./RIGOR_COLD_STORAGE_GUIDE.md) | Cold storage |

Code facts: [`../developer/CODE_SSOT_PROTOCOL_FACTS.md`](../developer/CODE_SSOT_PROTOCOL_FACTS.md).

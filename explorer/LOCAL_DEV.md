# Explorer indexer — local development

The Explorer lives in this repository at `explorer/`. It is a read-only indexer. It is not consensus.

## Two networks

| | Mainnet production | Testnet development |
| --- | --- | --- |
| Config | `deploy/env.production.example` | `.env.testnet.example` |
| `RIGOR_NETWORK` | `mainnet` | `testnet` |
| RPC port | `8761` | `18761` |
| Address HRP | `rg` | `trg` |
| Database | its own PostgreSQL database | a different database |
| Public site | `http://explorer.rigorchain.com` (HTTPS after TLS) | not the public site |

Production must not load the testnet env file. If `RIGOR_NETWORK` and the node chain disagree, the process exits. It does not fall back to another network.

`.env.example` is a mainnet-shaped reference for local layout. Do not point it at the public production datadir or database.

## Prerequisites

1. PostgreSQL, with a database used by only one chain
2. `rigord` and `rigor-cli` built from `core/`
3. A node whose `getblockchaininfo.chain` matches `RIGOR_NETWORK`

Mainnet RPC stays on `127.0.0.1:8761`. Do not publish RPC.

## Quick start

```bash
cd explorer
cp .env.example .env.local
# edit paths. For testnet development, start from .env.testnet.example instead.

set -a && source .env.local && set +a
npm run dev
```

When the log says the indexer is listening, open `http://127.0.0.1:8080/`.

```bash
curl -s http://127.0.0.1:8080/healthz
curl -s http://127.0.0.1:8080/api/status
```

## Production unit

The canonical systemd unit name is `rigor-explorer.service`.
`deploy/systemd/explorer-indexer-mvp.service` is not that unit and is not installed on the public host.

The production environment file is `/etc/rigor/explorer.env`, shaped like `deploy/env.production.example`: mainnet, port 8761, and a mainnet-only database.

Qt wallets do not hardcode the explorer URL. The production example for `RigorExplorer/ApiBaseUrl` is `http://explorer.rigorchain.com` until TLS is issued.

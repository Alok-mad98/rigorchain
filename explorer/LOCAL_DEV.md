# Explorer indexer — local dev bring-up

## Prerequisites

1. **PostgreSQL** on `127.0.0.1:5432` with database `rigor_explorer`
2. **Built binaries:** `make -C ./core/src rigord rigor-cli`
3. **Running testnet node** (optional for import; required for clean compliance state):

```bash
./core/src/rigord -testnet -datadir=/path/to/testnet/datadir -daemon
```

Default config uses `/var/lib/rigor/testnet` — that path often does not exist on laptops.

**This machine (closed testnet NODE-C):**

```text
RIGOR_DATADIR=./.runtime/closed-testnet/node-c
```

Do **not** use `~/.rigor` (parent dir) or `~/.rigor/golden-vector-archive-regtest` (regtest 2.1M archive — separate chain).

## Quick start

```bash
cd ./explorer-indexer-mvp
cp .env.example .env.local
# edit .env.local if needed

set -a && source .env.local && set +a
pg_isready -h 127.0.0.1 -p 5432
npm run dev
```

Keep this terminal **open**. When you see:

```text
Explorer indexer MVP listening on http://127.0.0.1:8080
```

open in browser: http://127.0.0.1:8080/index.html

Verify:

```bash
curl -s http://127.0.0.1:8080/healthz | jq .
curl -s http://127.0.0.1:8080/compliance | jq .
```

## Common errors

| Error | Fix |
|-------|-----|
| `ECONNREFUSED 5432` | Start PostgreSQL |
| `spawn rigor-cli ENOENT` | Set `RIGOR_CLI_BIN=./core/src/rigor-cli` |
| `ERR_CONNECTION_REFUSED :8080` | `npm run dev` not running, or terminal was closed |
| Importer RPC timeout | Start `rigord` or fix `RIGOR_DATADIR` |
| Red mismatch banner | Expected when RPC/node unavailable; UI still testable |

## WSL + Windows browser

If the server runs in WSL and the browser is on Windows, `http://127.0.0.1:8080` should forward on recent WSL2. If it fails, try the WSL IP from `hostname -I` or run the browser inside WSL.

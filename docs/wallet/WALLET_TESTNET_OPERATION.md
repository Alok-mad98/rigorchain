# WALLET_TESTNET_OPERATION

## Closed Testnet Integration

Wallet supports NODE-C explorer API and localhost rigord RPC with txindex assumptions.

## Operational Requirements

- auto refresh every 30s
- stale tip warning
- root mismatch warning
- explorer sync lag warning
- reorg-safe refresh by polling authoritative endpoints repeatedly

## Local Development Commands

From `wallet-observer-mvp/`:

```bash
npm install
npm run dev
```

Build test:

```bash
npm run build
npm run start
```

## Connection Guide

1. Ensure explorer-indexer-mvp is running and healthy (`/healthz`).
2. Ensure rigord RPC credentials in `.env.local` are valid.
3. Ensure `txindex=1` for transaction lookup reliability.
4. Open wallet UI and verify dashboard warning state is clear.

## Metadata

- Risk Classification: MEDIUM
- Wallet Classification: TESTNET-OPS
- Consensus Impact: NONE
- Replay Impact: INDIRECT
- Determinism Impact: INDIRECT
- UX Risk: MEDIUM
- Minimal Safe Recommendation: Operate only against local trusted infrastructure and keep warning banners as operator-visible blockers.

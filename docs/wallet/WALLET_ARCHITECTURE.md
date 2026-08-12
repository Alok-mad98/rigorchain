# WALLET_ARCHITECTURE

## Objective

Build a deterministic observer wallet foundation for RIGOR testnet operations.

## Architecture

Wallet UI -> Next.js local proxy API -> explorer-indexer-mvp API -> rigord RPC.

The UI must not run local consensus interpretation logic. Convergence eligibility and winner data are authority-derived from `getconvergenceresult` or explorer convergence tables.

## Modes

- Watch Mode:
  - ORU address import
  - RES address import
  - txid import
  - convergence txid import
- Local Node Mode:
  - localhost explorer API
  - localhost rigord RPC

## Prohibited in this phase

- signing wallet features
- mnemonic or seed storage
- token abstraction layer
- alternate protocol semantics

## Metadata

- Risk Classification: MEDIUM
- Wallet Classification: OBSERVER-FOUNDATION
- Consensus Impact: NONE
- Replay Impact: INDIRECT
- Determinism Impact: INDIRECT
- UX Risk: MEDIUM
- Minimal Safe Recommendation: Keep all authoritative interpretation in explorer/RPC and preserve txid-primary identity.

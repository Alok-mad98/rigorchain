# WALLET_SECURITY_BOUNDARIES

## Security Boundary

This wallet is an observer-only interface. It does not hold keys, does not construct signed transactions, and does not mutate chain state.

## Allowed

- Read-only RPC calls
- Read-only explorer API queries
- Local watch target persistence (browser localStorage)

## Forbidden

- mnemonic generation
- signing in browser
- PSBT and fee engine
- hardware wallet connection
- remote hosted wallet service

## Identity Rules

- txid primary identity for routes and references
- wtxid secondary informational field only

## Consensus Safety Boundary

No consensus logic is implemented in UI. Eligibility and draw status are authority-derived from explorer/RPC outputs.

## Metadata

- Risk Classification: HIGH
- Wallet Classification: SECURITY-BOUNDARY
- Consensus Impact: NONE
- Replay Impact: INDIRECT
- Determinism Impact: INDIRECT
- UX Risk: LOW
- Minimal Safe Recommendation: Enforce read-only boundaries and reject any feature that introduces signing or hidden local state transitions.

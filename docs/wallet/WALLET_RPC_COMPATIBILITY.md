# WALLET_RPC_COMPATIBILITY

## Required Sources

- Explorer API:
  - `/healthz`
  - `/chain/tip`
  - `/address/:addr`
  - `/tx/:txid`
  - `/res/mints`
  - `/res/events`
- rigord RPC:
  - `getblockchaininfo`
  - `getnetworkinfo`
  - `getconvergenceresult`

## Compatibility Rules

- Wallet does not consume unstable RPC surfaces.
- Eligibility authority is from `getconvergenceresult` or explorer convergence tables only.
- Any RPC failure must be exposed as UI warning/error, not silently ignored.

## txid/wtxid Rule

- txid primary identity for lookup and routes
- wtxid secondary display only

## Metadata

- Risk Classification: HIGH
- Wallet Classification: RPC-COMPATIBILITY
- Consensus Impact: NONE
- Replay Impact: INDIRECT
- Determinism Impact: INDIRECT
- UX Risk: MEDIUM
- Minimal Safe Recommendation: Pin observer behavior to stable RPC and explorer fields; reject local recomputation.

# WALLET_DATA_FLOW

## Data Flow

1. UI requests observer routes every 30 seconds.
2. Next.js proxy routes query explorer endpoints (`/chain/tip`, `/healthz`, `/address/:addr`, `/tx/:txid`, `/res/mints`, `/res/events`).
3. Proxy routes query rigord RPC for authority checks (`getblockchaininfo`, `getnetworkinfo`, `getconvergenceresult`).
4. UI renders read-only tables and warnings.

## Deterministic Constraints

- txid is the canonical identifier.
- wtxid is display-only metadata.
- No local recomputation of convergence eligibility.
- Display `RES_PROBABILITY_TARGET` only.

## Refresh and Warning Flow

- Auto refresh: 30s.
- Stale tip warning: delayed `chain_state.updated_at`.
- Root mismatch warning: `critical_root_mismatch = true`.
- Explorer sync lag warning: node height - indexer height > threshold.

## Metadata

- Risk Classification: MEDIUM
- Wallet Classification: DATA-FLOW-SPEC
- Consensus Impact: NONE
- Replay Impact: INDIRECT
- Determinism Impact: INDIRECT
- UX Risk: MEDIUM
- Minimal Safe Recommendation: Use proxy-only data access and explicit warning paths for stale/reorg-sensitive conditions.

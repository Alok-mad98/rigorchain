# WALLET_CONVERGENCE_VISIBILITY

## Visibility Scope

- latest convergence events
- draw eligibility status
- winner txid context
- `RES_PROBABILITY_TARGET`
- convergence metadata on transaction details

## Authority Rule

The wallet must not derive convergence eligibility locally. It must consume:

- `getconvergenceresult`
- explorer convergence tables

## Burn Tracking

Burn visibility is read-only and RPC-derived. Burn lineage references convergence txid and related eligibility context.

## Forbidden Display Terms

- `rigor_target`
- `res_draw_target`
- `T_conv`
- bits-derived convergence target

## Metadata

- Risk Classification: HIGH
- Wallet Classification: CONVERGENCE-VISIBILITY
- Consensus Impact: NONE
- Replay Impact: INDIRECT
- Determinism Impact: INDIRECT
- UX Risk: HIGH
- Minimal Safe Recommendation: Treat convergence and burn fields as authority-read outputs, never as locally computed facts.

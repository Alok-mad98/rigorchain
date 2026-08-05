# WALLET_UI_MINIMUM_SPEC

## Required Pages

1. Dashboard
   - chain
   - current height
   - current tip hash
   - peer count
   - explorer sync status
2. ORU Assets
   - ORU balance
   - confirmed/unconfirmed
   - burn outputs
   - tx history
3. RES Assets
   - RES balance
   - mint history
   - convergence-linked mint display
   - bind height
   - convergence root
   - convergence txid
4. Convergence Activity
   - latest convergence events
   - eligible set size
   - winner txid
   - draw probability target
   - mint status
5. Transaction Viewer
   - txid lookup
   - tx details
   - inputs/outputs
   - witness summary
   - txid + wtxid display
   - convergence metadata

## Display Rules

- `RES_PROBABILITY_TARGET` must be displayed.
- `rigor_target`, `res_draw_target`, `T_conv`, and bits-derived convergence target must not be displayed.
- ORU and RES must be visually separated as dual native protocol assets.

## Metadata

- Risk Classification: MEDIUM
- Wallet Classification: UI-MIN-SPEC
- Consensus Impact: NONE
- Replay Impact: INDIRECT
- Determinism Impact: NONE
- UX Risk: MEDIUM
- Minimal Safe Recommendation: Keep labels explicit and avoid mixed-asset or token-style rendering semantics.

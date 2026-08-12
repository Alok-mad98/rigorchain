# WALLET_RES_DISPLAY_RULES

## Principle

RES and ORU are dual native protocol assets and must never be rendered as interchangeable token balances.

## Mandatory RES Fields

- mint txid
- bind txid
- bind height
- convergence root
- convergence txid
- RES issuance amount

## Display Separation

- ORU card/table is separate from RES card/table.
- Combined totals are forbidden.
- Token-style badge systems are forbidden.

## Convergence Link

RES mint rows must preserve convergence linkage from explorer-indexed fields and RPC authority.

## Metadata

- Risk Classification: MEDIUM
- Wallet Classification: RES-DISPLAY-RULES
- Consensus Impact: NONE
- Replay Impact: INDIRECT
- Determinism Impact: NONE
- UX Risk: MEDIUM
- Minimal Safe Recommendation: Preserve explicit dual-asset semantics and show full mint linkage context.

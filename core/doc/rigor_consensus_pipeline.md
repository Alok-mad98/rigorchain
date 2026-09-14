# Rigor Consensus Pipeline Checklist

This document is a pre-merge checklist for RIGOR consensus changes.

Monetary truth is the **UTXO set**. Scripts classify outputs; they are not assets
and they do not mint. Consensus state is derived from block + transaction + UTXO
+ deterministic rules. There is no global mint accumulator.

## Canonical block validation (ConnectBlock)

1. Header / PoW / merkle checks.
2. Coinbase `scriptSig` is the BIP34 height push only
   (`core/src/consensus/rigor_coinbase_layout.h`). The coinbase does not mint RES.
3. Body transactions may burn ORU (`0xbd`) and mint Witness v1 RES in the same
   transaction when `CheckResMintArithmetic(R, B, h)` holds.
4. Coinbase ORU is subsidy + claimable fees. Coinbase RES is fees / mockable
   bootstrap only — never new issuance.

## RES issuance rules

- New RES exists only as Witness v1 outputs on a body transaction that also
  burns ORU.
- Gate: `mint_authorized(h) ∧ (R = 0 ∨ R · (2²⁵+h)² ≤ B << 40)`.
- The CONVERGE marker (`0xbd`) is the burn. It is not a lottery ticket.

## Convergence burn rules

- Shape: `<20-byte HASH160> <0xbd>` (22 bytes).
- `AddCoins` skips the output: the ORU is destroyed, not locked in the UTXO set.
- CONVERGE outputs are unspendable.
- `MIN_CONVERGENCE_AMOUNT` is 1 atom. The one-atom mint floor is
  `ResMinMintBurnAtoms(h)` ORU, not a second burn minimum.

## Strictly forbidden changes

- Modifying the 80-byte block header layout
- Adding an asset field to `CTxOut`
- Writing CONVERGE-marker outputs into the UTXO set
- Reintroducing coinbase RES mint, draw window, σ, or `GetResMintReward`
- Reintroducing CVGE cumulative / PCORB / covenant mint paths
- Introducing non-deterministic consensus dependencies

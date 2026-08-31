# Rigor Consensus Pipeline Checklist

This document is a pre-merge checklist for RIGOR consensus changes.

Monetary truth is the **UTXO set**. Scripts classify outputs; they are not assets
and they do not mint. Consensus state is derived from block + transaction + UTXO
+ deterministic rules. There is no global mint accumulator.

## Canonical block validation (ConnectBlock)

1. Header / PoW / merkle checks.
2. Reconstruct the **parent** draw set F over the last W=32 blocks. Exclude tickets
   already paid inside that window (`m_paid_winner_txid` on each index).
3. Resolve the parent draw: σ = Hash256(txid ‖ C ‖ B_{h-1} ‖ LE32(h)), eligible
   iff σ < T · f_i, winner = argmin by 512-bit cross-multiplication.
4. This block's coinbase `scriptSig` must be exactly `<BIP34 height> <32-byte C>`
   where C is the window root this block commits (`bad-cb-convergence-root`).
5. If the parent draw named a winner and W(h) > 0, the coinbase must carry
   exactly one `OP_RES_MINT` paying that ticket's HASH160 at GetResMintReward(h).
   Otherwise it must carry none.
6. Body transactions cannot create RES (`mixed-asset-output`).
7. Coinbase total RES equals the mint (plus mockable bootstrap only).

Do not skip or reorder the mint coupling against the parent draw.

## Single source rules

- The draw paid at height h is a function of the **parent** block's committed F.
- Template C is `GetTemplateConvergenceRoot` (windowed). Do not use the
  block-local `ComputeConvergenceRootFromConvergenceSet` in a producer.
- Read-path winners go through `GetConvergenceWinnerForBlock`, not a single-block
  `GetBlockWinnerFromBlockUndo`.

## Commitment rules

- The only consensus commitment of F is the coinbase **scriptSig** C.
- Empty F ⇒ C = 0. Any nonempty F ⇒ Hash256(concat(sort(txids))).
- CVGE / RIGR / PCORB coinbase `OP_RETURN` tags are **not** required, matched, or
  rejected. `bad-convergence-cumulative` and `bad-convergence-draw-candidate-root`
  do not exist.

## RES issuance rules

- New RES exists only as coinbase RES mint marker (`0xbb`).
- At most one mint per block.
- No winner ⇒ no mint. Winner and W(h) > 0 ⇒ exactly one mint.
- The CONVERGE marker (`0xbd`) is a ticket/commitment lock, not an issuance event.

## Convergence ticket rules

- Ticket shape: `<20-byte HASH160> <0xbd CONVERGE marker>` (22 bytes). Retired witness-v1
  CVG2 programs are consensus-invalid.
- Ownership is that HASH160. Scripts do not hold a monetary balance.
- `AddCoins` skips the output: the ORU is destroyed, not locked in the UTXO set.
- CONVERGE outputs are unspendable (not a consensus spend predicate).

## Randomness binding (σ)

```
σ = Hash256( txid(32) ‖ C(32) ‖ B_{h-1}(32) ‖ LE32(h) )   // 100 bytes
```

T is `RES_PROBABILITY_TARGET` (constant). Parent `nBits` is **not** a σ input.

## Strictly forbidden changes

- Modifying the 80-byte block header layout
- Adding an asset field to `CTxOut`
- Writing CONVERGE-marker outputs into the UTXO set
- Allowing body transactions to create RES
- Allowing more than one RES mint marker per coinbase
- Allowing a mint without a winner, or omitting a mint when a winner exists and W(h) > 0
- Reintroducing CVGE cumulative / PCORB / covenant mint paths
- Introducing non-deterministic consensus dependencies

# UTXO `Coin::nFlags` — format and consensus follow-up

This document complements the inline comments on `Coin` in `src/coins.h`.

## On-disk UTXO format

- **Prefix:** `VARINT(height|coinbase)` + compressed `CTxOut`.
- **Flags:** always `VARINT(nFlags)` (`Coin::Serialize` in `coins.h`). Concatenated snapshot streams stay aligned.
- Disk / undo then append CRC-32C (`CoinDiskFormatter`). Checksum covers height, coinbase, value, scriptPubKey, and nFlags.

Existing chainstates without the flags tail must be wiped or `-reindex`ed.

## Consensus bit

| Bit | Name | Rule |
|-----|------|------|
| 0 | `COIN_FLAG_RES_MINT` | Body RES mint Witness v1 output in a Convergence+RES transaction. Unspendable while `nSpendHeight - coin.nHeight < nCoinbaseMaturity` (same predicate as coinbase). Reject: `bad-txns-premature-spend-of-res-mint`. |

Other bits remain reserved / UTXO-only.

## Block undo

Undo encoding is `CoinDiskFormatter` (same as disk Coin). `COIN_FLAG_RES_MINT` is part of the Coin leaf; there is no Bitcoin dummy version byte.

`DisconnectBlock` checks `TxOutputIsBodyResMint(tx, o) == coin.IsResMint()` on unspent outputs so a reorg cannot drop the mint flag.

## Sibling metadata

When legacy undo omits height, `ApplyTxInUndo` copies `nHeight` / `fCoinBase` from another output of the same transaction. **Do not** copy `nFlags` from a sibling (per-output). Current undo always stores height on the Coin itself.

## References

- `src/coins.h` — `COIN_FLAG_RES_MINT`, `Coin::Serialize`, `TxOutputIsBodyResMint`
- `src/undo.h` — `TxInUndoFormatter`
- `src/consensus/tx_verify.cpp` — premature mint spend
- `docs/security/RES-ORU-MATURITY.md`

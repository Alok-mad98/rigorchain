# UTXO `Coin::nFlags` — format and consensus follow-up

This document complements the inline comments on `Coin` in `src/coins.h` and `TxInUndoFormatter` in `src/undo.h`.

## On-disk UTXO format

- **Legacy:** `VARINT(height|coinbase)` + compressed `CTxOut` (no tail).
- **Current:** same prefix, then **always** `VARINT(nFlags)` (`Coin::Serialize` in `coins.h`). Concatenated snapshot streams stay aligned.
- Old LevelDB values without the tail deserialize as **`nFlags == 0`**.

## Consensus bit

| Bit | Name | Rule |
|-----|------|------|
| 0 | `COIN_FLAG_RES_MINT` | Body RES mint Witness v1 output. Unspendable while `nSpendHeight - coin.nHeight < nCoinbaseMaturity` (same predicate as coinbase). Reject: `bad-txns-premature-spend-of-res-mint`. |

Other bits remain reserved / UTXO-only.

## Block undo

Do **not** append a variable-length nFlags tail after `CTxOut` in undo (a prior attempt desynchronized multi-`vprevout` streams).

`COIN_FLAG_RES_MINT` is stored in the historical dummy version byte (`nHeight > 0`). Other nFlags bits are not restored from undo.

`DisconnectBlock` checks `TxOutputIsBodyResMint(tx, o) == coin.IsResMint()` on unspent outputs so a reorg cannot drop the mint flag.

## Sibling metadata

When legacy undo omits height, `ApplyTxInUndo` copies `nHeight` / `fCoinBase` from another output of the same transaction. **Do not** copy `nFlags` from a sibling (per-output).

## References

- `src/coins.h` — `COIN_FLAG_RES_MINT`, `Coin::Serialize`
- `src/undo.h` — `TxInUndoFormatter`
- `src/consensus/tx_verify.cpp` — premature mint spend
- `docs/security/RES-ORU-MATURITY.md`

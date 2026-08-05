# UTXO `Coin::nFlags` — format and consensus follow-up

This document complements the inline comments on `Coin` in `src/coins.h` and `TxInUndoFormatter` in `src/undo.h`.

## On-disk format (backward compatible)

- **Legacy:** `VARINT(height|coinbase)` + compressed `CTxOut` (no tail).
- **Extended:** same as above, then **if and only if** `nFlags != 0`, append **4 bytes** little-endian `uint32_t` (`Serialize`/`Unserialize` in `coins.h`).
- Old LevelDB values without the tail deserialize as **`nFlags == 0`**.

Block undo for spent inputs uses the same optional tail in `TxInUndoFormatter` so disconnect can restore coins including flags.

## What future consensus upgrades must address

1. **Meaning of bits**  
   Any consensus rule that interprets `nFlags` must be specified together with activation (height/version), exactly like any other consensus change.

2. **Relationship to `CTxOut`**  
   Today, spendable state is fully described by `CTxOut` plus height/coinbase. If `nFlags` becomes consensus-relevant, define whether it duplicates information in `vout` or carries **UTXO-only** state, and how it affects `txid` commitments (if at all).

3. **`DisconnectBlock` / `ConnectBlock` consistency**  
   `DisconnectBlock` checks that removed UTXOs match the block’s outputs (and height/coinbase). If `nFlags` is part of consensus, **extend those checks** and the connect path so reorgs cannot leave the UTXO set inconsistent.

4. **`ApplyTxInUndo` and sibling metadata**  
   When legacy undo omits height, the code copies `nHeight` / `fCoinBase` from another output of the same transaction. **Do not** copy `nFlags` from a sibling unless you have a clear rule that all outputs of that tx must share flags; flags may be **per-output**.

5. **Tests**  
   Add or extend unit tests for any new serialization or disconnect/connect behaviour.

## References

- `src/coins.h` — `Coin::Serialize` / `Unserialize`
- `src/undo.h` — `TxInUndoFormatter`
- `src/validation.cpp` — `ApplyTxInUndo`, `DisconnectBlock`

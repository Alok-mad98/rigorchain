# Assumeutxo `hash_serialized_2` commitment fix

**Date:** 2026-09-30
**Category:** B (adapt). Snapshot commitment only.

## Defect

`ApplyHash` for `CoinStatsHashType::HASH_SERIALIZED` used to write:

```text
VARINT(height * 2 + coinbase_flag ? 1 : 0)
```

In C++ the conditional operator binds looser than addition, so the hashed integer was `(height * 2 + coinbase_flag) ? 1 : 0`. Every coin above height 0 committed the constant 1. The coinbase flag was not in the digest.

`LoadAssumeutxo` and the background snapshot check both compare this digest to the chainparams pin. A snapshot that lied about height or the coinbase bit could still match. Maturity reads height from the coin. The commitment did not bind that field.

## Correction

The snapshot serializer now writes `SnapshotCoinCode(height, coinbase)`:

```text
(uint64_t(height) << 1) | (coinbase ? 1 : 0)
```

That is the same code a coin already stores as `(height << 1) | coinbase`. MuHash continues to use `TxOutSer`, which already committed `uint32_t(height * 2 + coinbase)`. MuHash was not rewritten.

## What did not change

- UTXO validity, `ConnectBlock`, transaction validation, and maturity rules are untouched.
- `core/src/validation.cpp` is untouched.
- No mainnet assumeutxo pin is published. `m_assumeutxo_data` on mainnet stays empty.
- `nChainTx` at the regtest pins is unchanged (111 at height 110, 201 at height 200).

## Regtest pins

Regenerated with `TestChain100Setup` on disk via `rigor_assumeutxo_param_gen` (`RIGOR_GEN_ASSUMEUTXO=1`):

| Height | `nChainTx` | `hash_serialized` |
|--------|------------|-------------------|
| 110 | 111 | `70a2b06fdb2cac946a7818131150891d356116c4ea8d4ff48f665b3b542f2e49` |
| 200 | 201 | `887ab6c056b5f1944bb574b5de03ed9e585992f3ce51fc3f571ef6e3d7abd72a` |

Previous pins (`342bd84c…` and `0d687545…`) were computed with the weak serializer. A snapshot produced by the old code will not activate. That is fail-closed. No mainnet snapshot was published under the weak hash.

## Tests

`coinstatsindex_tests/hash_serialized_commits_height_and_coinbase` checks:

- different heights produce different `hash_serialized_2` digests
- a coinbase coin and a non-coinbase coin at the same height produce different digests
- MuHash of the same coin still matches a direct `TxOutSer` insert, and different heights still produce different MuHash digests

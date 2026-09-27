# Serialization vectors 

Real byte corpus for the map:

```
bytes → CBlock/CTransaction → validation rule → decision → KernelInput candidate
```

This directory does **not** change the wire format.

## Layout

| Path | Role |
|------|------|
| `codec.py` | Canonical CompactSize + witness rules matching `serialize.h` / `transaction.h` / `block.h` |
| `generate_corpus.py` | Pulls live genesis (regtest + testnet) and one mined regtest block; copies attack_lab / formal vectors |
| `cases/*.json` | One file per case: `case_id`, `raw_block_hex` or `raw_tx_hex`, `decode_result`, `consensus_result`, `expected` |
| `manifest.json` | Index |
| `test_roundtrip.py` | `encode(decode(H)) == H` for fully consumed legal encodings |

`data/` is a throwaway datadir for the generator. Do not commit it.

## Sources

1. Live `getblock` genesis (regtest, testnet)
2. Live mined regtest height 1
3. Existing lab `testnet/data/node-001/.../blk00000.dat` height 1 (if present)
4. `audit/security/consensus_equivalence_dataset.json`
5. `formal_verification/vectors/rigor_connectblock_vectors.json`
6. Constructed non-canonical CompactSize, leftover suffix, superfluous witness, truncated header

## Run

```bash
python3 tests/serialization_vectors/generate_corpus.py
python3 tests/serialization_vectors/test_roundtrip.py
```

Requires in-tree `core/src/rigord`.

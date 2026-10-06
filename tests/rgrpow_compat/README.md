# RGRPOW01 Compatibility Test Suite v1.0

**Do not change consensus rules.** This directory only tests that implementations
match `rigor_kernel::GetRigorPowHash`.

## Compatibility standard

| Item | Value |
|------|--------|
| Algorithm | `rgrpow01` |
| Human name | OpuHash |
| Hash | `SHA3-256("RGRPOW01" \|\| header_80)` |
| Input | exactly 88 bytes |
| Header | Bitcoin-compatible 80-byte block header |
| Comparison | `Uint256_LE(digest) <= target` |
| Primitive | FIPS PUB 202 SHA3-256 (suffix `0x06`) |

Forbidden: `SHA3-256(header)`, Ethereum Keccak-256 (`0x01`), `SHA256d(header)` as PoW.

## Run

From the repository root:

```
python3 ./test_rgrpow_compat
```

or:

```
python3 tests/rgrpow_compat/run_suite.py
pytest tests/rgrpow_compat
```

Requires `gcc` and `g++` (compiles `miner/rgrpow.c` and `core/src/rigor_kernel/keccak.cpp`).
Rust (`cargo`) is optional; when present the suite also builds
`tests/rgrpow_compat/native/rust_cli` (same `Sha3_256` construction as
`core/src/rigor_kernel` / `miner/rgrpow.c`).

## Files

| Path | Role |
|------|------|
| `vectors/rgrpow_vectors.json` | Golden digests from the live kernel |
| `test_hash.py` | Vectors 001–003 |
| `test_header_serialization.py` | 80-byte LE header |
| `test_endianness.py` | digest[0] is LSB |
| `test_negative_cases.py` | wrong tag / Keccak-256 |
| `test_cross_implementation.py` | C++ / C miner / Python / Rust |
| `test_rpc_mining.py` | GBT field → header80 → rgrpow01 |
| `test_coinbase.py` | immutable scriptSig, OP_RETURN extra |
| `test_stratum.py` | subscribe / notify / submit |
| `test_fuzz.py` | header avalanche |
| `native/` | thin CLIs over consensus/miner sources |

External developer package: [`../../docs/RGRPOW01_COMPATIBILITY.md`](../../docs/RGRPOW01_COMPATIBILITY.md).
Reference vectors: [`../../docs/RGRPOW01_REFERENCE_VECTOR_v1.0.md`](../../docs/RGRPOW01_REFERENCE_VECTOR_v1.0.md).

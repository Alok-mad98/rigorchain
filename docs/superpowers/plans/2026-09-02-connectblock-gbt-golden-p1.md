# ConnectBlock / GBT Golden Vector P1

**Goal:** Pin height-1 GBT coinbase layout and a serialized regtest block through production `ProcessNewBlock` (AcceptBlock → ConnectBlock).

**Constraints:** No consensus formula changes. Bitcoin `tx_valid.json` stays skipped. Python remains harness-only.

## Files

- `formal_verification/vectors/rigor_gbt_layout_vectors.json` — coinbasevalue, mint presence/script, witness commitment index
- `formal_verification/vectors/rigor_connectblock_vectors.json` — valid height-1 hex + merkle-stale invalid hex
- `core/src/test/rigor_connectblock_gbt_vector_tests.cpp`
- `core/src/Makefile.test.include`, `install_cpp_vectors.py`
- `formal_verification/tests/test_p0_016_vector_lock.py`

## Fixture

Regtest, mock time = genesis + 100, coinbase key `00…01` (same as `TestChain100Setup`). Empty mempool. Height-1 from genesis.

## Tasks

1. GBT JSON + live `BlockAssembler::CreateNewBlock` replay
2. ConnectBlock JSON + `ProcessNewBlock` accept/reject
3. Python hex/layout lock

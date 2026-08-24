# OpuHash (Proof-of-Work)

OpuHash is a **compute-heavy** PoW: a **64 KiB** ChaCha20-filled buffer (fits typical **CPU L2 cache**) plus **256** rounds of hashing-based mixing. Consensus changes require a coordinated network upgrade.

## Architecture goals

- **ASIC-friendly (low-cost SRAM):** A 64 KiB working set maps cleanly to on-chip SRAM, avoiding large off-chip DRAM footprints that dominate area and power on dedicated silicon.
- **CPU L2 cache optimization:** Keeping the buffer in L2 reduces memory-bandwidth advantage for wide-GPU paths and shifts competition toward **serial hash throughput**, narrowing the CPU vs GPU gap in practice.
- **Determinism:** Same inputs produce the same `uint256` on every platform and thread.

The design keeps **simple indexing** (`ReadLE32(current_h)` masked to the buffer,
8-byte loads, clamped bounds) for **auditability** and cross-platform stability—no SIMD requirement in the consensus path. Round count does not enter the index (a historical `round << 24` XOR was a no-op under the 16-bit mask and MUST NOT be reintroduced).

## Why 256 mix rounds?

Two hundred fifty-six rounds increase **computational depth** (each round applies Bitcoin’s standard `CHashWriter` / double-SHA256 pipeline) so the function is **compute-saturated** relative to the small buffer. The count is a power of two for clarity and sits between **under-mixing** (weak diffusion) and **over-mixing** (needlessly slow block validation and larger ASIC cycle budgets). It is a deliberate balance between **cryptographic heft** and **node verification cost**.

## Parameters

| Parameter | Value |
|-----------|--------|
| Buffer | **64 KiB** (`Consensus::OPUS_POW_BUFFER_SIZE = 64 * 1024`), ChaCha20 block-aligned |
| Mix rounds | **256** (`Consensus::OPUS_POW_MIX_ROUNDS`) |
| Index | `ReadLE32(current_h)` masked; offset clamped for 8-byte `ReadLE64` |
| Fill | ChaCha20 keystream, 32-byte key from `base_hash`, IV/counter zeroed |
| Round update | `CHashWriter(SER_GETHASH, PROTOCOL_VERSION) << current_h << mem_val` → `GetHash()` |
| Final PoW | `CHashWriter` over `base_hash` and `mixed_hash` → `GetHash()` |

**Block identifier:** `base_hash = block.GetHash()` (double-SHA256 of the serialized header). Merkle, txid, and P2P layouts are unchanged.

**Genesis:** `consensus.hashGenesisBlock` is exempt from the work check after valid `nBits` encoding; all later blocks must meet OpuHash vs target.

## Test vectors

See `src/test/pow_tests.cpp` `OpuHash_v3plus_reference_vectors` and `test/functional/test_framework/opus_pow.py` (self-test on import).

## Implementation

- `src/consensus/rigor_consensus_lock.h` — `OPUS_POW_BUFFER_SIZE`, `OPUS_POW_MIX_ROUNDS`.
- `src/pow_opus.h` / `src/pow_opus.cpp` — thread-local buffer, no heap on the hot path.
- `src/pow.cpp` — `CheckProofOfWork` uses `GetOpusHashFromBaseHash` (genesis exempt).

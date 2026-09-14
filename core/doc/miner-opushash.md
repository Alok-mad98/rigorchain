# CPU Miner OpuHash Integration (Minimal Guide)

> **P0-098.** Official SOLO `minerd` now hashes SHA3-256(`RGRPOW01` || header_80).
> This file is a historical OpuHash grind-path note. See `miner/README_MINER.md`.

This guide summarizes how the in-tree CPU miner evaluates the historical
OpuHash mix. It does not define current node consensus.

## Goal

Evaluate **OpuHash** on the block header id while keeping the **80-byte header** layout identical to Bitcoin-style serialization.

- Header format: unchanged
- Non-PoW hashing: unchanged
- PoW decision: compare `GetOpusHashFromBaseHash(header_hash)` to the target from `nBits`

## Reference behavior (node)

1. `header_hash = CBlockHeader::GetHash()` (double-SHA256 of the serialized 80-byte header).
2. `pow_hash = GetOpusHashFromBaseHash(header_hash)` (see `src/pow_opus.cpp`).
3. Valid if `UintToArith256(pow_hash) <= ArithUint256(nBits target)`.

The miner must match this exactly.

## Non-negotiable consensus parameters

Locked in `consensus/rigor_consensus_lock.h` and implemented in `pow_opus.cpp`:

- Buffer size: **64 KiB**
- Mix rounds: **256**
- ChaCha20 key: first **32 bytes** of `header_hash` (uint256 memory order), IV/counter zeroed, **64 KiB** keystream
- Each round: `CHashWriter` (double-SHA256) over `current_h || mem_val` (see node sources)
- Final digest: `CHashWriter` over `base_hash || mixed_hash`

## Cross-check

1. Run `src/test/pow_tests.cpp` vectors (`OpuHash_v3plus_reference_vectors`).
2. Compare against `test/functional/test_framework/opus_pow.py` self-test.
3. Compare miner `opus_hash_selftest()` against the same frozen vectors.

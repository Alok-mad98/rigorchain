// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/*
 * OpuHash — compute-heavy PoW with a small L2-sized buffer.
 *
 * Indexing and round structure are consensus-critical; buffer size and round
 * count are locked in consensus/rigor_consensus_lock.h. ChaCha20 fills a 64 KiB
 * buffer (multiple of 64-byte blocks for aligned keystream generation).
 */

#include <pow_opus.h>

#include <crypto/chacha20.h>
#include <crypto/common.h>
#include <hash.h>
#include <primitives/block.h>
#include <version.h>

#include <cstdint>

namespace {

static_assert(OPUS_POW_BUFFER_SIZE % 64 == 0, "OpuHash buffer must be ChaCha20 block-aligned (64 bytes)");
static_assert(OPUS_POW_BUFFER_SIZE <= 0xFFFFFFFFu, "OpuHash buffer size must fit 32-bit index mask");

constexpr uint32_t OPUS_POW_BUFFER_MASK = static_cast<uint32_t>(OPUS_POW_BUFFER_SIZE - 1);

/** Deterministic ChaCha20 expansion: key = 32-byte seed, IV/counter zeroed. */
/* Not named "FillMemory": Windows headers define FillMemory as a macro (RtlFillMemory). */
void OpusChaChaFillBuffer(unsigned char* mem, const uint256& seed)
{
    ChaCha20 chacha(seed.begin());
    chacha.SetIV(0);
    chacha.Seek64(0);
    chacha.Keystream(mem, OPUS_POW_BUFFER_SIZE);
}

/**
 * Mixing loop: OPUS_POW_MIX_ROUNDS iterations, byte-indexed 8-byte reads.
 * Thread-local scratch; no heap allocation on the hot path.
 */
uint256 ComputeOpusMix(const uint256& input)
{
    thread_local static unsigned char s_mem[OPUS_POW_BUFFER_SIZE];
    OpusChaChaFillBuffer(s_mem, input);

    uint256 current_h = input;

    for (int round = 0; round < OPUS_POW_MIX_ROUNDS; ++round) {
        // Index is ReadLE32(current_h) & (SIZE-1). A historical `(round << 24)` XOR
        // was a no-op under this mask (bits 24–31); it is omitted so independent
        // implementations cannot "fix" it into a consensus fork.
        const uint32_t val = ReadLE32(current_h.begin());
        uint32_t idx = val & OPUS_POW_BUFFER_MASK;
        if (idx > OPUS_POW_BUFFER_SIZE - 8) {
            idx = static_cast<uint32_t>(OPUS_POW_BUFFER_SIZE - 8);
        }

        const uint64_t mem_val = ReadLE64(s_mem + idx);

        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << current_h;
        ss << mem_val;
        current_h = ss.GetHash();
    }

    return current_h;
}

} // namespace

uint256 GetOpusHashFromBaseHash(const uint256& base_hash)
{
    const uint256 mixed_hash = ComputeOpusMix(base_hash);

    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << base_hash;
    ss << mixed_hash;
    return ss.GetHash();
}

uint256 GetOpusHash(const CBlockHeader& block)
{
    return GetOpusHashFromBaseHash(block.GetHash());
}

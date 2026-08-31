// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rigor_kernel/pow_digest.h>

#include <crypto/common.h>
#include <primitives/block.h>
#include <rigor_kernel/keccak.h>

#include <cstring>

namespace rigor_kernel {

void BuildPowPreimage(const CBlockHeader& header, unsigned char out[POW_PREIMAGE_LEN]) noexcept
{
    // Written field by field into a stack buffer rather than through
    // CDataStream: this runs once per nonce and must not allocate. The layout
    // is the consensus header serialization and is pinned to CDataStream by
    // rigor_pow_digest_tests.
    std::memcpy(out, POW_DOMAIN_TAG, POW_DOMAIN_TAG_LEN);

    unsigned char* p = out + POW_DOMAIN_TAG_LEN;
    WriteLE32(p, static_cast<uint32_t>(header.nVersion));
    p += 4;
    std::memcpy(p, header.hashPrevBlock.begin(), 32);
    p += 32;
    std::memcpy(p, header.hashMerkleRoot.begin(), 32);
    p += 32;
    WriteLE32(p, header.nTime);
    p += 4;
    WriteLE32(p, header.nBits);
    p += 4;
    WriteLE32(p, header.nNonce);
}

uint256 GetRigorPowHash(const CBlockHeader& header) noexcept
{
    unsigned char preimage[POW_PREIMAGE_LEN];
    BuildPowPreimage(header, preimage);

    uint256 digest;
    Sha3_256Short(preimage, POW_PREIMAGE_LEN, digest.begin());
    return digest;
}

} // namespace rigor_kernel

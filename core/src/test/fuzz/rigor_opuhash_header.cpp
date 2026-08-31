// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <hash.h>
#include <pow.h>
#include <primitives/block.h>
#include <rigor_kernel/pow_digest.h>
#include <streams.h>
#include <test/fuzz/fuzz.h>
#include <uint256.h>
#include <version.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <ios>
#include <vector>

void initialize_rigor_opuhash_header()
{
    SelectParams(CBaseChainParams::MAIN);
}

FUZZ_TARGET_INIT(rigor_opuhash_header, initialize_rigor_opuhash_header)
{
    if (buffer.size() < 80) return;
    std::vector<unsigned char> raw(buffer.begin(), buffer.begin() + 80);
    CBlockHeader h;
    CDataStream ss(raw, SER_NETWORK, PROTOCOL_VERSION);
    try {
        ss >> h;
    } catch (const std::ios_base::failure&) {
        return;
    }
    CDataStream out(SER_NETWORK, PROTOCOL_VERSION);
    out << h;
    if (out.size() != 80) return;

    const uint256 id = h.GetHash();
    const uint256 from_bytes = Hash(Span<const unsigned char>{UCharCast(out.data()), out.size()});
    assert(id == from_bytes);

    unsigned char preimage[rigor_kernel::POW_PREIMAGE_LEN];
    rigor_kernel::BuildPowPreimage(h, preimage);
    assert(std::equal(out.begin(), out.end(), preimage + rigor_kernel::POW_DOMAIN_TAG_LEN,
                      [](std::byte a, unsigned char b) {
                          return static_cast<unsigned char>(a) == b;
                      }));
    assert(rigor_kernel::GetRigorPowHash(h) == rigor_kernel::GetRigorPowHash(h));
    (void)CheckProofOfWork(h, h.nBits, Params().GetConsensus());
}

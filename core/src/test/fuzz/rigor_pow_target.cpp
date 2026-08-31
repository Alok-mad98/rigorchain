// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <arith_uint256.h>
#include <chainparams.h>
#include <pow.h>
#include <primitives/block.h>
#include <rigor_kernel/pow_digest.h>
#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/fuzz.h>
#include <test/fuzz/util.h>
#include <uint256.h>

void initialize_rigor_pow_target()
{
    SelectParams(CBaseChainParams::MAIN);
}

FUZZ_TARGET_INIT(rigor_pow_target, initialize_rigor_pow_target)
{
    FuzzedDataProvider rng(buffer.data(), buffer.size());
    const Consensus::Params& consensus = Params().GetConsensus();
    const uint32_t nbits = rng.ConsumeIntegral<uint32_t>();

    // Drive the header fields, not a header id: the PoW is verified against a
    // header, and under SPEC-008 it becomes a function of the 80 header bytes.
    CBlockHeader header;
    header.nVersion = rng.ConsumeIntegral<int32_t>();
    header.hashPrevBlock = ConsumeUInt256(rng);
    header.hashMerkleRoot = ConsumeUInt256(rng);
    header.nTime = rng.ConsumeIntegral<uint32_t>();
    header.nBits = rng.ConsumeIntegral<uint32_t>();
    header.nNonce = rng.ConsumeIntegral<uint32_t>();

    bool neg = false, ovf = false;
    arith_uint256 target;
    target.SetCompact(nbits, &neg, &ovf);
    const bool compact_ok = !neg && !ovf && target != 0 && target <= UintToArith256(consensus.powLimit);
    const bool pow_ok = CheckProofOfWork(header, nbits, consensus);
    if (header.GetHash() == consensus.hashGenesisBlock) {
        assert(pow_ok == compact_ok);
        return;
    }
    if (!compact_ok) {
        assert(!pow_ok);
        return;
    }
    assert(pow_ok == !(UintToArith256(rigor_kernel::GetRigorPowHash(header)) > target));
}

// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <arith_uint256.h>
#include <chain.h>
#include <chainparams.h>
#include <pow.h>
#include <primitives/block.h>
#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/fuzz.h>

#include <cassert>
#include <limits>

void initialize_rigor_asert()
{
    SelectParams(CBaseChainParams::MAIN);
}

FUZZ_TARGET_INIT(rigor_asert, initialize_rigor_asert)
{
    FuzzedDataProvider rng(buffer.data(), buffer.size());
    const Consensus::Params& consensus = Params().GetConsensus();
    const CBlock& genesis_block = Params().GenesisBlock();

    CBlockIndex genesis{};
    genesis.nHeight = 0;
    genesis.pprev = nullptr;
    genesis.nBits = genesis_block.nBits;
    genesis.nTime = genesis_block.nTime;

    CBlockIndex b1{};
    b1.nHeight = 1;
    b1.pprev = &genesis;
    b1.nBits = genesis.nBits;
    b1.nTime = genesis.nTime + static_cast<uint32_t>(consensus.nPowTargetSpacing);

    CBlockIndex last{};
    last.nHeight = rng.ConsumeIntegralInRange<int>(1, 4096);
    last.pprev = last.nHeight == 1 ? &genesis : &b1;
    if (last.nHeight == 1) {
        last.nBits = genesis.nBits;
        last.nTime = b1.nTime;
        last.pprev = &genesis;
    } else {
        last.nBits = genesis.nBits;
        const uint32_t dt = rng.ConsumeIntegral<uint32_t>();
        last.nTime = b1.nTime + (dt % (30U * 24U * 3600U));
        last.pprev = &b1;
        last.nHeight = 2;
    }

    CBlockHeader child;
    child.nTime = rng.ConsumeIntegral<uint32_t>();
    const unsigned a = GetNextWorkRequired(&last, &child, consensus);
    child.nTime = child.nTime ^ 0x00ffffffU;
    const unsigned b = GetNextWorkRequired(&last, &child, consensus);
    assert(a == b);

    arith_uint256 tgt;
    bool neg = false, ovf = false;
    tgt.SetCompact(a, &neg, &ovf);
    assert(!neg && !ovf);
    assert(tgt > 0);
    assert(tgt <= UintToArith256(consensus.powLimit));
    assert(PermittedDifficultyTransition(consensus, last.nHeight + 1, last.nBits, a));
}

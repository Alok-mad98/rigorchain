// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POW_H
#define BITCOIN_POW_H

#include <consensus/params.h>

#include <stdint.h>

class CBlockHeader;
class CBlockIndex;
class uint256;

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params&);
unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params&);

/**
 * Verify nBits and (unless genesis) the proof of work for a header.
 * Genesis (header id == params.hashGenesisBlock): compact target is checked; work is skipped.
 *
 * The header is taken by reference and never by header id. Under SPEC-008 the
 * PoW digest is SHA3-256("RGRPOW01" || header_80), a function of the 80 raw
 * header bytes, so a uint256 is not enough to compute it. The former
 * CheckProofOfWork(uint256, ...) overload was removed for that reason: keeping
 * it would have left an API that cannot be implemented after the switch.
 */
bool CheckProofOfWork(const CBlockHeader& block, unsigned int nBits, const Consensus::Params& params);

/**
 * Return false if the proof-of-work requirement specified by new_nbits at a
 * given height is not possible, given the proof-of-work on the prior block as
 * specified by old_nbits.
 *
 * This function checks that the new compact target is within the consensus
 * per-block clamp (currently ±12.5% in target space, after compact re-encoding)
 * of the previous block's target. The same constants bound ASERT's output, so
 * an honest ASERT header always passes this headers-sync check.
 *
 * Always returns true on networks where min difficulty blocks are allowed,
 * such as regtest/testnet.
 */
bool PermittedDifficultyTransition(const Consensus::Params& params, int64_t height, uint32_t old_nbits, uint32_t new_nbits);

#endif // BITCOIN_POW_H

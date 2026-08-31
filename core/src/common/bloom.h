// Copyright (c) 2012-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_COMMON_BLOOM_H
#define BITCOIN_COMMON_BLOOM_H

#include <span.h>

#include <cstdint>
#include <vector>

/**
 * RollingBloomFilter is a probabilistic "keep track of most recently inserted" set.
 * Construct it with the number of items to keep track of, and a false-positive
 * rate. By default nTweak is set to a cryptographically secure random value.
 * reset() also changes nTweak to decrease the impact of false-positives.
 *
 * contains(item) will always return true if item was one of the last N to 1.5*N
 * insert()'ed ... but may also return true for items that were not inserted.
 *
 * BIP37 CBloomFilter is not part of RIGOR.
 */
class CRollingBloomFilter
{
public:
    CRollingBloomFilter(const unsigned int nElements, const double nFPRate);

    void insert(Span<const unsigned char> vKey);
    bool contains(Span<const unsigned char> vKey) const;

    void reset();

private:
    int nEntriesPerGeneration;
    int nEntriesThisGeneration;
    int nGeneration;
    std::vector<uint64_t> data;
    unsigned int nTweak;
    int nHashFuncs;
};

#endif // BITCOIN_COMMON_BLOOM_H

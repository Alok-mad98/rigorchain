// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_CONSENSUS_H
#define BITCOIN_CONSENSUS_CONSENSUS_H

#include <consensus/rigor_consensus_lock.h>

#include <cstdlib>
#include <stdint.h>

/** The maximum allowed size for a serialized block, in bytes (only for buffer size limits) */
static const unsigned int MAX_BLOCK_SERIALIZED_SIZE = Consensus::BLOCK_SERIALIZED_SIZE_LIMIT;
/** The maximum allowed weight for a block, see BIP 141 (network rule) */
static const unsigned int MAX_BLOCK_WEIGHT = Consensus::BLOCK_WEIGHT_LIMIT;
/** The maximum allowed number of signature check operations in a block (network rule) */
static const int64_t MAX_BLOCK_SIGOPS_COST = Consensus::BLOCK_SIGOPS_COST_LIMIT;
/** Coinbase transaction outputs can only be spent after this number of new blocks (network rule) */
static const int COINBASE_MATURITY = 100;
/** Body RES mint outputs use the same confirmation depth as ORU coinbase (intentional SSOT). */
static const int RES_MINT_MATURITY = COINBASE_MATURITY;
static_assert(RES_MINT_MATURITY == COINBASE_MATURITY,
    "RES mint maturity is locked to COINBASE_MATURITY; runtime uses Params::nCoinbaseMaturity");

static_assert(MAX_BLOCK_SERIALIZED_SIZE == Consensus::BLOCK_SERIALIZED_SIZE_LIMIT,
    "MAX_BLOCK_SERIALIZED_SIZE must match consensus lock");
static_assert(MAX_BLOCK_WEIGHT == Consensus::BLOCK_WEIGHT_LIMIT,
    "MAX_BLOCK_WEIGHT must match consensus lock");
static_assert(MAX_BLOCK_SIGOPS_COST == Consensus::BLOCK_SIGOPS_COST_LIMIT,
    "MAX_BLOCK_SIGOPS_COST must match consensus lock");

static const int WITNESS_SCALE_FACTOR = 4;

static const size_t MIN_TRANSACTION_WEIGHT = WITNESS_SCALE_FACTOR * 60; // 60 is the lower bound for the size of a valid serialized CTransaction
static const size_t MIN_SERIALIZABLE_TRANSACTION_WEIGHT = WITNESS_SCALE_FACTOR * 10; // 10 is the lower bound for the size of a serialized CTransaction

/** Flags for nSequence and nLockTime locks */
/** Interpret sequence numbers as relative lock-time constraints. */
static constexpr unsigned int LOCKTIME_VERIFY_SEQUENCE = (1 << 0);

#endif // BITCOIN_CONSENSUS_CONSENSUS_H

// Copyright (c) 2020 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPC_MINING_H
#define BITCOIN_RPC_MINING_H

/** Default max iterations to try in RPC generatetodescriptor, generatetoaddress, and generateblock. */
static const uint64_t DEFAULT_MAX_TRIES{1000000};

/** HTTP workers that getblocktemplate longpoll may never occupy, so submitblock always gets one. */
static const int GBT_LONGPOLL_WORKER_RESERVE{2};

/** Seconds a getblocktemplate longpoll may park its HTTP worker before returning the current template. */
static const int64_t DEFAULT_RPC_LONGPOLL_TIMEOUT{120};

#endif // BITCOIN_RPC_MINING_H

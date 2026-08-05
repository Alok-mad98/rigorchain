// Copyright (c) 2017-2022 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPC_MEMPOOL_H
#define BITCOIN_RPC_MEMPOOL_H

class ChainstateManager;
class CTxMemPool;
class UniValue;

/** Mempool information to JSON (no chain: tip convergence summary fields omitted). */
UniValue MempoolInfoToJSON(const CTxMemPool& pool);
/** Mempool information to JSON; when \p chainman is non-null, adds cumulative_convergence and current_convergence_sum from the active tip. */
UniValue MempoolInfoToJSON(const CTxMemPool& pool, ChainstateManager* chainman);

/** Mempool to JSON */
UniValue MempoolToJSON(const CTxMemPool& pool, bool verbose = false, bool include_mempool_sequence = false);

#endif // BITCOIN_RPC_MEMPOOL_H

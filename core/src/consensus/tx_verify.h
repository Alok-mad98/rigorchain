// Copyright (c) 2017-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_TX_VERIFY_H
#define BITCOIN_CONSENSUS_TX_VERIFY_H

#include <consensus/amount.h>
#include <primitives/transaction.h>

#include <stdint.h>
#include <optional>
#include <vector>

class CBlockIndex;
class CCoinsViewCache;
class CTransaction;
namespace Consensus { struct Params; }
class TxValidationState;

/** Transaction validation functions */

namespace Consensus {
/**
 * RFC-003 semantic domain from prevouts: `semantic = f(input_asset_domain)`.
 * RES prevouts dominate (dual-pool RES Spend with ORU fee inputs → ASSET_RES).
 * ORU-only prevouts → ASSET_ORU. Mixed ORU+RES is legal at input layer when dual-pool rules hold.
 * @return nullopt only when no ORU/RES prevout is present.
 */
[[nodiscard]] std::optional<AssetType> GetTransactionInputAsset(const CTransaction& tx, const CCoinsViewCache& inputs);

/**
 * Dual-pool conservation: POOL-T (transfer) per asset; POOL-F network fee always ORU (`oru_in - oru_out`).
 * RES transfer requires ORU fee inputs; RES amounts never fund the network fee.
 * @param[out] txfee ORU network fee (sat) on success.
 * Preconditions: tx.IsCoinBase() is false.
 */
[[nodiscard]] bool CheckTxInputs(const CTransaction& tx, TxValidationState& state, const CCoinsViewCache& inputs, int nSpendHeight, const Consensus::Params& consensus_params, CAmount& txfee);

/**
 * Body Res mint vs Res spend: asset I/O rules are here (`mixed-asset-output`, etc.). Proof **Mint vs Claim**
 * for the unified proof layout is **only** from `GetTransactionInputAsset` — see `rigor_res.cpp`.
 */
} // namespace Consensus

/** Auxiliary functions for transaction validation (ideally should not be exposed) */

/**
 * Count ECDSA signature operations the old-fashioned (pre-0.6) way
 * @return number of sigops this transaction's outputs will produce when spent
 * @see CTransaction::FetchInputs
 */
unsigned int GetLegacySigOpCount(const CTransaction& tx);

/**
 * Count ECDSA signature operations in pay-to-script-hash inputs.
 *
 * @param[in] mapInputs Map of previous transactions that have outputs we're spending
 * @return maximum number of sigops required to validate this transaction's inputs
 * @see CTransaction::FetchInputs
 */
unsigned int GetP2SHSigOpCount(const CTransaction& tx, const CCoinsViewCache& mapInputs);

/**
 * Compute total signature operation cost of a transaction.
 * @param[in] tx     Transaction for which we are computing the cost
 * @param[in] inputs Map of previous transactions that have outputs we're spending
 * @param[in] flags Script verification flags
 * @return Total signature operation cost of tx
 */
int64_t GetTransactionSigOpCost(const CTransaction& tx, const CCoinsViewCache& inputs, uint32_t flags);

/**
 * Check if transaction is final and can be included in a block with the
 * specified height and time. Consensus critical.
 */
bool IsFinalTx(const CTransaction &tx, int nBlockHeight, int64_t nBlockTime);

/**
 * Calculates the block height and previous block's median time past at
 * which the transaction will be considered final in the context of BIP 68.
 * For each input that is not sequence locked, the corresponding entries in
 * prevHeights are set to 0 as they do not affect the calculation.
 */
std::pair<int, int64_t> CalculateSequenceLocks(const CTransaction &tx, int flags, std::vector<int>& prevHeights, const CBlockIndex& block);

bool EvaluateSequenceLocks(const CBlockIndex& block, std::pair<int, int64_t> lockPair);
/**
 * Check if transaction is final per BIP 68 sequence numbers and can be included in a block.
 * Consensus critical. Takes as input a list of heights at which tx's inputs (in order) confirmed.
 */
bool SequenceLocks(const CTransaction &tx, int flags, std::vector<int>& prevHeights, const CBlockIndex& block);

#endif // BITCOIN_CONSENSUS_TX_VERIFY_H

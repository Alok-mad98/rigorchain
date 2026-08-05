// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// RIGOR Fee Mechanism - Pure Fee Rate Auction
// Normative source: RIGOR Fee Mechanism Implementation Specification (FINAL LOCK)
// This header is consensus-critical and must NEVER be altered without a hard fork.

#ifndef BITCOIN_CONSENSUS_RIGOR_FEE_H
#define BITCOIN_CONSENSUS_RIGOR_FEE_H

#include <consensus/amount.h>
#include <primitives/transaction.h>

#include <cstddef>
#include <cstdint>
#include <optional>

class CCoinsViewCache;

namespace rigor {

/**
 * RIGOR Fee Mechanism Specification §1.1: Fee Calculation (the sole rule)
 * fee_i = sum(inputs_ORUM) - sum(outputs_ORUM)
 *
 * Constraints:
 * - Unit: minimum unit (satoshi-like)
 * - Type: uint64 (unsigned 64-bit integer)
 * - fee_i MUST >= 0
 * - fee_i == 0 → transaction MUST NOT enter candidate set
 *
 * FORBIDDEN:
 * - Bitcoin weight / vsize / witness discount
 * - EIP-1559 base fee
 * - fee multiplier / fee bucket / fee class
 * - dynamic adjustment parameters
 * - minimum fee at protocol layer
 *
 * @param tx Transaction to calculate fee for
 * @param inputs UTXO set for input lookup
 * @param[out] fee_out Calculated fee in ORUM (satoshi-like units)
 * @return true if fee calculation succeeded, false on error
 */
bool CalculateFee_ORUM(const CTransaction& tx, const CCoinsViewCache& inputs, uint64_t& fee_out);

/**
 * RIGOR Fee Mechanism Specification §1.2: Transaction Size Definition (the sole standard)
 * tx_size_bytes = serialized transaction size in bytes
 *
 * Requirements:
 * - Use network serialized byte length (wire format)
 * - FORBIDDEN: weight, virtual size, witness discount, any discount mechanisms
 * - size MUST be exact byte count
 *
 * @param tx Transaction to calculate size for
 * @return Exact serialized byte size of transaction
 */
size_t GetTransactionSizeBytes(const CTransaction& tx);

/**
 * RIGOR Fee Mechanism Specification §1.3: Fee Rate Definition (ordering only; never enters consensus)
 * fee_rate_i = fee_i / tx_size_bytes
 *
 * Note:
 * - fee_rate is ONLY used for mempool / block packing strategy
 * - fee_rate NEVER enters consensus calculation
 * - FORBIDDEN floating point: use integer comparison (cross multiplication)
 *
 * RIGOR Fee Mechanism Specification §3.1: Mempool ordering rules
 * All nodes MUST sort by fee_rate from high to low
 * Comparison (floating point forbidden):
 *   a outranks b ⇔ (a.fee * b.size) > (b.fee * a.size)
 *
 * @param fee_a Fee of transaction A (ORUM)
 * @param size_a Size of transaction A (bytes)
 * @param fee_b Fee of transaction B (ORUM)
 * @param size_b Size of transaction B (bytes)
 * @return true if A has higher fee rate than B
 */
bool CompareFeeRate(uint64_t fee_a, size_t size_a, uint64_t fee_b, size_t size_b);

/**
 * RIGOR Fee Mechanism Specification §2: f_i Definition (consensus binding, absolutely core)
 * f_i =
 *     if RES Mint:   Convergen_amount (ORUM)
 *     if RES Spend:  fee_i (ORUM)
 *
 * §2.1 Mandatory constraints
 * - f_i MUST > 0
 * - f_i MUST be uint64
 * - f_i MUST directly enter consensus inequality
 * - FORBIDDEN any transformation: log(f_i), f_i^β, scaling, normalization
 *
 * @param tx Transaction to calculate f_i for
 * @param inputs UTXO set for input lookup
 * @param[out] f_i_out Calculated f_i value (ORUM)
 * @return true if f_i calculation succeeded, false on error
 */
bool CalculateF_i(const CTransaction& tx, const CCoinsViewCache& inputs, uint64_t& f_i_out);

/**
 * RIGOR Fee Mechanism Specification §2: f_i Definition (consensus binding)
 * Helper function to determine if transaction is RES Mint or RES Spend
 * 
 * @param tx Transaction to classify
 * @param inputs UTXO set for input lookup
 * @return true if RES Mint, false if RES Spend or error
 */
std::optional<bool> IsRESMintTx(const CTransaction& tx, const CCoinsViewCache& inputs);

/**
 * RIGOR Fee Mechanism Specification §7.3: Single asset
 * - Fee MUST use ORUM
 * - RES NOT allowed as fee
 * - NOT allowed multi-asset fee
 *
 * @param tx Transaction to check
 * @param inputs UTXO set for input lookup
 * @return true if transaction pays fee in ORUM, false otherwise
 */
bool IsORUMFeeTx(const CTransaction& tx, const CCoinsViewCache& inputs);

} // namespace rigor

#endif // BITCOIN_CONSENSUS_RIGOR_FEE_H

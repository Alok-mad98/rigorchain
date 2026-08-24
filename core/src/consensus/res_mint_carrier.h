// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_RES_MINT_CARRIER_H
#define BITCOIN_CONSENSUS_RES_MINT_CARRIER_H

#include <consensus/amount.h>
#include <primitives/transaction.h>
#include <uint256.h>

/**
 * Canonical tagged OP_RETURN matching `Consensus::ParseConvergeProof`: `PCOR` ‖ 116 field bytes,
 * at nValue 0, in the coinbase of the block that pays the draw.
 *
 * This is not a witness builder. Vin witnesses are ordinary spends and no body transaction
 * carries the tag — see the FROZEN CARRIER block in `consensus/res_proof.h`.
 */
CScript BuildPcorbCarrierScript(const uint256& convergence_txid,
    CAmount convergence_amount_sat,
    const uint256& bind_hash,
    uint64_t proof_height,
    uint32_t target_prev,
    const uint256& convergence_root);

/** The same script as a zero-value output, ready to append to the coinbase. */
CTxOut BuildPcorbCarrierOutput(const uint256& convergence_txid,
    CAmount convergence_amount_sat,
    const uint256& bind_hash,
    uint64_t proof_height,
    uint32_t target_prev,
    const uint256& convergence_root);

#endif // BITCOIN_CONSENSUS_RES_MINT_CARRIER_H

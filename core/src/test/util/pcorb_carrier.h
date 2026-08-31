// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_TEST_UTIL_PCORB_CARRIER_H
#define BITCOIN_TEST_UTIL_PCORB_CARRIER_H

#include <consensus/amount.h>
#include <primitives/transaction.h>
#include <uint256.h>

/**
 * Test-only PCORB builder and the retired proof parser.
 *
 * Not linked into libbitcoin_consensus (TASK-010 C-3 / R-5). Production
 * rejects every PCORB carrier as `bad-pcorb-carrier` and never decodes one.
 */
CScript BuildPcorbCarrierScript(const uint256& convergence_txid,
    CAmount convergence_amount_sat,
    const uint256& bind_hash,
    uint64_t proof_height,
    uint32_t target_prev,
    const uint256& convergence_root);

CTxOut BuildPcorbCarrierOutput(const uint256& convergence_txid,
    CAmount convergence_amount_sat,
    const uint256& bind_hash,
    uint64_t proof_height,
    uint32_t target_prev,
    const uint256& convergence_root);

namespace Consensus {

struct ConvergeProof {
    uint256 convergence_txid;
    uint64_t convergence_amount_sat{0};
    uint256 bind_hash;
    uint64_t height{0};
    uint32_t target_prev{0};
    uint256 convergence_root{};
};

bool ParseConvergeProof(const CTransaction& coinbase, ConvergeProof& out);
bool CheckConvergeProofBinding(const ConvergeProof& proof, CAmount committed_convergence_amount);
bool VerifyConvergeProof(const ConvergeProof& proof, const uint256& res_probability_target, const uint256* winner_txid, const uint256* expected_convergence_root);
bool VerifyConvergeProof(const CTransaction& coinbase, const uint256& res_probability_target);
uint256 ConvergeProofDigest(const uint256& convergence_txid, const uint256& bind_hash, uint64_t height, const uint256& convergence_root);

} // namespace Consensus

#endif // BITCOIN_TEST_UTIL_PCORB_CARRIER_H

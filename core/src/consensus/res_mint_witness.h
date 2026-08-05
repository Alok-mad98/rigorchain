// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_RES_MINT_WITNESS_H
#define BITCOIN_CONSENSUS_RES_MINT_WITNESS_H

#include <consensus/amount.h>
#include <primitives/transaction.h>
#include <uint256.h>

#include <cstddef>

/** Pushes in a PCORB body-mint witness: exactly the six proof fields, nothing else.
 *
 *  SECURITY-001-A / A-7 removed five further pushes: an Ed25519 signature over σ and the key that
 *  verified it (which authorized nothing, see `consensus/res_proof.h`), the leaf script, and two
 *  reserved-zero placeholders. The latter three were only ever inspected by the RES script path that
 *  A-7 also removed. Funding inputs that carry this witness must drop exactly this many pushes.
 */
static constexpr size_t PCORB_BODY_RES_MINT_WITNESS_ITEMS = 6;

/** PCORB body-mint witness stack matching `Consensus::ParseConvergeProof`. */
CScriptWitness BuildPcorbBodyResMintWitness(const uint256& convergence_txid,
    CAmount convergence_amount_sat,
    const uint256& bind_hash,
    uint64_t proof_height,
    uint32_t target_prev,
    const uint256& convergence_root);

#endif // BITCOIN_CONSENSUS_RES_MINT_WITNESS_H

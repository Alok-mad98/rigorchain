// Copyright (c) The Rigor Chain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// SECURITY-001-A finding A-5 — RES reward owner binding.
//
// A convergence transaction burns ORU into an unspendable OP_CONVERGE output and buys a
// probability of minting RES. The owner is the HASH160 inside that same scriptPubKey —
// the digest OP_RES_MINT pays. There is no sibling OP_RETURN and no extra-UTXO owner table.
// The declaration is authenticated by the owner's input signatures because it is part of
// the transaction the owner created and signed.

#ifndef BITCOIN_CONSENSUS_RES_OWNER_H
#define BITCOIN_CONSENSUS_RES_OWNER_H

#include <uint256.h>

#include <optional>
#include <vector>

class CBlock;
class CScript;
class CTransaction;

namespace Consensus {

/** Owner-declared RES payout: HASH160 of the 32-byte witness v1 RES program. */
struct ResOwnerDeclaration {
    uint160 payout_hash160;
};

/**
 * The declaration carried by \p tx: HASH160 inside its unique positive-value OP_CONVERGE output.
 *
 * Exactly one well-formed lock. Zero or several means "no declaration", which makes the
 * transaction ineligible for the draw (it is then an ordinary ORU transfer, or invalid
 * structure if it carries more than one converge output).
 */
std::optional<ResOwnerDeclaration> GetResOwnerDeclaration(const CTransaction& tx);

/** True iff \p tx carries exactly one well-formed OP_CONVERGE owner lock. */
bool HasResOwnerDeclaration(const CTransaction& tx);

/**
 * RFC A-5 R2 — the RES minted by a block must pay the winning convergence's declared owner.
 *
 * \p payout_script must be `OP_RES_MINT` whose pubkey hash equals \p declared_hash160.
 */
bool CheckResRewardOwner(const uint160& declared_hash160, const CScript& payout_script);

} // namespace Consensus

#endif // BITCOIN_CONSENSUS_RES_OWNER_H

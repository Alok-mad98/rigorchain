// Copyright (c) The Rigor Chain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// SECURITY-001-A finding A-5 — RES reward owner binding.
//
// A convergence transaction burns ORU into an unspendable CVG2 output and buys a
// probability of minting RES. Before this rule consensus never checked *whose* script
// received that RES: the miner filled `vout[0].scriptPubKey` freely, so a block producer
// could redirect the reward earned by any user's convergence to itself.
//
// The owner declares its RES payout key in an OP_RETURN output of the convergence
// transaction. The declaration is authenticated by the owner's own input signatures
// because it is part of the transaction the owner created and signed. The full 32-byte
// program must be published rather than a hash of it: a RES output is
// `OP_1 <32-byte ed25519 key>` and the miner constructs that output, so it needs the key
// itself, not a commitment it cannot invert.
//
// See docs/security/SECURITY-001-A5-DESIGN-v2.md.

#ifndef BITCOIN_CONSENSUS_RES_OWNER_H
#define BITCOIN_CONSENSUS_RES_OWNER_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

class CBlock;
class CScript;
class CTransaction;
class uint256;

namespace Consensus {

/** ASCII "RESO" magic, version byte, then the 32-byte witness v1 RES program. */
static constexpr size_t RES_OWNER_PAYLOAD_SIZE = 4 + 1 + 32;
static constexpr uint8_t RES_OWNER_VERSION = 0x01;

/** Owner-declared RES payout target: the 32-byte witness v1 program (ed25519 public key). */
struct ResOwnerDeclaration {
    std::vector<unsigned char> program32;
};

/** Build the declaration output script: `OP_RETURN "RESO" || 0x01 || program32`. */
CScript ResOwnerDeclarationScript(const std::vector<unsigned char>& program32);

/**
 * Parse one OP_RETURN payload as a declaration. Byte-shape only: exact size, exact magic,
 * exact version. No transaction context, no script execution.
 *
 * The enclosing script must be exactly `OP_RETURN <minimal direct push> <payload>`; see
 * GetResOwnerDeclaration for why the push encoding is pinned rather than read with GetOp.
 */
std::optional<ResOwnerDeclaration> ParseResOwnerDeclarationPayload(const std::vector<unsigned char>& data);

/**
 * The declaration carried by \p tx, if it carries **exactly one**.
 *
 * Exactly one, not last-wins. The coinbase CVGE commitment can use last-wins because one
 * party writes it; owner binding is adversarial, and two declarations would leave an
 * ambiguity a miner could resolve in its own favour. Zero or several means "no declaration",
 * which under RFC A-5 R1 makes the transaction ineligible for the draw (it stays a valid
 * ORU burn).
 */
std::optional<ResOwnerDeclaration> GetResOwnerDeclaration(const CTransaction& tx);

/** True iff \p tx carries exactly one well-formed declaration (RFC A-5 R1 eligibility). */
bool HasResOwnerDeclaration(const CTransaction& tx);

/**
 * RFC A-5 R2 — the RES minted by a block must pay the winning convergence's declared owner.
 *
 * \p res_mint_tx must carry exactly one `IsResScript` output; its 32-byte witness program must
 * equal \p declared_program32, the program the winning convergence published.
 *
 * SECURITY-001-A8: the caller reads that program from the draw set of block h-1. The winning
 * convergence and the mint paying for it are no longer in the same block, so there is nothing
 * to locate in the block being validated. R1 guarantees a declaration exists for anything that
 * reached `F`; an empty program here means the candidate set and this check disagree, and the
 * size test fails closed.
 */
bool CheckResRewardOwner(const std::vector<unsigned char>& declared_program32,
    const CTransaction& res_mint_tx);

} // namespace Consensus

#endif // BITCOIN_CONSENSUS_RES_OWNER_H

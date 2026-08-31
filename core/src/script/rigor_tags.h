// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_RIGOR_TAGS_H
#define BITCOIN_SCRIPT_RIGOR_TAGS_H

#include <array>
#include <cstddef>

/** ASCII / script tag bytes shared by consensus (convergence, Res v1 asset prefix on wire, coinbase RIGOR commitment). */
namespace RigorTags {
inline constexpr std::array<unsigned char, 4> RIGOR = {'R', 'I', 'G', 'R'};
inline constexpr std::array<unsigned char, 4> CVG2 = {'C', 'V', 'G', '2'};
/** On-chain witness v1 asset program prefix (4 ASCII bytes); consensus-locked wire spelling "RES2". */
inline constexpr std::array<unsigned char, 4> Res2WirePrefix = {'R', 'E', 'S', '2'};
/**
 * Coinbase RES-mint PCORB carrier tag (4 ASCII bytes, wire spelling "PCOR").
 * Distinct from RIGOR, CVG2 and RES2. Not an asset class. Production consensus
 * rejects every carrier (`bad-pcorb-carrier`); the retired decoder lives in
 * `test/util/pcorb_carrier.h`.
 */
inline constexpr std::array<unsigned char, 4> PCOR = {'P', 'C', 'O', 'R'};
/** Six PCORB fields concatenated, no inner length prefixes: 32+8+32+8+4+32. */
inline constexpr std::size_t PCORB_PROOF_FIELDS_BYTES = 116;
/** The single RIGOR_MARKER_RETURN push: PCOR ‖ fields. */
inline constexpr std::size_t PCORB_OPRETURN_PAYLOAD_BYTES = 4 + PCORB_PROOF_FIELDS_BYTES;
/**
 * Canonical carrier script length: RIGOR_MARKER_RETURN ‖ RIGOR_PUSHDATA1 ‖ 0x78 ‖ 120-byte payload.
 * 120 bytes has exactly one minimal push encoding, so any other encoding of the same
 * payload is a different script and is not a proof (RFC-009 §0).
 */
inline constexpr std::size_t PCORB_CARRIER_SCRIPT_BYTES = 1 + 2 + PCORB_OPRETURN_PAYLOAD_BYTES;

/** RIGOR_MARKER_RETURN payload: tag RIGOR + 4-byte LE compact res_draw_target nBits (see `IsRigorCommitment`). */
inline constexpr std::size_t RIGOR_COMMITMENT_PAYLOAD_SIZE = 8;

/** P0-018: 8-byte ASCII sighash domain tags. Prefix-position, one per spend path. */
inline constexpr std::array<unsigned char, 8> SighashOruV1 = {'R', 'G', 'R', 'O', 'R', 'U', '0', '1'};
inline constexpr std::array<unsigned char, 8> SighashResV1 = {'R', 'G', 'R', 'R', 'E', 'S', '0', '1'};
inline constexpr std::array<unsigned char, 8> SighashMintV1 = {'R', 'G', 'R', 'M', 'N', 'T', '0', '1'};
} // namespace RigorTags

#endif // BITCOIN_SCRIPT_RIGOR_TAGS_H

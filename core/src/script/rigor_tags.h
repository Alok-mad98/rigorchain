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

/** OP_RETURN payload: tag RIGOR + 4-byte LE compact res_draw_target nBits (see `IsRigorCommitment`). */
inline constexpr std::size_t RIGOR_COMMITMENT_PAYLOAD_SIZE = 8;
} // namespace RigorTags

#endif // BITCOIN_SCRIPT_RIGOR_TAGS_H

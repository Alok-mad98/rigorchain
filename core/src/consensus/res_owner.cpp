// Copyright (c) The Rigor Chain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/res_owner.h>

#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/standard.h>
#include <uint256.h>

#include <cstring>

namespace Consensus {

namespace {
constexpr uint8_t RES_OWNER_MAGIC[4] = {0x52, 0x45, 0x53, 0x4f}; // "RESO"
constexpr size_t RES_PROGRAM_SIZE = 32;
} // namespace

CScript ResOwnerDeclarationScript(const std::vector<unsigned char>& program32)
{
    std::vector<unsigned char> payload(RES_OWNER_PAYLOAD_SIZE);
    std::memcpy(payload.data(), RES_OWNER_MAGIC, 4);
    payload[4] = RES_OWNER_VERSION;
    if (program32.size() == RES_PROGRAM_SIZE) {
        std::memcpy(payload.data() + 5, program32.data(), RES_PROGRAM_SIZE);
    }
    return CScript() << OP_RETURN << payload;
}

std::optional<ResOwnerDeclaration> ParseResOwnerDeclarationPayload(const std::vector<unsigned char>& data)
{
    // RFC-009 §0: consensus-critical data has exactly ONE valid byte representation.
    // Exact size, not a minimum: a longer payload would admit two encodings of one meaning.
    if (data.size() != RES_OWNER_PAYLOAD_SIZE) return std::nullopt;
    if (std::memcmp(data.data(), RES_OWNER_MAGIC, 4) != 0) return std::nullopt;
    if (data[4] != RES_OWNER_VERSION) return std::nullopt;
    ResOwnerDeclaration out;
    out.program32.assign(data.begin() + 5, data.end());
    return out;
}

std::optional<ResOwnerDeclaration> GetResOwnerDeclaration(const CTransaction& tx)
{
    // Canonical script shape, checked byte-wise rather than through GetOp: OP_RETURN followed
    // by a single minimal direct push of the payload. GetOp would also accept PUSHDATA1/2/4
    // encodings of the same 37 bytes, which is three spellings of one declaration — the
    // opposite of RFC-009 §0, and an alignment hazard for the off-chain assembler that has to
    // recognise exactly what consensus recognises.
    static constexpr size_t kScriptSize = 2 + RES_OWNER_PAYLOAD_SIZE;
    std::optional<ResOwnerDeclaration> found;
    for (const auto& out : tx.vout) {
        const CScript& script = out.scriptPubKey;
        if (script.size() != kScriptSize) continue;
        if (script[0] != OP_RETURN) continue;
        if (script[1] != static_cast<unsigned char>(RES_OWNER_PAYLOAD_SIZE)) continue;
        const std::vector<unsigned char> vch(script.begin() + 2, script.end());
        auto parsed = ParseResOwnerDeclarationPayload(vch);
        if (!parsed.has_value()) continue;
        // Exactly one. A second declaration is an ambiguity the block producer would get to
        // resolve, so it disqualifies the transaction rather than picking a winner.
        if (found.has_value()) return std::nullopt;
        found = std::move(parsed);
    }
    return found;
}

bool HasResOwnerDeclaration(const CTransaction& tx)
{
    return GetResOwnerDeclaration(tx).has_value();
}

bool CheckResRewardOwner(const std::vector<unsigned char>& declared_program32,
    const CTransaction& res_mint_tx)
{
    if (declared_program32.size() != RES_PROGRAM_SIZE) return false;

    // Exactly one payout output. Several RES outputs would let a producer pay the owner a
    // token amount and route the rest elsewhere, since only the shape is checked otherwise.
    const CTxOut* payout = nullptr;
    for (const auto& o : res_mint_tx.vout) {
        if (!IsResScript(o.scriptPubKey)) continue;
        if (payout != nullptr) return false;
        payout = &o;
    }
    if (payout == nullptr) return false;

    int version{0};
    std::vector<unsigned char> program;
    if (!payout->scriptPubKey.IsWitnessProgram(version, program)) return false;
    if (version != 1 || program.size() != RES_PROGRAM_SIZE) return false;

    return program == declared_program32;
}

} // namespace Consensus

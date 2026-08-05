// Copyright (c) The Rigor Chain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/convergence_commitment.h>

#include <crypto/common.h>
#include <script/script.h>

#include <cstring>

namespace {
constexpr uint8_t CVGE_COMMITMENT_MAGIC[4] = {0x43, 0x56, 0x47, 0x45}; // "CVGE"
constexpr size_t COMMITMENT_PAYLOAD_V2_SIZE = 4 + 1 + 8 + 32;
} // namespace

std::optional<ConvergenceCommitment> ParseConvergenceCommitmentPayload(const std::vector<unsigned char>& data)
{
    if (data.size() < COMMITMENT_PAYLOAD_V2_SIZE) return std::nullopt;
    if (std::memcmp(data.data(), CVGE_COMMITMENT_MAGIC, 4) != 0) return std::nullopt;
    ConvergenceCommitment fc;
    fc.m_version = data[4];
    if (fc.m_version != ConvergenceCommitment::EXPECTED_VERSION) return std::nullopt;
    fc.m_cumulative_convergence = ReadLE64(data.data() + 5);
    fc.m_draw_candidates.hash.SetNull();
    std::memcpy(fc.m_draw_candidates.root.begin(), data.data() + 13, 32);
    return fc;
}

CScript ConvergenceCommitmentScript(const ConvergenceCommitment& fc)
{
    std::vector<unsigned char> payload(COMMITMENT_PAYLOAD_V2_SIZE);
    std::memcpy(payload.data(), CVGE_COMMITMENT_MAGIC, 4);
    payload[4] = fc.m_version;
    WriteLE64(payload.data() + 5, fc.m_cumulative_convergence);
    std::memcpy(payload.data() + 13, fc.m_draw_candidates.root.begin(), 32);
    return CScript() << OP_RETURN << payload;
}

std::optional<ConvergenceCommitment> FindLastConvergenceCommitmentInCoinbase(const CTransaction& coinbase)
{
    if (!coinbase.IsCoinBase()) return std::nullopt;
    std::optional<ConvergenceCommitment> last;
    for (const auto& out : coinbase.vout) {
        const CScript& script = out.scriptPubKey;
        CScript::const_iterator pc = script.begin();
        opcodetype opcode;
        std::vector<unsigned char> vch;
        if (!script.GetOp(pc, opcode) || opcode != OP_RETURN) continue;
        if (!script.GetOp(pc, opcode, vch)) continue;
        if (auto fc = ParseConvergenceCommitmentPayload(vch)) last = fc;
    }
    return last;
}

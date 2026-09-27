// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/standard.h>

#include <consensus/amount.h>
#include <crypto/ed25519_provider.h>
#include <crypto/sha256.h>
#include <hash.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <script/rigor_tags.h>
#include <script/predicate_verify.h>
#include <script/script.h>
#include <span.h>
#include <util/strencodings.h>

#include <algorithm>
#include <limits>
#include <optional>
#include <string>

typedef std::vector<unsigned char> valtype;

bool IsConvergenceScript(const CScript& scriptPubKey)
{
    return IsConvergeScript(scriptPubKey);
}

bool IsConvergenceV1ScriptPubKey(const CScript& scriptPubKey)
{
    return IsConvergenceScript(scriptPubKey);
}

bool IsResScript(const CScript& scriptPubKey)
{
    std::vector<std::vector<unsigned char>> sols;
    const TxoutType t = ClassifyRigorTxout(scriptPubKey, sols);
    return t == TxoutType::WITNESS_V1_RES || t == TxoutType::RES_MINT;
}

bool IsRigorCommitment(const CScript& script_pub_key, RigorCommitment& commitment_out)
{
    opcodetype opcode;
    CScript::const_iterator pc = script_pub_key.begin();
    if (!script_pub_key.GetOp(pc, opcode) || opcode != RIGOR_MARKER_RETURN) return false;
    std::vector<unsigned char> payload;
    if (!script_pub_key.GetOp(pc, opcode, payload)) return false;
    if (payload.size() != RigorTags::RIGOR_COMMITMENT_PAYLOAD_SIZE) return false;
    if (!std::equal(RigorTags::RIGOR.begin(), RigorTags::RIGOR.end(), payload.begin())) return false;
    commitment_out.res_target_nbits = static_cast<uint32_t>(payload[4]) |
                                      (static_cast<uint32_t>(payload[5]) << 8) |
                                      (static_cast<uint32_t>(payload[6]) << 16) |
                                      (static_cast<uint32_t>(payload[7]) << 24);
    return true;
}

std::optional<RigorCommitment> ExtractRigorDrawTargetCommitment(const CBlock& block)
{
    if (block.vtx.empty() || !block.vtx[0]->IsCoinBase()) return std::nullopt;
    const CTransaction& cb = *block.vtx[0];
    for (const auto& out : cb.vout) {
        RigorCommitment c{};
        if (IsRigorCommitment(out.scriptPubKey, c)) {
            return c;
        }
    }
    return std::nullopt;
}

bool ExtractConvergenceAmount(const CScript& scriptPubKey, CAmount& amount)
{
    // Amount is nValue on the RIGOR_MARKER_CONVERGE output, not encoded in the script.
    (void)scriptPubKey;
    (void)amount;
    return false;
}

uint64_t GetConvergenceAmountFromScript(const CScript& scriptPubKey)
{
    CAmount a;
    if (!ExtractConvergenceAmount(scriptPubKey, a)) return 0;
    return static_cast<uint64_t>(a);
}

CScriptID::CScriptID(const CScript& in) : BaseHash(Hash160(in)) {}
CScriptID::CScriptID(const ScriptHash& in) : BaseHash(static_cast<uint160>(in)) {}

ScriptHash::ScriptHash(const CScript& in) : BaseHash(Hash160(in)) {}
ScriptHash::ScriptHash(const CScriptID& in) : BaseHash(static_cast<uint160>(in)) {}

PKHash::PKHash(const CPubKey& pubkey) : BaseHash(pubkey.GetID()) {}
PKHash::PKHash(const CKeyID& pubkey_id) : BaseHash(pubkey_id) {}

WitnessV0KeyHash::WitnessV0KeyHash(const CPubKey& pubkey) : BaseHash(pubkey.GetEd25519KeyHash()) {}
WitnessV0KeyHash::WitnessV0KeyHash(const PKHash& pubkey_hash) : BaseHash(static_cast<uint160>(pubkey_hash)) {}

CKeyID ToKeyID(const PKHash& key_hash)
{
    return CKeyID{static_cast<uint160>(key_hash)};
}

CKeyID ToKeyID(const WitnessV0KeyHash& key_hash)
{
    return CKeyID{static_cast<uint160>(key_hash)};
}

WitnessV0ScriptHash::WitnessV0ScriptHash(const CScript& in)
{
    CSHA256().Write(in.data(), in.size()).Finalize(begin());
}

std::string GetTxnOutputType(TxoutType t)
{
    switch (t) {
    case TxoutType::NONSTANDARD: return "nonstandard";
    case TxoutType::PUBKEY: return "pubkey";
    case TxoutType::PUBKEYHASH: return "pubkeyhash";
    case TxoutType::SCRIPTHASH: return "scripthash";
    case TxoutType::MULTISIG: return "multisig";
    case TxoutType::NULL_DATA: return "nulldata";
    case TxoutType::WITNESS_V0_KEYHASH: return "witness_v0_keyhash";
    case TxoutType::WITNESS_V0_SCRIPTHASH: return "witness_v0_scripthash";
    case TxoutType::WITNESS_V1_RES: return "witness_v1_res";
    case TxoutType::RES_MINT: return "res_mint";
    case TxoutType::CONVERGE: return "converge";
    case TxoutType::WITNESS_UNKNOWN: return "witness_unknown";
    } // no default case, so the compiler can warn about missing cases
    assert(false);
    return ""; // silence -Wreturn-type (assert may be compiled out)
}

TxoutType Solver(const CScript& scriptPubKey, std::vector<std::vector<unsigned char>>& vSolutionsRet)
{
    return ClassifyRigorTxout(scriptPubKey, vSolutionsRet);
}

bool ExtractDestination(const CScript& scriptPubKey, CTxDestination& addressRet)
{
    std::vector<valtype> vSolutions;
    TxoutType whichType = Solver(scriptPubKey, vSolutions);

    switch (whichType) {
    case TxoutType::WITNESS_V0_KEYHASH: {
        WitnessV0KeyHash hash;
        std::copy(vSolutions[0].begin(), vSolutions[0].end(), hash.begin());
        addressRet = hash;
        return true;
    }
    case TxoutType::WITNESS_V1_RES: {
        if (vSolutions[0].size() != WITNESS_V1_RES_SIZE) {
            return false;
        }
        WitnessV1Res tap;
        std::copy(vSolutions[0].begin(), vSolutions[0].end(), tap.begin());
        addressRet = tap;
        return true;
    }
    case TxoutType::PUBKEY:
    case TxoutType::PUBKEYHASH:
    case TxoutType::SCRIPTHASH:
    case TxoutType::WITNESS_V0_SCRIPTHASH:
    case TxoutType::RES_MINT:
    case TxoutType::CONVERGE:
    case TxoutType::WITNESS_UNKNOWN:
    case TxoutType::MULTISIG:
    case TxoutType::NULL_DATA:
    case TxoutType::NONSTANDARD:
        return false;
    } // no default case, so the compiler can warn about missing cases
    assert(false);
    return false; // silence -Wreturn-type (assert may be compiled out)
}

namespace {
class CScriptVisitor
{
public:
    CScript operator()(const CNoDestination& dest) const
    {
        return CScript();
    }

    CScript operator()(const PKHash&) const
    {
        return CScript();
    }

    CScript operator()(const ScriptHash&) const
    {
        return CScript();
    }

    CScript operator()(const WitnessV0KeyHash& id) const
    {
        return CScript() << RIGOR_PUSH_0 << ToByteVector(id);
    }

    CScript operator()(const WitnessV0ScriptHash&) const
    {
        return CScript();
    }

    CScript operator()(const WitnessV1Res& tap) const
    {
        return CScript() << RIGOR_PUSH_1 << ToByteVector(tap);
    }

    CScript operator()(const WitnessUnknown&) const
    {
        return CScript();
    }
};
} // namespace

CScript GetScriptForDestination(const CTxDestination& dest)
{
    return std::visit(CScriptVisitor(), dest);
}

bool IsValidDestination(const CTxDestination& dest) {
    if (std::holds_alternative<WitnessV0KeyHash>(dest)) return true;
    if (const auto* res = std::get_if<WitnessV1Res>(&dest)) {
 // wallet, RPC, GUI and template construction all reach a
        // destination through here, so it is where a non-spendable RES program is
        // refused. Consensus classification stays byte-only; see
        // IsRigorResNativeWitnessProgram.
        return IsRigorResNativeWitnessProgram({res->begin(), res->end()}) &&
               ed25519::IsCanonicalOnCurvePubkey(Span{res->data(), res->size()});
    }
    return false;
}

// REMOVED: TaprootBuilder, TaprootSpendData, and related Taproot/MAST implementation
// violates RIGOR "Script as Asset" constitution (no Taproot/BIP341 script path complexity)

// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/standard.h>

#include <consensus/amount.h>
#include <crypto/sha256.h>
#include <hash.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <script/rigor_tags.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <util/strencodings.h>

#include <algorithm>
#include <limits>
#include <optional>
#include <string>

typedef std::vector<unsigned char> valtype;

bool IsConvergenceScript(const CScript& scriptPubKey)
{
    int witnessversion;
    std::vector<unsigned char> program;
    if (!scriptPubKey.IsWitnessProgram(witnessversion, program)) return false;
    if (witnessversion != 1 || program.size() != WITNESS_V1_RES_SIZE) return false;
    return std::equal(RigorTags::CVG2.begin(), RigorTags::CVG2.end(), program.begin());
}

bool IsConvergenceV1ScriptPubKey(const CScript& scriptPubKey)
{
    return IsConvergenceScript(scriptPubKey);
}

bool IsResScript(const CScript& scriptPubKey)
{
    int witnessversion;
    std::vector<unsigned char> witnessprogram;
    if (!scriptPubKey.IsWitnessProgram(witnessversion, witnessprogram)) return false;
    if (witnessversion != 1 || witnessprogram.size() != WITNESS_V1_RES_SIZE) return false;
    if (std::equal(RigorTags::CVG2.begin(), RigorTags::CVG2.end(), witnessprogram.begin())) return false;
    return true;
}

// RFC-ADDRESS-001: Validate address-asset binding (witness version matches asset type)
bool ValidateAddressAssetBinding(const CTxDestination& dest, AssetType asset)
{
    const CScript script = GetScriptForDestination(dest);
    int ver; std::vector<unsigned char> prog;
    if (!script.IsWitnessProgram(ver, prog)) return false;
    if (asset == ASSET_ORU && ver != 0) {
        // Witness-v1 CVG2 convergence outputs are still ORU-domain (see AssetTypeFromScriptPubKey).
        if (ver == 1 && IsConvergenceScript(script)) {
            return true;
        }
        return false;
    }
    if (asset == ASSET_RES && ver != 1) return false;
    return true;
}

bool IsV1Program(const CScript& script)
{
    return script.size() == 34 && script[0] == OP_1 && script[1] == 0x20;
}

bool IsRigorCommitment(const CScript& script_pub_key, RigorCommitment& commitment_out)
{
    opcodetype opcode;
    CScript::const_iterator pc = script_pub_key.begin();
    if (!script_pub_key.GetOp(pc, opcode) || opcode != OP_RETURN) return false;
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
    if (!IsConvergenceScript(scriptPubKey)) return false;
    int witnessversion;
    std::vector<unsigned char> program;
    if (!scriptPubKey.IsWitnessProgram(witnessversion, program)) return false;
    assert(static_cast<int>(program.size()) >= 12);
    uint64_t val = 0;
    for (int i = 4; i < 12; ++i) {
        val = (val << 8) | static_cast<uint64_t>(program[i]);
    }
    if (val > static_cast<uint64_t>(std::numeric_limits<CAmount>::max())) return false;
    amount = static_cast<CAmount>(val);
    return MoneyRange(amount);
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

WitnessV0KeyHash::WitnessV0KeyHash(const CPubKey& pubkey) : BaseHash(pubkey.GetID()) {}
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
    case TxoutType::WITNESS_UNKNOWN: return "witness_unknown";
    } // no default case, so the compiler can warn about missing cases
    assert(false);
}

/** Test for "small positive integer" script opcodes - OP_1 through OP_16. */
static constexpr bool IsSmallInteger(opcodetype opcode)
{
    return opcode >= OP_1 && opcode <= OP_16;
}

/** Retrieve a minimally-encoded number in range [min,max] from an (opcode, data) pair,
 *  whether it's OP_n or through a push. */
static std::optional<int> GetScriptNumber(opcodetype opcode, valtype data, int min, int max)
{
    int count;
    if (IsSmallInteger(opcode)) {
        count = CScript::DecodeOP_N(opcode);
    } else if (IsPushdataOp(opcode)) {
        if (!CheckMinimalPush(data, opcode)) return {};
        try {
            count = CScriptNum(data, /* fRequireMinimal = */ true).getint();
        } catch (const scriptnum_error&) {
            return {};
        }
    } else {
        return {};
    }
    if (count < min || count > max) return {};
    return count;
}

static bool MatchPayToPubkey(const CScript& script, valtype& pubkey)
{
    if (script.size() == CPubKey::SIZE + 2 && script[0] == CPubKey::SIZE && script.back() == OP_CHECKSIG) {
        pubkey = valtype(script.begin() + 1, script.begin() + CPubKey::SIZE + 1);
        return CPubKey::ValidSize(pubkey);
    }
    if (script.size() == CPubKey::COMPRESSED_SIZE + 2 && script[0] == CPubKey::COMPRESSED_SIZE && script.back() == OP_CHECKSIG) {
        pubkey = valtype(script.begin() + 1, script.begin() + CPubKey::COMPRESSED_SIZE + 1);
        return CPubKey::ValidSize(pubkey);
    }
    return false;
}

static bool MatchPayToPubkeyHash(const CScript& script, valtype& pubkeyhash)
{
    if (script.size() == 25 && script[0] == OP_DUP && script[1] == OP_HASH160 && script[2] == 20 && script[23] == OP_EQUALVERIFY && script[24] == OP_CHECKSIG) {
        pubkeyhash = valtype(script.begin() + 3, script.begin() + 23);
        return true;
    }
    return false;
}

static bool MatchMultisig(const CScript& script, int& required_sigs, std::vector<valtype>& pubkeys)
{
    opcodetype opcode;
    valtype data;

    CScript::const_iterator it = script.begin();
    if (script.size() < 1 || script.back() != OP_CHECKMULTISIG) return false;

    if (!script.GetOp(it, opcode, data)) return false;
    const auto req_sigs = GetScriptNumber(opcode, data, 1, MAX_PUBKEYS_PER_MULTISIG);
    if (!req_sigs) return false;
    required_sigs = *req_sigs;
    while (script.GetOp(it, opcode, data) && CPubKey::ValidSize(data)) {
        pubkeys.emplace_back(std::move(data));
    }
    const auto num_keys = GetScriptNumber(opcode, data, required_sigs, MAX_PUBKEYS_PER_MULTISIG);
    if (!num_keys) return false;
    if (pubkeys.size() != static_cast<size_t>(*num_keys)) return false;

    return (it + 1 == script.end());
}

/**
 * Bitcoin-compatible templates not covered by ClassifyRigorTxout (bare P2PK, P2SH, CHECKMULTISIG).
 * Chained from Solver() only when ClassifyRigorTxout returns NONSTANDARD — IsStandard() still uses
 * ClassifyRigorTxout alone so bare outputs remain non-standard on-chain.
 */
static TxoutType ClassifyLegacyScriptTemplates(const CScript& scriptPubKey, std::vector<std::vector<unsigned char>>& vSolutionsRet)
{
    vSolutionsRet.clear();

    if (scriptPubKey.IsPayToScriptHash()) {
        vSolutionsRet.emplace_back(scriptPubKey.begin() + 2, scriptPubKey.begin() + 22);
        return TxoutType::SCRIPTHASH;
    }

    valtype data;
    if (MatchPayToPubkey(scriptPubKey, data)) {
        vSolutionsRet.push_back(std::move(data));
        return TxoutType::PUBKEY;
    }
    if (MatchPayToPubkeyHash(scriptPubKey, data)) {
        vSolutionsRet.push_back(std::move(data));
        return TxoutType::PUBKEYHASH;
    }

    int required = 0;
    std::vector<valtype> keys;
    if (MatchMultisig(scriptPubKey, required, keys)) {
        vSolutionsRet.push_back({static_cast<unsigned char>(required)});
        vSolutionsRet.insert(vSolutionsRet.end(), keys.begin(), keys.end());
        vSolutionsRet.push_back({static_cast<unsigned char>(keys.size())});
        return TxoutType::MULTISIG;
    }

    return TxoutType::NONSTANDARD;
}

std::optional<std::pair<int, std::vector<Span<const unsigned char>>>> MatchMultiA(const CScript& script)
{
    std::vector<Span<const unsigned char>> keyspans;

    // Redundant, but very fast and selective test.
    if (script.size() == 0 || script[0] != 32 || script.back() != OP_NUMEQUAL) return {};

    // Parse keys
    auto it = script.begin();
    while (script.end() - it >= 34) {
        if (*it != 32) return {};
        ++it;
        keyspans.emplace_back(&*it, 32);
        it += 32;
        if (*it != (keyspans.size() == 1 ? OP_CHECKSIG : OP_CHECKSIGADD)) return {};
        ++it;
    }
    if (keyspans.size() == 0 || keyspans.size() > MAX_PUBKEYS_PER_MULTI_A) return {};

    // Parse threshold.
    opcodetype opcode;
    std::vector<unsigned char> data;
    if (!script.GetOp(it, opcode, data)) return {};
    if (it == script.end()) return {};
    if (*it != OP_NUMEQUAL) return {};
    ++it;
    if (it != script.end()) return {};
    auto threshold = GetScriptNumber(opcode, data, 1, (int)keyspans.size());
    if (!threshold) return {};

    // Construct result.
    return std::pair{*threshold, std::move(keyspans)};
}

TxoutType ClassifyRigorTxout(const CScript& scriptPubKey, std::vector<std::vector<unsigned char>>& vSolutionsRet)
{
    vSolutionsRet.clear();

    int witnessversion;
    std::vector<unsigned char> witnessprogram;
    if (scriptPubKey.IsWitnessProgram(witnessversion, witnessprogram)) {
        if (witnessversion == 0 && witnessprogram.size() == WITNESS_V0_KEYHASH_SIZE) {
            vSolutionsRet.push_back(std::move(witnessprogram));
            return TxoutType::WITNESS_V0_KEYHASH;
        }
        // 32-byte v0 programs are classified as scripthash (BIP141); VerifyWitnessProgram disambiguates
        // P2WSH (SHA256(witness script)==program) from RIGOR native Ed25519 (program==pubkey, <sig><pubkey>).
        if (witnessversion == 0 && witnessprogram.size() == WITNESS_V0_SCRIPTHASH_SIZE) {
            vSolutionsRet.push_back(std::move(witnessprogram));
            return TxoutType::WITNESS_V0_SCRIPTHASH;
        }
        if (witnessversion == 1 && witnessprogram.size() == WITNESS_V1_RES_SIZE) {
            vSolutionsRet.push_back(std::move(witnessprogram));
            return TxoutType::WITNESS_V1_RES;
        }
        return TxoutType::NONSTANDARD;
    }

    if (scriptPubKey.size() >= 1 && scriptPubKey[0] == OP_RETURN && scriptPubKey.IsPushOnly(scriptPubKey.begin()+1)) {
        return TxoutType::NULL_DATA;
    }

    // Implied script inside native P2WPKH (BIP141): used by ProduceSignature/SignStep on the
    // synthetic witnessscript. Rigor's witness-only classifier must still recognize this template;
    // otherwise signing leaves an empty witness and VerifyWitnessProgram reports a mismatch.
    if (scriptPubKey.size() == 25 &&
        scriptPubKey[0] == OP_DUP &&
        scriptPubKey[1] == OP_HASH160 &&
        scriptPubKey[2] == 20 &&
        scriptPubKey[23] == OP_EQUALVERIFY &&
        scriptPubKey[24] == OP_CHECKSIG) {
        vSolutionsRet.emplace_back(scriptPubKey.begin() + 3, scriptPubKey.begin() + 23);
        return TxoutType::PUBKEYHASH;
    }

    return TxoutType::NONSTANDARD;
}

TxoutType Solver(const CScript& scriptPubKey, std::vector<std::vector<unsigned char>>& vSolutionsRet)
{
    const TxoutType rigor_type = ClassifyRigorTxout(scriptPubKey, vSolutionsRet);
    if (rigor_type != TxoutType::NONSTANDARD) return rigor_type;
    vSolutionsRet.clear();
    return ClassifyLegacyScriptTemplates(scriptPubKey, vSolutionsRet);
}

bool ExtractDestination(const CScript& scriptPubKey, CTxDestination& addressRet)
{
    std::vector<valtype> vSolutions;
    TxoutType whichType = Solver(scriptPubKey, vSolutions);

    switch (whichType) {
    case TxoutType::PUBKEY: {
        CPubKey pubKey(vSolutions[0]);
        if (!pubKey.IsValid())
            return false;

        addressRet = PKHash(pubKey);
        return true;
    }
    case TxoutType::PUBKEYHASH: {
        addressRet = PKHash(uint160(vSolutions[0]));
        return true;
    }
    case TxoutType::SCRIPTHASH: {
        addressRet = ScriptHash(uint160(vSolutions[0]));
        return true;
    }
    case TxoutType::WITNESS_V0_KEYHASH: {
        WitnessV0KeyHash hash;
        std::copy(vSolutions[0].begin(), vSolutions[0].end(), hash.begin());
        addressRet = hash;
        return true;
    }
    case TxoutType::WITNESS_V0_SCRIPTHASH: {
        WitnessV0ScriptHash hash;
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
    case TxoutType::WITNESS_UNKNOWN: {
        WitnessUnknown unk;
        unk.version = vSolutions[0][0];
        std::copy(vSolutions[1].begin(), vSolutions[1].end(), unk.program);
        unk.length = vSolutions[1].size();
        addressRet = unk;
        return true;
    }
    case TxoutType::MULTISIG:
    case TxoutType::NULL_DATA:
    case TxoutType::NONSTANDARD:
        return false;
    } // no default case, so the compiler can warn about missing cases
    assert(false);
}

namespace {
class CScriptVisitor
{
public:
    CScript operator()(const CNoDestination& dest) const
    {
        return CScript();
    }

    CScript operator()(const PKHash& keyID) const
    {
        return CScript() << OP_DUP << OP_HASH160 << ToByteVector(keyID) << OP_EQUALVERIFY << OP_CHECKSIG;
    }

    CScript operator()(const ScriptHash& scriptID) const
    {
        return CScript() << OP_HASH160 << ToByteVector(scriptID) << OP_EQUAL;
    }

    CScript operator()(const WitnessV0KeyHash& id) const
    {
        return CScript() << OP_0 << ToByteVector(id);
    }

    CScript operator()(const WitnessV0ScriptHash& id) const
    {
        return CScript() << OP_0 << ToByteVector(id);
    }

    CScript operator()(const WitnessV1Res& tap) const
    {
        return CScript() << OP_1 << ToByteVector(tap);
    }

    CScript operator()(const WitnessUnknown& id) const
    {
        return CScript() << CScript::EncodeOP_N(id.version) << std::vector<unsigned char>(id.program, id.program + id.length);
    }
};
} // namespace

CScript GetScriptForDestination(const CTxDestination& dest)
{
    return std::visit(CScriptVisitor(), dest);
}

CScript GetScriptForRawPubKey(const CPubKey& pubKey)
{
    return CScript() << std::vector<unsigned char>(pubKey.begin(), pubKey.end()) << OP_CHECKSIG;
}

CScript GetScriptForMultisig(int nRequired, const std::vector<CPubKey>& keys)
{
    CScript script;

    script << nRequired;
    for (const CPubKey& key : keys)
        script << ToByteVector(key);
    script << keys.size() << OP_CHECKMULTISIG;

    return script;
}

bool IsValidDestination(const CTxDestination& dest) {
    return dest.index() != 0;
}

// REMOVED: TaprootBuilder, TaprootSpendData, and related Taproot/MAST implementation
// violates RIGOR "Script as Asset" constitution (no Taproot/BIP341 script path complexity)

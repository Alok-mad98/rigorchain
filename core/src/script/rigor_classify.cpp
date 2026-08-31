// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Script-as-asset: witness/mint/converge/OP_RETURN parsers and ClassifyRigorTxout.
// SignatureHash and Ed25519 stay in predicate_verify.cpp.

#include <script/standard.h>

#include <primitives/transaction.h>
#include <script/predicate_verify.h>
#include <script/rigor_tags.h>
#include <script/script.h>
#include <uint256.h>

#include <algorithm>
#include <cstring>
#include <vector>

typedef std::vector<unsigned char> valtype;

namespace {
/** Canonical RIGOR_MARKER_CONVERGE lock: PUSHBYTES_20 || 20-byte hash || marker. */
constexpr size_t kConvergeScriptSize = 22;
} // namespace

bool IsRigorResNativeWitnessProgram(const std::vector<unsigned char>& program)
{
    if (program.size() != WITNESS_V1_RES_SIZE) return false;
    if (std::memcmp(program.data(), RigorTags::CVG2.data(), RigorTags::CVG2.size()) == 0) return false;
    if (std::memcmp(program.data(), RigorTags::Res2WirePrefix.data(), RigorTags::Res2WirePrefix.size()) == 0) return false;
    return true;
}

bool ParseResMintScript(const CScript& script, int64_t& maturity_height, std::vector<unsigned char>& pubkey_hash)
{
    CScript::const_iterator pc = script.begin();
    opcodetype opcode;
    valtype data;

    if (!script.GetOp(pc, opcode, data)) return false;
    try {
        if (opcode == RIGOR_PUSH_0) {
            maturity_height = 0;
        } else if (opcode >= RIGOR_PUSH_1 && opcode <= RIGOR_PUSH_16) {
            maturity_height = (int)opcode - (int)(RIGOR_PUSH_1 - 1);
        } else if (opcode <= RIGOR_PUSHDATA4) {
            const CScriptNum n(data, /*fRequireMinimal=*/true, 5);
            if (n < 0) return false;
            maturity_height = n.GetInt64();
        } else {
            return false;
        }
    } catch (const scriptnum_error&) {
        return false;
    }

    if (!script.GetOp(pc, opcode, data)) return false;
    if (opcode != 20 || data.size() != 20) return false;
    pubkey_hash = std::move(data);

    if (!script.GetOp(pc, opcode, data)) return false;
    if (opcode != RIGOR_MARKER_RES_MINT) return false;
    if (pc != script.end()) return false;

    // Unique byte representation (RIG-04): accept only the encoding GetResMintScript emits.
    if (pubkey_hash.size() != 20) return false;
    uint160 pkh;
    std::copy(pubkey_hash.begin(), pubkey_hash.end(), pkh.begin());
    return script == GetResMintScript(maturity_height, pkh);
}

bool IsResMintScript(const CScript& script)
{
    int64_t maturity_height = 0;
    valtype pubkey_hash;
    return ParseResMintScript(script, maturity_height, pubkey_hash);
}

CScript GetResMintScript(int64_t maturity_height, const uint160& pubkey_hash)
{
    return CScript() << CScriptNum(maturity_height) << ToByteVector(pubkey_hash) << RIGOR_MARKER_RES_MINT;
}

bool ParseConvergeScript(const CScript& script, uint160& owner_hash160)
{
    if (script.size() != kConvergeScriptSize) return false;
    if (script[0] != 20) return false;
    if (script[21] != RIGOR_MARKER_CONVERGE) return false;
    std::copy(script.begin() + 1, script.begin() + 21, owner_hash160.begin());
    return true;
}

bool IsConvergeScript(const CScript& script)
{
    uint160 hash;
    return ParseConvergeScript(script, hash);
}

CScript GetConvergeScript(const uint160& owner_hash160)
{
    return CScript() << ToByteVector(owner_hash160) << RIGOR_MARKER_CONVERGE;
}

/**
 * Script-as-asset parsers and classifier.
 *
 * Shape recognition (witness program, RES mint, converge lock, OP_RETURN)
 * lives here so classification is a function of script bytes and does not
 * pull SignatureHash / Ed25519 from predicate_verify.cpp.
 */
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
        if (witnessversion == 1 && witnessprogram.size() == WITNESS_V1_RES_SIZE &&
            IsRigorResNativeWitnessProgram(witnessprogram)) {
            vSolutionsRet.push_back(std::move(witnessprogram));
            return TxoutType::WITNESS_V1_RES;
        }
        return TxoutType::NONSTANDARD;
    }

    int64_t mint_maturity = 0;
    std::vector<unsigned char> mint_pkh;
    if (ParseResMintScript(scriptPubKey, mint_maturity, mint_pkh)) {
        vSolutionsRet.push_back(std::move(mint_pkh));
        return TxoutType::RES_MINT;
    }

    uint160 converge_hash;
    if (ParseConvergeScript(scriptPubKey, converge_hash)) {
        vSolutionsRet.emplace_back(converge_hash.begin(), converge_hash.end());
        return TxoutType::CONVERGE;
    }

    if (scriptPubKey.size() >= 1 && scriptPubKey[0] == RIGOR_MARKER_RETURN && scriptPubKey.IsPushOnly(scriptPubKey.begin()+1)) {
        return TxoutType::NULL_DATA;
    }

    // 25-byte P2PKH (DUP HASH160 <20> EQUALVERIFY CHECKSIG) is not a RIGOR
    // consensus type. ProduceSignature signs P2WPKH directly; do not classify
    // an implied P2PKH template here.
    return TxoutType::NONSTANDARD;
}

AssetType AssetTypeFromScriptPubKey(const CScript& scriptPubKey)
{
    std::vector<std::vector<unsigned char>> sols;
    switch (ClassifyRigorTxout(scriptPubKey, sols)) {
    case TxoutType::WITNESS_V0_KEYHASH:
    case TxoutType::CONVERGE:
        return ASSET_ORU;
    case TxoutType::WITNESS_V1_RES:
    case TxoutType::RES_MINT:
        return ASSET_RES;
    case TxoutType::NULL_DATA:
        return ASSET_NONE;
    default:
        return ASSET_INVALID;
    }
}

AssetType ClassifyRigorAsset(const CScript& scriptPubKey)
{
    return AssetTypeFromScriptPubKey(scriptPubKey);
}

bool IsRigorConstructibleScript(const CScript& scriptPubKey)
{
    return AssetTypeFromScriptPubKey(scriptPubKey) != ASSET_INVALID;
}

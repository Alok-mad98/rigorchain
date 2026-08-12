// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/standard.h>

#include <script/interpreter.h>
#include <script/script.h>

/**
 * Lives in libbitcoin_consensus so shared libbitcoinconsensus (-no-undefined on mingw)
 * can link CheckTxOutAssetWitnessBinding without pulling libbitcoin_common.
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

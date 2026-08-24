// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/transaction.h>

#include <consensus/amount.h>
#include <hash.h>
#include <script/interpreter.h>
#include <script/rigor_tags.h>
#include <script/script.h>
#include <serialize.h>
#include <tinyformat.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <version.h>

#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace {
/** Witness v1, 32-byte program that is not a retired CVG2 prefix → RES. Not Bitcoin Taproot. */
bool IsRigorResWitnessProgram(const std::vector<unsigned char>& witnessprogram)
{
    if (witnessprogram.size() != WITNESS_V1_RES_SIZE) return false;
    if (std::equal(RigorTags::CVG2.begin(), RigorTags::CVG2.end(), witnessprogram.begin())) return false;
    if (std::equal(RigorTags::Res2WirePrefix.begin(), RigorTags::Res2WirePrefix.end(), witnessprogram.begin())) return false;
    return true;
}
} // namespace

std::string COutPoint::ToString() const
{
    return strprintf("COutPoint(%s, %u)", hash.ToString().substr(0,10), n);
}

CTxIn::CTxIn(COutPoint prevoutIn, CScript scriptSigIn, uint32_t nSequenceIn)
{
    prevout = prevoutIn;
    scriptSig = scriptSigIn;
    nSequence = nSequenceIn;
}

CTxIn::CTxIn(uint256 hashPrevTx, uint32_t nOut, CScript scriptSigIn, uint32_t nSequenceIn)
{
    prevout = COutPoint(hashPrevTx, nOut);
    scriptSig = scriptSigIn;
    nSequence = nSequenceIn;
}

std::string CTxIn::ToString() const
{
    std::string str;
    str += "CTxIn(";
    str += prevout.ToString();
    if (prevout.IsNull())
        str += strprintf(", coinbase %s", HexStr(scriptSig));
    else
        str += strprintf(", scriptSig=%s", HexStr(scriptSig).substr(0, 24));
    if (nSequence != SEQUENCE_FINAL)
        str += strprintf(", nSequence=%u", nSequence);
    str += ")";
    return str;
}

CTxOut::CTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn)
{
    nValue = nValueIn;
    scriptPubKey = scriptPubKeyIn;
}

std::string CTxOut::ToString() const
{
    return strprintf("CTxOut(nValue=%d.%08d, scriptPubKey=%s)", nValue / COIN, nValue % COIN, HexStr(scriptPubKey).substr(0, 30));
}

CMutableTransaction::CMutableTransaction() : nVersion(CTransaction::CURRENT_VERSION), nLockTime(0) {}
CMutableTransaction::CMutableTransaction(const CTransaction& tx) : vin(tx.vin), vout(tx.vout), nVersion(tx.nVersion), nLockTime(tx.nLockTime) {}

uint256 CMutableTransaction::GetHash() const
{
    return SerializeHash(*this, SER_GETHASH, SERIALIZE_TRANSACTION_NO_WITNESS);
}

uint256 CTransaction::ComputeHash() const
{
    return SerializeHash(*this, SER_GETHASH, SERIALIZE_TRANSACTION_NO_WITNESS);
}

uint256 CTransaction::ComputeWitnessHash() const
{
    if (!HasWitness()) {
        return hash;
    }
    return SerializeHash(*this, SER_GETHASH, 0);
}

CTransaction::CTransaction(const CMutableTransaction& tx) : nVersion(tx.nVersion), vin(tx.vin), vout(tx.vout), nLockTime(tx.nLockTime), hash{ComputeHash()}, m_witness_hash{ComputeWitnessHash()} {}
CTransaction::CTransaction(CMutableTransaction&& tx) : nVersion(tx.nVersion), vin(std::move(tx.vin)), vout(std::move(tx.vout)), nLockTime(tx.nLockTime), hash{ComputeHash()}, m_witness_hash{ComputeWitnessHash()} {}

CAmount CTransaction::GetValueOut() const
{
    CAmount nValueOut = 0;
    for (const auto& tx_out : vout) {
        if (!MoneyRange(tx_out.nValue) || !MoneyRange(nValueOut + tx_out.nValue))
            throw std::runtime_error(std::string(__func__) + ": value out of range");
        nValueOut += tx_out.nValue;
    }
    assert(MoneyRange(nValueOut));
    return nValueOut;
}

AssetType AssetTypeFromScriptPubKey(const CScript& scriptPubKey)
{
    // RFC-ADDRESS-001: Address = Asset Lock Boundary
    // IF witness_version == 0 → asset MUST be ORU
    // IF witness_version == 1 → asset MUST be RES
    // OP_RES_MINT is the RES mint lock. OP_CONVERGE is the ORU-domain burn lock.
    if (IsResMintScript(scriptPubKey)) {
        return ASSET_RES;
    }
    if (IsConvergeScript(scriptPubKey)) {
        return ASSET_ORU;
    }
    int witnessversion;
    std::vector<unsigned char> witnessprogram;
    if (scriptPubKey.IsWitnessProgram(witnessversion, witnessprogram)) {
        if (witnessversion == 0 && (witnessprogram.size() == WITNESS_V0_KEYHASH_SIZE ||
                                    witnessprogram.size() == WITNESS_V0_SCRIPTHASH_SIZE)) {
            return ASSET_ORU;
        }
        if (witnessversion == 1 && witnessprogram.size() == WITNESS_V1_RES_SIZE) {
            if (IsRigorResWitnessProgram(witnessprogram)) {
                return ASSET_RES;
            }
        }
    }
    if (scriptPubKey.size() >= 1 && scriptPubKey[0] == OP_RETURN &&
        scriptPubKey.IsPushOnly(scriptPubKey.begin() + 1)) {
        return ASSET_NONE;
    }
    // Default fail-closed (RIG-06): unrecognized scripts are not an asset.
    return ASSET_NONE;
}

CAmount CTransaction::GetValueOut(AssetType asset) const
{
    CAmount sum{0};
    for (const auto& tx_out : vout) {
        if (AssetTypeFromScriptPubKey(tx_out.scriptPubKey) != asset) continue;
        if (!MoneyRange(tx_out.nValue) || !MoneyRange(sum + tx_out.nValue))
            throw std::runtime_error(std::string(__func__) + ": value out of range");
        sum += tx_out.nValue;
    }
    assert(MoneyRange(sum));
    return sum;
}

unsigned int CTransaction::GetTotalSize() const
{
    return ::GetSerializeSize(*this, PROTOCOL_VERSION);
}

std::string CTransaction::ToString() const
{
    std::string str;
    str += strprintf("CTransaction(hash=%s, ver=%d, vin.size=%u, vout.size=%u, nLockTime=%u)\n",
        GetHash().ToString().substr(0,10),
        nVersion,
        vin.size(),
        vout.size(),
        nLockTime);
    for (const auto& tx_in : vin)
        str += "    " + tx_in.ToString() + "\n";
    for (const auto& tx_in : vin)
        str += "    " + tx_in.scriptWitness.ToString() + "\n";
    for (const auto& tx_out : vout)
        str += "    " + tx_out.ToString() + "\n";
    return str;
}

// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P058 verify-only Transaction Kernel input.
// Not compiled into production (absent from Makefile.am).
// Forbidden fields: CCoinsView, CBlockIndex, ChainstateManager, RPC, Wallet, Miner.

#ifndef BITCOIN_CONSENSUS_TXKERNEL_TRANSACTION_KERNEL_INPUT_H
#define BITCOIN_CONSENSUS_TXKERNEL_TRANSACTION_KERNEL_INPUT_H

#include <script/predicate_verify.h>
#include <script/script.h>
#include <script/standard.h>

#include <cstdint>
#include <cstring>
#include <vector>

/** Explicit Transaction Kernel input. No UTXO / chain / RPC / wallet / miner. */
struct TransactionKernelInput {
    std::vector<unsigned char> scriptPubKey;
    std::vector<unsigned char> scriptSig;
    std::vector<std::vector<unsigned char>> witness;
    std::vector<unsigned char> pubkey;
    std::vector<unsigned char> signature;
    uint32_t flags{0};
    int64_t spend_height{0};
    bool scriptSig_push_only{true};
    bool stack_clean{true};
    bool keyhash_match{false};
    bool crypto_accept{false};
    std::vector<unsigned char> sighash_tag;
    std::vector<unsigned char> chain_id;
    int32_t nVersion{0};
    std::vector<unsigned char> hashPrevouts;
    std::vector<unsigned char> hashSequence;
    std::vector<unsigned char> hashOutputs;
    std::vector<unsigned char> prevout_hash;
    uint32_t prevout_n{0};
    std::vector<unsigned char> spent_script;
    int64_t amount{0};
    uint32_t nSequence{0};
    uint32_t nLockTime{0};
};

static constexpr uint32_t TK_SCRIPT_VERIFY_SIGPUSHONLY = 1u << 5;
static constexpr uint32_t TK_SCRIPT_VERIFY_CLEANSTACK = 1u << 8;
static constexpr uint32_t TK_SCRIPT_VERIFY_WITNESS = 1u << 11;
static constexpr uint32_t TK_SCRIPT_VERIFY_NULLFAIL = 1u << 14;
static constexpr size_t TK_MAX_SCRIPT_ELEMENT_SIZE = 520;

inline bool TransactionKernelWitnessItemsOk(const TransactionKernelInput& in)
{
    for (const auto& item : in.witness) {
        if (item.size() > TK_MAX_SCRIPT_ELEMENT_SIZE) return false;
    }
    return true;
}

inline bool TransactionKernelWitnessTwo(const TransactionKernelInput& in)
{
    if (in.witness.size() != 2) return false;
    return in.witness[0] == in.signature && in.witness[1] == in.pubkey;
}

inline bool TransactionKernelEncodingOk(const TransactionKernelInput& in)
{
    return in.signature.size() == 64 && in.pubkey.size() == 32;
}

/** C++-shaped VerifyRigorPredicate execution boundary over explicit input.
 *  HASH160 and Ed25519 are external: keyhash_match / crypto_accept.
 *  Not a second production parser. Production authority remains VerifyRigorPredicate. */
inline bool VerifyRigorPredicateBoundary(const TransactionKernelInput& in)
{
    if ((in.flags & TK_SCRIPT_VERIFY_SIGPUSHONLY) != 0 && !in.scriptSig_push_only) {
        return false;
    }

    const CScript spk(in.scriptPubKey.begin(), in.scriptPubKey.end());

    if (IsResMintScript(spk)) {
        if (!in.scriptSig_push_only) return false;
        int64_t maturity = 0;
        std::vector<unsigned char> pkh;
        if (!ParseResMintScript(spk, maturity, pkh) || pkh.size() != 20) return false;
        if (maturity < 0) return false;
        if (in.spend_height < maturity) return false;
        if (!TransactionKernelEncodingOk(in)) return false;
        if (!in.keyhash_match) return false;
        if (!in.crypto_accept) return false;
        if ((in.flags & TK_SCRIPT_VERIFY_CLEANSTACK) != 0 && !in.stack_clean) return false;
        if ((in.flags & TK_SCRIPT_VERIFY_WITNESS) != 0 && !in.witness.empty()) return false;
        return true;
    }

    int witnessversion = 0;
    std::vector<unsigned char> program;
    if (!spk.IsWitnessProgram(witnessversion, program)) {
        return false;
    }
    if (witnessversion != 0 && witnessversion != 1) {
        return false;
    }
    if (witnessversion == 1 && program.size() != 32) {
        return false;
    }
    if ((in.flags & TK_SCRIPT_VERIFY_WITNESS) == 0) {
        return false;
    }
    if (!in.scriptSig.empty()) {
        return false;
    }

    if (witnessversion == 0) {
        if (program.size() != WITNESS_V0_KEYHASH_SIZE) return false;
        if (!TransactionKernelWitnessTwo(in)) return false;
        if (!TransactionKernelWitnessItemsOk(in)) return false;
        if (!in.keyhash_match) return false;
        if (in.pubkey.size() != 32) return false;
        if (in.signature.size() != 64) return false;
        return in.crypto_accept;
    }

    if (witnessversion == 1 && program.size() == WITNESS_V1_RES_SIZE &&
        IsRigorResNativeWitnessProgram(program)) {
        if (in.witness.empty()) return false;
        if (!TransactionKernelWitnessItemsOk(in)) return false;
        if (!TransactionKernelWitnessTwo(in)) return false;
        if (in.pubkey.size() != 32) return false;
        if (in.pubkey != program) return false;
        if (in.signature.size() != 64) return false;
        return in.crypto_accept;
    }

    return false;
}

inline void AppendLe32(std::vector<unsigned char>& out, uint32_t v)
{
    out.push_back(static_cast<unsigned char>(v & 0xff));
    out.push_back(static_cast<unsigned char>((v >> 8) & 0xff));
    out.push_back(static_cast<unsigned char>((v >> 16) & 0xff));
    out.push_back(static_cast<unsigned char>((v >> 24) & 0xff));
}

inline void AppendLe64(std::vector<unsigned char>& out, uint64_t v)
{
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<unsigned char>((v >> (8 * i)) & 0xff));
    }
}

inline void AppendCompactSize(std::vector<unsigned char>& out, size_t n)
{
    if (n < 253) {
        out.push_back(static_cast<unsigned char>(n));
        return;
    }
    if (n <= 0xffff) {
        out.push_back(253);
        out.push_back(static_cast<unsigned char>(n & 0xff));
        out.push_back(static_cast<unsigned char>((n >> 8) & 0xff));
        return;
    }
    if (n <= 0xffffffffULL) {
        out.push_back(254);
        AppendLe32(out, static_cast<uint32_t>(n));
        return;
    }
    out.push_back(255);
    AppendLe64(out, static_cast<uint64_t>(n));
}

/** C++ HashWriter order. No SIGHASH_ALL byte. No scriptCode mutation. */
inline std::vector<unsigned char> CppSighashPreimage(const TransactionKernelInput& in)
{
    std::vector<unsigned char> out;
    out.insert(out.end(), in.sighash_tag.begin(), in.sighash_tag.end());
    out.insert(out.end(), in.chain_id.begin(), in.chain_id.end());
    AppendLe32(out, static_cast<uint32_t>(in.nVersion));
    out.insert(out.end(), in.hashPrevouts.begin(), in.hashPrevouts.end());
    out.insert(out.end(), in.hashSequence.begin(), in.hashSequence.end());
    out.insert(out.end(), in.hashOutputs.begin(), in.hashOutputs.end());
    out.insert(out.end(), in.prevout_hash.begin(), in.prevout_hash.end());
    AppendLe32(out, in.prevout_n);
    const std::vector<unsigned char>& script = in.spent_script.empty() ? in.scriptPubKey : in.spent_script;
    AppendCompactSize(out, script.size());
    out.insert(out.end(), script.begin(), script.end());
    AppendLe64(out, static_cast<uint64_t>(in.amount));
    AppendLe32(out, in.nSequence);
    AppendLe32(out, in.nLockTime);
    return out;
}

inline bool CppSighashHasHashtypeByte(const std::vector<unsigned char>& preimage)
{
    (void)preimage;
    return false;
}

#endif // BITCOIN_CONSENSUS_TXKERNEL_TRANSACTION_KERNEL_INPUT_H

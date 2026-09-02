// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P058 Transaction Kernel input + spend-predicate boundary.
//
// A1: VerifyRigorPredicate (the unique production entry in validation.cpp)
// marshals through MakeKernelInput and decides accept/reject with
// VerifyRigorPredicateBoundary. This header is included by
// predicate_verify.cpp; it is still absent from Makefile.am as its own
// translation unit. Forbidden fields: CCoinsView, CBlockIndex,
// ChainstateManager, RPC, Wallet, Miner.

#ifndef BITCOIN_CONSENSUS_TXKERNEL_TRANSACTION_KERNEL_INPUT_H
#define BITCOIN_CONSENSUS_TXKERNEL_TRANSACTION_KERNEL_INPUT_H

#include <script/predicate_verify.h>
#include <script/script.h>
#include <script/standard.h>
#include <script/rigor_tags.h>

#include <crypto/ed25519_provider.h>
#include <hash.h>

#include <algorithm>
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
    bool sighash_fields_present{false};
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

static_assert(TK_SCRIPT_VERIFY_SIGPUSHONLY == SCRIPT_VERIFY_SIGPUSHONLY, "SIGPUSHONLY bit drift");
static_assert(TK_SCRIPT_VERIFY_CLEANSTACK == SCRIPT_VERIFY_CLEANSTACK, "CLEANSTACK bit drift");
static_assert(TK_SCRIPT_VERIFY_WITNESS == SCRIPT_VERIFY_WITNESS, "WITNESS bit drift");
static_assert(TK_SCRIPT_VERIFY_NULLFAIL == SCRIPT_VERIFY_NULLFAIL, "NULLFAIL bit drift");
static_assert(TK_MAX_SCRIPT_ELEMENT_SIZE == MAX_SCRIPT_ELEMENT_SIZE, "push limit drift");

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
    return in.signature.size() == ed25519::SIGNATURE_SIZE && in.pubkey.size() == ed25519::PUBKEY_SIZE;
}

inline void AssignKernelSighashTag(TransactionKernelInput& in, SigVersion v)
{
    const auto& tag = (v == SigVersion::RES_MINT) ? RigorTags::SighashMintV1
                     : (v == SigVersion::WITNESS_V1_RES) ? RigorTags::SighashResV1
                     : RigorTags::SighashOruV1;
    in.sighash_tag.assign(tag.begin(), tag.end());
}

/** Tag C++ MakeKernelInput would write for this script. Unknown / non-native
 *  scripts get empty, not ORU. Same assignment the Coq path_sighash_tag uses. */
inline std::vector<unsigned char> KernelAssignedSighashTag(const CScript& scriptPubKey)
{
    if (IsResMintScript(scriptPubKey)) {
        return std::vector<unsigned char>(RigorTags::SighashMintV1.begin(), RigorTags::SighashMintV1.end());
    }
    int version = 0;
    std::vector<unsigned char> program;
    if (scriptPubKey.IsWitnessProgram(version, program)) {
        if (version == 0) {
            return std::vector<unsigned char>(RigorTags::SighashOruV1.begin(), RigorTags::SighashOruV1.end());
        }
        if (version == 1 && program.size() == WITNESS_V1_RES_SIZE &&
            IsRigorResNativeWitnessProgram(program)) {
            return std::vector<unsigned char>(RigorTags::SighashResV1.begin(), RigorTags::SighashResV1.end());
        }
    }
    return {};
}

inline bool KernelPathSighashTagOk(const TransactionKernelInput& in, const CScript& spk)
{
    return in.sighash_tag == KernelAssignedSighashTag(spk);
}

inline void AssignKernelSighashFromView(TransactionKernelInput& in, const KernelSighashTxView& v)
{
    in.chain_id.assign(v.chain_id.begin(), v.chain_id.end());
    in.nVersion = v.nVersion;
    in.hashPrevouts.assign(v.hashPrevouts.begin(), v.hashPrevouts.end());
    in.hashSequence.assign(v.hashSequence.begin(), v.hashSequence.end());
    in.hashOutputs.assign(v.hashOutputs.begin(), v.hashOutputs.end());
    in.prevout_hash.assign(v.prevout_hash.begin(), v.prevout_hash.end());
    in.prevout_n = v.prevout_n;
    in.amount = v.amount;
    in.nSequence = v.nSequence;
    in.nLockTime = v.nLockTime;
}

/** Path tag + spent script for mint / v0 / native RES only. Tx fields only
 *  when the checker has a transaction. No synthetic zeros: missing tx
 *  context is observable (sighash_fields_present = false). */
inline void FillMakeKernelSighash(TransactionKernelInput& in, const CScript& scriptPubKey,
                                  SigVersion v, const BaseSignatureChecker& checker)
{
    in.spent_script.assign(scriptPubKey.begin(), scriptPubKey.end());
    AssignKernelSighashTag(in, v);
    KernelSighashTxView view;
    if (checker.GetKernelSighashTxView(view)) {
        AssignKernelSighashFromView(in, view);
        in.sighash_fields_present = true;
    } else {
        in.sighash_fields_present = false;
    }
}

inline bool KernelHash160Equals(const std::vector<unsigned char>& pubkey,
                                const std::vector<unsigned char>& expect)
{
    if (expect.size() != WITNESS_V0_KEYHASH_SIZE) return false;
    if (pubkey.size() != ed25519::PUBKEY_SIZE) return false;
    const uint160 h = Hash160(pubkey);
    return std::equal(h.begin(), h.end(), expect.begin());
}

/** Derive the explicit kernel input from the arguments production consensus
 *  actually receives. Oracle fields are computed with the same primitives
 *  VerifyRigorPredicate used before A1: IsPushOnly, EvalPushOnlyScript,
 *  Hash160, and the checker.
 *
 *  crypto_accept is gated on every check the pre-A1 predicate performed
 *  before it reached CheckEd25519Signature, including the outer
 *  SIGPUSHONLY / WITNESS-flag / empty-scriptSig gates. Two reasons, both
 *  load-bearing rather than cosmetic:
 *
 *  - GenericTransactionSignatureChecker::CheckEd25519Signature routes
 *    amount < 0 into HandleMissingData, which is assert(!"Missing data")
 *    under MissingDataBehavior::ASSERT_FAIL. Reaching the checker on a
 *    spend that used to be rejected by script shape alone would convert
 *    a ScriptError into a node abort.
 *  - Ed25519 verification is ~63us. An ungated call spends it on spends
 *    that are already rejected.
 *
 *  test_predicate_boundary_differential counts checker invocations, since
 *  a verdict-only comparison cannot observe either regression. */
inline TransactionKernelInput MakeKernelInput(const CScript& scriptSig,
                                              const CScript& scriptPubKey,
                                              const CScriptWitness& witness,
                                              unsigned int flags,
                                              const BaseSignatureChecker& checker)
{
    TransactionKernelInput in;
    in.scriptPubKey.assign(scriptPubKey.begin(), scriptPubKey.end());
    in.scriptSig.assign(scriptSig.begin(), scriptSig.end());
    in.witness = witness.stack;
    in.flags = flags;
    in.spend_height = checker.GetSpendHeight();
    in.scriptSig_push_only = scriptSig.IsPushOnly();
    in.stack_clean = true;
    in.keyhash_match = false;
    in.crypto_accept = false;

    const bool pushonly_gate_ok =
        (flags & TK_SCRIPT_VERIFY_SIGPUSHONLY) == 0 || in.scriptSig_push_only;

    if (IsResMintScript(scriptPubKey)) {
        std::vector<std::vector<unsigned char>> stack;
        const bool parsed_sig = EvalPushOnlyScript(scriptSig, stack) && stack.size() >= 2;
        if (parsed_sig) {
            in.pubkey = stack[stack.size() - 1];
            in.signature = stack[stack.size() - 2];
            in.stack_clean = (stack.size() == 2);
        }
        int64_t maturity = 0;
        std::vector<unsigned char> pkh;
        const bool parsed_lock = ParseResMintScript(scriptPubKey, maturity, pkh) &&
                                 pkh.size() == WITNESS_V0_KEYHASH_SIZE;
        if (parsed_lock) {
            in.keyhash_match = KernelHash160Equals(in.pubkey, pkh);
        }
        FillMakeKernelSighash(in, scriptPubKey, SigVersion::RES_MINT, checker);
        if (pushonly_gate_ok && in.scriptSig_push_only && parsed_lock && maturity >= 0 &&
            in.spend_height >= maturity && parsed_sig &&
            in.pubkey.size() == ed25519::PUBKEY_SIZE && in.keyhash_match &&
            in.signature.size() == ed25519::SIGNATURE_SIZE) {
            in.crypto_accept = checker.CheckEd25519Signature(
                in.signature, in.pubkey, scriptPubKey, SigVersion::RES_MINT);
        }
        return in;
    }

    int version = 0;
    std::vector<unsigned char> program;
    if (!scriptPubKey.IsWitnessProgram(version, program)) {
        return in;
    }
    if (witness.stack.size() == 2) {
        in.signature = witness.stack[0];
        in.pubkey = witness.stack[1];
    }

    // Outer gates the pre-A1 predicate applied before VerifyWitnessProgram.
    const bool witness_gate_ok = pushonly_gate_ok &&
                                 (version == 0 || version == 1) &&
                                 (version != 1 || program.size() == WITNESS_V1_RES_SIZE) &&
                                 (flags & TK_SCRIPT_VERIFY_WITNESS) != 0 &&
                                 in.scriptSig.empty();

    if (version == 0) {
        FillMakeKernelSighash(in, scriptPubKey, SigVersion::WITNESS_V0, checker);
        in.keyhash_match = KernelHash160Equals(in.pubkey, program);
        if (witness_gate_ok && program.size() == WITNESS_V0_KEYHASH_SIZE &&
            TransactionKernelWitnessTwo(in) && TransactionKernelWitnessItemsOk(in) &&
            in.keyhash_match && in.pubkey.size() == ed25519::PUBKEY_SIZE &&
            in.signature.size() == ed25519::SIGNATURE_SIZE) {
            in.crypto_accept = checker.CheckEd25519Signature(
                in.signature, in.pubkey, scriptPubKey, SigVersion::WITNESS_V0);
        }
        return in;
    }
    if (version == 1 && program.size() == WITNESS_V1_RES_SIZE &&
        IsRigorResNativeWitnessProgram(program)) {
        FillMakeKernelSighash(in, scriptPubKey, SigVersion::WITNESS_V1_RES, checker);
    }
    if (witness_gate_ok && version == 1 && program.size() == WITNESS_V1_RES_SIZE &&
        IsRigorResNativeWitnessProgram(program) && !in.witness.empty() &&
        TransactionKernelWitnessItemsOk(in) && TransactionKernelWitnessTwo(in) &&
        in.pubkey.size() == ed25519::PUBKEY_SIZE && in.pubkey == program &&
        in.signature.size() == ed25519::SIGNATURE_SIZE) {
        in.crypto_accept = checker.CheckEd25519Signature(
            in.signature, in.pubkey, scriptPubKey, SigVersion::WITNESS_V1_RES);
    }
    return in;
}

/** C++-shaped VerifyRigorPredicate execution boundary over explicit input.
 *  HASH160 and Ed25519 are external: keyhash_match / crypto_accept.
 *  Production VerifyRigorPredicate is this function after MakeKernelInput. */
inline bool VerifyRigorPredicateBoundary(const TransactionKernelInput& in)
{
    if ((in.flags & TK_SCRIPT_VERIFY_SIGPUSHONLY) != 0 && !in.scriptSig_push_only) {
        return false;
    }
    // Empty scriptPubKey cannot be mint or witness. Skip CScript allocation.
    if (in.scriptPubKey.empty()) {
        return false;
    }

    const CScript spk(in.scriptPubKey.begin(), in.scriptPubKey.end());

    if (IsResMintScript(spk)) {
        if (!in.scriptSig_push_only) return false;
        int64_t maturity = 0;
        std::vector<unsigned char> pkh;
        if (!ParseResMintScript(spk, maturity, pkh) || pkh.size() != WITNESS_V0_KEYHASH_SIZE) return false;
        if (maturity < 0) return false;
        if (in.spend_height < maturity) return false;
        if (!TransactionKernelEncodingOk(in)) return false;
        if (!in.keyhash_match) return false;
        if (!in.crypto_accept) return false;
        if ((in.flags & TK_SCRIPT_VERIFY_CLEANSTACK) != 0 && !in.stack_clean) return false;
        if ((in.flags & TK_SCRIPT_VERIFY_WITNESS) != 0 && !in.witness.empty()) return false;
        if (!KernelPathSighashTagOk(in, spk)) return false;
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
    if (witnessversion == 1 && program.size() != WITNESS_V1_RES_SIZE) {
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
        if (in.pubkey.size() != ed25519::PUBKEY_SIZE) return false;
        if (in.signature.size() != ed25519::SIGNATURE_SIZE) return false;
        if (!KernelPathSighashTagOk(in, spk)) return false;
        return in.crypto_accept;
    }

    if (witnessversion == 1 && program.size() == WITNESS_V1_RES_SIZE &&
        IsRigorResNativeWitnessProgram(program)) {
        if (in.witness.empty()) return false;
        if (!TransactionKernelWitnessItemsOk(in)) return false;
        if (!TransactionKernelWitnessTwo(in)) return false;
        if (in.pubkey.size() != ed25519::PUBKEY_SIZE) return false;
        if (in.pubkey != program) return false;
        if (in.signature.size() != ed25519::SIGNATURE_SIZE) return false;
        if (!KernelPathSighashTagOk(in, spk)) return false;
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

/** C++ HashWriter order. No SIGHASH_ALL byte. No scriptCode mutation.
 *  Missing tx context is not a tagged message: empty tag, absent fields,
 *  or a 32-byte slot that is not 32 bytes all serialize to empty.
 *  P0-077: reserve the output buffer. Written bytes are unchanged when present. */
inline std::vector<unsigned char> CppSighashPreimage(const TransactionKernelInput& in)
{
    if (in.sighash_tag.empty() || !in.sighash_fields_present) return {};
    if (in.chain_id.size() != 32 || in.hashPrevouts.size() != 32 ||
        in.hashSequence.size() != 32 || in.hashOutputs.size() != 32 ||
        in.prevout_hash.size() != 32) {
        return {};
    }
    const std::vector<unsigned char>& script = in.spent_script;
    std::vector<unsigned char> out;
    out.reserve(in.sighash_tag.size() + in.chain_id.size() + in.hashPrevouts.size() +
                in.hashSequence.size() + in.hashOutputs.size() + in.prevout_hash.size() +
                script.size() + 32);
    out.insert(out.end(), in.sighash_tag.begin(), in.sighash_tag.end());
    out.insert(out.end(), in.chain_id.begin(), in.chain_id.end());
    AppendLe32(out, static_cast<uint32_t>(in.nVersion));
    out.insert(out.end(), in.hashPrevouts.begin(), in.hashPrevouts.end());
    out.insert(out.end(), in.hashSequence.begin(), in.hashSequence.end());
    out.insert(out.end(), in.hashOutputs.begin(), in.hashOutputs.end());
    out.insert(out.end(), in.prevout_hash.begin(), in.prevout_hash.end());
    AppendLe32(out, in.prevout_n);
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

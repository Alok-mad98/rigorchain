// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Test-only transcription of MakeKernelInput.v make_crypto_accept.
// Must stay in lockstep with formal_verification/coq/MakeKernelInput.v.
// tools/lint-flag-bit-lock.py pins the flag bit positions.

#ifndef BITCOIN_TEST_MAKEKERNELINPUT_COQ_ORACLE_H
#define BITCOIN_TEST_MAKEKERNELINPUT_COQ_ORACLE_H

#include <script/rigor_tags.h>
#include <script/standard.h>
#include <test/predicate_boundary_bridge.h>

#include <cstdint>
#include <vector>

struct CoqRawPredicateInput {
    int64_t flags{0};
    bool scriptSig_empty{true};
    bool scriptSig_push_only{true};
    bool is_mint{false};
    int64_t witness_version{0};
    int64_t witness_program_len{0};
    bool is_res_native_v1{false};
    bool encoding_ok{false};
    bool keyhash_match{false};
    bool witness_two{false};
    bool witness_items_ok{true};
    bool mint_parsed{true};
    bool mint_mature{true};
    bool crypto_ok{false};
    bool pubkey_eq_program{true};
    std::vector<unsigned char> sighash_tag;
};

inline std::vector<unsigned char> CoqPathSighashTag(bool is_mint, int64_t ver, bool native)
{
    if (is_mint) {
        return std::vector<unsigned char>(RigorTags::SighashMintV1.begin(), RigorTags::SighashMintV1.end());
    }
    if (ver == 0) {
        return std::vector<unsigned char>(RigorTags::SighashOruV1.begin(), RigorTags::SighashOruV1.end());
    }
    if (ver == 1 && native) {
        return std::vector<unsigned char>(RigorTags::SighashResV1.begin(), RigorTags::SighashResV1.end());
    }
    return {};
}

inline bool CoqRawFlagSigpushonly(int64_t f) { return (f & (int64_t{1} << 5)) != 0; }
inline bool CoqRawFlagWitness(int64_t f) { return (f & (int64_t{1} << 11)) != 0; }

inline bool CoqPushonlyGateOk(const CoqRawPredicateInput& r)
{
    if (CoqRawFlagSigpushonly(r.flags)) return r.scriptSig_push_only;
    return true;
}

inline bool CoqWitnessVersionOk(const CoqRawPredicateInput& r)
{
    if (r.witness_version == 0) return true;
    if (r.witness_version == 1) return r.witness_program_len == 32;
    return false;
}

inline bool CoqWitnessGateOk(const CoqRawPredicateInput& r)
{
    return CoqPushonlyGateOk(r) && CoqWitnessVersionOk(r) &&
           CoqRawFlagWitness(r.flags) && r.scriptSig_empty;
}

inline bool CoqMakeCryptoAccept(const CoqRawPredicateInput& r)
{
    if (r.sighash_tag != CoqPathSighashTag(r.is_mint, r.witness_version, r.is_res_native_v1)) return false;
    if (r.is_mint) {
        return CoqPushonlyGateOk(r) && r.scriptSig_push_only && r.mint_parsed &&
               r.mint_mature && r.encoding_ok && r.keyhash_match && r.crypto_ok;
    }
    const bool v0 = r.witness_version == 0 && r.witness_program_len == 20 && r.keyhash_match;
    const bool v1 = r.witness_version == 1 && r.is_res_native_v1 && r.pubkey_eq_program;
    const bool ver_ok = r.witness_version == 0 ? v0 : (r.witness_version == 1 ? v1 : false);
    return CoqWitnessGateOk(r) && r.encoding_ok && r.witness_two && r.witness_items_ok &&
           ver_ok && r.crypto_ok;
}

inline CoqRawPredicateInput ExtractRaw(const PredicateConcreteInput& c, const TransactionKernelInput& in)
{
    CoqRawPredicateInput r;
    r.flags = static_cast<int64_t>(c.flags);
    r.scriptSig_empty = c.scriptSig.empty();
    r.scriptSig_push_only = c.scriptSig.IsPushOnly();
    r.is_mint = IsResMintScript(c.scriptPubKey);
    int version = 0;
    std::vector<unsigned char> program;
    if (c.scriptPubKey.IsWitnessProgram(version, program)) {
        r.witness_version = version;
        r.witness_program_len = static_cast<int64_t>(program.size());
        r.is_res_native_v1 = version == 1 && IsRigorResNativeWitnessProgram(program);
        r.pubkey_eq_program = in.pubkey == program;
    } else {
        r.witness_version = 2;
        r.witness_program_len = 0;
        r.is_res_native_v1 = false;
        r.pubkey_eq_program = false;
    }
    r.encoding_ok = in.signature.size() == ed25519::SIGNATURE_SIZE &&
                    in.pubkey.size() == ed25519::PUBKEY_SIZE;
    r.keyhash_match = in.keyhash_match;
    r.witness_two = TransactionKernelWitnessTwo(in);
    r.witness_items_ok = TransactionKernelWitnessItemsOk(in);
    r.crypto_ok = c.ed25519_ok;
    r.mint_parsed = false;
    r.mint_mature = false;
    if (r.is_mint) {
        std::vector<std::vector<unsigned char>> stack;
        const bool parsed_sig = EvalPushOnlyScript(c.scriptSig, stack) && stack.size() >= 2;
        int64_t maturity = 0;
        std::vector<unsigned char> pkh;
        const bool parsed_lock = ParseResMintScript(c.scriptPubKey, maturity, pkh) &&
                                 pkh.size() == WITNESS_V0_KEYHASH_SIZE;
        r.mint_parsed = parsed_sig && parsed_lock;
        r.mint_mature = parsed_lock && maturity >= 0 && in.spend_height >= maturity;
    }
    r.sighash_tag = in.sighash_tag;
    return r;
}

#endif // BITCOIN_TEST_MAKEKERNELINPUT_COQ_ORACLE_H

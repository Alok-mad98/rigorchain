// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Reject-path ScriptError walk must not re-enter Ed25519. MakeKernelInput
// already asked; pass that verdict through. If the classifier asks a
// different (sig, pubkey, scriptCode, sigversion) tuple than the first
// question on this call, TupleMismatch() is incremented. That count is a
// diagnostic, not a consensus accept condition.

#ifndef BITCOIN_SCRIPT_FIXED_ED25519_CHECKER_H
#define BITCOIN_SCRIPT_FIXED_ED25519_CHECKER_H

#include <crypto/sha256.h>
#include <script/predicate_verify.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

/** Last VerifyRigorPredicate call's classifier tuple-mismatch count.
 *  Test/fuzz metric only. Not a consensus input. */
inline thread_local int g_rigor_fixed_ed25519_tuple_mismatch = 0;

inline int LastFixedEd25519TupleMismatch()
{
    return g_rigor_fixed_ed25519_tuple_mismatch;
}

class FixedEd25519Checker final : public DeferringSignatureChecker
{
public:
    FixedEd25519Checker(const BaseSignatureChecker& checker, bool verdict)
        : DeferringSignatureChecker(checker), m_verdict(verdict)
    {
        g_rigor_fixed_ed25519_tuple_mismatch = 0;
    }

    ~FixedEd25519Checker()
    {
        g_rigor_fixed_ed25519_tuple_mismatch = m_mismatch;
    }

    bool CheckEd25519Signature(const std::vector<unsigned char>& vchSig,
                               const std::vector<unsigned char>& vchPubKey,
                               const CScript& scriptCode,
                               SigVersion sigversion) const override
    {
        const auto digest = Digest(vchSig, vchPubKey, scriptCode, sigversion);
        if (!m_seen) {
            m_first = digest;
            m_seen = true;
        } else if (digest != m_first) {
            ++m_mismatch;
        }
        return m_verdict;
    }

    int TupleMismatch() const { return m_mismatch; }

private:
    static std::array<unsigned char, CSHA256::OUTPUT_SIZE> Digest(
        const std::vector<unsigned char>& sig,
        const std::vector<unsigned char>& pubkey,
        const CScript& script,
        SigVersion sigversion)
    {
        CSHA256 h;
        const auto write_len = [&](uint64_t n) {
            unsigned char b[8];
            for (int i = 0; i < 8; ++i) {
                b[i] = static_cast<unsigned char>((n >> (8 * i)) & 0xff);
            }
            h.Write(b, 8);
        };
        write_len(sig.size());
        if (!sig.empty()) h.Write(sig.data(), sig.size());
        write_len(pubkey.size());
        if (!pubkey.empty()) h.Write(pubkey.data(), pubkey.size());
        write_len(script.size());
        if (!script.empty()) h.Write(script.data(), script.size());
        const unsigned char ver = static_cast<unsigned char>(sigversion);
        h.Write(&ver, 1);
        std::array<unsigned char, CSHA256::OUTPUT_SIZE> out{};
        h.Finalize(out.data());
        return out;
    }

    bool m_verdict;
    mutable bool m_seen{false};
    mutable std::array<unsigned char, CSHA256::OUTPUT_SIZE> m_first{};
    mutable int m_mismatch{0};
};

#endif // BITCOIN_SCRIPT_FIXED_ED25519_CHECKER_H

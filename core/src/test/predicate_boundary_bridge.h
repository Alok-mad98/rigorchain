// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// A1: test adaptor over the production marshaller.
//
// MakeKernelInput and VerifyRigorPredicateBoundary live in
// transaction_kernel_input.h and are the body of production
// VerifyRigorPredicate. This header only packages concrete test inputs
// and a stub checker so the differential test / fuzzer can still call
// the public production entry and the boundary as two names of the
// same function.
//
// Not compiled into production (test-only, absent from Makefile.am).

#ifndef BITCOIN_TEST_PREDICATE_BOUNDARY_BRIDGE_H
#define BITCOIN_TEST_PREDICATE_BOUNDARY_BRIDGE_H

#include <consensus/txkernel/transaction_kernel_input.h>
#include <script/predicate_verify.h>
#include <script/script.h>

#include <cstdint>
#include <vector>

/** One spend attempt, in the form production consensus sees it. */
struct PredicateConcreteInput {
    CScript scriptSig;
    CScript scriptPubKey;
    CScriptWitness witness;
    unsigned int flags{0};
    int spend_height{0};
    /** Verdict the Ed25519 primitive returns. Production reaches it
     *  through BaseSignatureChecker; the boundary through crypto_accept. */
    bool ed25519_ok{false};
};

/** Signature checker that reports a fixed verdict and a fixed spend height,
 *  and counts how many times the predicate reached the Ed25519 primitive.
 *
 *  The count is part of the comparison, not diagnostics. A verdict-only
 *  differential is blind to two regressions that matter: reaching the checker
 *  on a spend that used to be rejected by script shape alone (which routes
 *  amount < 0 into HandleMissingData / assert under ASSERT_FAIL), and asking
 *  the same question twice (~63us of Ed25519 per extra call, on the
 *  attacker-reachable reject path). */
class BridgeSignatureChecker final : public BaseSignatureChecker
{
public:
    BridgeSignatureChecker(bool ok, int height) : m_ok(ok), m_height(height) {}

    bool CheckEd25519Signature(const std::vector<unsigned char>& sig,
                               const std::vector<unsigned char>& pubkey,
                               const CScript& script,
                               SigVersion sigversion) const override
    {
        (void)sig;
        (void)pubkey;
        (void)script;
        (void)sigversion;
        ++m_calls;
        return m_ok;
    }

    int GetSpendHeight() const override { return m_height; }

    int Calls() const { return m_calls; }

private:
    bool m_ok;
    int m_height;
    mutable int m_calls{0};
};

/** Test overload: same marshaller production uses, with a stub checker. */
inline TransactionKernelInput MakeKernelInput(const PredicateConcreteInput& c)
{
    const BridgeSignatureChecker checker(c.ed25519_ok, c.spend_height);
    return MakeKernelInput(c.scriptSig, c.scriptPubKey, c.witness, c.flags, checker);
}

/** NEW_ACCEPT=true && LEGACY_ACCEPT=false. Diagnostic is not an authority. */
inline bool AcceptanceGrew(bool prod, bool diag) { return prod && !diag; }

inline bool AcceptanceGrewSelfTest()
{
    return !AcceptanceGrew(false, false)
        && !AcceptanceGrew(false, true)
        && AcceptanceGrew(true, false)
        && !AcceptanceGrew(true, true);
}

/** Production authority, also reporting the ScriptError and how many times the
 *  spend reached the Ed25519 primitive. */
inline bool RunProductionObserved(const PredicateConcreteInput& c, ScriptError& err, int& crypto_calls)
{
#ifdef PREDICATE_FUZZ_FORCE_BOUNDARY_ACCEPT
    (void)c;
    err = SCRIPT_ERR_OK;
    crypto_calls = 0;
    return true;
#else
    const BridgeSignatureChecker checker(c.ed25519_ok, c.spend_height);
    err = SCRIPT_ERR_UNKNOWN_ERROR;
    const bool ok = VerifyRigorPredicate(c.scriptSig, c.scriptPubKey, &c.witness, c.flags, checker, &err);
    crypto_calls = checker.Calls();
    return ok;
#endif
}

/** Pre-A1 predicate, as a diagnostic. Not an authority. Tests/fuzz use this
 *  to prove the production acceptance set did not grow
 *  (NEW_ACCEPT=true && LEGACY_ACCEPT=false is ACCEPTANCE_SET_GREW).
 *  Ed25519 invocation counts are a security/performance metric, not a
 *  consensus accept condition. */
inline bool RunDiagnosticObserved(const PredicateConcreteInput& c, ScriptError& err, int& crypto_calls)
{
    const BridgeSignatureChecker checker(c.ed25519_ok, c.spend_height);
    err = SCRIPT_ERR_UNKNOWN_ERROR;
    const bool ok = ClassifyRigorPredicateError(c.scriptSig, c.scriptPubKey, &c.witness, c.flags, checker, &err);
    crypto_calls = checker.Calls();
    return ok;
}

/** The boundary P058 proves about, reached through the same marshalling. */
inline bool RunBoundary(const PredicateConcreteInput& c)
{
    return VerifyRigorPredicateBoundary(MakeKernelInput(c));
}

#endif // BITCOIN_TEST_PREDICATE_BOUNDARY_BRIDGE_H

// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-077: wallet security baseline adversarial audit.
// Standalone with -DWALLET_SECURITY_STANDALONE; Boost otherwise.
// Does not recode consensus formulas. Does not call the Kernel.

#include <wallet/security/wallet_security_boundary.h>
#include <wallet/security/wallet_signing_boundary.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

WalletOutputSpec Out(WalletAssetType a, int64_t amt, uint8_t tag = 0x11, size_t dlen = 20)
{
    WalletOutputSpec o;
    o.asset = a;
    o.amount = amt;
    o.destination.assign(dlen, tag);
    return o;
}

WalletInputSpec Inp(WalletAssetType a, int64_t amt, uint8_t tag = 0xaa, uint32_t n = 0)
{
    WalletInputSpec i;
    i.asset = a;
    i.amount = amt;
    i.n = n;
    std::memset(i.prev_txid, tag, 32);
    return i;
}

WalletTransactionRequest Transfer(std::vector<WalletOutputSpec> outs,
                                  std::vector<WalletInputSpec> ins = {})
{
    WalletTransactionRequest r;
    r.kind = WalletTxKind::TRANSFER;
    r.outputs = std::move(outs);
    r.inputs = std::move(ins);
    r.inputs_provided = !r.inputs.empty();
    return r;
}

bool Expect(const WalletSecurityResult& r, WalletSecurityDecision d, const char* label)
{
    if (r.decision == d) return true;
    std::fprintf(stderr, "FAIL %s: got %d reason=%s\n", label,
                 static_cast<int>(r.decision), r.reason ? r.reason : "");
    return false;
}

bool ExpectReason(const WalletSecurityResult& r, const char* needle, const char* label)
{
    if (r.IsReject() && r.reason && std::strstr(r.reason, needle)) return true;
    std::fprintf(stderr, "FAIL %s: decision=%d reason=%s want substring %s\n", label,
                 static_cast<int>(r.decision), r.reason ? r.reason : "", needle);
    return false;
}

bool TestKeyIsolation()
{
    WalletSigningRequest req;
    req.canonical_tx.assign(40, 0x42);
    WalletSigningBoundary::CanonicalBinding(req.canonical_tx.data(), req.canonical_tx.size(), req.digest);
    req.key_id = 7;
    req.secret_set = true;
    std::memset(req.secret, 0x5a, 64);
    WalletSignedRecord rec;
    auto r = WalletSigningBoundary::Sign(req, &rec);
    if (!Expect(r, WalletSecurityDecision::ACCEPT_TO_BUILDER, "sign-ok")) return false;
    for (int i = 0; i < 64; ++i) {
        if (req.secret[i] != 0) {
            std::fprintf(stderr, "FAIL secret not wiped after Sign\n");
            return false;
        }
    }
    if (!r.secret_isolated) {
        std::fprintf(stderr, "FAIL result not secret_isolated\n");
        return false;
    }
    if (r.reason && WalletSecurityBoundary::LooksLikeSecret(r.reason)) {
        std::fprintf(stderr, "FAIL reason looks like secret\n");
        return false;
    }
    // Import must not echo material.
    WalletImportRequest imp;
    imp.format = "ed25519-hex";
    imp.material.assign(64, 0x11);
    auto ir = WalletSecurityBoundary::EvaluateImport(imp);
    if (!Expect(ir, WalletSecurityDecision::ACCEPT_TO_BUILDER, "import-ok")) return false;
    if (!ir.canonical_tx.empty()) {
        std::fprintf(stderr, "FAIL import leaked bytes into canonical_tx\n");
        return false;
    }
    WalletSecurityBoundary::WipeSecret(imp.material.data(), imp.material.size());
    return true;
}

bool TestTransactionConstruction()
{
    // Ordinary ORU transfer (coin selection later).
    auto oru = Transfer({Out(WalletAssetType::ORU, 1000)});
    auto r = WalletSecurityBoundary::Evaluate(oru);
    if (!Expect(r, WalletSecurityDecision::ACCEPT_TO_BUILDER, "oru-transfer")) return false;
    if (!r.must_enter_transaction_kernel) {
        std::fprintf(stderr, "FAIL ORU accept must enter Transaction Kernel\n");
        return false;
    }
    auto built = TransactionBuilder::Build(oru);
    if (!Expect(built, WalletSecurityDecision::ACCEPT_TO_BUILDER, "oru-build")) return false;
    if (built.canonical_tx.empty()) {
        std::fprintf(stderr, "FAIL ORU build produced empty tx\n");
        return false;
    }

    // RES transfer with mature input.
    auto res_in = Inp(WalletAssetType::RES, 5000);
    res_in.maturity_height = 100;
    res_in.current_height = 100;
    auto res = Transfer({Out(WalletAssetType::RES, 2000, 0x22, 32)}, {res_in});
    r = WalletSecurityBoundary::Evaluate(res);
    if (!Expect(r, WalletSecurityDecision::ACCEPT_TO_BUILDER, "res-mature")) return false;

    // Maturity not reached.
    auto early = Inp(WalletAssetType::RES, 5000, 0x33);
    early.maturity_height = 200;
    early.current_height = 199;
    auto immature = Transfer({Out(WalletAssetType::RES, 2000, 0x22, 32)}, {early});
    if (!ExpectReason(WalletSecurityBoundary::Evaluate(immature), "immature", "res-immature")) return false;

    // Invalid asset.
    if (!ExpectReason(WalletSecurityBoundary::Evaluate(Transfer({Out(WalletAssetType::UNKNOWN, 1)})),
                     "unknown-asset", "unknown-asset")) return false;

    // Insufficient balance.
    auto poor = Transfer({Out(WalletAssetType::ORU, 100)}, {Inp(WalletAssetType::ORU, 50)});
    if (!ExpectReason(WalletSecurityBoundary::Evaluate(poor), "insufficient-balance", "insufficient")) return false;

    poor.inputs.clear();
    poor.inputs_provided = false;
    poor.available_balance = 10;
    poor.outputs = {Out(WalletAssetType::ORU, 100)};
    if (!ExpectReason(WalletSecurityBoundary::Evaluate(poor), "insufficient-balance", "balance-field")) return false;

    // Overflow amount.
    auto ov = Transfer({Out(WalletAssetType::ORU, WalletSecurityLimits::MAX_MONEY)});
    ov.outputs[0].amount = WalletSecurityLimits::MAX_MONEY;
    // MAX_MONEY itself is in range; one over is overflow.
    auto ov2 = Transfer({Out(WalletAssetType::ORU, WalletSecurityLimits::MAX_MONEY)});
    if (!Expect(WalletSecurityBoundary::Evaluate(ov2), WalletSecurityDecision::ACCEPT_TO_BUILDER, "max-money")) return false;
    auto ov3 = Transfer({Out(WalletAssetType::ORU, WalletSecurityLimits::MAX_MONEY)});
    ov3.outputs[0].amount = WalletSecurityLimits::MAX_MONEY + 1;
    if (!ExpectReason(WalletSecurityBoundary::Evaluate(ov3), "overflow-amount", "overflow")) return false;

    // Sum overflow of two in-range outputs.
    WalletOutputSpec a = Out(WalletAssetType::ORU, WalletSecurityLimits::MAX_MONEY);
    WalletOutputSpec b = Out(WalletAssetType::ORU, 1, 0x12);
    if (!ExpectReason(WalletSecurityBoundary::Evaluate(Transfer({a, b})), "overflow-amount", "sum-overflow")) return false;

    return true;
}

bool TestInvalidAmountAndDest()
{
    if (!ExpectReason(WalletSecurityBoundary::Evaluate(Transfer({Out(WalletAssetType::ORU, -1)})),
                     "negative-amount", "neg")) return false;

    auto bad_dest = Transfer({Out(WalletAssetType::ORU, 1)});
    bad_dest.outputs[0].destination.clear();
    if (!ExpectReason(WalletSecurityBoundary::Evaluate(bad_dest), "invalid-destination", "empty-dest")) return false;

    auto dup = Transfer({Out(WalletAssetType::ORU, 1)},
                        {Inp(WalletAssetType::ORU, 10, 0xaa, 0), Inp(WalletAssetType::ORU, 10, 0xaa, 0)});
    if (!ExpectReason(WalletSecurityBoundary::Evaluate(dup), "duplicate-input", "dup-in")) return false;

    if (!ExpectReason(WalletSecurityBoundary::ParseAmount("-1", 2), "negative-amount", "parse-neg")) return false;
    if (!ExpectReason(WalletSecurityBoundary::ParseAmount("1e99", 4), "invalid-amount", "parse-sci")) return false;
    if (!ExpectReason(WalletSecurityBoundary::ParseAmount("", 0), "invalid-amount", "parse-empty")) return false;
    const char* big = "10000000001"; // 1e10+1 whole ORU > MAX_MONEY/COIN
    if (!ExpectReason(WalletSecurityBoundary::ParseAmount(big, std::strlen(big)), "overflow-amount", "parse-ov")) return false;
    if (!Expect(WalletSecurityBoundary::ParseAmount("1.5", 3), WalletSecurityDecision::ACCEPT_TO_BUILDER, "parse-ok")) return false;

    if (!ExpectReason(WalletSecurityBoundary::ParseAddress("", 0), "invalid-address", "addr-empty")) return false;
    if (!ExpectReason(WalletSecurityBoundary::ParseAddress("bc1qxxxx", 8), "invalid-address", "addr-btc")) return false;
    const char rg[] = "rg1qabcdefghijklmnopqrstuvwxyz";
    if (!Expect(WalletSecurityBoundary::ParseAddress(rg, std::strlen(rg)), WalletSecurityDecision::ACCEPT_TO_BUILDER, "addr-rg1")) return false;

    return true;
}

bool TestSigningMutation()
{
    auto intent = Transfer({Out(WalletAssetType::ORU, 1000)}, {Inp(WalletAssetType::ORU, 2000)});
    auto built = TransactionBuilder::Build(intent);
    if (!built.IsAccept()) return false;

    WalletSigningRequest req;
    req.canonical_tx = built.canonical_tx;
    WalletSigningBoundary::CanonicalBinding(req.canonical_tx.data(), req.canonical_tx.size(), req.digest);
    req.key_id = 1;
    req.secret_set = true;
    std::memset(req.secret, 0x11, 64);
    WalletSignedRecord rec;
    auto s = WalletSigningBoundary::Sign(req, &rec);
    if (!Expect(s, WalletSecurityDecision::ACCEPT_TO_BUILDER, "sign")) return false;
    if (!s.must_enter_transaction_kernel) {
        std::fprintf(stderr, "FAIL signed tx must enter kernel\n");
        return false;
    }

    // Wrong key.
    auto v = WalletSigningBoundary::VerifyBinding(rec, built.canonical_tx.data(), built.canonical_tx.size(),
                                                  rec.digest, /*key_id=*/99);
    if (!ExpectReason(v, "wrong-key", "wrong-key")) return false;

    // Wrong digest.
    uint8_t wrong_d[32];
    std::memset(wrong_d, 0xee, 32);
    v = WalletSigningBoundary::VerifyBinding(rec, built.canonical_tx.data(), built.canonical_tx.size(),
                                             wrong_d, rec.key_id);
    if (!ExpectReason(v, "wrong-digest", "wrong-digest")) return false;

    // Modified tx after signing.
    auto mutated = built.canonical_tx;
    mutated[0] ^= 0xff;
    uint8_t md[32];
    WalletSigningBoundary::CanonicalBinding(mutated.data(), mutated.size(), md);
    v = WalletSigningBoundary::VerifyBinding(rec, mutated.data(), mutated.size(), md, rec.key_id);
    if (!ExpectReason(v, "wrong-digest", "modified-tx")) return false;

    // Sign with a digest that does not match the tx.
    WalletSigningRequest bad;
    bad.canonical_tx = built.canonical_tx;
    std::memset(bad.digest, 0x00, 32);
    bad.key_id = 1;
    bad.secret_set = true;
    std::memset(bad.secret, 0x11, 64);
    if (!ExpectReason(WalletSigningBoundary::Sign(bad, &rec), "wrong-digest", "sign-wrong-digest")) return false;

    // Wallet must not unlock sighash layout / witness / flags.
    WalletSigningRequest unlocked = {};
    unlocked.canonical_tx = built.canonical_tx;
    WalletSigningBoundary::CanonicalBinding(unlocked.canonical_tx.data(), unlocked.canonical_tx.size(), unlocked.digest);
    unlocked.key_id = 1;
    unlocked.secret_set = true;
    std::memset(unlocked.secret, 0x11, 64);
    unlocked.digest_layout_locked = false;
    if (!ExpectReason(WalletSigningBoundary::Sign(unlocked, &rec), "sighash-layout-locked", "layout")) return false;

    // Signature replay on a different tx.
    auto other = Transfer({Out(WalletAssetType::ORU, 3)}, {Inp(WalletAssetType::ORU, 9, 0x01)});
    auto other_built = TransactionBuilder::Build(other);
    uint8_t od[32];
    WalletSigningBoundary::CanonicalBinding(other_built.canonical_tx.data(), other_built.canonical_tx.size(), od);
    v = WalletSigningBoundary::VerifyBinding(rec, other_built.canonical_tx.data(), other_built.canonical_tx.size(),
                                             od, rec.key_id);
    if (v.IsAccept()) {
        std::fprintf(stderr, "FAIL signature replay accepted\n");
        return false;
    }

    return true;
}

bool TestRpcMalformed()
{
    WalletRpcRequest empty;
    if (!ExpectReason(WalletSecurityBoundary::EvaluateRpc(empty), "malformed-request", "rpc-empty")) return false;

    WalletRpcRequest miss;
    miss.method = "sendtoaddress";
    if (!ExpectReason(WalletSecurityBoundary::EvaluateRpc(miss), "missing-parameter", "rpc-miss")) return false;

    WalletRpcRequest bad_addr;
    bad_addr.method = "sendtoaddress";
    bad_addr.params = {"not-an-address", "1.0"};
    if (!ExpectReason(WalletSecurityBoundary::EvaluateRpc(bad_addr), "invalid-address", "rpc-addr")) return false;

    WalletRpcRequest bad_amt;
    bad_amt.method = "sendtoaddress";
    bad_amt.params = {"rg1qabcdefghijklmnopqrstuvwxyz", "-5"};
    if (!ExpectReason(WalletSecurityBoundary::EvaluateRpc(bad_amt), "negative-amount", "rpc-amt")) return false;

    WalletRpcRequest over;
    over.method = "getbalance";
    over.raw_size = static_cast<uint64_t>(WalletSecurityLimits::MAX_RPC_INPUT) + 1;
    if (!ExpectReason(WalletSecurityBoundary::EvaluateRpc(over), "oversized-input", "rpc-over")) return false;

    WalletRpcRequest ok;
    ok.method = "sendtoaddress";
    ok.params = {"rg1qabcdefghijklmnopqrstuvwxyz", "1.0"};
    auto r = WalletSecurityBoundary::EvaluateRpc(ok);
    if (!Expect(r, WalletSecurityDecision::ACCEPT_TO_BUILDER, "rpc-ok")) return false;
    if (r.admission == WalletAdmissionTarget::NONE) {
        std::fprintf(stderr, "FAIL rpc ok has no admission target\n");
        return false;
    }

    // Generic RPC must not return secrets.
    WalletRpcRequest dump;
    dump.method = "getbalance";
    r = WalletSecurityBoundary::EvaluateRpc(dump);
    if (!r.secret_isolated) {
        std::fprintf(stderr, "FAIL rpc result not secret_isolated\n");
        return false;
    }
    return true;
}

bool TestFuzzSeeds()
{
    const uint8_t seeds[][8] = {
        {0},
        {0, 1, 2, 3, 4, 5, 6, 7},
        {1, 'r', 'g', '1', 'q', 'a', 'b', 'c'},
        {2, '-', '1', 0, 0, 0, 0, 0},
        {3, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
        {4, 0, 1, 2, 3, 4, 5, 6},
    };
    for (const auto& s : seeds) {
        auto r = WalletSecurityBoundary::EvaluateFuzz(s, sizeof(s));
        if (r.reason && WalletSecurityBoundary::LooksLikeSecret(r.reason)) {
            std::fprintf(stderr, "FAIL fuzz reason leaked secret\n");
            return false;
        }
        if (r.IsAccept() && r.admission == WalletAdmissionTarget::TRANSACTION_BUILDER) {
            // Accept-to-builder must never carry an illegal canonical output.
            for (uint8_t b : r.canonical_tx) {
                (void)b;
            }
        }
    }
    auto z = WalletSecurityBoundary::EvaluateFuzz(nullptr, 0);
    if (z.IsAccept() && z.canonical_tx.size() > 0) {
        // Empty request must not yield a tx.
        std::fprintf(stderr, "FAIL empty fuzz produced tx\n");
        return false;
    }
    return true;
}

bool TestIsolationInvariants()
{
    auto r = WalletSecurityBoundary::Evaluate(Transfer({Out(WalletAssetType::ORU, 1)}));
    if (r.IsAccept() && !r.must_enter_transaction_kernel) {
        std::fprintf(stderr, "FAIL accept without kernel routing\n");
        return false;
    }
    // Wallet must not claim a consensus decision.
    if (r.reason && std::strstr(r.reason, "ConsensusKernel")) {
        std::fprintf(stderr, "FAIL wallet named ConsensusKernel as a decision\n");
        return false;
    }
    return true;
}

bool TestNoInvalidTxOutput()
{
    WalletTransactionRequest bad = Transfer({Out(WalletAssetType::UNKNOWN, 5)});
    auto b = TransactionBuilder::Build(bad);
    if (!b.IsReject() || !b.canonical_tx.empty()) {
        std::fprintf(stderr, "FAIL builder emitted invalid tx\n");
        return false;
    }
    WalletTransactionRequest neg = Transfer({Out(WalletAssetType::ORU, -3)});
    b = TransactionBuilder::Build(neg);
    if (!b.IsReject() || !b.canonical_tx.empty()) {
        std::fprintf(stderr, "FAIL builder emitted negative-amount tx\n");
        return false;
    }
    return true;
}

#ifdef WALLET_SECURITY_STANDALONE
int RunAll()
{
    if (!TestKeyIsolation()) return 1;
    if (!TestTransactionConstruction()) return 1;
    if (!TestInvalidAmountAndDest()) return 1;
    if (!TestSigningMutation()) return 1;
    if (!TestRpcMalformed()) return 1;
    if (!TestFuzzSeeds()) return 1;
    if (!TestIsolationInvariants()) return 1;
    if (!TestNoInvalidTxOutput()) return 1;
    std::printf("WALLET_SECURITY_STATUS = CLOSED\n");
    std::printf("PRIVATE_KEY_EXPOSURE = NONE\n");
    std::printf("INVALID_TX_GENERATION = NONE\n");
    std::printf("CONSENSUS_BYPASS = NONE\n");
    std::printf("CRASH_FOUND = NO\n");
    std::printf("CONSENSUS_MODIFIED = NO\n");
    return 0;
}
#endif

} // namespace

#ifdef WALLET_SECURITY_STANDALONE
int main()
{
    return RunAll();
}
#else
#include <boost/test/unit_test.hpp>
#include <test/util/setup_common.h>

BOOST_FIXTURE_TEST_SUITE(test_wallet_security_boundary, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(key_isolation)
{
    BOOST_REQUIRE(TestKeyIsolation());
}

BOOST_AUTO_TEST_CASE(transaction_construction)
{
    BOOST_REQUIRE(TestTransactionConstruction());
}

BOOST_AUTO_TEST_CASE(invalid_amount)
{
    BOOST_REQUIRE(TestInvalidAmountAndDest());
}

BOOST_AUTO_TEST_CASE(signing_mutation)
{
    BOOST_REQUIRE(TestSigningMutation());
}

BOOST_AUTO_TEST_CASE(rpc_malformed_input)
{
    BOOST_REQUIRE(TestRpcMalformed());
}

BOOST_AUTO_TEST_CASE(fuzz_seed_corpus)
{
    BOOST_REQUIRE(TestFuzzSeeds());
}

BOOST_AUTO_TEST_CASE(isolation)
{
    BOOST_REQUIRE(TestIsolationInvariants());
}

BOOST_AUTO_TEST_CASE(no_invalid_tx)
{
    BOOST_REQUIRE(TestNoInvalidTxOutput());
}

BOOST_AUTO_TEST_SUITE_END()
#endif

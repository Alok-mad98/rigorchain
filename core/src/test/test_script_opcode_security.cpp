// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-078: Script / opcode boundary adversarial audit.
// Standalone with -DSCRIPT_OPCODE_SECURITY_STANDALONE; Boost otherwise.
// Does not recode VerifyRigorPredicate, sighash, or Ed25519.

#include <consensus/txkernel/transaction_kernel_input.h>
#include <script/script.h>
#include <script/standard.h>
#include <uint256.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

int g_fails = 0;

void Fail(const char* msg)
{
    std::fprintf(stderr, "FAIL %s\n", msg);
    ++g_fails;
}

void ExpectReject(const TransactionKernelInput& in, const char* label)
{
    if (VerifyRigorPredicateBoundary(in)) {
        Fail(label);
    }
}

void ExpectAccept(const TransactionKernelInput& in, const char* label)
{
    if (!VerifyRigorPredicateBoundary(in)) {
        Fail(label);
    }
}

std::vector<unsigned char> Bytes(const CScript& s)
{
    return std::vector<unsigned char>(s.begin(), s.end());
}

TransactionKernelInput BaseInput()
{
    TransactionKernelInput in;
    in.flags = TK_SCRIPT_VERIFY_WITNESS | TK_SCRIPT_VERIFY_NULLFAIL
             | TK_SCRIPT_VERIFY_SIGPUSHONLY | TK_SCRIPT_VERIFY_CLEANSTACK;
    in.scriptSig_push_only = true;
    in.stack_clean = true;
    in.signature.assign(64, 0x07);
    in.pubkey.assign(32, 0x09);
    in.sighash_tag = {'R', 'G', 'R', 'O', 'R', 'U', '0', '1'};
    in.chain_id.assign(32, 0x11);
    in.hashPrevouts.assign(32, 0x22);
    in.hashSequence.assign(32, 0x33);
    in.hashOutputs.assign(32, 0x44);
    in.prevout_hash.assign(32, 0x55);
    in.nVersion = 2;
    in.amount = 1000;
    in.prevout_n = 0;
    in.nSequence = 0xffffffff;
    in.nLockTime = 0;
    in.sighash_fields_present = true;
    return in;
}

TransactionKernelInput ValidOru()
{
    const CScript oru = CScript() << RIGOR_PUSH_0
                                  << std::vector<unsigned char>(WITNESS_V0_KEYHASH_SIZE, 0x11);
    TransactionKernelInput in = BaseInput();
    in.scriptPubKey = Bytes(oru);
    in.spent_script = in.scriptPubKey;
    in.witness = {in.signature, in.pubkey};
    in.keyhash_match = true;
    in.crypto_accept = true;
    return in;
}

TransactionKernelInput ValidRes()
{
    const CScript res = CScript() << RIGOR_PUSH_1
                                  << std::vector<unsigned char>(WITNESS_V1_RES_SIZE, 0xaa);
    TransactionKernelInput in = BaseInput();
    in.scriptPubKey = Bytes(res);
    in.spent_script = in.scriptPubKey;
    in.pubkey.assign(32, 0xaa);
    in.witness = {in.signature, in.pubkey};
    in.keyhash_match = true;
    in.crypto_accept = true;
    in.sighash_tag = {'R', 'G', 'R', 'R', 'E', 'S', '0', '1'};
    return in;
}

bool TestEmptyAndRandom()
{
    TransactionKernelInput in = BaseInput();
    in.witness = {in.signature, in.pubkey};
    in.keyhash_match = true;
    in.crypto_accept = true;
    ExpectReject(in, "empty-script");

    in.scriptPubKey = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0};
    ExpectReject(in, "random-bytes");

    in.scriptPubKey.assign(64, 0xff);
    ExpectReject(in, "random-ff");
    return g_fails == 0;
}

bool TestUnknownAndTruncatedOpcode()
{
    TransactionKernelInput in = BaseInput();
    in.witness = {in.signature, in.pubkey};
    in.keyhash_match = true;
    in.crypto_accept = true;

    in.scriptPubKey = {static_cast<unsigned char>(RIGOR_RESERVED_LEGACY_CHECKSIG)};
    ExpectReject(in, "unknown-opcode-checksig");
    in.scriptPubKey = {static_cast<unsigned char>(RIGOR_RESERVED_LEGACY_CHECKMULTISIG)};
    ExpectReject(in, "unknown-opcode-checkmultisig");
    in.scriptPubKey = {static_cast<unsigned char>(RIGOR_RESERVED_LEGACY_CLTV)};
    ExpectReject(in, "unknown-opcode-cltv");
    in.scriptPubKey = {static_cast<unsigned char>(RIGOR_RESERVED_LEGACY_CSV)};
    ExpectReject(in, "unknown-opcode-csv");
    in.scriptPubKey = {static_cast<unsigned char>(RIGOR_RESERVED_LEGACY_DUP)};
    ExpectReject(in, "unknown-opcode-dup");
    in.scriptPubKey = {0xff};
    ExpectReject(in, "unknown-opcode-ff");
    in.scriptPubKey = {0xbe};
    ExpectReject(in, "unknown-opcode-be");

    // Truncated witness v0: claims 20-byte program, supplies 10.
    in.scriptPubKey = {RIGOR_PUSH_0, 20};
    in.scriptPubKey.insert(in.scriptPubKey.end(), 10, 0x11);
    ExpectReject(in, "truncated-opcode");

    in.scriptPubKey = {RIGOR_PUSHDATA1};
    ExpectReject(in, "truncated-pushdata1");
    return g_fails == 0;
}

bool TestOversizedAndInvalidStack()
{
    TransactionKernelInput in = ValidOru();
    in.witness[0].assign(TK_MAX_SCRIPT_ELEMENT_SIZE + 1, 0x07);
    in.signature = in.witness[0];
    ExpectReject(in, "oversized-witness");

    in = ValidOru();
    in.witness = {in.signature};
    ExpectReject(in, "invalid-stack-one-item");

    in = ValidOru();
    in.witness = {in.signature, in.pubkey, {0x01}};
    ExpectReject(in, "invalid-stack-three-items");

    in = ValidOru();
    in.witness = {};
    ExpectReject(in, "invalid-stack-empty-witness");

    in = ValidOru();
    in.scriptSig = {0x01, 0x00};
    ExpectReject(in, "invalid-stack-scriptsig-on-witness");
    return g_fails == 0;
}

bool TestBadSigPubKeySighash()
{
    TransactionKernelInput in = ValidOru();
    in.crypto_accept = false;
    ExpectReject(in, "invalid-signature-crypto");

    in = ValidOru();
    in.signature.assign(63, 0x07);
    in.witness = {in.signature, in.pubkey};
    in.crypto_accept = true;
    ExpectReject(in, "invalid-signature-size");

    in = ValidRes();
    in.pubkey.assign(32, 0x09);
    in.witness = {in.signature, in.pubkey};
    ExpectReject(in, "wrong-pubkey");

    in = ValidOru();
    in.crypto_accept = false;
    TransactionKernelInput mutated = in;
    mutated.amount = in.amount + 1;
    const auto p0 = CppSighashPreimage(in);
    const auto p1 = CppSighashPreimage(mutated);
    if (p0 == p1) Fail("sighash-amount-unchanged");
    ExpectReject(mutated, "wrong-sighash-amount");

    mutated = in;
    mutated.prevout_n = 7;
    if (CppSighashPreimage(in) == CppSighashPreimage(mutated)) Fail("sighash-index-unchanged");
    ExpectReject(mutated, "wrong-sighash-index");

    mutated = in;
    mutated.hashOutputs.assign(32, 0x99);
    if (CppSighashPreimage(in) == CppSighashPreimage(mutated)) Fail("sighash-output-unchanged");
    ExpectReject(mutated, "wrong-sighash-output");

    mutated = in;
    mutated.spent_script.assign(22, 0x51); // different asset / script bytes
    if (CppSighashPreimage(in) == CppSighashPreimage(mutated)) Fail("sighash-asset-unchanged");
    ExpectReject(mutated, "wrong-sighash-asset");

    mutated = in;
    mutated.nVersion = 3;
    if (CppSighashPreimage(in) == CppSighashPreimage(mutated)) Fail("sighash-version-unchanged");
    ExpectReject(mutated, "wrong-sighash-tx-field");

    mutated = in;
    mutated.chain_id.assign(32, 0xee);
    if (CppSighashPreimage(in) == CppSighashPreimage(mutated)) Fail("sighash-chain-unchanged");
    ExpectReject(mutated, "wrong-sighash-chain");

    if (CppSighashHasHashtypeByte(p0)) Fail("sighash-hashtype-byte");
    return g_fails == 0;
}

bool TestBitcoinResidueTemplates()
{
    TransactionKernelInput in = BaseInput();
    in.witness = {in.signature, in.pubkey};
    in.keyhash_match = true;
    in.crypto_accept = true;

    const CScript p2pkh = CScript() << RIGOR_RESERVED_LEGACY_DUP << RIGOR_RESERVED_LEGACY_HASH160
                                    << std::vector<unsigned char>(20, 0x00)
                                    << RIGOR_RESERVED_LEGACY_EQUALVERIFY
                                    << RIGOR_RESERVED_LEGACY_CHECKSIG;
    in.scriptPubKey = Bytes(p2pkh);
    ExpectReject(in, "bitcoin-p2pkh");

    const CScript p2wsh = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(32, 0x00);
    in.scriptPubKey = Bytes(p2wsh);
    ExpectReject(in, "bitcoin-p2wsh");

    const CScript p2sh = CScript() << RIGOR_RESERVED_LEGACY_HASH160
                                   << std::vector<unsigned char>(20, 0x00)
                                   << RIGOR_RESERVED_LEGACY_EQUAL;
    in.scriptPubKey = Bytes(p2sh);
    ExpectReject(in, "bitcoin-p2sh");

    const CScript v2 = CScript() << RIGOR_PUSH_2 << std::vector<unsigned char>(20, 0x00);
    in.scriptPubKey = Bytes(v2);
    ExpectReject(in, "bitcoin-unknown-witness");

    const CScript opret = CScript() << RIGOR_MARKER_RETURN << std::vector<unsigned char>{0x01};
    in.scriptPubKey = Bytes(opret);
    ExpectReject(in, "op-return-unspendable");

    uint160 z;
    z.SetNull();
    in.scriptPubKey = Bytes(GetConvergeScript(z));
    ExpectReject(in, "converge-unspendable");
    return g_fails == 0;
}

bool TestValidStillAccepts()
{
    ExpectAccept(ValidOru(), "valid-oru-must-accept");
    ExpectAccept(ValidRes(), "valid-res-must-accept");
    return g_fails == 0;
}

bool TestNoPartialMutation()
{
    TransactionKernelInput in = ValidOru();
    in.crypto_accept = false;
    const auto before = in;
    if (VerifyRigorPredicateBoundary(in)) Fail("reject-must-not-accept");
    if (in.scriptPubKey != before.scriptPubKey) Fail("partial-spk-mutation");
    if (in.signature != before.signature) Fail("partial-sig-mutation");
    if (in.witness != before.witness) Fail("partial-witness-mutation");
    if (in.amount != before.amount) Fail("partial-amount-mutation");
    return g_fails == 0;
}

#ifdef SCRIPT_OPCODE_SECURITY_STANDALONE
int RunAll()
{
    g_fails = 0;
    (void)TestValidStillAccepts();
    (void)TestEmptyAndRandom();
    (void)TestUnknownAndTruncatedOpcode();
    (void)TestOversizedAndInvalidStack();
    (void)TestBadSigPubKeySighash();
    (void)TestBitcoinResidueTemplates();
    (void)TestNoPartialMutation();
    if (g_fails != 0) {
        std::fprintf(stderr, "test_script_opcode_security failures=%d\n", g_fails);
        return 1;
    }
    std::printf("empty script: REJECT\n");
    std::printf("random bytes: REJECT\n");
    std::printf("unknown opcode: REJECT\n");
    std::printf("truncated opcode: REJECT\n");
    std::printf("oversized witness: REJECT\n");
    std::printf("invalid stack data: REJECT\n");
    std::printf("invalid signature: REJECT\n");
    std::printf("wrong pubkey: REJECT\n");
    std::printf("wrong sighash: REJECT\n");
    std::printf("bitcoin compatibility residue: REJECT\n");
    std::printf("CRASH_FOUND = NO\n");
    return 0;
}
#endif

} // namespace

#ifdef SCRIPT_OPCODE_SECURITY_STANDALONE
int main()
{
    return RunAll();
}
#else
#include <boost/test/unit_test.hpp>
#include <test/util/setup_common.h>

BOOST_FIXTURE_TEST_SUITE(test_script_opcode_security, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(empty_and_random)
{
    g_fails = 0;
    BOOST_REQUIRE(TestEmptyAndRandom());
}

BOOST_AUTO_TEST_CASE(unknown_and_truncated)
{
    g_fails = 0;
    BOOST_REQUIRE(TestUnknownAndTruncatedOpcode());
}

BOOST_AUTO_TEST_CASE(oversized_and_stack)
{
    g_fails = 0;
    BOOST_REQUIRE(TestOversizedAndInvalidStack());
}

BOOST_AUTO_TEST_CASE(sig_pubkey_sighash)
{
    g_fails = 0;
    BOOST_REQUIRE(TestBadSigPubKeySighash());
}

BOOST_AUTO_TEST_CASE(bitcoin_residue)
{
    g_fails = 0;
    BOOST_REQUIRE(TestBitcoinResidueTemplates());
}

BOOST_AUTO_TEST_CASE(valid_accept)
{
    g_fails = 0;
    BOOST_REQUIRE(TestValidStillAccepts());
}

BOOST_AUTO_TEST_CASE(no_partial_mutation)
{
    g_fails = 0;
    BOOST_REQUIRE(TestNoPartialMutation());
}

BOOST_AUTO_TEST_SUITE_END()
#endif

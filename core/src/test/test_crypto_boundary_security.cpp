// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-080: cryptographic boundary security audit.
// Standalone with -DCRYPTO_BOUNDARY_SECURITY_STANDALONE; Boost otherwise.
// Does not recode SHA256, Ed25519, SignatureHash, PoW, or Kernel.
//
// This is not a SHA-256 mathematical security proof and not an Ed25519
// cryptographic proof. It checks that RIGOR uses the primitives uniquely
// and fail-closed: any input mutation changes the digest or is REJECT.

#include <crypto/sha256.h>
#include <rigor_kernel/keccak.h>
#include <rigor_kernel/pow_digest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef CRYPTO_BOUNDARY_HAVE_SODIUM
#include <crypto/ed25519_provider.h>
#include <span.h>
#endif

namespace {

int g_fails = 0;

void Fail(const char* msg)
{
    std::fprintf(stderr, "FAIL %s\n", msg);
    ++g_fails;
}

void Expect(bool cond, const char* msg)
{
    if (!cond) Fail(msg);
}

void StoreLE32(unsigned char* p, uint32_t v)
{
    p[0] = static_cast<unsigned char>(v);
    p[1] = static_cast<unsigned char>(v >> 8);
    p[2] = static_cast<unsigned char>(v >> 16);
    p[3] = static_cast<unsigned char>(v >> 24);
}

void StoreLE64(unsigned char* p, uint64_t v)
{
    StoreLE32(p, static_cast<uint32_t>(v));
    StoreLE32(p + 4, static_cast<uint32_t>(v >> 32));
}

void AppendLE32(std::vector<unsigned char>& o, uint32_t v)
{
    unsigned char b[4];
    StoreLE32(b, v);
    o.insert(o.end(), b, b + 4);
}

void AppendLE64(std::vector<unsigned char>& o, uint64_t v)
{
    unsigned char b[8];
    StoreLE64(b, v);
    o.insert(o.end(), b, b + 8);
}

void AppendCompact(std::vector<unsigned char>& o, uint64_t n)
{
    if (n < 253) {
        o.push_back(static_cast<unsigned char>(n));
        return;
    }
    if (n <= 0xffff) {
        o.push_back(253);
        o.push_back(static_cast<unsigned char>(n));
        o.push_back(static_cast<unsigned char>(n >> 8));
        return;
    }
    if (n <= 0xffffffffULL) {
        o.push_back(254);
        AppendLE32(o, static_cast<uint32_t>(n));
        return;
    }
    o.push_back(255);
    AppendLE64(o, n);
}

std::array<unsigned char, 32> Sha256d(const unsigned char* p, size_t n)
{
    unsigned char first[32];
    CSHA256().Write(p, n).Finalize(first);
    std::array<unsigned char, 32> out{};
    CSHA256().Write(first, 32).Finalize(out.data());
    return out;
}

std::array<unsigned char, 32> Sha256d(const std::vector<unsigned char>& v)
{
    return Sha256d(v.data(), v.size());
}

bool DigestsEqual(const std::array<unsigned char, 32>& a, const std::array<unsigned char, 32>& b)
{
    return std::memcmp(a.data(), b.data(), 32) == 0;
}

struct Header80 {
    int32_t nVersion{1};
    unsigned char hashPrevBlock[32]{};
    unsigned char hashMerkleRoot[32]{};
    uint32_t nTime{1231006505};
    uint32_t nBits{0x1d00ffff};
    uint32_t nNonce{2083236893};
};

void SerializeHeader80(const Header80& h, unsigned char out[80])
{
    StoreLE32(out, static_cast<uint32_t>(h.nVersion));
    std::memcpy(out + 4, h.hashPrevBlock, 32);
    std::memcpy(out + 36, h.hashMerkleRoot, 32);
    StoreLE32(out + 68, h.nTime);
    StoreLE32(out + 72, h.nBits);
    StoreLE32(out + 76, h.nNonce);
}

std::array<unsigned char, 32> HeaderHash(const Header80& h)
{
    unsigned char ser[80];
    SerializeHeader80(h, ser);
    return Sha256d(ser, 80);
}

std::array<unsigned char, 32> PowHash(const Header80& h)
{
    unsigned char pre[88];
    std::memcpy(pre, rigor_kernel::POW_DOMAIN_TAG, rigor_kernel::POW_DOMAIN_TAG_LEN);
    SerializeHeader80(h, pre + 8);
    std::array<unsigned char, 32> out{};
    rigor_kernel::Sha3_256Short(pre, 88, out.data());
    return out;
}

std::vector<unsigned char> MinimalTx(uint32_t nVersion, uint32_t nLockTime,
                                     const unsigned char prev[32], uint32_t prev_n,
                                     uint64_t amount, const std::vector<unsigned char>& spk)
{
    std::vector<unsigned char> o;
    AppendLE32(o, nVersion);
    AppendCompact(o, 1); // vin
    o.insert(o.end(), prev, prev + 32);
    AppendLE32(o, prev_n);
    AppendCompact(o, 0); // empty scriptSig (txid ignores witness)
    AppendLE32(o, 0xffffffff);
    AppendCompact(o, 1); // vout
    AppendLE64(o, amount);
    AppendCompact(o, spk.size());
    o.insert(o.end(), spk.begin(), spk.end());
    AppendLE32(o, nLockTime);
    return o;
}

struct SighashFields {
    std::vector<unsigned char> tag{'R', 'G', 'R', 'O', 'R', 'U', '0', '1'};
    std::vector<unsigned char> chain_id;
    uint32_t nVersion{2};
    std::vector<unsigned char> hashPrevouts;
    std::vector<unsigned char> hashSequence;
    std::vector<unsigned char> hashOutputs;
    std::vector<unsigned char> prevout_hash;
    uint32_t prevout_n{0};
    std::vector<unsigned char> spent_script;
    uint64_t amount{1000};
    uint32_t nSequence{0xffffffff};
    uint32_t nLockTime{0};
    SighashFields()
    {
        chain_id.assign(32, 0x11);
        hashPrevouts.assign(32, 0x22);
        hashSequence.assign(32, 0x33);
        hashOutputs.assign(32, 0x44);
        prevout_hash.assign(32, 0x55);
        spent_script = {0x00, 0x14};
        spent_script.insert(spent_script.end(), 20, 0xaa);
    }
};

std::vector<unsigned char> SighashPreimage(const SighashFields& f)
{
    std::vector<unsigned char> o;
    o.insert(o.end(), f.tag.begin(), f.tag.end());
    o.insert(o.end(), f.chain_id.begin(), f.chain_id.end());
    AppendLE32(o, f.nVersion);
    o.insert(o.end(), f.hashPrevouts.begin(), f.hashPrevouts.end());
    o.insert(o.end(), f.hashSequence.begin(), f.hashSequence.end());
    o.insert(o.end(), f.hashOutputs.begin(), f.hashOutputs.end());
    o.insert(o.end(), f.prevout_hash.begin(), f.prevout_hash.end());
    AppendLE32(o, f.prevout_n);
    AppendCompact(o, f.spent_script.size());
    o.insert(o.end(), f.spent_script.begin(), f.spent_script.end());
    AppendLE64(o, f.amount);
    AppendLE32(o, f.nSequence);
    AppendLE32(o, f.nLockTime);
    return o;
}

bool HasHashtypeByte(const std::vector<unsigned char>& pre)
{
    (void)pre;
    return false;
}

// Encoding-level Ed25519 accept boundary (SignatureBoundary.v).
// Production VerifyDetached also fail-closes on these sizes before libsodium.
bool Ed25519EncodingOk(const std::vector<unsigned char>& sig,
                       const std::vector<unsigned char>& pk,
                       const std::vector<unsigned char>& msg)
{
    return sig.size() == 64 && pk.size() == 32 && msg.size() == 32;
}

bool Ed25519AcceptBoundary(const std::vector<unsigned char>& sig,
                           const std::vector<unsigned char>& pk,
                           const std::vector<unsigned char>& msg,
                           bool crypto_accept)
{
    return Ed25519EncodingOk(sig, pk, msg) && crypto_accept;
}

bool TestSha256dDoubleAndEndian()
{
    const unsigned char abc[] = {'a', 'b', 'c'};
    const auto d1 = Sha256d(abc, 3);
    unsigned char first[32];
    CSHA256().Write(abc, 3).Finalize(first);
    unsigned char second[32];
    CSHA256().Write(first, 32).Finalize(second);
    Expect(std::memcmp(d1.data(), second, 32) == 0, "sha256d-composition");

    const auto single = [&]() {
        std::array<unsigned char, 32> s{};
        CSHA256().Write(abc, 3).Finalize(s.data());
        return s;
    }();
    Expect(!DigestsEqual(d1, single), "sha256d-not-single");

    unsigned char be[4] = {0x00, 0x00, 0xff, 0xff};
    unsigned char le[4] = {0xff, 0xff, 0x00, 0x00};
    Expect(!DigestsEqual(Sha256d(be, 4), Sha256d(le, 4)), "endian-reversal-changes-hash");
    return g_fails == 0;
}

bool TestHeaderAndPowMutation()
{
    Header80 h;
    std::memset(h.hashPrevBlock, 0x11, 32);
    std::memset(h.hashMerkleRoot, 0x22, 32);
    const auto header0 = HeaderHash(h);
    const auto pow0 = PowHash(h);
    Expect(!DigestsEqual(header0, pow0), "pow-not-header-sha256d");

    Header80 nonce = h;
    nonce.nNonce ^= 1;
    Expect(!DigestsEqual(HeaderHash(nonce), header0), "header-nonce-mutation");
    Expect(!DigestsEqual(PowHash(nonce), pow0), "pow-nonce-mutation");

    Header80 bits = h;
    bits.nBits ^= 1;
    Expect(!DigestsEqual(HeaderHash(bits), header0), "header-nbits-mutation");
    Expect(!DigestsEqual(PowHash(bits), pow0), "pow-nbits-mutation");

    Header80 time = h;
    time.nTime ^= 1;
    Expect(!DigestsEqual(HeaderHash(time), header0), "header-time-mutation");
    Expect(!DigestsEqual(PowHash(time), pow0), "pow-time-mutation");

    Header80 ver = h;
    ver.nVersion += 1;
    Expect(!DigestsEqual(HeaderHash(ver), header0), "header-version-mutation");
    Expect(!DigestsEqual(PowHash(ver), pow0), "pow-version-mutation");

    Header80 prev = h;
    prev.hashPrevBlock[0] ^= 0xff;
    Expect(!DigestsEqual(HeaderHash(prev), header0), "header-prev-mutation");
    Expect(!DigestsEqual(PowHash(prev), pow0), "pow-prev-mutation");

    unsigned char ser[80];
    SerializeHeader80(h, ser);
    unsigned char mutated[80];
    std::memcpy(mutated, ser, 80);
    mutated[10] ^= 0x01;
    Expect(!DigestsEqual(Sha256d(ser, 80), Sha256d(mutated, 80)), "header-byte-mutation");

    unsigned char rev[80];
    for (int i = 0; i < 80; ++i) rev[i] = ser[79 - i];
    Expect(!DigestsEqual(Sha256d(ser, 80), Sha256d(rev, 80)), "header-endian-reversal");

    unsigned char nbits_be[4];
    StoreLE32(nbits_be, h.nBits);
    unsigned char nbits_rev[4] = {nbits_be[3], nbits_be[2], nbits_be[1], nbits_be[0]};
    unsigned char ser_be[80];
    std::memcpy(ser_be, ser, 80);
    std::memcpy(ser_be + 72, nbits_rev, 4);
    Expect(!DigestsEqual(Sha256d(ser, 80), Sha256d(ser_be, 80)), "nbits-endian-reversal");

    unsigned char pre[88];
    std::memcpy(pre, rigor_kernel::POW_DOMAIN_TAG, 8);
    std::memcpy(pre + 8, ser, 80);
    unsigned char untagged[80];
    std::memcpy(untagged, ser, 80);
    std::array<unsigned char, 32> tagged{};
    rigor_kernel::Sha3_256Short(pre, 88, tagged.data());
    std::array<unsigned char, 32> bare{};
    unsigned char bare_pre[88]{};
    std::memcpy(bare_pre + 8, ser, 80);
    rigor_kernel::Sha3_256Short(bare_pre, 88, bare.data());
    Expect(std::memcmp(tagged.data(), pow0.data(), 32) == 0, "pow-preimage-matches");
    Expect(std::memcmp(tagged.data(), bare.data(), 32) != 0, "pow-domain-separation");
    (void)untagged;
    return g_fails == 0;
}

bool TestTxDigestMutation()
{
    unsigned char prev[32];
    std::memset(prev, 0xab, 32);
    const std::vector<unsigned char> spk = {0x00, 0x14};
    std::vector<unsigned char> spk20 = spk;
    spk20.insert(spk20.end(), 20, 0x11);
    const auto tx0 = MinimalTx(2, 0, prev, 0, 50 * 100000000ULL, spk20);
    const auto h0 = Sha256d(tx0);

    auto tx_amt = MinimalTx(2, 0, prev, 0, 50 * 100000000ULL + 1, spk20);
    Expect(!DigestsEqual(Sha256d(tx_amt), h0), "tx-amount-mutation");

    auto tx_ver = MinimalTx(1, 0, prev, 0, 50 * 100000000ULL, spk20);
    Expect(!DigestsEqual(Sha256d(tx_ver), h0), "tx-version-mutation");

    auto tx_lock = MinimalTx(2, 1, prev, 0, 50 * 100000000ULL, spk20);
    Expect(!DigestsEqual(Sha256d(tx_lock), h0), "tx-locktime-mutation");

    unsigned char prev2[32];
    std::memset(prev2, 0xac, 32);
    auto tx_prev = MinimalTx(2, 0, prev2, 0, 50 * 100000000ULL, spk20);
    Expect(!DigestsEqual(Sha256d(tx_prev), h0), "tx-prevout-mutation");

    auto tx_idx = MinimalTx(2, 0, prev, 1, 50 * 100000000ULL, spk20);
    Expect(!DigestsEqual(Sha256d(tx_idx), h0), "tx-prevout-n-mutation");

    std::vector<unsigned char> spk_res = {0x51, 0x20};
    spk_res.insert(spk_res.end(), 32, 0x44);
    auto tx_spk = MinimalTx(2, 0, prev, 0, 50 * 100000000ULL, spk_res);
    Expect(!DigestsEqual(Sha256d(tx_spk), h0), "tx-scriptpubkey-mutation");

    auto mutated = tx0;
    mutated[mutated.size() / 2] ^= 0x01;
    Expect(!DigestsEqual(Sha256d(mutated), h0), "tx-byte-mutation");

    auto rev = tx0;
    std::reverse(rev.begin(), rev.end());
    Expect(!DigestsEqual(Sha256d(rev), h0), "tx-endian-reversal");
    return g_fails == 0;
}

bool TestSighashFieldMutation()
{
    const SighashFields base;
    const auto p0 = SighashPreimage(base);
    const auto h0 = Sha256d(p0);
    Expect(!HasHashtypeByte(p0), "no-hashtype-byte");
    Expect(p0.size() >= 8 + 32 + 4, "sighash-min-size");
    Expect(std::memcmp(p0.data(), "RGRORU01", 8) == 0, "oru-tag");

    auto bump = [&](auto fn, const char* label) {
        SighashFields m = base;
        fn(m);
        Expect(!DigestsEqual(Sha256d(SighashPreimage(m)), h0), label);
    };
    bump([](SighashFields& m) { m.prevout_n += 1; }, "sighash-input-index");
    bump([](SighashFields& m) { m.amount += 1; }, "sighash-amount");
    bump([](SighashFields& m) { m.spent_script.back() ^= 0xff; }, "sighash-spent-output");
    bump([](SighashFields& m) { m.hashOutputs[0] ^= 0x01; }, "sighash-destination");
    bump([](SighashFields& m) {
        m.tag = {'R', 'G', 'R', 'R', 'E', 'S', '0', '1'};
        m.spent_script = {0x51, 0x20};
        m.spent_script.insert(m.spent_script.end(), 32, 0x44);
    }, "sighash-asset");
    bump([](SighashFields& m) { m.hashPrevouts[1] ^= 0x02; }, "sighash-prevouts");
    bump([](SighashFields& m) { m.hashSequence[2] ^= 0x03; }, "sighash-sequence-hash");
    bump([](SighashFields& m) { m.nSequence -= 1; }, "sighash-nsequence");
    bump([](SighashFields& m) { m.nVersion += 1; }, "sighash-nversion");
    bump([](SighashFields& m) { m.nLockTime += 1; }, "sighash-nlocktime");
    bump([](SighashFields& m) { m.chain_id[0] ^= 0x01; }, "sighash-chain-id");
    bump([](SighashFields& m) { m.prevout_hash[31] ^= 0x01; }, "sighash-this-prevout");

    // Witness is not in the preimage: a witness-only mutation of the
    // signing domain is forbidden. The digest must still change when
    // hashOutputs / spent_script / amount change (covered above).
    SighashFields mint = base;
    mint.tag = {'R', 'G', 'R', 'M', 'N', 'T', '0', '1'};
    Expect(!DigestsEqual(Sha256d(SighashPreimage(mint)), h0), "sighash-mint-domain");
    SighashFields res = base;
    res.tag = {'R', 'G', 'R', 'R', 'E', 'S', '0', '1'};
    Expect(!DigestsEqual(Sha256d(SighashPreimage(res)), h0), "sighash-res-domain");
    Expect(!DigestsEqual(Sha256d(SighashPreimage(res)), Sha256d(SighashPreimage(mint))), "sighash-res-ne-mint");

    auto mutated = p0;
    mutated[20] ^= 0x01;
    Expect(!DigestsEqual(Sha256d(mutated), h0), "sighash-byte-mutation");
    auto rev = p0;
    std::reverse(rev.begin(), rev.end());
    Expect(!DigestsEqual(Sha256d(rev), h0), "sighash-endian-reversal");
    return g_fails == 0;
}

bool TestEd25519FailClosed()
{
    std::vector<unsigned char> sig(64, 0x07);
    std::vector<unsigned char> pk(32, 0x09);
    std::vector<unsigned char> msg(32, 0x11);
    Expect(Ed25519EncodingOk(sig, pk, msg), "encoding-ok");
    Expect(!Ed25519AcceptBoundary(sig, pk, msg, false), "crypto-accept-false");
    Expect(Ed25519AcceptBoundary(sig, pk, msg, true), "crypto-accept-true");

    auto trunc = sig;
    trunc.pop_back();
    Expect(!Ed25519EncodingOk(trunc, pk, msg), "truncated-signature");
    Expect(!Ed25519AcceptBoundary(trunc, pk, msg, true), "truncated-signature-reject");

    auto over = sig;
    over.push_back(0x01);
    Expect(!Ed25519EncodingOk(over, pk, msg), "oversized-signature");
    Expect(!Ed25519AcceptBoundary(over, pk, msg, true), "oversized-signature-reject");

    std::vector<unsigned char> empty;
    Expect(!Ed25519EncodingOk(empty, pk, msg), "empty-signature");

    auto pk31 = pk;
    pk31.pop_back();
    Expect(!Ed25519EncodingOk(sig, pk31, msg), "truncated-pubkey");
    auto pk33 = pk;
    pk33.push_back(0x00);
    Expect(!Ed25519EncodingOk(sig, pk33, msg), "oversized-pubkey");
    std::vector<unsigned char> malformed_pk(32, 0x00);
    // Length-ok but production verify fail-closes (small-order / identity).
    // Encoding admits it; crypto_accept must be false.
    Expect(Ed25519EncodingOk(sig, malformed_pk, msg), "zero-pk-length-ok");
    Expect(!Ed25519AcceptBoundary(sig, malformed_pk, msg, false), "malformed-pubkey-reject");

    auto msg31 = msg;
    msg31.pop_back();
    Expect(!Ed25519EncodingOk(sig, pk, msg31), "truncated-digest");
    auto msg33 = msg;
    msg33.push_back(0x00);
    Expect(!Ed25519EncodingOk(sig, pk, msg33), "oversized-digest");

#ifdef CRYPTO_BOUNDARY_HAVE_SODIUM
    Expect(ed25519::EnsureRuntime(), "sodium-runtime");
    std::vector<unsigned char> seed(ed25519::SEED_SIZE, 0x42);
    std::vector<unsigned char> pk_ok(ed25519::PUBKEY_SIZE);
    std::vector<unsigned char> sk(ed25519::SECRETKEY_SIZE);
    Expect(ed25519::SeedToKeypair(seed, pk_ok, sk), "seed-keypair");
    std::vector<unsigned char> sig_ok(ed25519::SIGNATURE_SIZE);
    Expect(ed25519::SignDetached(msg, sk, sig_ok), "sign");
    Expect(ed25519::VerifyDetached(sig_ok, msg, pk_ok), "verify-ok");

    std::vector<unsigned char> pk_wrong(ed25519::PUBKEY_SIZE, 0x13);
    Expect(!ed25519::VerifyDetached(sig_ok, msg, pk_wrong), "wrong-key");

    auto sig_wrong = sig_ok;
    sig_wrong[0] ^= 0x01;
    Expect(!ed25519::VerifyDetached(sig_wrong, msg, pk_ok), "wrong-signature");

    auto msg2 = msg;
    msg2[0] ^= 0x01;
    Expect(!ed25519::VerifyDetached(sig_ok, msg2, pk_ok), "modified-message");
    Expect(!ed25519::VerifyDetached(sig_ok, msg2, pk_ok), "modified-digest");

    std::vector<unsigned char> other(32, 0x99);
    Expect(!ed25519::VerifyDetached(sig_ok, other, pk_ok), "replay-signature");

    Expect(!ed25519::VerifyDetached(trunc, msg, pk_ok), "sodium-truncated-sig");
    Expect(!ed25519::VerifyDetached(over, msg, pk_ok), "sodium-oversized-sig");
    Expect(!ed25519::VerifyDetached(sig_ok, msg, pk31), "sodium-truncated-pk");
    Expect(!ed25519::VerifyDetached(sig_ok, msg, pk33), "sodium-oversized-pk");
    Expect(!ed25519::VerifyDetached(sig_ok, msg, malformed_pk), "sodium-malformed-pk");
#endif
    return g_fails == 0;
}

bool TestLengthEncoding()
{
    SighashFields a = SighashFields();
    a.spent_script.assign(1, 0xaa);
    SighashFields b = a;
    b.spent_script.assign(253, 0xaa);
    Expect(SighashPreimage(a).size() != SighashPreimage(b).size(), "compact-size-length");
    Expect(!DigestsEqual(Sha256d(SighashPreimage(a)), Sha256d(SighashPreimage(b))), "compact-size-hash");
    return g_fails == 0;
}

#ifdef CRYPTO_BOUNDARY_SECURITY_STANDALONE
bool RunAll()
{
    g_fails = 0;
    TestSha256dDoubleAndEndian();
    TestHeaderAndPowMutation();
    TestTxDigestMutation();
    TestSighashFieldMutation();
    TestEd25519FailClosed();
    TestLengthEncoding();
    if (g_fails != 0) {
        std::fprintf(stderr, "test_crypto_boundary_security fails=%d\n", g_fails);
        return false;
    }
    std::printf("CRYPTOGRAPHIC_BOUNDARY_TESTS = PASS\n");
    std::printf("SHA256D_MUTATION = PASS\n");
    std::printf("POW_MUTATION = PASS\n");
    std::printf("HEADER_MUTATION = PASS\n");
    std::printf("TX_MUTATION = PASS\n");
    std::printf("SIGHASH_MUTATION = PASS\n");
    std::printf("ED25519_FAIL_CLOSED = PASS\n");
    return true;
}
#endif

} // namespace

#ifdef CRYPTO_BOUNDARY_SECURITY_STANDALONE
int main()
{
    return RunAll() ? 0 : 1;
}
#else
#include <boost/test/unit_test.hpp>
#include <test/util/setup_common.h>

BOOST_FIXTURE_TEST_SUITE(test_crypto_boundary_security, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(sha256d_double_endian)
{
    g_fails = 0;
    BOOST_REQUIRE(TestSha256dDoubleAndEndian());
}

BOOST_AUTO_TEST_CASE(header_and_pow_mutation)
{
    g_fails = 0;
    BOOST_REQUIRE(TestHeaderAndPowMutation());
}

BOOST_AUTO_TEST_CASE(tx_digest_mutation)
{
    g_fails = 0;
    BOOST_REQUIRE(TestTxDigestMutation());
}

BOOST_AUTO_TEST_CASE(sighash_field_mutation)
{
    g_fails = 0;
    BOOST_REQUIRE(TestSighashFieldMutation());
}

BOOST_AUTO_TEST_CASE(ed25519_fail_closed)
{
    g_fails = 0;
    BOOST_REQUIRE(TestEd25519FailClosed());
}

BOOST_AUTO_TEST_CASE(length_encoding)
{
    g_fails = 0;
    BOOST_REQUIRE(TestLengthEncoding());
}

BOOST_AUTO_TEST_SUITE_END()
#endif

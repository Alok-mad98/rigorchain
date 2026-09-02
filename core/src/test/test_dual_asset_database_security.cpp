// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-086B: dual-asset (ORU / RES) CoinsDB security audit harness.
// Standalone with -DDUAL_ASSET_DATABASE_SECURITY_STANDALONE.
// Audit-only replica. Does not modify coins.cpp, txdb.cpp, or consensus.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

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

class CodecError : public std::runtime_error {
public:
    explicit CodecError(const char* m) : std::runtime_error(m) {}
};

class Truncated : public std::runtime_error {
public:
    explicit Truncated(const char* m) : std::runtime_error(m) {}
};

std::vector<uint8_t> WriteVarInt(uint64_t n)
{
    uint8_t tmp[16];
    int len = 0;
    while (true) {
        tmp[len] = static_cast<uint8_t>((n & 0x7F) | (len ? 0x80 : 0x00));
        if (n <= 0x7F) break;
        n = (n >> 7) - 1;
        ++len;
    }
    std::vector<uint8_t> out;
    do {
        out.push_back(tmp[len]);
    } while (len--);
    return out;
}

uint64_t ReadVarInt(const std::vector<uint8_t>& buf, size_t& pos)
{
    uint64_t n = 0;
    while (true) {
        if (pos >= buf.size()) throw Truncated("VARINT unexpected EOF");
        const uint8_t ch = buf[pos++];
        if (n > (0xFFFFFFFFULL >> 7)) throw CodecError("VARINT too large");
        n = (n << 7) | (ch & 0x7F);
        if (ch & 0x80) {
            if (n == 0xFFFFFFFFULL) throw CodecError("VARINT overflow");
            ++n;
        } else {
            return n;
        }
    }
}

uint64_t CompressAmount(uint64_t n)
{
    if (n == 0) return 0;
    int e = 0;
    while ((n % 10) == 0 && e < 9) {
        n /= 10;
        ++e;
    }
    if (e < 9) {
        const int d = static_cast<int>(n % 10);
        n /= 10;
        return 1 + (n * 9 + d - 1) * 10 + e;
    }
    return 1 + (n - 1) * 10 + 9;
}

uint64_t DecompressAmount(uint64_t x)
{
    if (x == 0) return 0;
    --x;
    int e = static_cast<int>(x % 10);
    x /= 10;
    uint64_t n = 0;
    if (e < 9) {
        const int d = static_cast<int>(x % 9) + 1;
        x /= 9;
        n = x * 10 + d;
    } else {
        n = x + 1;
    }
    while (e--) n *= 10;
    return n;
}

constexpr int MAX_SCRIPT_SIZE = 10000;
constexpr unsigned N_SPECIAL = 6;
constexpr uint8_t RIGOR_MARKER_RETURN = 0x6a;
constexpr uint8_t RIGOR_MARKER_RES_MINT = 0xbb;
constexpr uint8_t RIGOR_MARKER_CONVERGE = 0xbd;
constexpr uint8_t DB_COIN = 'C';

enum class Asset { ORU, RES, NONE, INVALID };

struct Coin {
    int64_t n_value{0};
    std::vector<uint8_t> script;
    uint32_t height{0};
    bool coinbase{false};
};

std::vector<uint8_t> CompressScript(const std::vector<uint8_t>& script)
{
    auto out = WriteVarInt(script.size() + N_SPECIAL);
    out.insert(out.end(), script.begin(), script.end());
    return out;
}

std::vector<uint8_t> DecompressScript(const std::vector<uint8_t>& buf, size_t& pos)
{
    const uint64_t nsize_raw = ReadVarInt(buf, pos);
    if (nsize_raw < N_SPECIAL) throw CodecError("unexpected special in dual-asset native path");
    const uint64_t nsize = nsize_raw - N_SPECIAL;
    if (nsize > MAX_SCRIPT_SIZE) {
        if (pos + nsize > buf.size()) throw Truncated("oversized");
        pos += nsize;
        return {RIGOR_MARKER_RETURN};
    }
    if (pos + nsize > buf.size()) throw Truncated("script EOF");
    std::vector<uint8_t> s(buf.begin() + static_cast<long>(pos),
                           buf.begin() + static_cast<long>(pos + nsize));
    pos += nsize;
    return s;
}

std::vector<uint8_t> EncodeCoin(const Coin& c)
{
    auto out = WriteVarInt((static_cast<uint64_t>(c.height) << 1) | (c.coinbase ? 1 : 0));
    const auto amt = WriteVarInt(CompressAmount(static_cast<uint64_t>(c.n_value)));
    out.insert(out.end(), amt.begin(), amt.end());
    const auto sc = CompressScript(c.script);
    out.insert(out.end(), sc.begin(), sc.end());
    return out;
}

Coin DecodeCoin(const std::vector<uint8_t>& buf)
{
    size_t pos = 0;
    const uint64_t code = ReadVarInt(buf, pos);
    Coin c;
    c.height = static_cast<uint32_t>(code >> 1);
    c.coinbase = (code & 1) != 0;
    c.n_value = static_cast<int64_t>(DecompressAmount(ReadVarInt(buf, pos)));
    c.script = DecompressScript(buf, pos);
    return c;
}

std::vector<uint8_t> EncodeKey(const uint8_t txid[32], uint32_t n)
{
    std::vector<uint8_t> out;
    out.push_back(DB_COIN);
    out.insert(out.end(), txid, txid + 32);
    const auto vn = WriteVarInt(n);
    out.insert(out.end(), vn.begin(), vn.end());
    return out;
}

Asset Classify(const std::vector<uint8_t>& s)
{
    if (s.empty()) return Asset::INVALID;
    if (s[0] == RIGOR_MARKER_RETURN) return Asset::NONE;
    if (s.size() == 22 && s[0] == 20 && s[21] == RIGOR_MARKER_CONVERGE) return Asset::ORU;
    if (s.size() == 22 && s[0] == 0x00 && s[1] == 20) return Asset::ORU;
    if (s.size() == 34 && s[0] == 0x51 && s[1] == 32) {
        if (s.size() >= 6 && s[2] == 'C' && s[3] == 'V' && s[4] == 'G' && s[5] == '2') return Asset::INVALID;
        if (s.size() >= 6 && s[2] == 'R' && s[3] == 'E' && s[4] == 'S' && s[5] == '2') return Asset::INVALID;
        return Asset::RES;
    }
    if (s.size() >= 23 && s.back() == RIGOR_MARKER_RES_MINT) return Asset::RES;
    return Asset::INVALID;
}

std::vector<uint8_t> SpkOru()
{
    std::vector<uint8_t> s(22, 0x11);
    s[0] = 0x00;
    s[1] = 20;
    return s;
}

std::vector<uint8_t> SpkRes()
{
    std::vector<uint8_t> s(34, 0x22);
    s[0] = 0x51;
    s[1] = 32;
    return s;
}

std::vector<uint8_t> SpkMint()
{
    std::vector<uint8_t> s;
    s.push_back(0x50 + 5); // OP_5
    s.push_back(20);
    s.insert(s.end(), 20, 0x33);
    s.push_back(RIGOR_MARKER_RES_MINT);
    return s;
}

bool ProductionReadMapsToMissing(const std::vector<uint8_t>& buf)
{
    try {
        (void)DecodeCoin(buf);
        return false;
    } catch (const std::exception&) {
        return true;
    }
}

void TestSharedContainer()
{
    Coin oru;
    oru.n_value = 5000000000;
    oru.height = 1;
    oru.coinbase = true;
    oru.script = SpkOru();
    Coin res;
    res.n_value = 100000000;
    res.height = 1;
    res.coinbase = false;
    res.script = SpkRes();

    const auto oru_raw = EncodeCoin(oru);
    const auto res_raw = EncodeCoin(res);
    const Coin d_oru = DecodeCoin(oru_raw);
    const Coin d_res = DecodeCoin(res_raw);
    Expect(Classify(d_oru.script) == Asset::ORU, "ORU roundtrip classifies ORU");
    Expect(Classify(d_res.script) == Asset::RES, "RES roundtrip classifies RES");
    Expect(Classify(d_oru.script) != Classify(d_res.script), "ORU ≠ RES after decode");
    Expect(d_oru.n_value == oru.n_value && d_res.n_value == res.n_value, "amount preserved");
    Expect(EncodeCoin(d_oru) == oru_raw && EncodeCoin(d_res) == res_raw, "involution");

    uint8_t txid[32];
    std::memset(txid, 0xAB, 32);
    const auto k_oru = EncodeKey(txid, 0);
    const auto k_res = EncodeKey(txid, 1);
    Expect(k_oru[0] == 'C' && k_res[0] == 'C', "shared key type byte");
    Expect(k_oru != k_res, "different vout → different key");
}

void TestMintAndUnknown()
{
    Coin mint;
    mint.n_value = 40000000;
    mint.height = 1;
    mint.coinbase = true;
    mint.script = SpkMint();
    Expect(Classify(mint.script) == Asset::RES, "mint is RES");
    const auto raw = EncodeCoin(mint);
    Expect(Classify(DecodeCoin(raw).script) == Asset::RES, "mint survives ser/deser");

    std::vector<uint8_t> unknown = {0x76, 0xa9, 20};
    unknown.resize(25, 0x01);
    unknown.push_back(0x88);
    unknown.push_back(0xac);
    Expect(Classify(unknown) == Asset::INVALID, "P2PKH is INVALID not ORU");

    std::vector<uint8_t> res2(34, 0);
    res2[0] = 0x51;
    res2[1] = 32;
    res2[2] = 'R';
    res2[3] = 'E';
    res2[4] = 'S';
    res2[5] = '2';
    Expect(Classify(res2) == Asset::INVALID, "retired RES2 is INVALID not RES");
}

void TestMutationReclassifies()
{
    Coin oru;
    oru.n_value = 1;
    oru.height = 4;
    oru.script = SpkOru();
    std::vector<uint8_t> as_res(34, 0);
    as_res[0] = 0x51;
    as_res[1] = 32;
    std::memcpy(as_res.data() + 2, oru.script.data() + 2, 20);
    oru.script = as_res;
    Expect(Classify(oru.script) == Asset::RES, "ORU script mutation → RES");

    Coin res;
    res.n_value = 1;
    res.height = 4;
    res.script = SpkRes();
    std::vector<uint8_t> as_oru(22, 0);
    as_oru[0] = 0x00;
    as_oru[1] = 20;
    std::memcpy(as_oru.data() + 2, res.script.data() + 2, 20);
    res.script = as_oru;
    Expect(Classify(res.script) == Asset::ORU, "RES script mutation → ORU");
}

void TestCorruptionMissing()
{
    Coin c;
    c.n_value = 50;
    c.height = 2;
    c.coinbase = true;
    c.script = SpkOru();
    const auto good = EncodeCoin(c);
    Expect(!ProductionReadMapsToMissing(good), "valid ORU is not missing");
    Expect(ProductionReadMapsToMissing({}), "empty → missing");
    Expect(ProductionReadMapsToMissing({good[0]}), "trunc ORU → missing");

    c.script = SpkRes();
    const auto good_res = EncodeCoin(c);
    Expect(!ProductionReadMapsToMissing(good_res), "valid RES is not missing");
    Expect(ProductionReadMapsToMissing(std::vector<uint8_t>(good_res.begin(), good_res.begin() + 2)),
           "trunc RES → missing");
}

void TestAmountShared()
{
    const uint64_t samples[] = {0, 1, 1000000, 100000000, 5000000000ULL};
    for (uint64_t a : samples) {
        Expect(DecompressAmount(CompressAmount(a)) == a, "shared amount codec");
    }
    // Same integer type: ORU atoms + RES atoms is a number, not a unit.
    const int64_t oru = 5000000000;
    const int64_t res = 40000000;
    Expect(oru + res == 5040000000, "cross-asset atom sum is defined as int64");
}

} // namespace

int main()
{
    TestSharedContainer();
    TestMintAndUnknown();
    TestMutationReclassifies();
    TestCorruptionMissing();
    TestAmountShared();

    if (g_fails) {
        std::fprintf(stderr, "DUAL_ASSET_DATABASE_SECURITY_TESTS=FAIL (%d)\n", g_fails);
        return 1;
    }
    std::printf("DUAL_ASSET_DATABASE_SECURITY_TESTS=PASS\n");
    std::printf("ORU_RES_STORAGE_MODEL=SHARED_COIN_MODEL\n");
    std::printf("SCRIPT_MUTATION_RECLASSIFIES_ASSET=TRUE\n");
    std::printf("UNKNOWN_ASSET_FAIL_CLOSED=TRUE\n");
    std::printf("DESERIALIZE_ERROR_MAPS_TO_MISSING=TRUE\n");
    return 0;
}

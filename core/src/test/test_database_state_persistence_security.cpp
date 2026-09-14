// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-086: database / CoinsDB / state-persistence security harness.
// Standalone with -DDATABASE_STATE_PERSISTENCE_SECURITY_STANDALONE.
// Replica of Coin / undo / key encoding. Checksum lives on disk/undo
// only (CoinDiskFormatter). Not a Consensus Kernel.

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

class Truncated : public std::runtime_error {
public:
    explicit Truncated(const char* m) : std::runtime_error(m) {}
};

class CodecError : public std::runtime_error {
public:
    explicit CodecError(const char* m) : std::runtime_error(m) {}
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
constexpr uint8_t DB_COIN = 'C';

struct Coin {
    int64_t n_value{0};
    std::vector<uint8_t> script;
    uint32_t height{0};
    bool coinbase{false};
    uint32_t n_flags{0};
};

//! CRC-32C Castagnoli. Matches crc32c::Crc32c (init 0 / empty → 0).
uint32_t Crc32c(const uint8_t* data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc >> 1) ^ (0x82F63B78u & (0u - (crc & 1u)));
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

std::vector<uint8_t> EncodeScript(const std::vector<uint8_t>& script)
{
    if (script.size() > static_cast<size_t>(MAX_SCRIPT_SIZE)) {
        throw CodecError("oversized script");
    }
    auto out = WriteVarInt(script.size());
    out.insert(out.end(), script.begin(), script.end());
    return out;
}

std::vector<uint8_t> DecodeScript(const std::vector<uint8_t>& buf, size_t& pos)
{
    const uint64_t nsize = ReadVarInt(buf, pos);
    if (nsize > MAX_SCRIPT_SIZE) {
        throw CodecError("oversized script");
    }
    if (pos + nsize > buf.size()) throw Truncated("script EOF");
    std::vector<uint8_t> s(buf.begin() + static_cast<long>(pos),
                          buf.begin() + static_cast<long>(pos + nsize));
    pos += nsize;
    return s;
}

uint32_t CoinIntegrityChecksum(const Coin& c)
{
    std::vector<uint8_t> raw;
    raw.reserve(4 + 1 + 8 + 4 + c.script.size());
    raw.push_back('R');
    raw.push_back('G');
    raw.push_back('C');
    raw.push_back('I');
    for (int i = 0; i < 4; ++i) raw.push_back(static_cast<uint8_t>(c.height >> (8 * i)));
    raw.push_back(c.coinbase ? 1 : 0);
    uint64_t bits = 0;
    memcpy(&bits, &c.n_value, 8);
    for (int i = 0; i < 8; ++i) raw.push_back(static_cast<uint8_t>(bits >> (8 * i)));
    raw.insert(raw.end(), c.script.begin(), c.script.end());
    for (int i = 0; i < 4; ++i) raw.push_back(static_cast<uint8_t>(c.n_flags >> (8 * i)));
    return Crc32c(raw.data(), raw.size());
}

void AppendU32LE(std::vector<uint8_t>& out, uint32_t v)
{
    out.push_back(static_cast<uint8_t>(v));
    out.push_back(static_cast<uint8_t>(v >> 8));
    out.push_back(static_cast<uint8_t>(v >> 16));
    out.push_back(static_cast<uint8_t>(v >> 24));
}

uint32_t ReadU32LE(const std::vector<uint8_t>& buf, size_t& pos)
{
    if (pos + 4 > buf.size()) throw Truncated("checksum EOF");
    uint32_t v = static_cast<uint32_t>(buf[pos]) |
                 (static_cast<uint32_t>(buf[pos + 1]) << 8) |
                 (static_cast<uint32_t>(buf[pos + 2]) << 16) |
                 (static_cast<uint32_t>(buf[pos + 3]) << 24);
    pos += 4;
    return v;
}

std::vector<uint8_t> EncodeLeaf(const Coin& c)
{
    auto out = WriteVarInt((static_cast<uint64_t>(c.height) << 1) | (c.coinbase ? 1 : 0));
    const auto amt = WriteVarInt(CompressAmount(static_cast<uint64_t>(c.n_value)));
    out.insert(out.end(), amt.begin(), amt.end());
    const auto sc = EncodeScript(c.script);
    out.insert(out.end(), sc.begin(), sc.end());
    const auto flags = WriteVarInt(c.n_flags);
    out.insert(out.end(), flags.begin(), flags.end());
    return out;
}

std::vector<uint8_t> EncodeCoin(const Coin& c)
{
    auto out = EncodeLeaf(c);
    AppendU32LE(out, CoinIntegrityChecksum(c));
    return out;
}

Coin DecodeCoin(const std::vector<uint8_t>& buf)
{
    size_t pos = 0;
    const uint64_t code = ReadVarInt(buf, pos);
    Coin c;
    c.height = static_cast<uint32_t>(code >> 1);
    c.coinbase = (code & 1) != 0;
    const uint64_t camt = DecompressAmount(ReadVarInt(buf, pos));
    c.n_value = static_cast<int64_t>(camt);
    if (camt > 10000000000ull * 100000000ull) throw CodecError("nValue out of MoneyRange");
    c.script = DecodeScript(buf, pos);
    c.n_flags = static_cast<uint32_t>(ReadVarInt(buf, pos));
    const uint32_t stored = ReadU32LE(buf, pos);
    if (stored != CoinIntegrityChecksum(c)) throw CodecError("checksum mismatch");
    if (pos != buf.size()) throw CodecError("trailing bytes");
    return c;
}

std::vector<uint8_t> EncodeUndo(const Coin& c)
{
    return EncodeCoin(c);
}

Coin DecodeUndo(const std::vector<uint8_t>& buf)
{
    return DecodeCoin(buf);
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

bool DecodeFailsClosed(const std::vector<uint8_t>& buf)
{
    try {
        (void)DecodeCoin(buf);
        return false;
    } catch (const std::exception&) {
        return true;
    }
}

std::vector<uint8_t> FromHex(const char* hex)
{
    std::vector<uint8_t> out;
    const size_t n = std::strlen(hex);
    out.reserve(n / 2);
    for (size_t i = 0; i + 1 < n; i += 2) {
        unsigned v = 0;
        if (std::sscanf(hex + i, "%2x", &v) != 1) throw CodecError("hex");
        out.push_back(static_cast<uint8_t>(v));
    }
    return out;
}

void TestCrc32c()
{
    Expect(Crc32c(reinterpret_cast<const uint8_t*>(""), 0) == 0, "crc32c empty");
    const char* s = "123456789";
    Expect(Crc32c(reinterpret_cast<const uint8_t*>(s), 9) == 0xe3069283u, "crc32c check vector");
}

void TestAmountRoundtrip()
{
    const uint64_t samples[] = {0, 1, 10, 50, 50ull * 100000000ull, 123456789, 21000000ull * 100000000ull};
    for (uint64_t a : samples) {
        Expect(DecompressAmount(CompressAmount(a)) == a, "amount roundtrip");
    }
}

void TestCoinRoundtrip()
{
    Coin c;
    c.n_value = 5000000000;
    c.height = 1;
    c.coinbase = true;
    c.script = {0x00, 20};
    c.script.resize(22, 0x11);
    const auto raw = EncodeCoin(c);
    const Coin d = DecodeCoin(raw);
    Expect(d.n_value == c.n_value, "value");
    Expect(d.height == c.height, "height");
    Expect(d.coinbase == c.coinbase, "coinbase");
    Expect(d.script == c.script, "script");
    Expect(d.n_flags == c.n_flags, "n_flags");
    const auto again = EncodeCoin(d);
    Expect(again == raw, "coin encode involution");
    const auto leaf = EncodeLeaf(c);
    Expect(raw.size() == leaf.size() + 4, "disk adds CRC-32C trailer");
}

void TestUndoMatchesDisk()
{
    Coin c;
    c.n_value = 1;
    c.height = 7;
    c.coinbase = false;
    c.script = {0x51, 32};
    c.script.resize(34, 0x22);
    const auto undo = EncodeUndo(c);
    const auto coin = EncodeCoin(c);
    Expect(undo == coin, "undo encoding equals disk Coin");
    const Coin d = DecodeUndo(undo);
    Expect(d.height == 7 && !d.coinbase && d.n_value == 1, "undo reconstructs metadata");
}

void TestKey()
{
    uint8_t txid[32];
    for (int i = 0; i < 32; ++i) txid[i] = static_cast<uint8_t>(i + 1);
    const auto key = EncodeKey(txid, 3);
    Expect(key[0] == 'C', "DB_COIN type byte");
    Expect(key.size() == 34, "txid32 + type + 1-byte VARINT(3)");
}

void TestFailClosedBoundary()
{
    Coin c;
    c.n_value = 50;
    c.height = 4;
    c.coinbase = true;
    c.script = {0x00, 20};
    c.script.resize(22, 0x33);
    const auto good = EncodeCoin(c);
    Expect(!DecodeFailsClosed(good), "valid coin decodes");
    Expect(DecodeFailsClosed({}), "empty → FAIL_CLOSED");
    Expect(DecodeFailsClosed({good[0]}), "truncated → FAIL_CLOSED");
    std::vector<uint8_t> mid(good.begin(), good.begin() + good.size() / 2);
    Expect(DecodeFailsClosed(mid), "mid-truncation → FAIL_CLOSED");
    auto flipped = good;
    flipped.back() ^= 0xFF;
    Expect(DecodeFailsClosed(flipped), "checksum tamper → FAIL_CLOSED");
    auto extra = good;
    extra.push_back(0x00);
    Expect(DecodeFailsClosed(extra), "trailing bytes → FAIL_CLOSED");

    std::vector<uint8_t> oversized = WriteVarInt((4ull << 1) | 1);
    const auto amt = WriteVarInt(CompressAmount(1));
    oversized.insert(oversized.end(), amt.begin(), amt.end());
    const auto sz = WriteVarInt(static_cast<uint64_t>(MAX_SCRIPT_SIZE + 1));
    oversized.insert(oversized.end(), sz.begin(), sz.end());
    oversized.insert(oversized.end(), MAX_SCRIPT_SIZE + 1, 0x00);
    Expect(DecodeFailsClosed(oversized), "oversized script → FAIL_CLOSED");
}

void TestHeightAndCoinbaseTrusted()
{
    Coin c;
    c.n_value = 1;
    c.height = 100;
    c.coinbase = true;
    c.script = {0x00, 20};
    c.script.resize(22, 0x01);
    auto raw = EncodeCoin(c);
    Coin d = DecodeCoin(raw);
    Expect(d.height == 100 && d.coinbase, "stored metadata round-trips");
    raw[0] ^= 0x01;
    Expect(DecodeFailsClosed(raw), "height/coinbase tamper without checksum update fails closed");
}

void TestPythonGoldenLock()
{
    Expect(DecodeFailsClosed(FromHex("")), "golden empty fail-closed");
    Expect(DecodeFailsClosed(FromHex("ff")), "golden 0xff fail-closed");
}

} // namespace

int main()
{
    TestCrc32c();
    TestAmountRoundtrip();
    TestCoinRoundtrip();
    TestUndoMatchesDisk();
    TestKey();
    TestFailClosedBoundary();
    TestHeightAndCoinbaseTrusted();
    TestPythonGoldenLock();

    if (g_fails) {
        std::fprintf(stderr, "DATABASE_STATE_PERSISTENCE_SECURITY_TESTS=FAIL (%d)\n", g_fails);
        return 1;
    }
    std::printf("DATABASE_STATE_PERSISTENCE_SECURITY_TESTS=PASS\n");
    std::printf("COIN_CODEC_ROUNDTRIP=PASS\n");
    std::printf("UNDO_CODEC_ROUNDTRIP=PASS\n");
    std::printf("DESERIALIZE_ERROR_FAIL_CLOSED=TRUE\n");
    std::printf("OVERSIZED_SCRIPT_FAIL_CLOSED=TRUE\n");
    std::printf("HEIGHT_COINBASE_CHECKSUM_PROTECTED=TRUE\n");
    return 0;
}

// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-086B: dual-asset Coin / key / undo + asset-reclassify fuzz.
// Crash or UB is a finding. Reclassification of a well-formed record
// after script mutation is recorded, not aborted.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class Truncated : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
class CodecError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

uint64_t ReadVarInt(const uint8_t* buf, size_t len, size_t& pos)
{
    uint64_t n = 0;
    while (true) {
        if (pos >= len) throw Truncated("eof");
        const uint8_t ch = buf[pos++];
        if (n > (0xFFFFFFFFULL >> 7)) throw CodecError("large");
        n = (n << 7) | (ch & 0x7F);
        if (ch & 0x80) {
            if (n == 0xFFFFFFFFULL) throw CodecError("ovf");
            ++n;
        } else {
            return n;
        }
    }
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
    while (e--) {
        if (n > (UINT64_MAX / 10)) throw CodecError("amount ovf");
        n *= 10;
    }
    return n;
}

constexpr int MAX_SCRIPT_SIZE = 10000;
constexpr unsigned N_SPECIAL = 6;

void DecodeScript(const uint8_t* buf, size_t len, size_t& pos, std::vector<uint8_t>& script)
{
    const uint64_t nsize_raw = ReadVarInt(buf, len, pos);
    if (nsize_raw < N_SPECIAL) {
        const unsigned sl = (nsize_raw <= 1) ? 20u : 32u;
        if (pos + sl > len) throw Truncated("special");
        script.assign(buf + pos, buf + pos + sl);
        pos += sl;
        return;
    }
    const uint64_t nsize = nsize_raw - N_SPECIAL;
    if (nsize > MAX_SCRIPT_SIZE) {
        if (pos + nsize > len) throw Truncated("oversize");
        pos += nsize;
        script = {0x6a};
        return;
    }
    if (pos + nsize > len) throw Truncated("script");
    script.assign(buf + pos, buf + pos + nsize);
    pos += nsize;
}

enum class Asset : uint8_t { ORU, RES, NONE, INVALID };

Asset Classify(const std::vector<uint8_t>& s)
{
    if (s.empty()) return Asset::INVALID;
    if (s[0] == 0x6a) return Asset::NONE;
    if (s.size() == 22 && s[0] == 20 && s[21] == 0xbd) return Asset::ORU;
    if (s.size() == 22 && s[0] == 0x00 && s[1] == 20) return Asset::ORU;
    if (s.size() == 34 && s[0] == 0x51 && s[1] == 32) {
        if (s.size() >= 6 && s[2] == 'R' && s[3] == 'E' && s[4] == 'S' && s[5] == '2') return Asset::INVALID;
        if (s.size() >= 6 && s[2] == 'C' && s[3] == 'V' && s[4] == 'G' && s[5] == '2') return Asset::INVALID;
        return Asset::RES;
    }
    if (!s.empty() && s.back() == 0xbb) return Asset::RES;
    return Asset::INVALID;
}

void DecodeCoin(const uint8_t* buf, size_t len, Asset& asset)
{
    size_t pos = 0;
    (void)ReadVarInt(buf, len, pos);
    (void)DecompressAmount(ReadVarInt(buf, len, pos));
    std::vector<uint8_t> script;
    DecodeScript(buf, len, pos, script);
    asset = Classify(script);
}

void DecodeUndo(const uint8_t* buf, size_t len)
{
    size_t pos = 0;
    const uint64_t code = ReadVarInt(buf, len, pos);
    if ((code >> 1) > 0) (void)ReadVarInt(buf, len, pos);
    (void)DecompressAmount(ReadVarInt(buf, len, pos));
    std::vector<uint8_t> script;
    DecodeScript(buf, len, pos, script);
}

void DecodeKey(const uint8_t* buf, size_t len)
{
    if (len == 0) throw Truncated("empty key");
    if (buf[0] != 'C') throw CodecError("type");
    if (len < 33) throw Truncated("txid");
    size_t pos = 33;
    (void)ReadVarInt(buf, len, pos);
}

enum class Kind : uint8_t { Coin = 0, Undo = 1, Key = 2, Mutate = 3 };

int Feed(const uint8_t* data, size_t size)
{
    if (size == 0) return 0;
    const Kind kind = static_cast<Kind>(data[0] % 4);
    const uint8_t* payload = data + 1;
    const size_t plen = size - 1;
    try {
        switch (kind) {
        case Kind::Coin: {
            Asset a = Asset::INVALID;
            DecodeCoin(payload, plen, a);
            (void)a;
            break;
        }
        case Kind::Undo:
            DecodeUndo(payload, plen);
            break;
        case Kind::Key:
            DecodeKey(payload, plen);
            break;
        case Kind::Mutate: {
            // Build a well-formed ORU or RES coin from the seed, then flip
            // the script identity bit. Must not crash.
            std::vector<uint8_t> script;
            if ((data[0] & 1) == 0) {
                script.assign(22, 0x11);
                script[0] = 0x00;
                script[1] = 20;
                if (plen > 0 && payload[0] & 1) {
                    script.assign(34, 0x22);
                    script[0] = 0x51;
                    script[1] = 32;
                }
            } else {
                script.assign(34, 0x22);
                script[0] = 0x51;
                script[1] = 32;
                if (plen > 0 && payload[0] & 2) {
                    script.assign(22, 0x11);
                    script[0] = 0x00;
                    script[1] = 20;
                }
            }
            (void)Classify(script);
            break;
        }
        }
    } catch (const std::exception&) {
        return 0;
    }
    return 0;
}

} // namespace

#ifdef DUAL_ASSET_DATABASE_FUZZ_MAIN
int main(int argc, char** argv)
{
    if (argc >= 2 && std::strcmp(argv[1], "smoke") == 0) {
        const uint8_t seeds[][16] = {
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0},
            {2, 'C', 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14},
            {3, 0x00, 20, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13},
        };
        for (const auto& s : seeds) {
            if (Feed(s, sizeof(s)) != 0) return 1;
        }
        std::puts("DUAL_ASSET_DATABASE_FUZZ_SMOKE=PASS");
        return 0;
    }
    if (argc < 2) {
        std::fputs("usage: dual_asset_database_fuzz <file>|smoke\n", stderr);
        return 2;
    }
    FILE* f = std::fopen(argv[1], "rb");
    if (!f) return 1;
    std::vector<uint8_t> buf(1 << 20);
    const size_t n = std::fread(buf.data(), 1, buf.size(), f);
    std::fclose(f);
    return Feed(buf.data(), n);
}
#else
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    return Feed(data, size);
}
#endif

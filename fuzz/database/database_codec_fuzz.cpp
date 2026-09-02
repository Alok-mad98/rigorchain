// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-086: Coin / key / undo decoder fuzz. Crash or UB is a finding.
// Garbage must throw CodecError/Truncated (FAIL_CLOSED) or decode as a
// well-formed record. The production node aborts on deserialize failure;
// this replica must not abort the fuzzer process.

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

void DecodeScript(const uint8_t* buf, size_t len, size_t& pos)
{
    const uint64_t nsize = ReadVarInt(buf, len, pos);
    if (nsize > MAX_SCRIPT_SIZE) {
        throw CodecError("oversize");
    }
    if (pos + nsize > len) throw Truncated("script");
    pos += nsize;
}

void DecodeCoin(const uint8_t* buf, size_t len)
{
    size_t pos = 0;
    const uint64_t code = ReadVarInt(buf, len, pos);
    if ((code >> 1) > 0xFFFFFFFFull) throw CodecError("height");
    const uint64_t amt = DecompressAmount(ReadVarInt(buf, len, pos));
    if (amt > 10000000000ull * 100000000ull) throw CodecError("money");
    DecodeScript(buf, len, pos);
    if (pos + 4 > len) throw Truncated("checksum");
    pos += 4;
    if (pos != len) throw CodecError("trailing");
}

void DecodeUndo(const uint8_t* buf, size_t len)
{
    DecodeCoin(buf, len);
}

void DecodeKey(const uint8_t* buf, size_t len)
{
    if (len == 0) throw Truncated("empty key");
    if (buf[0] != 'C') throw CodecError("type");
    if (len < 33) throw Truncated("txid");
    size_t pos = 33;
    (void)ReadVarInt(buf, len, pos);
}

enum class Kind : uint8_t { Coin = 0, Undo = 1, Key = 2, Overlay = 3 };

int Feed(const uint8_t* data, size_t size)
{
    if (size == 0) return 0;
    const Kind kind = static_cast<Kind>(data[0] % 4);
    const uint8_t* payload = data + 1;
    const size_t plen = size - 1;
    try {
        switch (kind) {
        case Kind::Coin:
            DecodeCoin(payload, plen);
            break;
        case Kind::Undo:
            DecodeUndo(payload, plen);
            break;
        case Kind::Key:
            DecodeKey(payload, plen);
            break;
        case Kind::Overlay: {
            // Cache/database transition smoke: decode a sequence of
            // (add|spend) ops against an in-memory map. Fail-closed on
            // malformed Coin bytes; never abort.
            std::vector<std::vector<uint8_t>> store;
            size_t i = 0;
            while (i < plen) {
                const uint8_t op = payload[i++];
                if ((op & 1) == 0) {
                    if (i >= plen) break;
                    const size_t n = payload[i++];
                    if (i + n > plen) break;
                    DecodeCoin(payload + i, n);
                    store.emplace_back(payload + i, payload + i + n);
                    i += n;
                } else if (!store.empty()) {
                    store.pop_back();
                }
            }
            break;
        }
        }
    } catch (const std::exception&) {
        return 0;
    }
    return 0;
}

} // namespace

#ifdef DATABASE_CODEC_FUZZ_MAIN
int main(int argc, char** argv)
{
    if (argc >= 2 && std::strcmp(argv[1], "smoke") == 0) {
        const uint8_t seeds[][8] = {
            {0, 0, 0, 0, 0, 0, 0, 0},
            {1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
            {2, 'C', 1, 2, 3, 4, 5, 6},
        };
        for (const auto& s : seeds) {
            if (Feed(s, sizeof(s)) != 0) return 1;
        }
        std::puts("DATABASE_CODEC_FUZZ_SMOKE=PASS");
        return 0;
    }
    if (argc < 2) {
        std::fputs("usage: database_codec_fuzz <file>|smoke\n", stderr);
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

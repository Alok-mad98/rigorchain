// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-089: wallet SQLite record / header mutation smoke.
// Crash or UB is a finding. Garbage must be detected/refused, not accepted
// as a healthy schema-0 wallet. This replica does not open CoinsDB.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr int32_t kWalletSchemaVersion = 0;
constexpr char kMagic[16] = {'S','Q','L','i','t','e',' ','f','o','r','m','a','t',' ','3',0};

enum class Verdict { Accept, Refuse };

uint16_t Be16(const uint8_t* p)
{
    return static_cast<uint16_t>((uint16_t(p[0]) << 8) | uint16_t(p[1]));
}

uint32_t Be32(const uint8_t* p)
{
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

Verdict Inspect(const uint8_t* data, size_t len)
{
    if (len < 100) return Verdict::Refuse;
    if (std::memcmp(data, kMagic, 16) != 0) return Verdict::Refuse;

    const uint16_t page_size_field = Be16(data + 16);
    const uint32_t page_size = page_size_field == 1 ? 65536u : uint32_t(page_size_field);
    if (page_size < 512 || (page_size & (page_size - 1)) != 0) return Verdict::Refuse;
    if (len < page_size) return Verdict::Refuse; // truncated first page

    const int32_t user_version = static_cast<int32_t>(Be32(data + 60));
    if (user_version != kWalletSchemaVersion) return Verdict::Refuse;

    // Mutated interior bytes after a valid header are not a verified wallet.
    // Production runs PRAGMA integrity_check; this replica refuses any payload
    // that is not exactly a well-formed header-sized zero-body smoke seed.
    bool mutated = false;
    for (size_t i = 100; i < len; ++i) {
        if (data[i] != 0) {
            mutated = true;
            break;
        }
    }
    if (mutated) return Verdict::Refuse;
    return Verdict::Accept;
}

int Smoke()
{
    std::vector<std::vector<uint8_t>> seeds;
    seeds.emplace_back(); // empty
    seeds.emplace_back(50, uint8_t{0xFF}); // truncated
    {
        std::vector<uint8_t> bad_magic(512, 0);
        std::memcpy(bad_magic.data(), "NotSqlite format", 16);
        seeds.push_back(std::move(bad_magic));
    }
    {
        std::vector<uint8_t> unk(512, 0);
        std::memcpy(unk.data(), kMagic, 16);
        unk[16] = 0x10;
        unk[17] = 0x00; // page_size 4096
        unk[61] = 0; unk[62] = 0; unk[63] = 99; // user_version 99 at 60..63 BE → write BE 99
        unk[60] = 0; unk[61] = 0; unk[62] = 0; unk[63] = 99;
        seeds.push_back(std::move(unk));
    }
    {
        std::vector<uint8_t> mut(4096, uint8_t{0xA5});
        std::memcpy(mut.data(), kMagic, 16);
        mut[16] = 0x10;
        mut[17] = 0x00;
        // user_version 0 already
        seeds.push_back(std::move(mut));
    }
    {
        std::vector<uint8_t> ok(512, 0);
        std::memcpy(ok.data(), kMagic, 16);
        ok[16] = 0x02;
        ok[17] = 0x00; // page_size 512
        seeds.push_back(std::move(ok));
    }

    int refused = 0;
    int accepted = 0;
    for (const auto& s : seeds) {
        if (Inspect(s.data(), s.size()) == Verdict::Refuse) ++refused;
        else ++accepted;
    }
    // empty, trunc, bad magic, unknown version, mutated page → refuse (5)
    // well-formed header-only seed → accept (1)
    if (refused != 5 || accepted != 1) {
        std::fprintf(stderr, "smoke unexpected refused=%d accepted=%d\n", refused, accepted);
        return 1;
    }
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc >= 2 && std::strcmp(argv[1], "smoke") == 0) {
        return Smoke();
    }
    if (argc < 2) {
        std::fprintf(stderr, "usage: wallet_sqlite_fuzz smoke | <file>\n");
        return 2;
    }
    FILE* f = std::fopen(argv[1], "rb");
    if (!f) {
        std::perror("fopen");
        return 1;
    }
    std::vector<uint8_t> buf(1 << 20);
    const size_t n = std::fread(buf.data(), 1, buf.size(), f);
    std::fclose(f);
    Inspect(buf.data(), n); // refuse or accept; must not crash
    return 0;
}

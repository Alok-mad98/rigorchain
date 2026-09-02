// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-086 ConnectBlock convergence fuzz smoke.
// This harness does not instantiate Chainstate::ConnectBlock (that needs a
// full node). It models the acceptance contract the Boost tests enforce:
//
//   invalid block → reject reason recorded → state_mutation_count stays 0
//   valid-shaped header → may accept the *model* only if every gate passes
//
// Crash => CRASH_FOUND. Model accept of a truncated/malformed header => ACCEPT_INVALID.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static void Die(const char* tag)
{
    std::fprintf(stderr, "%s\n", tag);
    std::abort();
}

struct ModelState {
    int mutation_count{0};
    std::string best_hex;
};

struct ModelResult {
    bool accept{false};
    const char* reason{"ok"};
};

static ModelResult ModelConnect(const uint8_t* data, size_t size, ModelState& st)
{
    // Gates, in documented order. A failure must not increment mutation_count.
    if (size < 80) {
        return {false, "malformed-truncated"};
    }
    if (size > 4 * 1024 * 1024) {
        return {false, "malformed-oversize"};
    }

    uint32_t nbits = 0;
    std::memcpy(&nbits, data + 72, 4);
    if (nbits == 0) {
        return {false, "invalid-target"};
    }

    // Treat a zero merkle (bytes 36..67) as invalid merkle.
    bool merkle_zero = true;
    for (int i = 36; i < 68; ++i) {
        if (data[i] != 0) merkle_zero = false;
    }
    if (merkle_zero) {
        return {false, "invalid-merkle"};
    }

    // Body required after header for a "block". Header-only is malformed for connect.
    if (size == 80) {
        return {false, "malformed-no-tx"};
    }

    // Synthetic coinbase gate: first body byte 0x00 is treated as empty-tx / bad coinbase.
    if (data[80] == 0x00) {
        return {false, "invalid-coinbase"};
    }
    if (data[80] == 0x01) {
        return {false, "invalid-tx"};
    }
    if (data[80] == 0x02) {
        return {false, "invalid-predicate"};
    }
    if (data[80] == 0x03) {
        return {false, "invalid-issuance"};
    }
    if (data[80] == 0x04) {
        return {false, "invalid-res"};
    }
    if (data[80] == 0x05) {
        return {false, "invalid-draw"};
    }
    if (data[80] == 0x06) {
        return {false, "invalid-supply"};
    }
    if (data[80] == 0x07) {
        return {false, "invalid-maturity"};
    }

    // Only a fully gated input may "commit".
    ++st.mutation_count;
    st.best_hex = "committed";
    return {true, "ok"};
}

static void CheckRejectUnchanged(const uint8_t* data, size_t size)
{
    ModelState st;
    st.best_hex = "origin";
    const ModelResult r = ModelConnect(data, size, st);
    if (!r.accept) {
        if (st.mutation_count != 0) Die("ACCEPT_INVALID"); // reject mutated state
        if (st.best_hex != "origin") Die("ACCEPT_INVALID");
        if (r.reason == nullptr || std::strlen(r.reason) == 0) Die("CRASH_FOUND");
    } else {
        if (st.mutation_count != 1) Die("CRASH_FOUND");
        // Disconnect symmetry of the model: revert commit.
        --st.mutation_count;
        st.best_hex = "origin";
        if (st.mutation_count != 0 || st.best_hex != "origin") Die("CRASH_FOUND");
    }
}

#ifdef CONNECTBLOCK_CONVERGENCE_FUZZ_MAIN
static std::vector<uint8_t> ReadAll(const char* path)
{
    if (std::strcmp(path, "smoke") == 0) {
        return std::vector<uint8_t>(40, 0);
    }
    FILE* f = std::fopen(path, "rb");
    if (!f) Die("CRASH_FOUND");
    std::vector<uint8_t> buf;
    uint8_t tmp[4096];
    while (true) {
        const size_t n = std::fread(tmp, 1, sizeof(tmp), f);
        buf.insert(buf.end(), tmp, tmp + n);
        if (n < sizeof(tmp)) break;
    }
    std::fclose(f);
    return buf;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <file>|smoke\n", argv[0]);
        return 2;
    }
    try {
        const auto buf = ReadAll(argv[1]);
        CheckRejectUnchanged(buf.data(), buf.size());
    } catch (...) {
        Die("CRASH_FOUND");
    }
    return 0;
}
#endif

// Standalone CSHA256 / SHA256d CLI over production crypto/sha256.cpp.
//
// Usage: cpp_hash_cli sha256|sha256d [hex]
//
// This is NOT a shared binary with Coq and NOT a refinement proof. It compiles
// the portable C++ Transform (no SSE/SHANI). Production SIMD paths are still
// required to match this Transform via SHA256AutoDetect/SelfTest.
// SHA256d here is SHA256(SHA256(x)), the same composition as CHash256 in hash.h.

#include <crypto/sha256.h>

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static std::vector<unsigned char> parse_hex(const char* s)
{
    const size_t n = std::strlen(s);
    if (n % 2 != 0) {
        std::fprintf(stderr, "odd hex length\n");
        std::exit(2);
    }
    std::vector<unsigned char> out;
    out.reserve(n / 2);
    for (size_t i = 0; i < n; i += 2) {
        const int hi = hex_nibble(s[i]);
        const int lo = hex_nibble(s[i + 1]);
        if (hi < 0 || lo < 0) {
            std::fprintf(stderr, "bad hex digit\n");
            std::exit(2);
        }
        out.push_back(static_cast<unsigned char>((hi << 4) | lo));
    }
    return out;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: cpp_hash_cli sha256|sha256d [hex]\n");
        return 2;
    }
    const std::string mode = argv[1];
    const char* hex = (argc > 2) ? argv[2] : "";
    const std::vector<unsigned char> payload = parse_hex(hex);

    unsigned char out[CSHA256::OUTPUT_SIZE];
    if (mode == "sha256") {
        CSHA256().Write(payload.data(), payload.size()).Finalize(out);
    } else if (mode == "sha256d" || mode == "hash256") {
        unsigned char mid[CSHA256::OUTPUT_SIZE];
        CSHA256().Write(payload.data(), payload.size()).Finalize(mid);
        CSHA256().Write(mid, CSHA256::OUTPUT_SIZE).Finalize(out);
    } else {
        std::fprintf(stderr, "mode must be sha256 or sha256d\n");
        return 2;
    }
    for (size_t i = 0; i < CSHA256::OUTPUT_SIZE; ++i) {
        std::printf("%02x", out[i]);
    }
    std::printf("\n");
    return 0;
}

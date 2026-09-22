// RGRPOW01 compat CLI: consensus Sha3_256Short / KeccakF1600.
#include <rigor_kernel/keccak.h>
#include <crypto/common.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int parse_hex(const char* hex, std::vector<unsigned char>& out)
{
    size_t n = std::strlen(hex);
    if (n % 2 != 0)
        return -1;
    out.resize(n / 2);
    for (size_t i = 0; i < out.size(); i++) {
        unsigned int b = 0;
        if (std::sscanf(hex + 2 * i, "%2x", &b) != 1)
            return -1;
        out[i] = static_cast<unsigned char>(b);
    }
    return 0;
}

static void keccak_like(const unsigned char* msg, size_t len, unsigned char suffix, unsigned char out[32])
{
    unsigned char block[136];
    std::memset(block, 0, sizeof(block));
    std::memcpy(block, msg, len);
    block[len] = suffix;
    block[135] |= 0x80;
    uint64_t state[25];
    std::memset(state, 0, sizeof(state));
    for (size_t i = 0; i < 17; i++)
        state[i] ^= ReadLE64(block + 8 * i);
    rigor_kernel::KeccakF1600(state);
    for (size_t i = 0; i < 4; i++)
        WriteLE64(out + 8 * i, state[i]);
}

static void print32(const unsigned char d[32])
{
    for (int i = 0; i < 32; i++)
        std::printf("%02x", d[i]);
    std::printf("\n");
}

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::fprintf(stderr, "usage: hash80_cpp rgrpow|sha3|keccak256 <hex>\n");
        return 2;
    }
    std::string mode = argv[1];
    std::vector<unsigned char> msg;
    if (parse_hex(argv[2], msg) != 0) {
        std::fprintf(stderr, "invalid hex\n");
        return 2;
    }
    unsigned char digest[32];
    if (mode == "rgrpow") {
        if (msg.size() != 80) {
            std::fprintf(stderr, "rgrpow requires 80-byte header\n");
            return 2;
        }
        unsigned char pre[88];
        std::memcpy(pre, "RGRPOW01", 8);
        std::memcpy(pre + 8, msg.data(), 80);
        rigor_kernel::Sha3_256Short(pre, 88, digest);
        print32(digest);
        return 0;
    }
    if (mode == "sha3") {
        if (msg.size() > 134) {
            std::fprintf(stderr, "sha3 short max 134 bytes\n");
            return 2;
        }
        rigor_kernel::Sha3_256Short(msg.data(), msg.size(), digest);
        print32(digest);
        return 0;
    }
    if (mode == "keccak256") {
        if (msg.size() > 134) {
            std::fprintf(stderr, "keccak256 short max 134 bytes\n");
            return 2;
        }
        keccak_like(msg.data(), msg.size(), 0x01, digest);
        print32(digest);
        return 0;
    }
    std::fprintf(stderr, "unknown mode\n");
    return 2;
}

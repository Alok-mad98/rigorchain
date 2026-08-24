// Test-only CLI over production ComputeSigmaHash and
// ComputeConvergenceRootFromEligibilityTxids.
//
// Usage:
//   cpp_sigma_root_cli sigma [--preimage] <txid_hex> <croot_hex> <prev_hex> <height>
//   cpp_sigma_root_cli root [<txid_hex> ...]
//
// Hex arguments are raw uint256 memory order (Python bytes.hex()), not Bitcoin
// RPC/SetHex display order. Height is decimal uint32.
//
// sigma prints the 32-byte σ digest as 64 hex chars. With --preimage, a second
// line is the 100-byte preimage (same layout as sigma_hash.cpp).
// root prints the 32-byte ConvergenceRoot; n == 0 yields 32 zero bytes before hashing.
//
// This is NOT a shared binary with Coq and NOT a refinement proof. Portable
// CSHA256 Transform only (no SSE/SHANI). Does not call
// ComputeConvergenceRootFromConvergenceSet.

#include <consensus/convergence_root_eligibility.h>
#include <consensus_rigor/sigma_hash.h>
#include <span.h>
#include <uint256.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
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

static uint256 parse_u256(const char* hex)
{
    const std::vector<unsigned char> bytes = parse_hex(hex);
    if (bytes.size() != 32) {
        std::fprintf(stderr, "expected 32-byte hex, got %zu bytes\n", bytes.size());
        std::exit(2);
    }
    return uint256(Span<const unsigned char>(bytes.data(), bytes.size()));
}

static void print_hex(const unsigned char* p, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        std::printf("%02x", p[i]);
    }
    std::printf("\n");
}

static void print_u256(const uint256& x)
{
    print_hex(x.begin(), 32);
}

static uint32_t parse_height(const char* s)
{
    char* end = nullptr;
    const unsigned long v = std::strtoul(s, &end, 10);
    if (end == s || *end != '\0' || v > std::numeric_limits<uint32_t>::max()) {
        std::fprintf(stderr, "bad height\n");
        std::exit(2);
    }
    return static_cast<uint32_t>(v);
}

static std::array<unsigned char, rigor::SIGMA_INPUT_SIZE>
encode_sigma_preimage(const uint256& txid,
                      const uint256& croot,
                      const uint256& prev,
                      uint32_t height)
{
    std::array<unsigned char, rigor::SIGMA_INPUT_SIZE> buf{};
    std::memcpy(buf.data() + rigor::SIGMA_OFF_TXID, txid.begin(), 32);
    std::memcpy(buf.data() + rigor::SIGMA_OFF_CROOT, croot.begin(), 32);
    std::memcpy(buf.data() + rigor::SIGMA_OFF_PREV_BLOCK, prev.begin(), 32);
    buf[rigor::SIGMA_OFF_HEIGHT + 0] = static_cast<unsigned char>((height >> 0) & 0xFF);
    buf[rigor::SIGMA_OFF_HEIGHT + 1] = static_cast<unsigned char>((height >> 8) & 0xFF);
    buf[rigor::SIGMA_OFF_HEIGHT + 2] = static_cast<unsigned char>((height >> 16) & 0xFF);
    buf[rigor::SIGMA_OFF_HEIGHT + 3] = static_cast<unsigned char>((height >> 24) & 0xFF);
    return buf;
}

static void usage()
{
    std::fprintf(stderr,
        "usage:\n"
        "  cpp_sigma_root_cli sigma [--preimage] <txid_hex> <croot_hex> <prev_hex> <height>\n"
        "  cpp_sigma_root_cli root [<txid_hex> ...]\n");
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        usage();
        return 2;
    }
    const std::string mode = argv[1];
    int i = 2;

    if (mode == "sigma") {
        bool want_preimage = false;
        if (i < argc && std::strcmp(argv[i], "--preimage") == 0) {
            want_preimage = true;
            ++i;
        }
        if (argc - i != 4) {
            usage();
            return 2;
        }
        const uint256 txid = parse_u256(argv[i]);
        const uint256 croot = parse_u256(argv[i + 1]);
        const uint256 prev = parse_u256(argv[i + 2]);
        const uint32_t height = parse_height(argv[i + 3]);
        const uint256 sigma = rigor::ComputeSigmaHash(txid, croot, prev, height);
        print_u256(sigma);
        if (want_preimage) {
            const auto pre = encode_sigma_preimage(txid, croot, prev, height);
            print_hex(pre.data(), pre.size());
        }
        return 0;
    }

    if (mode == "root") {
        std::vector<uint256> txids;
        txids.reserve(static_cast<size_t>(argc - i));
        for (; i < argc; ++i) {
            txids.push_back(parse_u256(argv[i]));
        }
        const uint256 root = Consensus::ComputeConvergenceRootFromEligibilityTxids(std::move(txids));
        print_u256(root);
        return 0;
    }

    std::fprintf(stderr, "mode must be sigma or root\n");
    usage();
    return 2;
}

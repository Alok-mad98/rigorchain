// Standalone AssetTypeFromScriptPubKey CLI over production rigor_classify.cpp.
//
// Usage: cpp_classify_cli
//   stdin: one hex scriptPubKey per line. Empty script is the line "-"
//          (blank lines are skipped). Prints ORU|RES|NONE|INVALID.
//
// This is NOT a shared binary with Coq and NOT a refinement proof. It compiles
// the production classifier (script as asset) plus CScript, without
// SignatureHash / Ed25519.

#include <primitives/transaction.h>
#include <script/script.h>

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
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

static const char* asset_tag(AssetType a)
{
    switch (a) {
    case ASSET_ORU: return "ORU";
    case ASSET_RES: return "RES";
    case ASSET_NONE: return "NONE";
    case ASSET_INVALID: return "INVALID";
    }
    return "INVALID";
}

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') continue;
        std::vector<unsigned char> raw;
        if (line == "-") {
            raw.clear();
        } else {
            raw = parse_hex(line.c_str());
        }
        const CScript spk(raw.begin(), raw.end());
        std::puts(asset_tag(AssetTypeFromScriptPubKey(spk)));
    }
    return 0;
}

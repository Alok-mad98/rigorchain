// C++ Consensus Kernel adapter CLI. Calls production Kernel functions only.
// Not a second issuance / classify / draw implementation.

#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/res_issuance.h>
#include <consensus/rigor_issuance.h>
#include <consensus/rigor_supply.h>
#include <primitives/transaction.h>
#include <script/script.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

static Consensus::Params FrozenParams()
{
    Consensus::Params p;
    p.nCoinbaseMaturity = 100;
    return p;
}

static void usage()
{
    std::fprintf(stderr,
        "cpp_consensus_kernel_cli (C++ Kernel adapter)\n"
        "epoch H | oru H | res-schedule H | res-price H | issuance H\n"
        "mature SPEND COIN | maturity-const\n"
        "classify HEX|-\n"
        "claimable FEES BURN H | ledger-delta H CONVERGE\n"
        "res-coinbase FEES BOOT\n");
    std::exit(2);
}

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

int main(int argc, char** argv)
{
    if (argc < 2) usage();
    const Consensus::Params p = FrozenParams();
    const std::string cmd = argv[1];
    if (cmd == "epoch" && argc == 3) {
        std::printf("%d\n", RigorIssuanceEpoch(std::atoi(argv[2])));
    } else if (cmd == "oru" && argc == 3) {
        std::printf("%lld\n", static_cast<long long>(GetBlockSubsidy(std::atoi(argv[2]), p)));
    } else if ((cmd == "res-schedule" || cmd == "res-price") && argc == 3) {
        std::printf("%lld\n", static_cast<long long>(Consensus::ResDestroyCostAtHeight(std::atoi(argv[2]))));
    } else if (cmd == "issuance" && argc == 3) {
        const RigorIssuance g = ComputeRigorIssuance(std::atoi(argv[2]), p);
        std::printf("%d %lld %lld\n", g.epoch, static_cast<long long>(g.oru_subsidy),
                    static_cast<long long>(Consensus::ResDestroyCostAtHeight(std::atoi(argv[2]))));
    } else if (cmd == "mature" && argc == 4) {
        const int spend = std::atoi(argv[2]);
        const int coin = std::atoi(argv[3]);
        std::puts((spend - coin >= p.nCoinbaseMaturity) ? "true" : "false");
    } else if (cmd == "maturity-const") {
        std::printf("%d\n", COINBASE_MATURITY);
    } else if (cmd == "classify" && argc == 3) {
        std::vector<unsigned char> raw;
        if (std::strcmp(argv[2], "-") != 0 && argv[2][0] != '\0') {
            raw = parse_hex(argv[2]);
        }
        const CScript spk(raw.begin(), raw.end());
        std::puts(asset_tag(ClassifyRigorAsset(spk)));
    } else if (cmd == "claimable" && argc == 5) {
        const CAmount fees = std::atoll(argv[2]);
        const CAmount burn = std::atoll(argv[3]);
        const CAmount sub = GetBlockSubsidy(std::atoi(argv[4]), p);
        std::printf("%lld\n", static_cast<long long>((fees - burn) + sub));
    } else if (cmd == "ledger-delta" && argc == 4) {
        const CAmount sub = GetBlockSubsidy(std::atoi(argv[2]), p);
        const CAmount conv = std::atoll(argv[3]);
        std::printf("%lld\n", static_cast<long long>(sub - conv));
    } else if (cmd == "res-coinbase" && argc == 4) {
        std::printf("%lld\n",
            static_cast<long long>(RigorResCoinbaseExpected(
                std::atoll(argv[2]), std::atoll(argv[3]))));
    } else {
        usage();
    }
    return 0;
}

// P053 C++ refinement CLI. Calls production Consensus Kernel only.
// Not a second issuance / draw / classify / supply implementation.

#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <consensus/convergence_logic.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_issuance.h>
#include <consensus/rigor_supply.h>
#include <consensus/validation.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/standard.h>
#include <uint256.h>

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

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static std::vector<unsigned char> parse_hex(const std::string& s)
{
    if (s.size() % 2 != 0) {
        std::fprintf(stderr, "odd hex length\n");
        std::exit(2);
    }
    std::vector<unsigned char> out;
    for (size_t i = 0; i < s.size(); i += 2) {
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

static uint256 uint256_from_hex(const std::string& hex)
{
    uint256 out;
    const std::vector<unsigned char> raw = parse_hex(hex);
    if (raw.size() != 32) {
        std::fprintf(stderr, "txid/hash must be 32 bytes\n");
        std::exit(2);
    }
    std::memcpy(out.begin(), raw.data(), 32);
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

static void usage()
{
    std::fprintf(stderr,
        "consensus_kernel_refinement_cli (C++ Kernel adapter, not a second kernel)\n"
        "issuance H\n"
        "mature SPEND COIN\n"
        "classify HEX|-\n"
        "supply HEIGHT FEES BURN CONVERGE MINT HAS_WINNER CARD ORU_CB RES_CB BOOT\n"
        "draw-empty\n"
        "draw H C_HEX B_HEX TXID:WEIGHT[,TXID:WEIGHT...]\n"
        "draw-t0 H C_HEX B_HEX TXID:WEIGHT[,TXID:WEIGHT...]\n");
    std::exit(2);
}

int main(int argc, char** argv)
{
    if (argc < 2) usage();
    const Consensus::Params p = FrozenParams();
    const std::string cmd = argv[1];

    if (cmd == "issuance" && argc == 3) {
        const int h = std::atoi(argv[2]);
        const RigorIssuance g = ComputeRigorIssuance(h, p);
        std::printf(
            "{\"height\":%d,\"epoch\":%d,\"oru_subsidy\":%lld,\"res_schedule\":%lld,\"res_issued\":%lld}\n",
            h, g.epoch, static_cast<long long>(g.oru_subsidy),
            static_cast<long long>(g.res_reward),
            h == 0 ? 0LL : static_cast<long long>(g.res_reward));
    } else if (cmd == "mature" && argc == 4) {
        const int spend = std::atoi(argv[2]);
        const int coin = std::atoi(argv[3]);
        const bool mature = (spend - coin >= p.nCoinbaseMaturity);
        std::printf(
            "{\"spend\":%d,\"coin\":%d,\"oru_mature\":%s,\"res_mature\":%s,"
            "\"mature\":%s,\"maturity\":%d}\n",
            spend, coin, mature ? "true" : "false", mature ? "true" : "false",
            mature ? "true" : "false", COINBASE_MATURITY);
    } else if (cmd == "classify" && argc == 3) {
        std::vector<unsigned char> raw;
        if (std::strcmp(argv[2], "-") != 0 && argv[2][0] != '\0') {
            raw = parse_hex(argv[2]);
        }
        const CScript spk(raw.begin(), raw.end());
        std::vector<std::vector<unsigned char>> sols;
        (void)ClassifyRigorTxout(spk, sols);
        std::printf("{\"script\":\"%s\",\"asset\":\"%s\"}\n",
            argv[2], asset_tag(ClassifyRigorAsset(spk)));
    } else if (cmd == "supply" && argc == 12) {
        RigorSupplyDeltaInput in;
        in.height = std::atoi(argv[2]);
        in.fees_oru = std::atoll(argv[3]);
        in.fee_burn = std::atoll(argv[4]);
        in.converge_sum = std::atoll(argv[5]);
        in.res_mint = std::atoll(argv[6]);
        in.has_winner = std::atoi(argv[7]) != 0;
        in.draw_cardinality = std::atoi(argv[8]);
        in.oru_coinbase = std::atoll(argv[9]);
        in.res_coinbase = std::atoll(argv[10]);
        in.res_bootstrap = std::atoll(argv[11]);
        in.fees_res = 0;
        BlockValidationState state;
        RigorSupplyDeltaResult out;
        const bool ok = RigorSupplyDelta(in, p, out, state);
        if (!ok) {
            std::printf("{\"status\":\"REJECT\",\"reason\":\"%s\"}\n",
                state.GetRejectReason().c_str());
        } else {
            std::printf(
                "{\"status\":\"OK\",\"oru_subsidy\":%lld,\"oru_claimable\":%lld,"
                "\"oru_circulating_delta\":%lld,\"res_delta\":%lld,"
                "\"res_reward_scheduled\":%lld}\n",
                static_cast<long long>(out.oru_subsidy),
                static_cast<long long>(out.oru_claimable),
                static_cast<long long>(out.oru_circulating_delta),
                static_cast<long long>(out.res_delta),
                static_cast<long long>(out.res_reward_scheduled));
        }
    } else if (cmd == "draw-empty") {
        ConvergenceDrawSet set;
        set.source_height = 0;
        const ConvergenceDrawOutcome o =
            ResolveConvergenceDraw(set, uint256S("5555555555555555555555555555555555555555555555555555555555555555"));
        std::printf("{\"draw\":\"%s\"}\n", o.HasWinner() ? "WINNER" : "NONE");
    } else if ((cmd == "draw" || cmd == "draw-t0") && argc == 6) {
        const int payout_h = std::atoi(argv[2]);
        ConvergenceDrawSet set;
        set.source_height = payout_h - 1;
        set.convergence_root = uint256_from_hex(argv[3]);
        set.source_block_hash = uint256_from_hex(argv[4]);
        const std::string spec = argv[5];
        std::vector<std::string> txid_hex;
        size_t pos = 0;
        while (pos < spec.size()) {
            const size_t comma = spec.find(',', pos);
            const std::string one = spec.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
            const size_t colon = one.find(':');
            if (colon == std::string::npos) {
                std::fprintf(stderr, "draw spec TXID:WEIGHT\n");
                return 2;
            }
            ConvergenceDrawCandidate c;
            c.txid = uint256_from_hex(one.substr(0, colon));
            c.weight = static_cast<uint64_t>(std::atoll(one.substr(colon + 1).c_str()));
            set.candidates.push_back(c);
            txid_hex.push_back(one.substr(0, colon));
            if (comma == std::string::npos) break;
            pos = comma + 1;
        }
        const uint256 T = (cmd == "draw-t0") ? uint256() :
            uint256S("5555555555555555555555555555555555555555555555555555555555555555");
        const ConvergenceDrawOutcome o = ResolveConvergenceDraw(set, T);
        if (!o.HasWinner()) {
            std::printf("{\"draw\":\"NONE\"}\n");
        } else {
            std::printf("{\"draw\":\"WINNER\",\"txid\":\"%s\",\"index\":%d}\n",
                txid_hex[static_cast<size_t>(o.winner_index)].c_str(), o.winner_index);
        }
    } else {
        usage();
    }
    return 0;
}

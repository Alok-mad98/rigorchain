// I/O adapter for the C ABI. Calls rigor_* only. Not a second kernel.

#ifndef RIGOR_KERNEL_ABI_VERIFY_ONLY
#define RIGOR_KERNEL_ABI_VERIFY_ONLY
#endif

#include <consensus/bridge/consensus_kernel_c_api.h>
#include <consensus/params.h>

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

static std::vector<uint8_t> parse_hex(const std::string& s)
{
    if (s.size() % 2 != 0) {
        std::fprintf(stderr, "odd hex\n");
        std::exit(2);
    }
    std::vector<uint8_t> out;
    for (size_t i = 0; i < s.size(); i += 2) {
        const int hi = hex_nibble(s[i]);
        const int lo = hex_nibble(s[i + 1]);
        if (hi < 0 || lo < 0) {
            std::fprintf(stderr, "bad hex\n");
            std::exit(2);
        }
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return out;
}

static rigor_u256 u256_from_hex(const std::string& hex)
{
    rigor_u256 out{};
    const std::vector<uint8_t> raw = parse_hex(hex);
    if (raw.size() != 32) {
        std::fprintf(stderr, "need 32 bytes\n");
        std::exit(2);
    }
    std::memcpy(out.bytes, raw.data(), 32);
    return out;
}

static std::string hex_of(const rigor_u256& h)
{
    static const char* k = "0123456789abcdef";
    std::string s;
    s.resize(64);
    for (int i = 0; i < 32; ++i) {
        s[static_cast<size_t>(i) * 2] = k[(h.bytes[i] >> 4) & 0xf];
        s[static_cast<size_t>(i) * 2 + 1] = k[h.bytes[i] & 0xf];
    }
    return s;
}

static const char* asset_name(rigor_asset_type a)
{
    switch (a) {
    case RIGOR_ASSET_ORU: return "ORU";
    case RIGOR_ASSET_RES: return "RES";
    case RIGOR_ASSET_NONE: return "NONE";
    default: return "INVALID";
    }
}

static void usage()
{
    std::fprintf(stderr,
        "consensus_kernel_c_abi_cli\n"
        "issuance H | mature SPEND COIN | classify HEX|-\n"
        "draw-empty | draw H C B SPEC | draw-t0 H C B SPEC\n"
        "supply HEIGHT FEES BURN CONVERGE MINT HAS_WINNER CARD ORU_CB\n");
    std::exit(2);
}

int main(int argc, char** argv)
{
    if (argc < 2) usage();
    const std::string cmd = argv[1];

    if (cmd == "issuance" && argc == 3) {
        rigor_issuance_result out{};
        if (!rigor_compute_issuance(static_cast<uint32_t>(std::atoi(argv[2])), &out)) return 1;
        std::printf("{\"epoch\":%u,\"oru_subsidy\":%llu,\"res_reward_schedule\":%llu}\n",
            out.epoch,
            static_cast<unsigned long long>(out.oru_subsidy),
            static_cast<unsigned long long>(out.res_reward_schedule));
    } else if (cmd == "mature" && argc == 4) {
        uint8_t el = 0;
        if (!rigor_coinbase_mature(static_cast<uint32_t>(std::atoi(argv[2])),
                static_cast<uint32_t>(std::atoi(argv[3])), &el)) return 1;
        std::printf("{\"eligible\":%s}\n", el ? "true" : "false");
    } else if (cmd == "classify" && argc == 3) {
        std::vector<uint8_t> raw;
        if (std::strcmp(argv[2], "-") != 0 && argv[2][0] != '\0') {
            raw = parse_hex(argv[2]);
        }
        rigor_asset_type a = RIGOR_ASSET_INVALID;
        if (!rigor_classify_script(raw.empty() ? nullptr : raw.data(), raw.size(), &a)) return 1;
        std::printf("{\"asset\":\"%s\"}\n", asset_name(a));
    } else if (cmd == "draw-empty") {
        rigor_draw_context ctx{};
        ctx.payout_height = 1;
        rigor_draw_result r{};
        if (!rigor_resolve_draw(nullptr, 0, &ctx, &r)) return 1;
        std::printf("{\"has_winner\":%s}\n", r.has_winner ? "true" : "false");
    } else if ((cmd == "draw" || cmd == "draw-t0") && argc == 6) {
        rigor_draw_context ctx{};
        ctx.payout_height = static_cast<uint32_t>(std::atoi(argv[2]));
        ctx.convergence_root = u256_from_hex(argv[3]);
        ctx.source_block_hash = u256_from_hex(argv[4]);
        if (cmd != "draw-t0") {
            const uint256 T = Consensus::RES_PROBABILITY_TARGET_CONST;
            std::memcpy(ctx.target.bytes, T.begin(), 32);
        }
        const std::string spec = argv[5];
        std::vector<rigor_candidate> cs;
        std::vector<std::string> hexes;
        size_t pos = 0;
        while (pos < spec.size()) {
            const size_t comma = spec.find(',', pos);
            const std::string one = spec.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
            const size_t colon = one.find(':');
            if (colon == std::string::npos) return 2;
            rigor_candidate c{};
            c.txid = u256_from_hex(one.substr(0, colon));
            c.value = static_cast<uint64_t>(std::atoll(one.substr(colon + 1).c_str()));
            cs.push_back(c);
            hexes.push_back(one.substr(0, colon));
            if (comma == std::string::npos) break;
            pos = comma + 1;
        }
        rigor_draw_result r{};
        if (!rigor_resolve_draw(cs.data(), cs.size(), &ctx, &r)) return 1;
        if (!r.has_winner) {
            std::printf("{\"has_winner\":false}\n");
        } else {
            std::string win;
            for (size_t i = 0; i < cs.size(); ++i) {
                if (std::memcmp(cs[i].txid.bytes, r.winner.bytes, 32) == 0) {
                    win = hexes[i];
                    break;
                }
            }
            if (win.empty()) win = hex_of(r.winner);
            std::printf("{\"has_winner\":true,\"winner\":\"%s\"}\n", win.c_str());
        }
    } else if (cmd == "supply" && argc == 11) {
        rigor_supply_input in{};
        in.height = static_cast<uint32_t>(std::atoi(argv[2]));
        in.fees_oru = static_cast<uint64_t>(std::atoll(argv[3]));
        in.burn = static_cast<uint64_t>(std::atoll(argv[4]));
        in.converge = static_cast<uint64_t>(std::atoll(argv[5]));
        in.mint = static_cast<uint64_t>(std::atoll(argv[6]));
        in.has_winner = static_cast<uint8_t>(std::atoi(argv[7]) != 0);
        in.card = static_cast<uint32_t>(std::atoi(argv[8]));
        in.oru_coinbase = static_cast<uint64_t>(std::atoll(argv[9]));
        in.subsidy = static_cast<uint64_t>(std::atoll(argv[10]));
        rigor_supply_result out{};
        if (!rigor_supply_delta(&in, &out)) {
            std::printf("{\"ok\":false}\n");
        } else {
            std::printf("{\"ok\":true,\"delta\":%lld}\n", static_cast<long long>(out.delta));
        }
    } else {
        usage();
    }
    return 0;
}

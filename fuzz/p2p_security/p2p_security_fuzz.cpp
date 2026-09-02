// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-076 P2P security fuzz harness.
// Random bytes → Evaluate. Crash => CRASH_FOUND. No Kernel call.

#include <net/security/p2p_message_boundary.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

static void One(const uint8_t* data, size_t size)
{
    if (size == 0) {
        (void)P2PMessageBoundary::EvaluateBlock(nullptr, 0);
        (void)P2PMessageBoundary::EvaluateTx(nullptr, 0);
        P2PSecurityInput empty;
        empty.command = "version";
        (void)P2PMessageBoundary::Evaluate(empty);
        (void)P2PMessageBoundary::EvaluatePeer(empty);
        return;
    }

    const uint8_t selector = data[0];
    const uint8_t* body = data + (size > 0 ? 1 : 0);
    const size_t n = size > 0 ? size - 1 : 0;

    switch (selector % 5) {
    case 0:
        (void)P2PMessageBoundary::EvaluateBlock(body, n);
        break;
    case 1:
        (void)P2PMessageBoundary::EvaluateTx(body, n);
        break;
    case 2: {
        P2PSecurityInput in;
        in.command = "version";
        in.payload.assign(body, body + n);
        (void)P2PMessageBoundary::Evaluate(in);
        break;
    }
    case 3: {
        P2PSecurityInput in;
        static const char* cmds[] = {"block", "tx", "version", "ping", "inv", "verack"};
        in.command = cmds[n ? (body[0] % 6) : 0];
        in.payload.assign(body, body + n);
        in.claimed_size = n;
        in.messages_in_window = n > 1 ? body[1] * 4u : 0;
        in.invalid_blocks_from_peer = n > 2 ? body[2] : 0;
        in.invalid_tx_from_peer = n > 3 ? body[3] : 0;
        in.handshake_complete = (selector & 1) != 0;
        if (n >= 8) {
            in.seen_block_keys.insert(P2PMessageBoundary::WireFingerprint(body, 8));
            in.seen_tx_keys.insert(P2PMessageBoundary::WireFingerprint(body, n));
        }
        (void)P2PMessageBoundary::Evaluate(in);
        (void)P2PMessageBoundary::EvaluatePeer(in);
        break;
    }
    default: {
        P2PSecurityInput in;
        in.command = "block";
        in.payload.assign(data, data + size);
        in.claimed_size = size;
        (void)P2PMessageBoundary::Evaluate(in);
        break;
    }
    }
}

#ifdef P2P_SECURITY_FUZZ_MAIN
int main(int argc, char** argv)
{
    try {
        if (argc >= 2 && std::strcmp(argv[1], "smoke") == 0) {
            const uint8_t seeds[][16] = {
                {0},
                {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
                {1, 0xff, 0xff, 0xff, 0xff},
                {2, 0x10, 0x27, 0x00, 0x00},
                {3, 0x00, 0x00, 0x00, 0x00, 0x01},
                {4, 0xaa, 0xbb, 0xcc, 0xdd},
            };
            for (const auto& s : seeds) One(s, sizeof(s));
            uint8_t longbuf[4096];
            for (size_t i = 0; i < sizeof(longbuf); ++i) longbuf[i] = static_cast<uint8_t>(i * 17u);
            One(longbuf, sizeof(longbuf));
            One(nullptr, 0);
            std::printf("FUZZ_STATUS=PASS\n");
            std::printf("CRASH_FOUND=NO\n");
            return 0;
        }
        if (argc >= 2) {
            FILE* f = std::fopen(argv[1], "rb");
            if (!f) return 2;
            std::vector<uint8_t> buf;
            uint8_t tmp[4096];
            size_t n;
            while ((n = std::fread(tmp, 1, sizeof(tmp), f)) > 0) {
                buf.insert(buf.end(), tmp, tmp + n);
            }
            std::fclose(f);
            One(buf.data(), buf.size());
            return 0;
        }
        uint8_t stdin_buf[65536];
        const size_t got = std::fread(stdin_buf, 1, sizeof(stdin_buf), stdin);
        One(stdin_buf, got);
        return 0;
    } catch (const std::exception&) {
        std::fprintf(stderr, "CRASH_FOUND\n");
        return 1;
    } catch (...) {
        std::fprintf(stderr, "CRASH_FOUND\n");
        return 1;
    }
}
#endif

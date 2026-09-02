// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-078 script/opcode boundary fuzz harness.
// Random scriptPubKey / scriptSig / witness / opcode bytes.
// Crash => CRASH_FOUND. Accept of malformed => ACCEPT_INVALID.
// crypto_accept=false accept => CONSENSUS_BYPASS.

#include <consensus/txkernel/transaction_kernel_input.h>
#include <script/script.h>
#include <script/standard.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <vector>

static void Die(const char* tag)
{
    std::fprintf(stderr, "%s\n", tag);
    std::abort();
}

static std::vector<unsigned char> Slice(const uint8_t* data, size_t size, size_t off, size_t n)
{
    if (off >= size) return {};
    const size_t take = std::min(n, size - off);
    return std::vector<unsigned char>(data + off, data + off + take);
}

static bool IndependentlyWellFormed(const TransactionKernelInput& in)
{
    if (!in.crypto_accept) return false;
    if (in.scriptPubKey.empty()) return false;
    if (!TransactionKernelEncodingOk(in)) return false;
    if (!TransactionKernelWitnessItemsOk(in)) return false;

    const CScript spk(in.scriptPubKey.begin(), in.scriptPubKey.end());
    if (IsResMintScript(spk)) {
        if (!in.scriptSig_push_only) return false;
        if (!in.keyhash_match) return false;
        if ((in.flags & TK_SCRIPT_VERIFY_WITNESS) != 0 && !in.witness.empty()) return false;
        return true;
    }

    int ver = 0;
    std::vector<unsigned char> program;
    if (!spk.IsWitnessProgram(ver, program)) return false;
    if ((in.flags & TK_SCRIPT_VERIFY_WITNESS) == 0) return false;
    if (!in.scriptSig.empty()) return false;
    if (!TransactionKernelWitnessTwo(in)) return false;
    if (ver == 0) {
        return program.size() == WITNESS_V0_KEYHASH_SIZE && in.keyhash_match;
    }
    if (ver == 1 && program.size() == WITNESS_V1_RES_SIZE &&
        IsRigorResNativeWitnessProgram(program)) {
        return in.pubkey == program;
    }
    return false;
}

static TransactionKernelInput FromBytes(const uint8_t* data, size_t size)
{
    TransactionKernelInput in;
    if (size == 0) return in;
    const uint8_t sel = data[0];
    in.flags = (sel & 1) ? TK_SCRIPT_VERIFY_WITNESS : 0;
    if (sel & 2) in.flags |= TK_SCRIPT_VERIFY_SIGPUSHONLY;
    if (sel & 4) in.flags |= TK_SCRIPT_VERIFY_CLEANSTACK;
    if (sel & 8) in.flags |= TK_SCRIPT_VERIFY_NULLFAIL;
    in.scriptSig_push_only = (sel & 16) != 0;
    in.stack_clean = (sel & 32) != 0;
    in.keyhash_match = (sel & 64) != 0;
    in.crypto_accept = (sel & 128) != 0;

    in.scriptPubKey = Slice(data, size, 1, 1 + (size > 1 ? data[1] % 80 : 0));
    in.scriptSig = Slice(data, size, 40, size > 40 ? data[40] % 40 : 0);
    in.signature = Slice(data, size, 80, 64);
    in.pubkey = Slice(data, size, 144, 32);
    const size_t w0n = size > 2 ? (size_t)data[2] * 4u : 0;
    const size_t w1n = size > 3 ? (size_t)data[3] * 4u : 0;
    if (sel % 5 == 0) {
        in.witness = {Slice(data, size, 20, w0n), Slice(data, size, 60, w1n)};
    } else if (sel % 5 == 1) {
        in.witness = {Slice(data, size, 20, w0n)};
    } else if (sel % 5 == 2) {
        in.witness = {in.signature, in.pubkey};
    } else {
        in.witness.clear();
    }
    in.spend_height = size > 4 ? static_cast<int64_t>(data[4]) : 0;
    in.amount = size > 8 ? static_cast<int64_t>(data[5]) * 1000 : 0;
    in.nVersion = 2;
    in.sighash_tag = Slice(data, size, 6, 8);
    in.chain_id.assign(32, size > 10 ? data[10] : 0);
    in.hashPrevouts.assign(32, 0);
    in.hashSequence.assign(32, 0);
    in.hashOutputs.assign(32, 0);
    in.prevout_hash.assign(32, 0);
    in.spent_script = in.scriptPubKey;
    in.prevout_n = size > 11 ? data[11] : 0;
    return in;
}

static void One(const uint8_t* data, size_t size)
{
    TransactionKernelInput in = FromBytes(data, size);
    const bool ok = VerifyRigorPredicateBoundary(in);
    if (ok && !in.crypto_accept) {
        Die("CONSENSUS_BYPASS");
    }
    if (ok && !IndependentlyWellFormed(in)) {
        Die("ACCEPT_INVALID");
    }
    (void)CppSighashPreimage(in);
    (void)CppSighashHasHashtypeByte(in.sighash_tag);

    // Opcode-byte scriptPubKey: every 0x00-0xff as a lone byte must not accept
    // unless independently well-formed (a single opcode byte never is).
    if (size > 0) {
        TransactionKernelInput op = in;
        op.scriptPubKey = {data[0]};
        op.crypto_accept = true;
        op.keyhash_match = true;
        op.witness = {op.signature, op.pubkey};
        op.flags |= TK_SCRIPT_VERIFY_WITNESS;
        if (VerifyRigorPredicateBoundary(op) && !IndependentlyWellFormed(op)) {
            Die("ACCEPT_INVALID");
        }
    }
}

#ifdef SCRIPT_OPCODE_FUZZ_MAIN
int main(int argc, char** argv)
{
    try {
        if (argc >= 2 && std::strcmp(argv[1], "smoke") == 0) {
            const uint8_t seeds[][32] = {
                {0},
                {0xff, 0xac, 0xae, 0xb1, 0xb2, 0x76, 0xa9, 0x88},
                {0x00, 0x14, 0x11, 0x11, 0x11, 0x11},
                {0x51, 0x20},
                {0x6a, 0x01, 0x00},
                {0xbb},
                {0x80, 0x16, 0x10, 0x10, 0x64},
            };
            for (const auto& s : seeds) One(s, sizeof(s));
            uint8_t longbuf[4096];
            for (size_t i = 0; i < sizeof(longbuf); ++i) {
                longbuf[i] = static_cast<uint8_t>(i * 19u + 7u);
            }
            One(longbuf, sizeof(longbuf));
            One(nullptr, 0);
            std::printf("FUZZ_STATUS=PASS\n");
            std::printf("CRASH_FOUND=NO\n");
            std::printf("ACCEPT_INVALID=NO\n");
            std::printf("CONSENSUS_BYPASS=NO\n");
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

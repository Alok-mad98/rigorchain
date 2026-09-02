// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Out-of-domain MakeKernelInput differential: random CScript, not the
// 9216-state abstract gate domain. Compares C++ marshalling against
// CoqMakeCryptoAccept(ExtractRaw). Also locks tag / preimage honesty
// (empty tag => empty preimage; native-RES only gets RGRRES01; no
// forged zero tx fields).

#include <test/makekernelinput_coq_oracle.h>
#include <test/predicate_boundary_bridge.h>

#include <consensus/amount.h>
#include <hash.h>
#include <primitives/transaction.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

class Reader
{
public:
    Reader(const uint8_t* data, size_t size) : m_data(data), m_size(size) {}

    uint8_t Byte()
    {
        if (m_pos >= m_size) return 0;
        return m_data[m_pos++];
    }

    std::vector<unsigned char> Take(size_t n)
    {
        std::vector<unsigned char> out;
        out.reserve(n);
        for (size_t i = 0; i < n; ++i) out.push_back(Byte());
        return out;
    }

private:
    const uint8_t* m_data;
    size_t m_size;
    size_t m_pos{0};
};

const std::vector<unsigned char> kPubKey(32, 0xaa);

std::vector<unsigned char> PickPubKey(Reader& r)
{
    const uint8_t mode = r.Byte();
    if ((mode & 0x03) == 0) return kPubKey;
    if ((mode & 0x03) == 1) {
        std::vector<unsigned char> pk = kPubKey;
        pk[mode % pk.size()] ^= 0xff;
        return pk;
    }
    if ((mode & 0x03) == 2) return r.Take(32);
    return r.Take(static_cast<size_t>(mode % 40));
}

std::vector<unsigned char> PickSignature(Reader& r)
{
    const uint8_t mode = r.Byte();
    if ((mode & 0x03) == 0) return std::vector<unsigned char>(64, 0x11);
    if ((mode & 0x03) == 1) return r.Take(64);
    if ((mode & 0x03) == 2) return r.Take(static_cast<size_t>(mode % 70));
    return r.Take(static_cast<size_t>(519 + (mode % 4)));
}

PredicateConcreteInput Decode(const uint8_t* data, size_t size)
{
    PredicateConcreteInput c;
    if (size == 0) return c;
    Reader r(data, size);
    const uint8_t shape = r.Byte() % 12;
    const std::vector<unsigned char> pk = PickPubKey(r);
    const std::vector<unsigned char> sig = PickSignature(r);

    switch (shape) {
    case 0: {
        const uint160 h = Hash160(kPubKey);
        c.scriptPubKey = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(h.begin(), h.end());
        break;
    }
    case 1:
        c.scriptPubKey = CScript() << RIGOR_PUSH_1 << kPubKey;
        break;
    case 2:
        c.scriptPubKey = CScript() << RIGOR_PUSH_1 << pk;
        break;
    case 3:
        c.scriptPubKey = GetResMintScript(static_cast<int64_t>(r.Byte()), Hash160(kPubKey));
        break;
    case 4:
        c.scriptPubKey = CScript() << static_cast<opcodetype>(RIGOR_PUSH_0 + (r.Byte() % 17))
                                   << r.Take(static_cast<size_t>(r.Byte() % 45));
        break;
    case 5: {
        const std::vector<unsigned char> raw = r.Take(static_cast<size_t>(r.Byte() % 60));
        c.scriptPubKey = CScript(raw.begin(), raw.end());
        break;
    }
    case 6:
        break;
    case 7: {
        std::vector<unsigned char> tagged(32, 0xaa);
        tagged[0] = 'C'; tagged[1] = 'V'; tagged[2] = 'G'; tagged[3] = '2';
        c.scriptPubKey = CScript() << RIGOR_PUSH_1 << tagged;
        break;
    }
    case 8:
        c.scriptPubKey = CScript() << RIGOR_PUSH_2 << r.Take(20);
        break;
    case 9: {
        const std::vector<unsigned char> raw = r.Take(static_cast<size_t>(80 + (r.Byte() % 40)));
        c.scriptPubKey = CScript(raw.begin(), raw.end());
        break;
    }
    case 10: {
        const CScript mint = GetResMintScript(0, Hash160(kPubKey));
        std::vector<unsigned char> raw(mint.begin(), mint.end());
        if (!raw.empty()) raw.resize(1 + (r.Byte() % raw.size()));
        c.scriptPubKey = CScript(raw.begin(), raw.end());
        break;
    }
    default:
        c.scriptPubKey = CScript() << r.Take(static_cast<size_t>(r.Byte() % 80));
        break;
    }

    const uint8_t sigscript_mode = r.Byte() % 5;
    if (sigscript_mode == 0) {
        c.scriptSig = CScript() << sig << pk;
    } else if (sigscript_mode == 1) {
        c.scriptSig = CScript() << r.Take(static_cast<size_t>(r.Byte() % 8)) << sig << pk;
    } else if (sigscript_mode == 2) {
        const std::vector<unsigned char> raw = r.Take(static_cast<size_t>(r.Byte() % 16));
        c.scriptSig = CScript(raw.begin(), raw.end());
    } else if (sigscript_mode == 3) {
        c.scriptSig = CScript() << RIGOR_RESERVED_LEGACY_NOP;
    }

    const uint8_t witness_mode = r.Byte() % 6;
    if (witness_mode == 0) {
        c.witness.stack = {sig, pk};
    } else if (witness_mode == 1) {
        c.witness.stack = {sig, kPubKey};
    } else if (witness_mode == 2) {
        c.witness.stack = {sig};
    } else if (witness_mode == 3) {
        c.witness.stack = {sig, pk, r.Take(static_cast<size_t>(r.Byte() % 6))};
    } else if (witness_mode == 4) {
        c.witness.stack = {r.Take(521 + (r.Byte() % 4)), pk};
    }

    unsigned int flags = 0;
    const uint8_t bits = r.Byte();
    if (bits & 0x01) flags |= SCRIPT_VERIFY_WITNESS;
    if (bits & 0x02) flags |= SCRIPT_VERIFY_CLEANSTACK;
    if (bits & 0x04) flags |= SCRIPT_VERIFY_SIGPUSHONLY;
    if (bits & 0x08) flags |= SCRIPT_VERIFY_NULLFAIL;
    flags |= static_cast<unsigned int>(r.Byte()) << 16;
    c.flags = flags;
    c.spend_height = static_cast<int>(r.Byte());
    c.ed25519_ok = (r.Byte() & 0x01) != 0;
    return c;
}

int g_accepted = 0;
int g_checked = 0;
int g_fields_present = 0;

CMutableTransaction FuzzTx()
{
    CMutableTransaction tx;
    tx.nVersion = 2;
    tx.nLockTime = 7;
    tx.vin.resize(1);
    tx.vin[0].prevout = COutPoint(uint256S("66"), 3);
    tx.vin[0].nSequence = 9;
    tx.vout.resize(1);
    tx.vout[0].nValue = 50 * COIN;
    tx.vout[0].scriptPubKey = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x11);
    return tx;
}

class FuzzTxChecker final : public MutableTransactionSignatureChecker
{
public:
    FuzzTxChecker(const CMutableTransaction* tx, const CAmount amount, const uint256& chain,
                  int height, bool ed25519_ok)
        : MutableTransactionSignatureChecker(tx, 0, amount, MissingDataBehavior::FAIL, chain, height),
          m_ok(ed25519_ok) {}

    bool CheckEd25519Signature(const std::vector<unsigned char>&,
                               const std::vector<unsigned char>&,
                               const CScript&,
                               SigVersion) const override
    {
        return m_ok;
    }

private:
    bool m_ok;
};

bool RunOne(const uint8_t* data, size_t size)
{
    const PredicateConcreteInput c = Decode(data, size);
    const CMutableTransaction tx = FuzzTx();
    const uint256 chain = uint256S("22");
    const CAmount amount = 40 * COIN;
    FuzzTxChecker checker(&tx, amount, chain, c.spend_height, c.ed25519_ok);
    const TransactionKernelInput in =
        MakeKernelInput(c.scriptSig, c.scriptPubKey, c.witness, c.flags, checker);
    ++g_checked;
    const CoqRawPredicateInput raw = ExtractRaw(c, in);
    const bool coq = CoqMakeCryptoAccept(raw);
    if (coq != in.crypto_accept) {
        std::fprintf(stderr, "MAKEKERNELINPUT_COQ_DIVERGENCE coq=%d cpp=%d\n", (int)coq, (int)in.crypto_accept);
        return false;
    }
    if ((in.sighash_tag.empty() || !in.sighash_fields_present) &&
        !CppSighashPreimage(in).empty()) {
        std::fprintf(stderr, "EMPTY_TAG_PREIMAGE_NONEMPTY\n");
        return false;
    }
    int version = 0;
    std::vector<unsigned char> program;
    const bool wit = c.scriptPubKey.IsWitnessProgram(version, program);
    if (wit && version == 1 &&
        !(program.size() == WITNESS_V1_RES_SIZE && IsRigorResNativeWitnessProgram(program))) {
        if (!in.sighash_tag.empty()) {
            std::fprintf(stderr, "NON_NATIVE_V1_CARRIES_RES_TAG\n");
            return false;
        }
    }
    if (!in.sighash_fields_present &&
        (!in.chain_id.empty() || !in.hashPrevouts.empty() || !in.hashSequence.empty() ||
         !in.hashOutputs.empty() || !in.prevout_hash.empty())) {
        std::fprintf(stderr, "FORGED_ZERO_SIGHASH_FIELDS\n");
        return false;
    }
    if (in.sighash_fields_present) {
        ++g_fields_present;
        const auto pre = CppSighashPreimage(in);
        if (pre.empty()) {
            std::fprintf(stderr, "FIELDS_PRESENT_EMPTY_PREIMAGE\n");
            return false;
        }
        SigVersion v = SigVersion::WITNESS_V0;
        if (IsResMintScript(c.scriptPubKey)) v = SigVersion::RES_MINT;
        else if (wit && version == 1) v = SigVersion::WITNESS_V1_RES;
        const uint256 from_kernel = Hash(pre);
        const uint256 live = SignatureHash(c.scriptPubKey, tx, 0, amount, v, chain);
        if (from_kernel != live) {
            std::fprintf(stderr, "MAKEKERNELINPUT_SIGHASH_NE_SIGNATUREHASH\n");
            return false;
        }
    }
    if (in.crypto_accept) ++g_accepted;
    return true;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (!RunOne(data, size)) std::abort();
    return 0;
}

#ifdef MAKEKERNELINPUT_FUZZ_MAIN
#include <cstdlib>
#include <fstream>

int main(int argc, char** argv)
{
    if (argc >= 2 && std::strcmp(argv[1], "smoke") == 0) {
        int iterations = 200000;
        if (argc >= 3) iterations = std::atoi(argv[2]);
        if (iterations < 1) iterations = 1;
        std::vector<unsigned char> buf(96);
        for (int iter = 0; iter < iterations; ++iter) {
            uint64_t s = 0x9e3779b97f4a7c15ULL * static_cast<uint64_t>(iter + 1);
            for (size_t i = 0; i < buf.size(); ++i) {
                s += 0x9e3779b97f4a7c15ULL;
                uint64_t z = s;
                z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
                z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
                buf[i] = static_cast<unsigned char>((z ^ (z >> 31)) & 0xff);
            }
            if (!RunOne(buf.data(), buf.size())) return 1;
        }
        std::printf("MAKEKERNELINPUT_FUZZ_STATUS=PASS\n");
        std::printf("MAKEKERNELINPUT_FUZZ_ITERS=%d\n", iterations);
        std::printf("MAKEKERNELINPUT_FUZZ_CHECKED=%d\n", g_checked);
        std::printf("MAKEKERNELINPUT_FUZZ_ACCEPTING=%d\n", g_accepted);
        std::printf("MAKEKERNELINPUT_FUZZ_FIELDS_PRESENT=%d\n", g_fields_present);
        if (g_accepted == 0) {
            std::fprintf(stderr, "FAIL: no accepting input; comparison is vacuous\n");
            return 1;
        }
        if (g_fields_present == 0) {
            std::fprintf(stderr, "FAIL: no tx-view sample; SignatureHash lock is vacuous\n");
            return 1;
        }
        return 0;
    }
    for (int i = 1; i < argc; ++i) {
        std::ifstream f(argv[i], std::ios::binary);
        const std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(f),
                                               std::istreambuf_iterator<char>()};
        if (!RunOne(bytes.data(), bytes.size())) return 1;
    }
    return 0;
}
#endif

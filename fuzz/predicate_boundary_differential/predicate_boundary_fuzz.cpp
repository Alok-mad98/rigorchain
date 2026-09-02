// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// A1-F2/F3: differential fuzz. Production vs P058 boundary (same function).
// Diagnostic ClassifyRigorPredicateError is not an authority: NEW_ACCEPT=true
// && LEGACY_ACCEPT=false is ACCEPTANCE_SET_GREW. Ed25519 invocation counts
// are a security/performance metric, not a consensus accept condition.
//
// Flags, witness, decode, malformed, and signature-path cases are in Decode.
// Divergence is BOUNDARY_DIVERGENCE / ACCEPTANCE_SET_GREW / EARLY_CRYPTO /
// REPEAT_CRYPTO and fails the run.

#include <script/fixed_ed25519_checker.h>
#include <test/predicate_boundary_bridge.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

/** Deterministic byte reader over the fuzz input. Runs off the end into zeroes
 *  so short inputs still produce a well-formed case. */
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

/** Sometimes the real key, sometimes a mutation of it, so the fuzzer reaches
 *  both the accepting and the keyhash-mismatch branches. */
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

unsigned int KnownFlags(unsigned int flags)
{
    return flags & (SCRIPT_VERIFY_WITNESS | SCRIPT_VERIFY_CLEANSTACK |
                    SCRIPT_VERIFY_SIGPUSHONLY | SCRIPT_VERIFY_NULLFAIL);
}

unsigned int PickFlags(Reader& r)
{
    const uint8_t bits = r.Byte();
    unsigned int flags = 0;
    if (bits & 0x01) flags |= SCRIPT_VERIFY_WITNESS;
    if (bits & 0x02) flags |= SCRIPT_VERIFY_CLEANSTACK;
    if (bits & 0x04) flags |= SCRIPT_VERIFY_SIGPUSHONLY;
    if (bits & 0x08) flags |= SCRIPT_VERIFY_NULLFAIL;
    flags |= static_cast<unsigned int>(r.Byte()) << 16;
    return flags;
}

PredicateConcreteInput Decode(const uint8_t* data, size_t size)
{
    PredicateConcreteInput c;
    if (size == 0) {
        return c;
    }
    Reader r(data, size);

    const uint8_t shape = r.Byte() % 10;
    const std::vector<unsigned char> pk = PickPubKey(r);
    const std::vector<unsigned char> sig = PickSignature(r);

    switch (shape) {
    case 0: { // witness v0 keyhash
        const uint160 h = Hash160(kPubKey);
        c.scriptPubKey = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(h.begin(), h.end());
        break;
    }
    case 1: // witness v1 RES, program is the real key
        c.scriptPubKey = CScript() << RIGOR_PUSH_1 << kPubKey;
        break;
    case 2: // witness v1 RES, program from the fuzzer
        c.scriptPubKey = CScript() << RIGOR_PUSH_1 << pk;
        break;
    case 3: // RES mint
        c.scriptPubKey = GetResMintScript(static_cast<int64_t>(r.Byte()), Hash160(kPubKey));
        break;
    case 4: // arbitrary version / program
        c.scriptPubKey = CScript() << static_cast<opcodetype>(RIGOR_PUSH_0 + (r.Byte() % 17))
                                   << r.Take(static_cast<size_t>(r.Byte() % 45));
        break;
    case 5: { // raw bytes (malformed decode)
        const std::vector<unsigned char> raw = r.Take(static_cast<size_t>(r.Byte() % 60));
        c.scriptPubKey = CScript(raw.begin(), raw.end());
        break;
    }
    case 6: // empty scriptPubKey
        break;
    case 7: { // oversize scriptPubKey
        const std::vector<unsigned char> raw = r.Take(static_cast<size_t>(80 + (r.Byte() % 40)));
        c.scriptPubKey = CScript(raw.begin(), raw.end());
        break;
    }
    case 8: { // truncated / malformed mint encoding
        const CScript mint = GetResMintScript(0, Hash160(kPubKey));
        std::vector<unsigned char> raw(mint.begin(), mint.end());
        if (!raw.empty()) raw.resize(1 + (r.Byte() % raw.size()));
        c.scriptPubKey = CScript(raw.begin(), raw.end());
        break;
    }
    default: { // witness v0 with an oversized item forced onto the stack later
        const uint160 h = Hash160(kPubKey);
        c.scriptPubKey = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(h.begin(), h.end());
        break;
    }
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

    const uint8_t witness_mode = r.Byte() % 7;
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
    } else if (witness_mode == 5) {
        c.witness.stack = {sig, std::vector<unsigned char>()};
    }

    if (shape == 9 && c.witness.stack.empty()) {
        c.witness.stack = {r.Take(521), pk};
    }

    c.flags = PickFlags(r);
    c.spend_height = static_cast<int>(r.Byte());
    c.ed25519_ok = (r.Byte() & 0x01) != 0;
    return c;
}

std::string Hex(const std::vector<unsigned char>& v)
{
    static const char* d = "0123456789abcdef";
    std::string s;
    for (unsigned char b : v) {
        s.push_back(d[b >> 4]);
        s.push_back(d[b & 0x0f]);
    }
    return s;
}

int g_accepted = 0;
int g_crypto_calls = 0;
int g_crypto_calls_max = 0;
int g_shrank = 0;
int g_diag_calls = 0;

bool RunOne(const uint8_t* data, size_t size)
{
    const PredicateConcreteInput c = Decode(data, size);
    ScriptError prod_err = SCRIPT_ERR_UNKNOWN_ERROR;
    ScriptError diag_err = SCRIPT_ERR_UNKNOWN_ERROR;
    int prod_calls = 0;
    int diag_calls = 0;
    const bool prod = RunProductionObserved(c, prod_err, prod_calls);
    const bool bound = RunBoundary(c);
    const bool diag = RunDiagnosticObserved(c, diag_err, diag_calls);
    g_crypto_calls += prod_calls;
    g_diag_calls += diag_calls;
    if (prod_calls > g_crypto_calls_max) g_crypto_calls_max = prod_calls;
    if (AcceptanceGrew(prod, diag)) {
        std::fprintf(stderr, "ACCEPTANCE_SET_GREW NEW_ACCEPT=TRUE LEGACY_ACCEPT=FALSE\n");
        std::fprintf(stderr, "  scriptPubKey=%s flags=%u\n",
                     Hex(std::vector<unsigned char>(c.scriptPubKey.begin(), c.scriptPubKey.end())).c_str(),
                     c.flags);
        return false;
    }
    if (prod != bound) {
        std::fprintf(stderr, "BOUNDARY_DIVERGENCE production=%d boundary=%d\n", (int)prod, (int)bound);
        std::fprintf(stderr, "  scriptPubKey=%s\n",
                     Hex(std::vector<unsigned char>(c.scriptPubKey.begin(), c.scriptPubKey.end())).c_str());
        std::fprintf(stderr, "  scriptSig=%s\n",
                     Hex(std::vector<unsigned char>(c.scriptSig.begin(), c.scriptSig.end())).c_str());
        std::fprintf(stderr, "  witness_items=%zu flags=%u height=%d ed25519_ok=%d\n",
                     c.witness.stack.size(), c.flags, c.spend_height, (int)c.ed25519_ok);
        return false;
    }
    if (LastFixedEd25519TupleMismatch() > 0) {
        std::fprintf(stderr, "FIXED_CHECKER_TUPLE_MISMATCH=%d\n", LastFixedEd25519TupleMismatch());
        return false;
    }
    if (!prod && diag) ++g_shrank;
    PredicateConcreteInput stripped = c;
    stripped.flags = KnownFlags(c.flags);
    ScriptError strip_err = SCRIPT_ERR_UNKNOWN_ERROR;
    int strip_calls = 0;
    const bool strip_prod = RunProductionObserved(stripped, strip_err, strip_calls);
    if (prod != strip_prod) {
        std::fprintf(stderr, "UNKNOWN_FLAG_NOT_INERT flags=%u stripped=%u\n", c.flags, stripped.flags);
        return false;
    }
    if (prod_calls > 1) {
        std::fprintf(stderr, "REPEAT_CRYPTO trips=%d\n", prod_calls);
        return false;
    }
    int wv = 0;
    std::vector<unsigned char> prog;
    const bool is_wit = c.scriptPubKey.IsWitnessProgram(wv, prog);
    if (is_wit && (c.flags & SCRIPT_VERIFY_WITNESS) == 0 && prod_calls > 0) {
        std::fprintf(stderr, "EARLY_CRYPTO bypass decode gate: missing WITNESS flag\n");
        return false;
    }
    if (is_wit && !c.scriptSig.empty() && prod_calls > 0) {
        std::fprintf(stderr, "EARLY_CRYPTO bypass decode gate: non-empty scriptSig\n");
        return false;
    }
    if (prod) ++g_accepted;
    return true;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (!RunOne(data, size)) std::abort();
    return 0;
}

#ifdef PREDICATE_BOUNDARY_FUZZ_MAIN
#include <fstream>

int main(int argc, char** argv)
{
    if (argc >= 2 && std::strcmp(argv[1], "--selftest") == 0) {
        if (!AcceptanceGrewSelfTest()) {
            std::fprintf(stderr, "FAIL: AcceptanceGrew self-test\n");
            return 2;
        }
        if (!AcceptanceGrew(true, false)) {
            std::fprintf(stderr, "FAIL: detector inert\n");
            return 2;
        }
        std::fprintf(stderr, "ACCEPTANCE_SET_GREW NEW_ACCEPT=TRUE LEGACY_ACCEPT=FALSE\n");
        return 1;
    }
    if (argc >= 2 && std::strcmp(argv[1], "smoke") == 0) {
        // Deterministic but decorrelated fill. A positional sweep keeps the
        // decoder's mode bytes correlated and never lines up an accepting
        // combination, so the comparison would be vacuous.
        const int iterations = 200000;
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
        std::printf("smoke iterations=%d accepting=%d\n", iterations, g_accepted);
        std::printf("ED25519_INVOCATIONS_RECORDED=%d max=%d\n", g_crypto_calls, g_crypto_calls_max);
        std::printf("ED25519_COUNT_NOT_CONSENSUS_CONDITION=TRUE\n");
        std::printf("ACCEPTANCE_SET_SHRANK=%d (metric, legacy is not an authority)\n", g_shrank);
        std::printf("ED25519_DIAGNOSTIC_TRIPS=%d\n", g_diag_calls);
        std::printf("FIXED_CHECKER_TUPLE_MISMATCH=0\n");
        if (g_accepted == 0) {
            std::fprintf(stderr, "FAIL: no accepting input reached; comparison is vacuous\n");
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

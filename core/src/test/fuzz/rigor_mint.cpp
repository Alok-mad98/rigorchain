// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/predicate_verify.h>
#include <script/script.h>
#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/fuzz.h>
#include <uint256.h>

#include <cassert>
#include <cstdint>
#include <vector>

FUZZ_TARGET(rigor_mint)
{
    FuzzedDataProvider rng(buffer.data(), buffer.size());
    const int64_t maturity = rng.ConsumeIntegralInRange<int64_t>(-100000, 400000000);
    std::vector<unsigned char> pkh = rng.ConsumeBytes<unsigned char>(20);
    pkh.resize(20, 0x11);

    uint160 hash;
    std::copy(pkh.begin(), pkh.end(), hash.begin());
    const CScript canonical = GetResMintScript(maturity, hash);

    int64_t parsed_mat = 0;
    std::vector<unsigned char> parsed_pkh;
    if (maturity < 0) {
        // Real maturities (spend height + coinbase maturity) are positive; the parser rejects
        // a negative one rather than round-tripping it.
        assert(!ParseResMintScript(canonical, parsed_mat, parsed_pkh));
        return;
    }
    assert(ParseResMintScript(canonical, parsed_mat, parsed_pkh));
    assert(parsed_mat == maturity);
    assert(parsed_pkh == pkh);
    assert(canonical == GetResMintScript(parsed_mat, hash));

    // The maturity prefix is always OP_0 or a 1..4 byte data push, on the
    // negative branch too: CScript << CScriptNum is << getvch(), which encodes
    // the sign in the payload. This excludes OP_1NEGATE (0x4f), OP_1..OP_16
    // (0x51..0x60) and PUSHDATA1 (0x4c), each a second encoding of a maturity
    // the encoder must never emit.
    assert(!canonical.empty());
    const unsigned char prefix = canonical[0];
    assert(prefix == RIGOR_PUSH_0 || (prefix >= 1 && prefix <= 4));

    {
        CScript mutated = canonical;
        const size_t idx = rng.ConsumeIntegralInRange<size_t>(0, mutated.size() - 1);
        mutated[idx] = static_cast<unsigned char>(mutated[idx] + 1U);
        if (mutated != canonical) {
            int64_t dummy = 0;
            std::vector<unsigned char> dummy_pkh;
            if (ParseResMintScript(mutated, dummy, dummy_pkh)) {
                uint160 h2;
                std::copy(dummy_pkh.begin(), dummy_pkh.end(), h2.begin());
                assert(mutated == GetResMintScript(dummy, h2));
            }
        }
    }

    CScript pushdata1;
    pushdata1.push_back(RIGOR_PUSHDATA1);
    pushdata1.push_back(1);
    pushdata1.push_back(5);
    pushdata1 << ToByteVector(hash) << RIGOR_MARKER_RES_MINT;
    int64_t dummy = 0;
    std::vector<unsigned char> dummy_pkh;
    assert(!ParseResMintScript(pushdata1, dummy, dummy_pkh));

    CScript op5;
    op5.push_back(static_cast<unsigned char>(RIGOR_PUSH_1 + 4));
    op5 << ToByteVector(hash) << RIGOR_MARKER_RES_MINT;
    assert(!ParseResMintScript(op5, dummy, dummy_pkh));

    CScript trailing = canonical;
    trailing.push_back(0x00);
    assert(!ParseResMintScript(trailing, dummy, dummy_pkh));
}

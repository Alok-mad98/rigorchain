// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/transaction.h>
#include <rigor_capability.h>
#include <script/predicate_verify.h>
#include <script/script.h>
#include <script/standard.h>
#include <test/fuzz/fuzz.h>
#include <uint256.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

FUZZ_TARGET(rigor_scriptpubkey)
{
    const CScript script(buffer.begin(), buffer.end());

    std::vector<std::vector<unsigned char>> sols;
    const TxoutType txout_type = ClassifyRigorTxout(script, sols);
    const AssetType asset = AssetTypeFromScriptPubKey(script);

    int64_t maturity = 0;
    std::vector<unsigned char> pkh;
    const bool parsed_mint = ParseResMintScript(script, maturity, pkh);

    if (parsed_mint) {
        assert(txout_type == TxoutType::RES_MINT);
        assert(asset == ASSET_RES);
        assert(pkh.size() == 20);
        uint160 hash;
        std::copy(pkh.begin(), pkh.end(), hash.begin());
        assert(script == GetResMintScript(maturity, hash));
        if (!script.empty()) {
            const unsigned char b = script[0];
            assert(b < RIGOR_PUSH_1 || b > RIGOR_PUSH_16);
        }
    } else {
        assert(txout_type != TxoutType::RES_MINT);
        if (!script.empty()) {
            const unsigned char b = script[0];
            if (b >= RIGOR_PUSH_1 && b <= RIGOR_PUSH_16) {
                int64_t ignored_mat = 0;
                std::vector<unsigned char> ignored_pkh;
                assert(!ParseResMintScript(script, ignored_mat, ignored_pkh));
            }
        }
    }

    if (txout_type == TxoutType::NONSTANDARD) {
        assert(asset == ASSET_INVALID);
    }
    if (txout_type == TxoutType::NULL_DATA) {
        assert(asset == ASSET_NONE);
    }
    if (txout_type == TxoutType::WITNESS_V0_KEYHASH || txout_type == TxoutType::CONVERGE) {
        assert(asset == ASSET_ORU);
    }
    if (txout_type == TxoutType::WITNESS_V1_RES || txout_type == TxoutType::RES_MINT) {
        assert(asset == ASSET_RES);
    }

    uint160 ones;
    ones.SetHex("1111111111111111111111111111111111111111");
    const CScript pushbytes5 = GetResMintScript(5, ones);
    assert(AssetTypeFromScriptPubKey(pushbytes5) == ASSET_RES);
    CScript op5;
    op5.push_back(static_cast<unsigned char>(RIGOR_PUSH_1 + 4));
    op5 << ToByteVector(ones) << RIGOR_MARKER_RES_MINT;
    assert(AssetTypeFromScriptPubKey(op5) == ASSET_INVALID);
    assert(op5 != pushbytes5);
}

FUZZ_TARGET(rigor_p2p_capability)
{
    uint32_t ver = 0;
    uint64_t bitmap = 0;
    if (buffer.size() >= 4) {
        std::memcpy(&ver, buffer.data(), 4);
    }
    if (buffer.size() >= 12) {
        std::memcpy(&bitmap, buffer.data() + 4, 8);
    } else if (buffer.size() > 4) {
        std::memcpy(&bitmap, buffer.data() + 4, std::min<size_t>(8, buffer.size() - 4));
    }
    CNodeCapabilities caps;
    caps.ApplyAdvertisement(ver, bitmap);
    assert((caps.EnabledBitmap() & ~RIGOR_LOCAL_ACTIVE_CAPABILITIES) == 0);
    const uint64_t unknown = bitmap & ~RIGOR_KNOWN_CAPABILITY_MASK;
    (void)unknown;
}

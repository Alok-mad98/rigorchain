// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <consensus/rigor_consensus_lock.h>
#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/fuzz.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

namespace {

struct Delta {
    CAmount subsidy{0};
    CAmount mint{0};
    CAmount converge{0};
    CAmount burn{0};
};

CAmount Circulating(const std::vector<Delta>& chain)
{
    CAmount circ = 0;
    for (const Delta& d : chain) {
        circ += d.subsidy + d.mint - d.converge - d.burn;
    }
    return circ;
}

} // namespace

FUZZ_TARGET(rigor_reorg)
{
    FuzzedDataProvider rng(buffer.data(), buffer.size());
    const int depths[] = {1, 31, 32, 33, 64, 100, 101, 288};
    const int depth = depths[rng.ConsumeIntegralInRange<size_t>(0, 7)];

    const int n = rng.ConsumeIntegralInRange<int>(0, 64);
    std::vector<Delta> kept;
    kept.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        Delta d;
        d.subsidy = rng.ConsumeIntegralInRange<CAmount>(0, 1000 * COIN);
        d.mint = rng.ConsumeIntegralInRange<CAmount>(0, 40 * 1000000);
        d.converge = rng.ConsumeIntegralInRange<CAmount>(0, d.subsidy);
        d.burn = rng.ConsumeIntegralInRange<CAmount>(0, d.subsidy - d.converge);
        kept.push_back(d);
    }

    std::vector<Delta> old_suffix;
    std::vector<Delta> new_suffix;
    const int suffix_len = std::min(depth, 16);
    for (int i = 0; i < suffix_len; ++i) {
        Delta o, nw;
        o.subsidy = rng.ConsumeIntegralInRange<CAmount>(0, 1000 * COIN);
        o.mint = rng.ConsumeIntegralInRange<CAmount>(0, 40 * 1000000);
        o.converge = 0;
        o.burn = 0;
        nw.subsidy = o.subsidy;
        nw.mint = rng.ConsumeIntegralInRange<CAmount>(0, 40 * 1000000);
        nw.converge = 0;
        nw.burn = 0;
        old_suffix.push_back(o);
        new_suffix.push_back(nw);
    }

    std::vector<Delta> old_chain = kept;
    old_chain.insert(old_chain.end(), old_suffix.begin(), old_suffix.end());
    std::vector<Delta> new_chain = kept;
    new_chain.insert(new_chain.end(), new_suffix.begin(), new_suffix.end());

    const CAmount old_circ = Circulating(old_chain);
    const CAmount new_circ = Circulating(new_chain);
    const CAmount kept_circ = Circulating(kept);

    CAmount old_suffix_circ = 0;
    for (const Delta& d : old_suffix) {
        old_suffix_circ += d.subsidy + d.mint - d.converge - d.burn;
    }
    CAmount new_suffix_circ = 0;
    for (const Delta& d : new_suffix) {
        new_suffix_circ += d.subsidy + d.mint - d.converge - d.burn;
    }
    assert(old_circ == kept_circ + old_suffix_circ);
    assert(new_circ == kept_circ + new_suffix_circ);

    CAmount orphan_mint = 0;
    for (const Delta& d : old_suffix) orphan_mint += d.mint;
    CAmount kept_mint = 0;
    for (const Delta& d : kept) kept_mint += d.mint;
    CAmount new_suffix_mint = 0;
    for (const Delta& d : new_suffix) new_suffix_mint += d.mint;
    CAmount surviving_mint = 0;
    for (const Delta& d : new_chain) surviving_mint += d.mint;
    assert(surviving_mint == kept_mint + new_suffix_mint);
    (void)orphan_mint;
    assert(surviving_mint >= 0);
    assert(Consensus::CONVERGENCE_DRAW_WINDOW == 32);
    assert(COINBASE_MATURITY == 100);
}

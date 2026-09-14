// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/amount.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/fuzz.h>
#include <util/overflow.h>
#include <validation.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>

FUZZ_TARGET(rigor_supply)
{
    FuzzedDataProvider rng(buffer.data(), buffer.size());
    const CAmount subsidy = rng.ConsumeIntegral<CAmount>();
    const CAmount mint = rng.ConsumeIntegral<CAmount>();
    const CAmount converge = rng.ConsumeIntegral<CAmount>();
    const CAmount fee = rng.ConsumeIntegral<CAmount>();

    const CAmount burn = Consensus::OruResTransferFeeBurnShare(fee);
    assert(burn >= 0);
    if (fee <= 0) {
        assert(burn == 0);
    } else {
        assert(burn <= fee);
        assert(burn == fee / 2);
    }

    if (subsidy < 0 || mint < 0 || converge < 0 || burn < 0) {
        return;
    }
    if (!MoneyRange(subsidy) || !MoneyRange(mint) || !MoneyRange(converge) || !MoneyRange(burn)) {
        return;
    }

    assert(!AddMoneyOverflows(0, subsidy));
    if (AddMoneyOverflows(subsidy, mint)) {
        return;
    }
    const CAmount issued = subsidy + mint;
    assert(issued >= subsidy);
    assert(issued >= mint);

    if (converge > issued) {
        return;
    }
    const CAmount after_converge = issued - converge;
    if (burn > after_converge) {
        return;
    }
    const CAmount circulating_delta = after_converge - burn;
    assert(circulating_delta >= 0);
    assert(circulating_delta == subsidy + mint - converge - burn);
    const CAmount ledger_oru_delta = subsidy - converge;
    assert(circulating_delta == ledger_oru_delta + mint - burn);

    assert(MAX_MONEY <= std::numeric_limits<int64_t>::max() / 4);
    assert(!AddMoneyOverflows(0, MAX_MONEY));
    assert(AddMoneyOverflows(MAX_MONEY, 1));

    const int32_t raw_h = rng.ConsumeIntegral<int32_t>();
    const int height = raw_h < 0 ? 0 : static_cast<int>(raw_h);

    Consensus::Params subsidy_params;

    const int forced[] = {0, 1, 209999, 210000, 210001, 419999, 420000, 420001};
    const int probe = rng.ConsumeBool() ? height : forced[rng.ConsumeIntegralInRange<size_t>(0, 7)];
    const int probe_epoch = probe / Consensus::RES_REWARD_EPOCH;
    if (probe == 0) {
        assert(GetBlockSubsidy(0, subsidy_params) == 0);
    } else {
        assert(GetBlockSubsidy(probe, subsidy_params) == Consensus::ORU_FLAT_BLOCK_SUBSIDY);
    }

    // Closed-form increment is fuzz coverage, not a theorem. Stay inside int64-safe heights.
    if (probe >= 1 && probe <= 630001) {
        const CAmount flat = Consensus::ORU_FLAT_BLOCK_SUBSIDY;
        auto oru_closed = [&](int64_t H) -> CAmount {
            if (H <= 0) return 0;
            return H * flat;
        };
        assert(oru_closed(probe) - oru_closed(probe - 1) == flat);
        (void)probe_epoch;
    }
}

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
    const int epoch = height / Consensus::ORU_SUBSIDY_HALVING_INTERVAL;
    assert(Consensus::ORU_SUBSIDY_HALVING_INTERVAL == Consensus::RES_REWARD_EPOCH);
    assert(epoch == height / Consensus::RES_REWARD_EPOCH);
    assert(Consensus::GetResMintReward(height) == Consensus::ResRewardForEpoch(epoch));

    // C++ subsidy API is `int` height. Coq uses Z. Do not silently feed uint64/UINT32_MAX
    // into GetBlockSubsidy — only the interval identity is checked past INT_MAX.
    const uint64_t wide = rng.ConsumeIntegral<uint64_t>();
    if (wide > static_cast<uint64_t>(std::numeric_limits<int>::max())) {
        assert(Consensus::ORU_SUBSIDY_HALVING_INTERVAL == Consensus::RES_REWARD_EPOCH);
    }

    Consensus::Params subsidy_params;
    subsidy_params.nBlockSubsidyBase = Consensus::ORU_INITIAL_BLOCK_SUBSIDY;
    subsidy_params.nSubsidyHalvingInterval = Consensus::ORU_SUBSIDY_HALVING_INTERVAL;
    subsidy_params.nBlockSubsidyMaxHalvingShifts = Consensus::ORU_SUBSIDY_MAX_SHIFTS;

    const int forced[] = {0, 1, 209999, 210000, 210001, 419999, 420000, 420001};
    const int probe = rng.ConsumeBool() ? height : forced[rng.ConsumeIntegralInRange<size_t>(0, 7)];
    const int probe_epoch = probe / Consensus::ORU_SUBSIDY_HALVING_INTERVAL;
    assert(probe_epoch == probe / Consensus::RES_REWARD_EPOCH);
    assert(Consensus::GetResMintReward(probe) == Consensus::ResRewardForEpoch(probe_epoch));
    if (probe == 0) {
        assert(GetBlockSubsidy(0, subsidy_params) == 0);
        assert(Consensus::GetResMintReward(0) == Consensus::RES_REWARD_GENESIS);
    } else {
        const int shifts = std::min(probe_epoch, Consensus::ORU_SUBSIDY_MAX_SHIFTS);
        assert(GetBlockSubsidy(probe, subsidy_params) ==
               (Consensus::ORU_INITIAL_BLOCK_SUBSIDY >> shifts));
    }

    // Closed-form increment is fuzz coverage, not a theorem. Stay inside int64-safe heights.
    if (probe >= 1 && probe <= 630001) {
        const CAmount r0 = Consensus::ORU_INITIAL_BLOCK_SUBSIDY;
        const CAmount r1 = r0 >> 1;
        const CAmount r2 = r0 >> 2;
        const CAmount r3 = r0 >> 3;
        auto oru_closed = [&](int64_t H) -> CAmount {
            if (H <= 0) return 0;
            if (H < 210000) return H * r0;
            if (H < 420000) return 209999 * r0 + (H - 209999) * r1;
            if (H < 630000) return 209999 * r0 + 210000 * r1 + (H - 419999) * r2;
            return 209999 * r0 + 210000 * r1 + 210000 * r2 + (H - 629999) * r3;
        };
        const CAmount expected = (probe_epoch == 0) ? r0
            : (probe_epoch == 1) ? r1
            : (probe_epoch == 2) ? r2
            : r3;
        assert(oru_closed(probe) - oru_closed(probe - 1) == expected);
        assert(Consensus::GetResMintReward(probe) ==
               Consensus::ResRewardForEpoch(probe_epoch));
    }
}

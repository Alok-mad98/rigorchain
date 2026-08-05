// Copyright (c) The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <policy/convergence_policy.h>

#include <consensus/amount.h>
#include <consensus/convergenceres.h>
#include <kernel/mempool_entry.h>
#include <policy/feerate.h>

#include <cmath>
#include <cstdlib>
#include <string>

#include <util/system.h>

bool ConvergenceMempoolBoostEnabled()
{
    return gArgs.GetBoolArg("-convmempoolboost", false);
}

double ConvergePolicyK(const std::string& convpolicyk_override)
{
    const std::string s = convpolicyk_override.empty()
        ? gArgs.GetArg("-convpolicyk", "0.01")
        : convpolicyk_override;
    char* end_ptr = nullptr;
    const double v = std::strtod(s.c_str(), &end_ptr);
    if (end_ptr == s.c_str() || v < 0.0 || !std::isfinite(v)) {
        return 0.01;
    }
    return v;
}

double MempoolConvergeEvictionScore(const CTxMemPoolEntry& entry)
{
    const CFeeRate dr(entry.GetModFeesWithDescendants(), entry.GetSizeWithDescendants());
    const double fee_per_k = static_cast<double>(dr.GetFeePerK());

#if CONVERGENCE_POLICY_V1_LOCK
    if (!ConvergenceMempoolBoostEnabled()) {
        return fee_per_k;
    }
    if (!Consensus::IsConvergenceTx(entry.GetTx())) {
        return fee_per_k;
    }
    const CAmount convergence_amt = Consensus::GetConvergenceAmount(entry.GetTx());
    return fee_per_k + ConvergePolicyK() * (static_cast<double>(convergence_amt) / static_cast<double>(COIN));
#else
#error "CONVERGENCE_POLICY_V1_LOCK must be 1 for convergence v1.0 mempool policy"
#endif
}

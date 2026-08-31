// Copyright (c) 2026 The Rigor developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_KERNEL_REGTEST_ASSUMEUTXO_SNAPSHOTS_H
#define BITCOIN_KERNEL_REGTEST_ASSUMEUTXO_SNAPSHOTS_H

#include <uint256.h>
#include <util/strencodings.h>

/**
 * Single source of truth for regtest assumeutxo HASH_SERIALIZED values used in
 * CRegTestParams::m_assumeutxo_data (see kernel/chainparams.cpp).
 *
 * After changing regtest genesis, subsidy, or anything that alters the UTXO set at
 * heights 110 / 200, regenerate and update the literals below:
 *
 *   RIGOR_GEN_ASSUMEUTXO=1 ./test/test_rigor --run_test=validation_chainstate_tests/rigor_assumeutxo_param_gen
 *
 * (see test/validation_chainstate_tests.cpp). Consensus behavior is unchanged when only
 * these constants are refreshed to match the generator output.
 */
namespace regtest_assumeutxo_snapshots {
[[nodiscard]] inline uint256 SerializedUtxoSetHashAtBlockHeight110()
{
    return uint256S("0xcfe210484087d6679c6a33c19f9f8ed98742502bef57ba959a29a331f136614e");
}
[[nodiscard]] inline uint256 SerializedUtxoSetHashAtBlockHeight200()
{
    return uint256S("0xf00b704dae2c496d9dfc322f9c2f3eb206953f5ea03c78b360df25ff54976813");
}
} // namespace regtest_assumeutxo_snapshots

#endif // BITCOIN_KERNEL_REGTEST_ASSUMEUTXO_SNAPSHOTS_H

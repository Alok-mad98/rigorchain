// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/transaction.h>
#include <script/predicate_verify.h>
#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/fuzz.h>
#include <test/fuzz/util.h>

#include <cassert>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

bool CastToBool(const std::vector<unsigned char>& vch);

FUZZ_TARGET(script_interpreter)
{
    FuzzedDataProvider fuzzed_data_provider(buffer.data(), buffer.size());
    {
        const CScript script_code = ConsumeScript(fuzzed_data_provider);
        const std::optional<CMutableTransaction> mtx = ConsumeDeserializable<CMutableTransaction>(fuzzed_data_provider);
        if (mtx) {
            const CTransaction tx_to{*mtx};
            const unsigned int in = fuzzed_data_provider.ConsumeIntegral<unsigned int>();
            if (in < tx_to.vin.size()) {
                const CAmount amount = ConsumeMoney(fuzzed_data_provider);
                const SigVersion sigversion = fuzzed_data_provider.PickValueInArray({SigVersion::WITNESS_V0, SigVersion::WITNESS_V1_RES, SigVersion::RES_MINT});
                // The cache only memoises hashPrevouts/hashSequence/hashOutputs of
                // txTo, so a cache built from txTo must not move the digest. A
                // divergence here is a consensus split between the connect path
                // (cached) and the mempool/signing paths (uncached).
                const PrecomputedTransactionData cache_of_tx_to{tx_to};
                const uint256 uncached = SignatureHash(script_code, tx_to, in, amount, sigversion, uint256::ZERO, nullptr);
                const uint256 cached = SignatureHash(script_code, tx_to, in, amount, sigversion, uint256::ZERO, &cache_of_tx_to);
                assert(uncached == cached);
                const std::optional<CMutableTransaction> mtx_precomputed = ConsumeDeserializable<CMutableTransaction>(fuzzed_data_provider);
                if (mtx_precomputed) {
                    // Cache of an unrelated transaction: the digest is unspecified,
                    // so this call is a crash probe only and must not be asserted on.
                    const CTransaction tx_precomputed{*mtx_precomputed};
                    const PrecomputedTransactionData precomputed_transaction_data{tx_precomputed};
                    (void)SignatureHash(script_code, tx_to, in, ConsumeMoney(fuzzed_data_provider), fuzzed_data_provider.PickValueInArray({SigVersion::WITNESS_V0, SigVersion::WITNESS_V1_RES, SigVersion::RES_MINT}), uint256::ZERO, &precomputed_transaction_data);
                }
            }
        }
    }
    {
        (void)CastToBool(ConsumeRandomLengthByteVector(fuzzed_data_provider));
    }
}

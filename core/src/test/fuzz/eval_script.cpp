// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/predicate_verify.h>
#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/fuzz.h>

FUZZ_TARGET(eval_script)
{
    FuzzedDataProvider fuzzed_data_provider(buffer.data(), buffer.size());
    const std::vector<uint8_t> script_bytes = fuzzed_data_provider.ConsumeRemainingBytes<uint8_t>();
    const CScript script(script_bytes.begin(), script_bytes.end());
    std::vector<std::vector<unsigned char>> stack;
    (void)EvalPushOnlyScript(script, stack);
}

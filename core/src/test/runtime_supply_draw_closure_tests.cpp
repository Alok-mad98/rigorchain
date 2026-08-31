// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/supply_monitor/runtime_supply_draw_closure.h>

#include <boost/test/unit_test.hpp>

#include <univalue.h>

#include <algorithm>

BOOST_AUTO_TEST_SUITE(runtime_supply_draw_closure_tests)

namespace {

bool HasString(const std::vector<std::string>& items, const std::string& value)
{
    return std::find(items.begin(), items.end(), value) != items.end();
}

} // namespace

BOOST_AUTO_TEST_CASE(draw_missing_canonical_encoder)
{
    const Consensus::DrawClosureResult result = Consensus::ResolveDrawClosureStatus();
    BOOST_CHECK(!result.condition.has_draw_canonical_encoder);
    BOOST_CHECK(HasString(result.missing_requirements, "DRAW-CANONICAL-ENCODER-MISSING"));
}

BOOST_AUTO_TEST_CASE(draw_missing_hash_kernel)
{
    const Consensus::DrawClosureResult result = Consensus::ResolveDrawClosureStatus();
    BOOST_CHECK(!result.condition.has_runtime_hash_kernel);
    BOOST_CHECK(HasString(result.missing_requirements, "RUNTIME-HASH-KERNEL-MISSING"));
}

BOOST_AUTO_TEST_CASE(draw_combined_impossibility)
{
    const Consensus::DrawClosureResult result = Consensus::ResolveDrawClosureStatus();
    BOOST_CHECK_EQUAL(Consensus::DrawClosureStatusToString(result.status), "IMPOSSIBLE");
    BOOST_CHECK(!result.equivalence_claimed);
    BOOST_CHECK(HasString(result.impossibility_reasons, "DRAW-CES-FundamentallyBlocked"));
    BOOST_CHECK(HasString(result.impossibility_reasons, "FVR-L-DRAW-PROOF-GATE-CLOSED"));
    BOOST_CHECK(result.missing_requirements.size() == 5);
}

BOOST_AUTO_TEST_CASE(draw_export_json_shape)
{
    const Consensus::DrawClosureResult result = Consensus::ResolveDrawClosureStatus();
    const UniValue json = Consensus::DrawClosureResultToJson(result);

    BOOST_CHECK_EQUAL(json["task"].get_str(), "FVR-M");
    BOOST_CHECK_EQUAL(json["draw_status"].get_str(), "IMPOSSIBLE");
    BOOST_CHECK_EQUAL(json["equivalence_claimed"].get_bool(), false);
    BOOST_CHECK_EQUAL(json["coq_equiv_runtime"].get_str(), "NOT_ESTABLISHED");
    BOOST_CHECK(json["missing_requirements"].isArray());
    BOOST_CHECK(json["impossibility_reasons"].isArray());
    BOOST_CHECK(json["DrawClosureCondition"].isObject());
    BOOST_CHECK_EQUAL(json["DrawClosureCondition"]["has_draw_canonical_encoder"].get_bool(),
                      false);
}

BOOST_AUTO_TEST_SUITE_END()

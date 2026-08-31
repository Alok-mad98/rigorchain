// Copyright (c) 2016-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/amount.h>
#include <policy/feerate.h>

#include <limits>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(amount_tests)

BOOST_AUTO_TEST_CASE(MoneyRangeTest)
{
    BOOST_CHECK_EQUAL(MoneyRange(CAmount(-1)), false);
    BOOST_CHECK_EQUAL(MoneyRange(CAmount(0)), true);
    BOOST_CHECK_EQUAL(MoneyRange(CAmount(1)), true);
    BOOST_CHECK_EQUAL(MoneyRange(MAX_MONEY), true);
    BOOST_CHECK_EQUAL(MoneyRange(MAX_MONEY + CAmount(1)), false);
}

/**
 * TASK-010 R-1: the bound must leave room to add in-range amounts. Two amounts
 * that each pass MoneyRange must be summable without signed overflow, because
 * that is what every `total += v; if (!MoneyRange(total))` in Bitcoin-derived
 * code silently assumes.
 */
BOOST_AUTO_TEST_CASE(MaxMoneyLeavesAdditionHeadroom)
{
    BOOST_CHECK(MAX_MONEY <= std::numeric_limits<int64_t>::max() / 4);
    // A four-term sum of maximal amounts is still representable.
    const CAmount quadruple = MAX_MONEY + MAX_MONEY + MAX_MONEY + MAX_MONEY;
    BOOST_CHECK_GT(quadruple, MAX_MONEY);
    BOOST_CHECK(!MoneyRange(quadruple));
}

BOOST_AUTO_TEST_CASE(AddMoneyOverflowsTest)
{
    BOOST_CHECK(!AddMoneyOverflows(0, 0));
    BOOST_CHECK(!AddMoneyOverflows(0, MAX_MONEY));
    BOOST_CHECK(!AddMoneyOverflows(MAX_MONEY, 0));
    BOOST_CHECK(!AddMoneyOverflows(MAX_MONEY - 1, 1));

    // Exactly at the bound is fine; one past it is not.
    BOOST_CHECK(AddMoneyOverflows(MAX_MONEY, 1));
    BOOST_CHECK(AddMoneyOverflows(MAX_MONEY, MAX_MONEY));

    // Out-of-range operands are overflows in their own right, so callers need no
    // separate MoneyRange guard.
    BOOST_CHECK(AddMoneyOverflows(-1, 0));
    BOOST_CHECK(AddMoneyOverflows(0, -1));
    BOOST_CHECK(AddMoneyOverflows(MAX_MONEY + 1, 0));
    BOOST_CHECK(AddMoneyOverflows(0, MAX_MONEY + 1));
    BOOST_CHECK(AddMoneyOverflows(std::numeric_limits<CAmount>::max(),
                                  std::numeric_limits<CAmount>::max()));
}

BOOST_AUTO_TEST_CASE(GetFeeTest)
{
    CFeeRate feeRate, altFeeRate;

    feeRate = CFeeRate(0);
    // Must always return 0
    BOOST_CHECK_EQUAL(feeRate.GetFee(0), CAmount(0));
    BOOST_CHECK_EQUAL(feeRate.GetFee(1e5), CAmount(0));

    feeRate = CFeeRate(1000);
    // Must always just return the arg
    BOOST_CHECK_EQUAL(feeRate.GetFee(0), CAmount(0));
    BOOST_CHECK_EQUAL(feeRate.GetFee(1), CAmount(1));
    BOOST_CHECK_EQUAL(feeRate.GetFee(121), CAmount(121));
    BOOST_CHECK_EQUAL(feeRate.GetFee(999), CAmount(999));
    BOOST_CHECK_EQUAL(feeRate.GetFee(1e3), CAmount(1e3));
    BOOST_CHECK_EQUAL(feeRate.GetFee(9e3), CAmount(9e3));

    feeRate = CFeeRate(-1000);
    // Must always just return -1 * arg
    BOOST_CHECK_EQUAL(feeRate.GetFee(0), CAmount(0));
    BOOST_CHECK_EQUAL(feeRate.GetFee(1), CAmount(-1));
    BOOST_CHECK_EQUAL(feeRate.GetFee(121), CAmount(-121));
    BOOST_CHECK_EQUAL(feeRate.GetFee(999), CAmount(-999));
    BOOST_CHECK_EQUAL(feeRate.GetFee(1e3), CAmount(-1e3));
    BOOST_CHECK_EQUAL(feeRate.GetFee(9e3), CAmount(-9e3));

    feeRate = CFeeRate(123);
    // Rounds up the result, if not integer
    BOOST_CHECK_EQUAL(feeRate.GetFee(0), CAmount(0));
    BOOST_CHECK_EQUAL(feeRate.GetFee(8), CAmount(1)); // Special case: returns 1 instead of 0
    BOOST_CHECK_EQUAL(feeRate.GetFee(9), CAmount(2));
    BOOST_CHECK_EQUAL(feeRate.GetFee(121), CAmount(15));
    BOOST_CHECK_EQUAL(feeRate.GetFee(122), CAmount(16));
    BOOST_CHECK_EQUAL(feeRate.GetFee(999), CAmount(123));
    BOOST_CHECK_EQUAL(feeRate.GetFee(1e3), CAmount(123));
    BOOST_CHECK_EQUAL(feeRate.GetFee(9e3), CAmount(1107));

    feeRate = CFeeRate(-123);
    // Truncates the result, if not integer
    BOOST_CHECK_EQUAL(feeRate.GetFee(0), CAmount(0));
    BOOST_CHECK_EQUAL(feeRate.GetFee(8), CAmount(-1)); // Special case: returns -1 instead of 0
    BOOST_CHECK_EQUAL(feeRate.GetFee(9), CAmount(-1));

    // check alternate constructor
    feeRate = CFeeRate(1000);
    altFeeRate = CFeeRate(feeRate);
    BOOST_CHECK_EQUAL(feeRate.GetFee(100), altFeeRate.GetFee(100));

    // Check full constructor
    BOOST_CHECK(CFeeRate(CAmount(-1), 0) == CFeeRate(0));
    BOOST_CHECK(CFeeRate(CAmount(0), 0) == CFeeRate(0));
    BOOST_CHECK(CFeeRate(CAmount(1), 0) == CFeeRate(0));
    // default value
    BOOST_CHECK(CFeeRate(CAmount(-1), 1000) == CFeeRate(-1));
    BOOST_CHECK(CFeeRate(CAmount(0), 1000) == CFeeRate(0));
    BOOST_CHECK(CFeeRate(CAmount(1), 1000) == CFeeRate(1));
    // lost precision (can only resolve satoshis per kB)
    BOOST_CHECK(CFeeRate(CAmount(1), 1001) == CFeeRate(0));
    BOOST_CHECK(CFeeRate(CAmount(2), 1001) == CFeeRate(1));
    // some more integer checks
    BOOST_CHECK(CFeeRate(CAmount(26), 789) == CFeeRate(32));
    BOOST_CHECK(CFeeRate(CAmount(27), 789) == CFeeRate(34));
    // Maximum size in bytes, should not crash
    CFeeRate(MAX_MONEY, std::numeric_limits<uint32_t>::max()).GetFeePerK();
    // TASK-010 R-1: MAX_MONEY * 1000 exceeds int64. The widened product must
    // saturate rather than wrap; a one-byte divisor is the worst case.
    BOOST_CHECK_EQUAL(CFeeRate(MAX_MONEY, 1).GetFeePerK(), std::numeric_limits<int64_t>::max());
    BOOST_CHECK_EQUAL(CFeeRate(-MAX_MONEY, 1).GetFeePerK(), std::numeric_limits<int64_t>::min());
}

BOOST_AUTO_TEST_CASE(BinaryOperatorTest)
{
    CFeeRate a, b;
    a = CFeeRate(1);
    b = CFeeRate(2);
    BOOST_CHECK(a < b);
    BOOST_CHECK(b > a);
    BOOST_CHECK(a == a);
    BOOST_CHECK(a <= b);
    BOOST_CHECK(a <= a);
    BOOST_CHECK(b >= a);
    BOOST_CHECK(b >= b);
    // a should be 0.00000002 ORU/kvB now
    a += a;
    BOOST_CHECK(a == b);
}

BOOST_AUTO_TEST_CASE(ToStringTest)
{
    CFeeRate feeRate;
    feeRate = CFeeRate(1);
    BOOST_CHECK_EQUAL(feeRate.ToString(), "0.00000001 ORU/kvB");
    BOOST_CHECK_EQUAL(feeRate.ToString(FeeEstimateMode::BTC_KVB), "0.00000001 ORU/kvB");
    BOOST_CHECK_EQUAL(feeRate.ToString(FeeEstimateMode::SAT_VB), "0.001 tharn/vB");
}

BOOST_AUTO_TEST_SUITE_END()

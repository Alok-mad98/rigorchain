// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <consensus_rigor/fee.h>
#include <coins.h>
#include <consensus/amount.h>
#include <consensus/convergenceres.h>
#include <consensus/res_amount.h>
#include <consensus/rigor_consensus_lock.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <test/util/script.h>
#include <serialize.h>
#include <version.h>

BOOST_AUTO_TEST_SUITE(rigor_fee_tests)

namespace {
// Native ORU: witness v0 / 20-byte program. Legacy P2PKH is ASSET_NONE (fail-closed),
// so fee_i would be undefined even when nValue looks like an ORU transfer.
CScript NativeOruSpk()
{
    return CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x22);
}
} // namespace

// ============================================================================
// RIGOR Fee Mechanism Specification §7.1: unit tests
// MUST verify:
// ✔ fee is computed correctly
// ✔ size is computed consistently
// ✔ fee_rate ordering is consistent
// ✔ f_i enters consensus correctly
// ============================================================================

BOOST_AUTO_TEST_CASE(fee_calculation_orum)
{
    // RIGOR Fee Mechanism Specification §1.1: Fee Calculation (the only rule)
    // fee_i = sum(inputs_ORUM) - sum(outputs_ORUM)
    
    CCoinsView backed;
    CCoinsViewCache view(&backed);
    
    // Create a simple transaction with ORUM inputs and outputs
    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].prevout = COutPoint(uint256::ONE, 0);
    tx.vout.resize(2);
    
    // Set up input: 100 ORUM
    Coin coin;
    coin.out.nValue = 100 * COIN;
    coin.out.scriptPubKey = NativeOruSpk();
    coin.nHeight = 100;
    view.AddCoin(tx.vin[0].prevout, std::move(coin), false);
    
    // Set up outputs: 60 ORUM and 30 ORUM (fee = 100 - 60 - 30 = 10 ORUM)
    tx.vout[0].nValue = 60 * COIN;
    tx.vout[0].scriptPubKey = NativeOruSpk();
    tx.vout[1].nValue = 30 * COIN;
    tx.vout[1].scriptPubKey = NativeOruSpk();
    
    CTransaction tx_const(tx);
    uint64_t fee = 0;
    BOOST_CHECK(rigor::CalculateFee_ORUM(tx_const, view, fee));
    BOOST_CHECK_EQUAL(fee, 10 * COIN);
}

BOOST_AUTO_TEST_CASE(fee_calculation_multiple_inputs_outputs)
{
    // Test fee calculation with multiple inputs and outputs
    CCoinsView backed;
    CCoinsViewCache view(&backed);
    
    CMutableTransaction tx;
    tx.vin.resize(3);
    tx.vout.resize(4);
    
    // Set up inputs: 50 + 30 + 20 = 100 ORUM
    for (size_t i = 0; i < 3; ++i) {
        tx.vin[i].prevout = COutPoint(uint256::ONE, i);
        Coin coin;
        coin.out.nValue = (i == 0 ? 50 : (i == 1 ? 30 : 20)) * COIN;
        coin.out.scriptPubKey = NativeOruSpk();
        coin.nHeight = 100;
        view.AddCoin(tx.vin[i].prevout, std::move(coin), false);
    }
    
    // Set up outputs: 25 + 20 + 15 + 10 = 70 ORUM (fee = 100 - 70 = 30 ORUM)
    CAmount outputs[] = {25, 20, 15, 10};
    for (size_t i = 0; i < 4; ++i) {
        tx.vout[i].nValue = outputs[i] * COIN;
        tx.vout[i].scriptPubKey = NativeOruSpk();
    }
    
    CTransaction tx_const(tx);
    uint64_t fee = 0;
    BOOST_CHECK(rigor::CalculateFee_ORUM(tx_const, view, fee));
    BOOST_CHECK_EQUAL(fee, 30 * COIN);
}

BOOST_AUTO_TEST_CASE(fee_calculation_zero_fee)
{
    // RIGOR Fee Mechanism Specification §1.1: fee_i == 0 → transaction MUST NOT enter candidate set
    
    CCoinsView backed;
    CCoinsViewCache view(&backed);
    
    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].prevout = COutPoint(uint256::ONE, 0);
    tx.vout.resize(1);
    
    // Set up input: 100 ORUM
    Coin coin;
    coin.out.nValue = 100 * COIN;
    coin.out.scriptPubKey = NativeOruSpk();
    coin.nHeight = 100;
    view.AddCoin(tx.vin[0].prevout, std::move(coin), false);
    
    // Set up output: 100 ORUM (fee = 0)
    tx.vout[0].nValue = 100 * COIN;
    tx.vout[0].scriptPubKey = NativeOruSpk();
    
    CTransaction tx_const(tx);
    uint64_t fee = 0;
    BOOST_CHECK(rigor::CalculateFee_ORUM(tx_const, view, fee));
    BOOST_CHECK_EQUAL(fee, 0);
}

BOOST_AUTO_TEST_CASE(fee_calculation_invalid_negative_fee)
{
    // RIGOR Fee Mechanism Specification §1.1: fee_i MUST >= 0
    
    CCoinsView backed;
    CCoinsViewCache view(&backed);
    
    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].prevout = COutPoint(uint256::ONE, 0);
    tx.vout.resize(1);
    
    // Set up input: 100 ORUM
    Coin coin;
    coin.out.nValue = 100 * COIN;
    coin.out.scriptPubKey = NativeOruSpk();
    coin.nHeight = 100;
    view.AddCoin(tx.vin[0].prevout, std::move(coin), false);
    
    // Set up output: 150 ORUM (invalid, outputs > inputs)
    tx.vout[0].nValue = 150 * COIN;
    tx.vout[0].scriptPubKey = NativeOruSpk();
    
    CTransaction tx_const(tx);
    uint64_t fee = 0;
    BOOST_CHECK(!rigor::CalculateFee_ORUM(tx_const, view, fee));
    BOOST_CHECK_EQUAL(fee, 0);
}

BOOST_AUTO_TEST_CASE(tx_size_bytes_consistency)
{
    // RIGOR Fee Mechanism Specification §1.2: Transaction Size Definition (the only standard)
    // tx_size_bytes = serialized transaction size in bytes
    // RIGOR Fee Mechanism Specification §7.1: size is computed consistently
    
    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].prevout = COutPoint(uint256::ONE, 0);
    tx.vout.resize(1);
    tx.vout[0].nValue = 50 * COIN;
    tx.vout[0].scriptPubKey = NativeOruSpk();
    
    CTransaction tx_const(tx);
    size_t size1 = rigor::GetTransactionSizeBytes(tx_const);
    size_t size2 = rigor::GetTransactionSizeBytes(tx_const);
    
    // Size must be consistent across multiple calls
    BOOST_CHECK_EQUAL(size1, size2);
    
    // Size must match the serialized size
    size_t expected_size = ::GetSerializeSize(tx_const, PROTOCOL_VERSION);
    BOOST_CHECK_EQUAL(size1, expected_size);
}

BOOST_AUTO_TEST_CASE(tx_size_bytes_different_transactions)
{
    // Test that different transactions have different sizes
    CMutableTransaction tx1, tx2;
    
    tx1.vin.resize(1);
    tx1.vin[0].prevout = COutPoint(uint256::ONE, 0);
    tx1.vout.resize(1);
    tx1.vout[0].nValue = 50 * COIN;
    tx1.vout[0].scriptPubKey = NativeOruSpk();
    
    tx2.vin.resize(2);
    tx2.vin[0].prevout = COutPoint(uint256::ONE, 0);
    tx2.vin[1].prevout = COutPoint(uint256::ONE, 1);
    tx2.vout.resize(2);
    tx2.vout[0].nValue = 25 * COIN;
    tx2.vout[1].nValue = 25 * COIN;
    tx2.vout[0].scriptPubKey = NativeOruSpk();
    tx2.vout[1].scriptPubKey = NativeOruSpk();
    
    CTransaction tx1_const(tx1);
    CTransaction tx2_const(tx2);
    
    size_t size1 = rigor::GetTransactionSizeBytes(tx1_const);
    size_t size2 = rigor::GetTransactionSizeBytes(tx2_const);
    
    // Different transactions should have different sizes
    BOOST_CHECK_NE(size1, size2);
}

BOOST_AUTO_TEST_CASE(fee_rate_comparison_consistency)
{
    // RIGOR Fee Mechanism Specification §1.3: Fee Rate Definition (ordering only, never enters consensus)
    // RIGOR Fee Mechanism Specification §7.1: fee_rate ordering is consistent
    
    // Test transitivity: if A > B and B > C, then A > C
    uint64_t fee_a = 100, fee_b = 50, fee_c = 25;
    size_t size_a = 100, size_b = 100, size_c = 100;
    
    BOOST_CHECK(rigor::CompareFeeRate(fee_a, size_a, fee_b, size_b));
    BOOST_CHECK(rigor::CompareFeeRate(fee_b, size_b, fee_c, size_c));
    BOOST_CHECK(rigor::CompareFeeRate(fee_a, size_a, fee_c, size_c));
    
    // Test consistency across multiple calls
    BOOST_CHECK_EQUAL(rigor::CompareFeeRate(fee_a, size_a, fee_b, size_b),
                      rigor::CompareFeeRate(fee_a, size_a, fee_b, size_b));
}

BOOST_AUTO_TEST_CASE(fee_rate_comparison_basic)
{
    // RIGOR Fee Mechanism Specification §1.3: Fee Rate Definition (ordering only, never enters consensus)
    // a ranks above b ⇔ (a.fee * b.size) > (b.fee * a.size)
    
    // Transaction A: fee=100, size=200 bytes (rate=0.5)
    // Transaction B: fee=50, size=200 bytes (rate=0.25)
    // A should have higher fee rate than B
    BOOST_CHECK(rigor::CompareFeeRate(100, 200, 50, 200));
    
    // Transaction A: fee=100, size=200 bytes (rate=0.5)
    // Transaction B: fee=100, size=100 bytes (rate=1.0)
    // B should have higher fee rate than A
    BOOST_CHECK(!rigor::CompareFeeRate(100, 200, 100, 100));
    
    // Equal fee rates
    BOOST_CHECK(!rigor::CompareFeeRate(100, 200, 50, 100));
}

BOOST_AUTO_TEST_CASE(f_i_calculation_res_mint)
{
    // RIGOR Fee Mechanism Specification §2: f_i Definition (consensus-binding, absolutely core)
    // f_i = if RES Mint: Convergen_amount (ORUM)
    // RIGOR Fee Mechanism Specification §7.1: f_i enters consensus correctly
    //
    // The asset domain comes from the script: witness v0 is ORU, witness v1 is RES except for
    // the CVG2 program, which is the ORU-domain convergence burn. A P2PKH/P2SH fixture — what
    // this case used to build — is not in either domain, so it could never have exercised the
    // mint branch. What must hold is that f_i is the convergence amount and NOT the fee.

    CCoinsView backed;
    CCoinsViewCache view(&backed);

    const CAmount input_value = 100 * COIN;
    const CAmount convergence_amount = 30 * COIN;
    const CAmount change = 60 * COIN; // leaves a 10 ORU fee, deliberately different from f_i

    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].prevout = COutPoint(uint256::ONE, 0);

    Coin coin;
    coin.out.nValue = input_value;
    coin.out.scriptPubKey = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x22); // ORU
    coin.nHeight = 100;
    view.AddCoin(tx.vin[0].prevout, std::move(coin), false);

    tx.vout.resize(3);
    tx.vout[0].nValue = convergence_amount;
    tx.vout[0].scriptPubKey = Consensus::ConvergenceOutputScript();
    tx.vout[1].nValue = change;
    tx.vout[1].scriptPubKey = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x22);
    // A RES payout output is what makes this a mint rather than a plain ORU transfer.
    tx.vout[2].nValue = 0;
    tx.vout[2].scriptPubKey = TestNativeResScript();

    const CTransaction tx_const(tx);
    BOOST_REQUIRE(rigor::IsRESMintTx(tx_const, view).value_or(false));

    uint64_t f_i = 0;
    BOOST_REQUIRE(rigor::CalculateF_i(tx_const, view, f_i));
    BOOST_CHECK_EQUAL(f_i, static_cast<uint64_t>(convergence_amount));

    // Pin the distinction: the fee is a different number, so a regression that returned the fee
    // on the mint branch cannot pass.
    uint64_t fee = 0;
    BOOST_REQUIRE(rigor::CalculateFee_ORUM(tx_const, view, fee));
    BOOST_CHECK_EQUAL(fee, static_cast<uint64_t>(input_value - convergence_amount - change));
    BOOST_CHECK(f_i != fee);
}

BOOST_AUTO_TEST_CASE(f_i_calculation_res_spend)
{
    // RIGOR Fee Mechanism Specification §2: f_i Definition (consensus-binding, absolutely core)
    // f_i = if RES Spend: fee_i (ORUM)
    // RIGOR Fee Mechanism Specification §7.1: f_i enters consensus correctly
    //
    // A RES spend pays its fee in ORU — RES is never a fee asset (§7.3) — so the transaction has
    // to carry an ORU input alongside the RES one, and f_i is that ORU differential.

    CCoinsView backed;
    CCoinsViewCache view(&backed);

    const CAmount res_in = 100 * COIN;
    const CAmount res_out = 100 * COIN; // RES is conserved; it is not the fee
    const CAmount oru_in = 5 * COIN;
    const CAmount oru_out = 3 * COIN;

    CMutableTransaction tx;
    tx.vin.resize(2);
    tx.vin[0].prevout = COutPoint(uint256::ONE, 0);
    tx.vin[1].prevout = COutPoint(uint256::ONE, 1);

    Coin res_coin;
    res_coin.out.nValue = res_in;
    res_coin.out.scriptPubKey = TestNativeResScript(); // RES
    res_coin.nHeight = 100;
    view.AddCoin(tx.vin[0].prevout, std::move(res_coin), false);

    Coin oru_coin;
    oru_coin.out.nValue = oru_in;
    oru_coin.out.scriptPubKey = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x22); // ORU
    oru_coin.nHeight = 100;
    view.AddCoin(tx.vin[1].prevout, std::move(oru_coin), false);

    tx.vout.resize(2);
    tx.vout[0].nValue = res_out;
    tx.vout[0].scriptPubKey = TestNativeResScript2();
    tx.vout[1].nValue = oru_out;
    tx.vout[1].scriptPubKey = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x22);

    const CTransaction tx_const(tx);
    BOOST_REQUIRE(!rigor::IsRESMintTx(tx_const, view).value_or(true));

    uint64_t f_i = 0;
    BOOST_REQUIRE(rigor::CalculateF_i(tx_const, view, f_i));
    BOOST_CHECK_EQUAL(f_i, static_cast<uint64_t>(oru_in - oru_out));
}

// ============================================================================
// RIGOR Fee Mechanism Specification §7.3: extreme-value tests
// ✔ minimal tx
// ✔ oversized tx
// ✔ fee = 1
// ✔ fee near the uint64 upper bound
// ============================================================================

BOOST_AUTO_TEST_CASE(extreme_test_very_small_fee)
{
    // RIGOR Fee Mechanism Specification §7.3: fee = 1
    // minimal tx
    
    CCoinsView backed;
    CCoinsViewCache view(&backed);
    
    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].prevout = COutPoint(uint256::ONE, 0);
    tx.vout.resize(1);
    
    // Set up input: 101 ORUM
    Coin coin;
    coin.out.nValue = 101;
    coin.out.scriptPubKey = NativeOruSpk();
    coin.nHeight = 100;
    view.AddCoin(tx.vin[0].prevout, std::move(coin), false);
    
    // Set up output: 100 ORUM (fee = 1)
    tx.vout[0].nValue = 100;
    tx.vout[0].scriptPubKey = NativeOruSpk();
    
    CTransaction tx_const(tx);
    uint64_t fee = 0;
    BOOST_CHECK(rigor::CalculateFee_ORUM(tx_const, view, fee));
    BOOST_CHECK_EQUAL(fee, 1);
}

BOOST_AUTO_TEST_CASE(extreme_test_very_large_fee)
{
    // RIGOR Fee Mechanism Specification §7.3: fee near the uint64 upper bound
    
    CCoinsView backed;
    CCoinsViewCache view(&backed);
    
    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].prevout = COutPoint(uint256::ONE, 0);
    tx.vout.resize(1);
    
    // Even amount so floor(input/2) + floor(input/2) == input (UINT64_MAX/2 is odd).
    const uint64_t large_amount = (UINT64_MAX / 2) - 1;
    Coin coin;
    coin.out.nValue = large_amount;
    coin.out.scriptPubKey = NativeOruSpk();
    coin.nHeight = 100;
    view.AddCoin(tx.vin[0].prevout, std::move(coin), false);
    
    // Output: half of input; fee = inputs_ORUM - outputs_ORUM == large_amount / 2
    tx.vout[0].nValue = large_amount / 2;
    tx.vout[0].scriptPubKey = NativeOruSpk();
    
    CTransaction tx_const(tx);
    uint64_t fee = 0;
    BOOST_CHECK(rigor::CalculateFee_ORUM(tx_const, view, fee));
    BOOST_CHECK_EQUAL(fee, large_amount / 2);
}

BOOST_AUTO_TEST_CASE(extreme_test_very_small_tx)
{
    // RIGOR Fee Mechanism Specification §7.3: minimal tx
    
    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].prevout = COutPoint(uint256::ONE, 0);
    tx.vout.resize(1);
    tx.vout[0].nValue = 1;
    tx.vout[0].scriptPubKey = CScript() << RIGOR_MARKER_RETURN;
    
    CTransaction tx_const(tx);
    size_t size = rigor::GetTransactionSizeBytes(tx_const);
    
    // Very small transaction should have small size
    BOOST_CHECK_GT(size, 0);
    BOOST_CHECK_LT(size, 100); // Should be less than 100 bytes
}

BOOST_AUTO_TEST_CASE(extreme_test_very_large_tx)
{
    // RIGOR Fee Mechanism Specification §7.3: oversized tx
    
    CMutableTransaction tx;
    tx.vin.resize(100);
    tx.vout.resize(100);
    
    for (size_t i = 0; i < 100; ++i) {
        tx.vin[i].prevout = COutPoint(uint256::ONE, i);
        tx.vout[i].nValue = 1 * COIN;
        tx.vout[i].scriptPubKey = NativeOruSpk();
    }
    
    CTransaction tx_const(tx);
    size_t size = rigor::GetTransactionSizeBytes(tx_const);
    
    // Large transaction should have large size
    BOOST_CHECK_GT(size, 1000); // Should be more than 1KB
}

BOOST_AUTO_TEST_CASE(extreme_test_fee_rate_comparison_boundary)
{
    // RIGOR Fee Mechanism Specification §7.3: extreme-value tests
    // Test fee rate comparison with boundary values
    
    // Very small fee (1)
    BOOST_CHECK(rigor::CompareFeeRate(2, 100, 1, 100));
    BOOST_CHECK(!rigor::CompareFeeRate(1, 100, 2, 100));
    
    // Very large fee (near uint64 max)
    BOOST_CHECK(rigor::CompareFeeRate(UINT64_MAX / 2, 1000, UINT64_MAX / 4, 1000));
    BOOST_CHECK(!rigor::CompareFeeRate(UINT64_MAX / 4, 1000, UINT64_MAX / 2, 1000));
    
    // Very small size (1 byte)
    BOOST_CHECK(rigor::CompareFeeRate(100, 1, 50, 1));
    BOOST_CHECK(!rigor::CompareFeeRate(50, 1, 100, 1));
    
    // Very large size (1MB)
    BOOST_CHECK(rigor::CompareFeeRate(1000, 1000000, 500, 1000000));
    BOOST_CHECK(!rigor::CompareFeeRate(500, 1000000, 1000, 1000000));
}

BOOST_AUTO_TEST_CASE(rfc018_res_transfer_fee_burn_share)
{
    using Consensus::OruResTransferFeeBurnShare;
    using Consensus::ORU_RES_TRANSFER_FEE_BURN_NUM;
    using Consensus::ORU_RES_TRANSFER_FEE_BURN_DEN;

    BOOST_CHECK_EQUAL(ORU_RES_TRANSFER_FEE_BURN_NUM, 1);
    BOOST_CHECK_EQUAL(ORU_RES_TRANSFER_FEE_BURN_DEN, 2);

    BOOST_CHECK_EQUAL(OruResTransferFeeBurnShare(0), 0);
    BOOST_CHECK_EQUAL(OruResTransferFeeBurnShare(1), 0);
    BOOST_CHECK_EQUAL(OruResTransferFeeBurnShare(2), 1);
    BOOST_CHECK_EQUAL(OruResTransferFeeBurnShare(3), 1);
    BOOST_CHECK_EQUAL(OruResTransferFeeBurnShare(1000), 500);
    BOOST_CHECK_EQUAL(OruResTransferFeeBurnShare(1 * COIN), (1 * COIN) / 2);

    // Claimable remainder for a mixed block: ORU fee intact, RES-transfer fee half-burned.
    const CAmount oru_only_fee = 4000;
    const CAmount res_xfer_fee = 6000;
    const CAmount burn = OruResTransferFeeBurnShare(res_xfer_fee);
    const CAmount total_fees = oru_only_fee + res_xfer_fee;
    const CAmount claimable = total_fees - burn;
    BOOST_CHECK_EQUAL(burn, 3000);
    BOOST_CHECK_EQUAL(claimable, 7000);
    BOOST_CHECK_LT(claimable, total_fees);
}

BOOST_AUTO_TEST_CASE(res_display_six_decimals)
{
    BOOST_CHECK_EQUAL(RES_COIN, 1000000);
    BOOST_CHECK_NE(RES_COIN, 100);
    BOOST_CHECK_EQUAL(FormatResAmount(0), "0.000000");
    BOOST_CHECK_EQUAL(FormatResAmount(RES_COIN), "1.000000");
    BOOST_CHECK_EQUAL(FormatResAmount(RES_COIN / 10), "0.100000");
    BOOST_CHECK_EQUAL(FormatResAmount(RES_ATOMIC_UNIT), "0.000001");
}

BOOST_AUTO_TEST_SUITE_END()

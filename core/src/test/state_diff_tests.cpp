// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/state_diff.h>

#include <boost/test/unit_test.hpp>

#include <coins.h>
#include <test/util/setup_common.h>
#include <uint256.h>

#include <string>

// STEP-0102 unit tests for the deterministic chainstate diff tool.
//
// These tests cover the diff comparator and the ChainstateSummary
// serialization round-trip. They deliberately do NOT exercise
// `CaptureChainstateSummary` against a live coins view: that is the job of
// a higher-level integration test (covered separately and run by the user
// via `make check`). The contract here is the canonical first-divergence
// behavior and the byte-stable wire format, both of which are pure
// functions of the struct values.

BOOST_FIXTURE_TEST_SUITE(state_diff_tests, BasicTestingSetup)

namespace {

//! Build a minimal but fully-populated ChainstateSummary for tests.
Consensus::ChainstateSummary BuildSampleSummary()
{
    Consensus::ChainstateSummary s;
    s.height = 42;
    s.block_hash = uint256::ONE;
    s.state_hash = uint256{}; // zero
    s.utxo_count = 7;
    s.res_balance = 12345;
    s.other_balance = 67890;
    s.mint_count = 3;
    s.lock_count = 5;
    s.converge_count = 0;

    for (size_t i = 0; i < Consensus::ChainstateSummary::kBucketCount; ++i) {
        // Deterministic, non-zero, distinct per-bucket value. Two summaries
        // built by this same function MUST agree byte-for-byte.
        uint256 v;
        v.begin()[0] = static_cast<unsigned char>(i);
        v.begin()[31] = static_cast<unsigned char>(i ^ 0xA5u);
        s.utxo_bucket_hashes[i] = v;
    }

    s.selection_present = true;
    s.selection_has_winner = true;
    s.selection_winner_index = 1;
    s.selection_candidate_count = 4;
    s.selection_winner_weight = 9000;
    s.selection_convergence_root = uint256::ONE;
    s.selection_winner_txid = uint256::ONE;
    s.selection_sigmas_hash = uint256::ONE;
    s.selection_weights_hash = uint256::ONE;
    return s;
}

} // namespace

// 1. Two identically-populated summaries must compare as identical.
BOOST_AUTO_TEST_CASE(diff_identical_summaries_report_identical)
{
    const auto a = BuildSampleSummary();
    const auto b = BuildSampleSummary();

    const auto r = Consensus::DiffChainstateSummaries(a, b);
    BOOST_CHECK(r.identical);
    BOOST_CHECK_EQUAL(r.field, "");
    BOOST_CHECK_EQUAL(r.bucket_index, -1);
}

// 2. The first coarse-field divergence is reported, even if many fields differ.
BOOST_AUTO_TEST_CASE(diff_returns_first_coarse_field_in_canonical_order)
{
    auto a = BuildSampleSummary();
    auto b = BuildSampleSummary();

    // Mutate two coarse fields; the diff must report `height` (#1), not
    // `state_hash` (#3), because canonical order says so.
    b.height = a.height + 1;
    b.state_hash = uint256::ONE;

    const auto r = Consensus::DiffChainstateSummaries(a, b);
    BOOST_CHECK(!r.identical);
    BOOST_CHECK_EQUAL(r.field, "height");
    BOOST_CHECK_EQUAL(r.bucket_index, -1);
    BOOST_CHECK_EQUAL(r.serialized_byte_offset, 4);
    BOOST_CHECK_EQUAL(r.wire_segment, "height");
}

// 2b. Serialized-wire scan matches semantic height field for the same mutation.
BOOST_AUTO_TEST_CASE(diff_serialized_detects_height_byte_change)
{
    auto a = BuildSampleSummary();
    auto b = BuildSampleSummary();
    b.height = a.height + 1;

    const std::string wa = a.Serialize();
    const std::string wb = b.Serialize();
    const auto r = Consensus::DiffSerializedChainstateSummaries(wa, wb);
    BOOST_CHECK(!r.identical);
    BOOST_CHECK_EQUAL(r.field, "serialized_byte");
    BOOST_CHECK_EQUAL(r.serialized_byte_offset, 4);
    BOOST_CHECK_EQUAL(r.wire_segment, "height");
}

// 2c. Identical wires compare identical under the byte scanner.
BOOST_AUTO_TEST_CASE(diff_serialized_identical_for_equal_serialize)
{
    const auto s = BuildSampleSummary();
    const std::string w = s.Serialize();
    const auto r = Consensus::DiffSerializedChainstateSummaries(w, w);
    BOOST_CHECK(r.identical);
}

// 2d. Truncated wire reports `wire_length` at the first past-end offset.
BOOST_AUTO_TEST_CASE(diff_serialized_truncation_reports_length)
{
    const auto s = BuildSampleSummary();
    const std::string wa = s.Serialize();
    std::string wb = wa.substr(0, wa.size() - 1);
    const auto r = Consensus::DiffSerializedChainstateSummaries(wa, wb);
    BOOST_CHECK(!r.identical);
    BOOST_CHECK_EQUAL(r.field, "wire_length");
    BOOST_CHECK_EQUAL(r.serialized_byte_offset, static_cast<int64_t>(wb.size()));
}

// 3. When only state_hash differs (counters agree), the diff returns at
//    state_hash without entering bucket drilldown.
BOOST_AUTO_TEST_CASE(diff_state_hash_short_circuits_before_buckets)
{
    auto a = BuildSampleSummary();
    auto b = BuildSampleSummary();
    b.state_hash = uint256::ONE;

    const auto r = Consensus::DiffChainstateSummaries(a, b);
    BOOST_CHECK(!r.identical);
    BOOST_CHECK_EQUAL(r.field, "state_hash");
    BOOST_CHECK_EQUAL(r.bucket_index, -1);
}

// 4. When all coarse fields agree but a single bucket differs, the diff
//    reports `utxo_bucket_hashes` and pinpoints the first mismatching index.
BOOST_AUTO_TEST_CASE(diff_locates_first_mismatching_bucket)
{
    auto a = BuildSampleSummary();
    auto b = BuildSampleSummary();
    // Flip bucket #137 only.
    b.utxo_bucket_hashes[137] = uint256::ONE;

    const auto r = Consensus::DiffChainstateSummaries(a, b);
    BOOST_CHECK(!r.identical);
    BOOST_CHECK_EQUAL(r.field, "utxo_bucket_hashes");
    BOOST_CHECK_EQUAL(r.bucket_index, 137);
    BOOST_CHECK_EQUAL(r.serialized_byte_offset, static_cast<int64_t>(108 + 137 * 32));
    BOOST_CHECK_EQUAL(r.wire_segment, "utxo_bucket[137]");
}

// 5. Among multiple bucket mismatches, the LOWEST index is reported (canonical).
BOOST_AUTO_TEST_CASE(diff_reports_lowest_bucket_index)
{
    auto a = BuildSampleSummary();
    auto b = BuildSampleSummary();
    b.utxo_bucket_hashes[200] = uint256::ONE;
    b.utxo_bucket_hashes[50] = uint256::ONE;
    b.utxo_bucket_hashes[150] = uint256::ONE;

    const auto r = Consensus::DiffChainstateSummaries(a, b);
    BOOST_CHECK_EQUAL(r.field, "utxo_bucket_hashes");
    BOOST_CHECK_EQUAL(r.bucket_index, 50);
}

// 6. When only one side reports a selection context, the diff flags
//    `selection_present` (canonical), not a downstream selection field.
BOOST_AUTO_TEST_CASE(diff_selection_present_takes_precedence)
{
    auto a = BuildSampleSummary();
    auto b = BuildSampleSummary();
    b.selection_present = false;
    b.selection_has_winner = false;
    b.selection_winner_index = -1;
    b.selection_candidate_count = 0;
    b.selection_winner_weight = 0;
    b.selection_convergence_root = uint256{};
    b.selection_winner_txid = uint256{};
    b.selection_sigmas_hash = uint256{};
    b.selection_weights_hash = uint256{};

    const auto r = Consensus::DiffChainstateSummaries(a, b);
    BOOST_CHECK_EQUAL(r.field, "selection_present");
}

// 7. Serialization round-trip is byte-stable and lossless.
BOOST_AUTO_TEST_CASE(serialize_roundtrip_is_byte_stable)
{
    const auto a = BuildSampleSummary();
    const std::string bytes = a.Serialize();

    Consensus::ChainstateSummary b;
    BOOST_REQUIRE(b.Deserialize(bytes));

    const auto r = Consensus::DiffChainstateSummaries(a, b);
    BOOST_CHECK(r.identical);

    // Bytes are stable: a second serialize must produce identical bytes.
    BOOST_CHECK(bytes == b.Serialize());
}

// 8. Deserialize refuses records of incorrect size.
BOOST_AUTO_TEST_CASE(deserialize_rejects_truncated_input)
{
    const auto a = BuildSampleSummary();
    const std::string bytes = a.Serialize();

    Consensus::ChainstateSummary b;
    BOOST_CHECK(!b.Deserialize(bytes.substr(0, bytes.size() - 1)));
    BOOST_CHECK(!b.Deserialize(bytes + std::string("\x00", 1)));
}

// 9. Deserialize refuses records with wrong magic.
BOOST_AUTO_TEST_CASE(deserialize_rejects_wrong_magic)
{
    const auto a = BuildSampleSummary();
    std::string bytes = a.Serialize();
    // Clobber the first magic byte.
    bytes[0] = static_cast<char>(bytes[0] ^ 0xFFu);

    Consensus::ChainstateSummary b;
    BOOST_CHECK(!b.Deserialize(bytes));
}

// -----------------------------------------------------------------------
// TASK-H2 H-2-1: end-to-end capture-determinism sanity.
//
// Two `CaptureChainstateSummary` calls on the same chainstate must yield
// byte-identical summaries. The view used here is empty (no cursor data),
// which is sufficient to defend against the hazards H-2-1 targets:
// hidden time/RNG inputs, uninitialized struct fields, and asymmetric
// handling of the optional SelectionContextView. Full UTXO-set coverage
// is the job of the functional-test harness (run separately by the user
// via `make check`).
// -----------------------------------------------------------------------

namespace {

//! Build a minimal but non-default SelectionContextView for injection.
Consensus::SelectionContextView BuildSampleSelection()
{
    Consensus::SelectionContextView sel;
    sel.has_winner = true;
    sel.winner_index = 0;
    sel.convergence_root = uint256::ONE;
    sel.winner_txid = uint256::ONE;
    sel.winner_weight = 1234;
    sel.sigmas.push_back(uint256::ONE);
    sel.weights.push_back(1234);
    return sel;
}

} // namespace

// 10. Two captures of the same empty view must agree byte-for-byte.
BOOST_AUTO_TEST_CASE(capture_same_view_twice_yields_identical_summary)
{
    CCoinsView base;
    CCoinsViewCache view(&base);

    const Consensus::ChainstateSummary a =
        Consensus::CaptureChainstateSummary(view, /*pindex=*/nullptr, /*selection=*/nullptr);
    const Consensus::ChainstateSummary b =
        Consensus::CaptureChainstateSummary(view, /*pindex=*/nullptr, /*selection=*/nullptr);

    const auto r = Consensus::DiffChainstateSummaries(a, b);
    BOOST_CHECK(r.identical);
    BOOST_CHECK_EQUAL(r.field, "");
    // Capture must also produce a byte-stable wire image on both sides.
    BOOST_CHECK(a.Serialize() == b.Serialize());
}

// 11. Two captures with the same injected selection context must agree.
BOOST_AUTO_TEST_CASE(capture_with_same_selection_yields_identical_summary)
{
    CCoinsView base;
    CCoinsViewCache view(&base);

    const auto sel = BuildSampleSelection();
    const Consensus::ChainstateSummary a =
        Consensus::CaptureChainstateSummary(view, /*pindex=*/nullptr, &sel);
    const Consensus::ChainstateSummary b =
        Consensus::CaptureChainstateSummary(view, /*pindex=*/nullptr, &sel);

    const auto r = Consensus::DiffChainstateSummaries(a, b);
    BOOST_CHECK(r.identical);
    BOOST_CHECK(a.selection_present);
    BOOST_CHECK(a.Serialize() == b.Serialize());
}

// 12. Capturing with vs. without a selection context must surface as a
//     `selection_present` divergence (canonical ordering preserved).
BOOST_AUTO_TEST_CASE(capture_with_vs_without_selection_diverges_on_selection_present)
{
    CCoinsView base;
    CCoinsViewCache view(&base);

    const auto sel = BuildSampleSelection();
    const Consensus::ChainstateSummary with_sel =
        Consensus::CaptureChainstateSummary(view, /*pindex=*/nullptr, &sel);
    const Consensus::ChainstateSummary without_sel =
        Consensus::CaptureChainstateSummary(view, /*pindex=*/nullptr, /*selection=*/nullptr);

    BOOST_REQUIRE(with_sel.selection_present);
    BOOST_REQUIRE(!without_sel.selection_present);

    const auto r = Consensus::DiffChainstateSummaries(with_sel, without_sel);
    BOOST_CHECK(!r.identical);
    BOOST_CHECK_EQUAL(r.field, "selection_present");
}

BOOST_AUTO_TEST_SUITE_END()

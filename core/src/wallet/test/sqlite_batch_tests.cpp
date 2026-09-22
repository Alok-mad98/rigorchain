// Copyright (c) 2024-2026 The Bitcoin Core developers
// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/util/setup_common.h>
#include <util/time.h>
#include <wallet/walletdb.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include <boost/test/unit_test.hpp>

namespace wallet {
BOOST_FIXTURE_TEST_SUITE(sqlite_batch_tests, BasicTestingSetup)

//! Regression for bitcoin/bitcoin#29110: a second SQLiteBatch destructor must
//! not ROLLBACK a txn it did not start. Old Close() used
//! sqlite3_get_autocommit()==0 and aborted the outer send/TopUp txn.
BOOST_AUTO_TEST_CASE(idle_inner_batch_does_not_abort_outer_txn)
{
    std::unique_ptr<WalletDatabase> db = CreateMockWalletDatabase();
    WalletBatch outer(*db);
    BOOST_REQUIRE(outer.TxnBegin());
    BOOST_REQUIRE(outer.WriteName("outer", "keep"));
    {
        WalletBatch inner(*db);
        BOOST_CHECK(!inner.HasActiveTxn());
    }
    BOOST_CHECK_MESSAGE(outer.TxnCommit(),
                        "inner SQLiteBatch destructor must not ROLLBACK the outer txn");
}

//! bitcoin/bitcoin#29112: writers on one connection are serialized. The second
//! batch waits until the first commits, then its write succeeds.
BOOST_AUTO_TEST_CASE(concurrent_write_waits_for_open_txn)
{
    std::unique_ptr<WalletDatabase> db = CreateMockWalletDatabase();
    std::atomic<bool> started{false};
    std::atomic<bool> inner_done{false};
    std::thread writer([&] {
        while (!started.load()) {
            UninterruptibleSleep(std::chrono::milliseconds{1});
        }
        WalletBatch inner(*db);
        BOOST_CHECK(inner.WriteName("inner", "ok"));
        inner_done = true;
    });

    WalletBatch outer(*db);
    BOOST_REQUIRE(outer.TxnBegin());
    BOOST_REQUIRE(outer.WriteName("outer", "first"));
    started = true;
    UninterruptibleSleep(std::chrono::milliseconds{50});
    BOOST_CHECK(!inner_done.load());
    BOOST_REQUIRE(outer.TxnCommit());
    writer.join();
    BOOST_CHECK(inner_done.load());
}

//! bitcoin/bitcoin ee43743: reopening a mock/in-memory DB would drop all rows.
BOOST_AUTO_TEST_CASE(in_memory_database_cannot_reopen)
{
    std::unique_ptr<WalletDatabase> db = CreateMockWalletDatabase();
    db->Close();
    BOOST_CHECK_THROW(db->Open(), std::runtime_error);
}

BOOST_AUTO_TEST_SUITE_END()
} // namespace wallet

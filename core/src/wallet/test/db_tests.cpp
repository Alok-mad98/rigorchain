// Copyright (c) 2018-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <test/util/setup_common.h>
#include <util/fs.h>
#include <util/translation.h>
#include <wallet/db.h>
#include <wallet/walletdb.h>

#include <fstream>
#include <string>
#include <vector>

namespace wallet {
BOOST_FIXTURE_TEST_SUITE(db_tests, BasicTestingSetup)

static void WriteFakeBerkeleyDBFile(const fs::path& path)
{
    std::ofstream file{path, std::ios::binary | std::ios::trunc};
    std::vector<char> buf(4096, 0);
    // Little-endian Berkeley DB Btree magic at offset 12: 62 31 05 00
    buf[12] = 0x62;
    buf[13] = 0x31;
    buf[14] = 0x05;
    buf[15] = 0x00;
    file.write(buf.data(), static_cast<std::streamsize>(buf.size()));
}

BOOST_AUTO_TEST_CASE(is_berkeley_db_file_detects_magic)
{
    const fs::path path = m_args.GetDataDirNet() / "legacy.dat";
    WriteFakeBerkeleyDBFile(path);
    BOOST_CHECK(IsBerkeleyDBFile(path));
    BOOST_CHECK(!IsBerkeleyDBFile(m_args.GetDataDirNet()));
}

BOOST_AUTO_TEST_CASE(make_database_rejects_bdb_file)
{
    const fs::path path = m_args.GetDataDirNet() / "wallet.dat";
    WriteFakeBerkeleyDBFile(path);

    DatabaseOptions options;
    DatabaseStatus status;
    bilingual_str error;
    std::unique_ptr<WalletDatabase> database = MakeDatabase(path, options, status, error);
    BOOST_CHECK(!database);
    BOOST_CHECK(status == DatabaseStatus::FAILED_BAD_FORMAT);
    BOOST_CHECK(error.original.find("Berkeley DB wallet") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(make_database_rejects_bdb_wallet_dat_directory)
{
    const fs::path dir = m_args.GetDataDirNet() / "bdbwallet";
    fs::create_directories(dir);
    WriteFakeBerkeleyDBFile(dir / "wallet.dat");

    DatabaseOptions options;
    DatabaseStatus status;
    bilingual_str error;
    std::unique_ptr<WalletDatabase> database = MakeDatabase(dir, options, status, error);
    BOOST_CHECK(!database);
    BOOST_CHECK(status == DatabaseStatus::FAILED_BAD_FORMAT);
    BOOST_CHECK(error.original.find("Berkeley DB wallet") != std::string::npos);
}

//! bitcoin/bitcoin#34156: failed restore/createfromdump must not recursive-delete
//! the wallets directory (empty wallet name => wallet_path == GetWalletDir()).
BOOST_AUTO_TEST_CASE(remove_created_wallet_database_leaves_siblings)
{
    const fs::path wallets = m_args.GetDataDirNet() / "wallets";
    const fs::path sibling_dir = wallets / "keepme";
    fs::create_directories(sibling_dir);
    const fs::path sibling = sibling_dir / "funds.txt";
    {
        std::ofstream file{sibling};
        file << "do-not-delete";
    }
    const fs::path db_file = SQLiteDataFile(wallets);
    {
        std::ofstream file{db_file};
        file << "tmp-restore";
    }
    RemoveCreatedWalletDatabase(wallets);
    BOOST_CHECK(!fs::exists(db_file));
    BOOST_CHECK(fs::exists(wallets));
    BOOST_CHECK(fs::exists(sibling));
}

BOOST_AUTO_TEST_SUITE_END()
} // namespace wallet

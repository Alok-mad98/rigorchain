// Copyright (c) 2012-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <dbwrapper.h>
#include <key.h>
#include <test/util/random.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <util/string.h>

#include <array>
#include <map>
#include <memory>

#include <boost/test/unit_test.hpp>

// Test if a string consists entirely of null characters
static bool is_null_key(const std::vector<unsigned char>& key) {
    bool isnull = true;

    for (unsigned int i = 0; i < key.size(); i++)
        isnull &= (key[i] == '\x00');

    return isnull;
}

BOOST_FIXTURE_TEST_SUITE(dbwrapper_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(dbwrapper)
{
    // Perform tests both obfuscated and non-obfuscated.
    for (const bool obfuscate : {false, true}) {
        fs::path ph = m_args.GetDataDirBase() / (obfuscate ? "dbwrapper_obfuscate_true" : "dbwrapper_obfuscate_false");
        CDBWrapper dbw({.path = ph, .cache_bytes = 1 << 20, .memory_only = true, .wipe_data = false, .obfuscate = obfuscate});
        uint8_t key{'k'};
        uint256 in = InsecureRand256();
        uint256 res;

        // Ensure that we're doing real obfuscation when obfuscate=true
        BOOST_CHECK(obfuscate != is_null_key(dbwrapper_private::GetObfuscateKey(dbw)));

        BOOST_CHECK(dbw.Write(key, in));
        BOOST_CHECK(dbw.Read(key, res));
        BOOST_CHECK_EQUAL(res.ToString(), in.ToString());
    }
}

BOOST_AUTO_TEST_CASE(dbwrapper_basic_data)
{
    // Perform tests both obfuscated and non-obfuscated.
    for (bool obfuscate : {false, true}) {
        fs::path ph = m_args.GetDataDirBase() / (obfuscate ? "dbwrapper_1_obfuscate_true" : "dbwrapper_1_obfuscate_false");
        CDBWrapper dbw({.path = ph, .cache_bytes = 1 << 20, .memory_only = false, .wipe_data = true, .obfuscate = obfuscate});

        uint256 res;
        uint32_t res_uint_32;
        bool res_bool;

        // Ensure that we're doing real obfuscation when obfuscate=true
        BOOST_CHECK(obfuscate != is_null_key(dbwrapper_private::GetObfuscateKey(dbw)));

        //Simulate block raw data - "b + block hash"
        std::string key_block = "b" + InsecureRand256().ToString();

        uint256 in_block = InsecureRand256();
        BOOST_CHECK(dbw.Write(key_block, in_block));
        BOOST_CHECK(dbw.Read(key_block, res));
        BOOST_CHECK_EQUAL(res.ToString(), in_block.ToString());

        //Simulate file raw data - "f + file_number"
        std::string key_file = strprintf("f%04x", InsecureRand32());

        uint256 in_file_info = InsecureRand256();
        BOOST_CHECK(dbw.Write(key_file, in_file_info));
        BOOST_CHECK(dbw.Read(key_file, res));
        BOOST_CHECK_EQUAL(res.ToString(), in_file_info.ToString());

        //Simulate transaction raw data - "t + transaction hash"
        std::string key_transaction = "t" + InsecureRand256().ToString();

        uint256 in_transaction = InsecureRand256();
        BOOST_CHECK(dbw.Write(key_transaction, in_transaction));
        BOOST_CHECK(dbw.Read(key_transaction, res));
        BOOST_CHECK_EQUAL(res.ToString(), in_transaction.ToString());

        //Simulate UTXO raw data - "c + transaction hash"
        std::string key_utxo = "c" + InsecureRand256().ToString();

        uint256 in_utxo = InsecureRand256();
        BOOST_CHECK(dbw.Write(key_utxo, in_utxo));
        BOOST_CHECK(dbw.Read(key_utxo, res));
        BOOST_CHECK_EQUAL(res.ToString(), in_utxo.ToString());

        //Simulate last block file number - "l"
        uint8_t key_last_blockfile_number{'l'};
        uint32_t lastblockfilenumber = InsecureRand32();
        BOOST_CHECK(dbw.Write(key_last_blockfile_number, lastblockfilenumber));
        BOOST_CHECK(dbw.Read(key_last_blockfile_number, res_uint_32));
        BOOST_CHECK_EQUAL(lastblockfilenumber, res_uint_32);

        //Simulate Is Reindexing - "R"
        uint8_t key_IsReindexing{'R'};
        bool isInReindexing = InsecureRandBool();
        BOOST_CHECK(dbw.Write(key_IsReindexing, isInReindexing));
        BOOST_CHECK(dbw.Read(key_IsReindexing, res_bool));
        BOOST_CHECK_EQUAL(isInReindexing, res_bool);

        //Simulate last block hash up to which UXTO covers - 'B'
        uint8_t key_lastblockhash_uxto{'B'};
        uint256 lastblock_hash = InsecureRand256();
        BOOST_CHECK(dbw.Write(key_lastblockhash_uxto, lastblock_hash));
        BOOST_CHECK(dbw.Read(key_lastblockhash_uxto, res));
        BOOST_CHECK_EQUAL(lastblock_hash, res);

        //Simulate file raw data - "F + filename_number + filename"
        std::string file_option_tag = "F";
        uint8_t filename_length = InsecureRandBits(8);
        std::string filename = "randomfilename";
        std::string key_file_option = strprintf("%s%01x%s", file_option_tag,filename_length,filename);

        bool in_file_bool = InsecureRandBool();
        BOOST_CHECK(dbw.Write(key_file_option, in_file_bool));
        BOOST_CHECK(dbw.Read(key_file_option, res_bool));
        BOOST_CHECK_EQUAL(res_bool, in_file_bool);
   }
}

// Test batch operations
BOOST_AUTO_TEST_CASE(dbwrapper_batch)
{
    // Perform tests both obfuscated and non-obfuscated.
    for (const bool obfuscate : {false, true}) {
        fs::path ph = m_args.GetDataDirBase() / (obfuscate ? "dbwrapper_batch_obfuscate_true" : "dbwrapper_batch_obfuscate_false");
        CDBWrapper dbw({.path = ph, .cache_bytes = 1 << 20, .memory_only = true, .wipe_data = false, .obfuscate = obfuscate});

        uint8_t key{'i'};
        uint256 in = InsecureRand256();
        uint8_t key2{'j'};
        uint256 in2 = InsecureRand256();
        uint8_t key3{'k'};
        uint256 in3 = InsecureRand256();

        uint256 res;
        CDBBatch batch(dbw);

        batch.Write(key, in);
        batch.Write(key2, in2);
        batch.Write(key3, in3);

        // Remove key3 before it's even been written
        batch.Erase(key3);

        BOOST_CHECK(dbw.WriteBatch(batch));

        BOOST_CHECK(dbw.Read(key, res));
        BOOST_CHECK_EQUAL(res.ToString(), in.ToString());
        BOOST_CHECK(dbw.Read(key2, res));
        BOOST_CHECK_EQUAL(res.ToString(), in2.ToString());

        // key3 should've never been written
        BOOST_CHECK(dbw.Read(key3, res) == false);
    }
}

BOOST_AUTO_TEST_CASE(dbwrapper_iterator)
{
    // Perform tests both obfuscated and non-obfuscated.
    for (const bool obfuscate : {false, true}) {
        fs::path ph = m_args.GetDataDirBase() / (obfuscate ? "dbwrapper_iterator_obfuscate_true" : "dbwrapper_iterator_obfuscate_false");
        CDBWrapper dbw({.path = ph, .cache_bytes = 1 << 20, .memory_only = true, .wipe_data = false, .obfuscate = obfuscate});

        // The two keys are intentionally chosen for ordering
        uint8_t key{'j'};
        uint256 in = InsecureRand256();
        BOOST_CHECK(dbw.Write(key, in));
        uint8_t key2{'k'};
        uint256 in2 = InsecureRand256();
        BOOST_CHECK(dbw.Write(key2, in2));

        std::unique_ptr<CDBIterator> it(const_cast<CDBWrapper&>(dbw).NewIterator());

        // Be sure to seek past the obfuscation key (if it exists)
        it->Seek(key);

        uint8_t key_res;
        uint256 val_res;

        BOOST_REQUIRE(it->GetKey(key_res));
        BOOST_REQUIRE(it->GetValue(val_res));
        BOOST_CHECK_EQUAL(key_res, key);
        BOOST_CHECK_EQUAL(val_res.ToString(), in.ToString());

        it->Next();

        BOOST_REQUIRE(it->GetKey(key_res));
        BOOST_REQUIRE(it->GetValue(val_res));
        BOOST_CHECK_EQUAL(key_res, key2);
        BOOST_CHECK_EQUAL(val_res.ToString(), in2.ToString());

        it->Next();
        BOOST_CHECK_EQUAL(it->Valid(), false);
    }
}

// Test that we do not obfuscation if there is existing data.
BOOST_AUTO_TEST_CASE(existing_data_no_obfuscate)
{
    // We're going to share this fs::path between two wrappers
    fs::path ph = m_args.GetDataDirBase() / "existing_data_no_obfuscate";
    fs::create_directories(ph);

    // Set up a non-obfuscated wrapper to write some initial data.
    std::unique_ptr<CDBWrapper> dbw = std::make_unique<CDBWrapper>(DBParams{.path = ph, .cache_bytes = 1 << 10, .memory_only = false, .wipe_data = false, .obfuscate = false});
    uint8_t key{'k'};
    uint256 in = InsecureRand256();
    uint256 res;

    BOOST_CHECK(dbw->Write(key, in));
    BOOST_CHECK(dbw->Read(key, res));
    BOOST_CHECK_EQUAL(res.ToString(), in.ToString());

    // Call the destructor to free leveldb LOCK
    dbw.reset();

    // Now, set up another wrapper that wants to obfuscate the same directory
    CDBWrapper odbw({.path = ph, .cache_bytes = 1 << 10, .memory_only = false, .wipe_data = false, .obfuscate = true});

    // Check that the key/val we wrote with unobfuscated wrapper exists and
    // is readable.
    uint256 res2;
    BOOST_CHECK(odbw.Read(key, res2));
    BOOST_CHECK_EQUAL(res2.ToString(), in.ToString());

    BOOST_CHECK(!odbw.IsEmpty()); // There should be existing data
    BOOST_CHECK(is_null_key(dbwrapper_private::GetObfuscateKey(odbw))); // The key should be an empty string

    uint256 in2 = InsecureRand256();
    uint256 res3;

    // Check that we can write successfully
    BOOST_CHECK(odbw.Write(key, in2));
    BOOST_CHECK(odbw.Read(key, res3));
    BOOST_CHECK_EQUAL(res3.ToString(), in2.ToString());
}

// Ensure that we start obfuscating during a reindex.
BOOST_AUTO_TEST_CASE(existing_data_reindex)
{
    // We're going to share this fs::path between two wrappers
    fs::path ph = m_args.GetDataDirBase() / "existing_data_reindex";
    fs::create_directories(ph);

    // Set up a non-obfuscated wrapper to write some initial data.
    std::unique_ptr<CDBWrapper> dbw = std::make_unique<CDBWrapper>(DBParams{.path = ph, .cache_bytes = 1 << 10, .memory_only = false, .wipe_data = false, .obfuscate = false});
    uint8_t key{'k'};
    uint256 in = InsecureRand256();
    uint256 res;

    BOOST_CHECK(dbw->Write(key, in));
    BOOST_CHECK(dbw->Read(key, res));
    BOOST_CHECK_EQUAL(res.ToString(), in.ToString());

    // Call the destructor to free leveldb LOCK
    dbw.reset();

    // Simulate a -reindex by wiping the existing data store
    CDBWrapper odbw({.path = ph, .cache_bytes = 1 << 10, .memory_only = false, .wipe_data = true, .obfuscate = true});

    // Check that the key/val we wrote with unobfuscated wrapper doesn't exist
    uint256 res2;
    BOOST_CHECK(!odbw.Read(key, res2));
    BOOST_CHECK(!is_null_key(dbwrapper_private::GetObfuscateKey(odbw)));

    uint256 in2 = InsecureRand256();
    uint256 res3;

    // Check that we can write successfully
    BOOST_CHECK(odbw.Write(key, in2));
    BOOST_CHECK(odbw.Read(key, res3));
    BOOST_CHECK_EQUAL(res3.ToString(), in2.ToString());
}

BOOST_AUTO_TEST_CASE(iterator_ordering)
{
    fs::path ph = m_args.GetDataDirBase() / "iterator_ordering";
    CDBWrapper dbw({.path = ph, .cache_bytes = 1 << 20, .memory_only = true, .wipe_data = false, .obfuscate = false});
    for (int x=0x00; x<256; ++x) {
        uint8_t key = x;
        uint32_t value = x*x;
        if (!(x & 1)) BOOST_CHECK(dbw.Write(key, value));
    }

    // Check that creating an iterator creates a snapshot
    std::unique_ptr<CDBIterator> it(const_cast<CDBWrapper&>(dbw).NewIterator());

    for (unsigned int x=0x00; x<256; ++x) {
        uint8_t key = x;
        uint32_t value = x*x;
        if (x & 1) BOOST_CHECK(dbw.Write(key, value));
    }

    for (const int seek_start : {0x00, 0x80}) {
        it->Seek((uint8_t)seek_start);
        for (unsigned int x=seek_start; x<255; ++x) {
            uint8_t key;
            uint32_t value;
            BOOST_CHECK(it->Valid());
            if (!it->Valid()) // Avoid spurious errors about invalid iterator's key and value in case of failure
                break;
            BOOST_CHECK(it->GetKey(key));
            if (x & 1) {
                BOOST_CHECK_EQUAL(key, x + 1);
                continue;
            }
            BOOST_CHECK(it->GetValue(value));
            BOOST_CHECK_EQUAL(key, x);
            BOOST_CHECK_EQUAL(value, x*x);
            it->Next();
        }
        BOOST_CHECK(!it->Valid());
    }
}

BOOST_AUTO_TEST_CASE(dbwrapper_ed25519_leveldb_compat_stress)
{
    static constexpr int RECORDS = 1000;
    fs::path ph = m_args.GetDataDirBase() / "dbwrapper_ed25519_leveldb_compat_stress";
    CDBWrapper dbw({.path = ph, .cache_bytes = 1 << 20, .memory_only = false, .wipe_data = true, .obfuscate = false});

    struct ExpectedRecord {
        std::array<unsigned char, 32> priv{};
        std::array<unsigned char, 33> pub{};
    };
    std::map<std::string, ExpectedRecord> expected;
    int prefix_00 = 0;

    for (int i = 0; i < RECORDS; ++i) {
        CKey key;
        key.MakeNewKey(true);
        BOOST_REQUIRE(key.IsValid());
        BOOST_REQUIRE(key.IsCompressed());

        CPubKey pub = key.GetPubKey();
        BOOST_REQUIRE(pub.IsValid());
        BOOST_REQUIRE_EQUAL(pub.size(), 33U);
        BOOST_REQUIRE(pub[0] == 0x00);
        ++prefix_00;

        std::array<unsigned char, 32> priv_bytes{};
        std::memcpy(priv_bytes.data(), key.begin(), priv_bytes.size());
        std::array<unsigned char, 33> pub_bytes{};
        std::memcpy(pub_bytes.data(), pub.data(), pub_bytes.size());

        CDataStream payload(SER_DISK, CLIENT_VERSION);
        payload << pub;
        payload.write(AsBytes(Span{priv_bytes}));

        const std::string db_key = strprintf("edrec_%04d", i);
        std::vector<unsigned char> blob(payload.size());
        std::memcpy(blob.data(), payload.data(), payload.size());
        BOOST_REQUIRE(dbw.Write(db_key, blob));
        expected.emplace(db_key, ExpectedRecord{priv_bytes, pub_bytes});
    }

    BOOST_CHECK(prefix_00 == RECORDS);
    BOOST_TEST_MESSAGE(strprintf("ED25519 pubkey prefix distribution: 0x00=%d", prefix_00));
    BOOST_CHECK(fs::exists(ph));
    BOOST_CHECK(fs::is_directory(ph));
    BOOST_CHECK(fs::exists(ph / "CURRENT"));

    std::unique_ptr<CDBIterator> it(const_cast<CDBWrapper&>(dbw).NewIterator());
    it->Seek(std::string("edrec_"));
    int seen = 0;
    while (it->Valid()) {
        std::string db_key;
        std::vector<unsigned char> blob;
        BOOST_REQUIRE(it->GetKey(db_key));
        if (db_key.rfind("edrec_", 0) != 0) break;
        BOOST_REQUIRE(it->GetValue(blob));

        auto exp_it = expected.find(db_key);
        BOOST_REQUIRE(exp_it != expected.end());

        CDataStream ss(MakeByteSpan(blob), SER_DISK, CLIENT_VERSION);
        CPubKey parsed_pub;
        ss >> parsed_pub;
        std::array<unsigned char, 32> parsed_priv{};
        ss.read(AsWritableBytes(Span{parsed_priv}));
        BOOST_CHECK(ss.empty());
        BOOST_CHECK_EQUAL(parsed_pub.size(), 33U);
        BOOST_CHECK(parsed_pub[0] == 0x00);
        BOOST_CHECK(std::memcmp(parsed_pub.data(), exp_it->second.pub.data(), exp_it->second.pub.size()) == 0);
        BOOST_CHECK(std::memcmp(parsed_priv.data(), exp_it->second.priv.data(), exp_it->second.priv.size()) == 0);

        CDataStream legacy(MakeByteSpan(blob), SER_DISK, CLIENT_VERSION);
        const uint64_t pub_len = ReadCompactSize(legacy);
        BOOST_CHECK_EQUAL(pub_len, 33U);
        std::array<unsigned char, 33> legacy_pub{};
        legacy.read(AsWritableBytes(Span{legacy_pub}));
        std::array<unsigned char, 32> legacy_priv{};
        legacy.read(AsWritableBytes(Span{legacy_priv}));
        BOOST_CHECK(legacy.empty());
        BOOST_CHECK(legacy_pub[0] == 0x00);
        BOOST_CHECK(std::memcmp(legacy_pub.data(), exp_it->second.pub.data(), exp_it->second.pub.size()) == 0);
        BOOST_CHECK(std::memcmp(legacy_priv.data(), exp_it->second.priv.data(), exp_it->second.priv.size()) == 0);

        ++seen;
        it->Next();
    }
    BOOST_CHECK_EQUAL(seen, RECORDS);
}

struct StringContentsSerializer {
    // Used to make two serialized objects the same while letting them have different lengths
    // This is a terrible idea
    std::string str;
    StringContentsSerializer() = default;
    explicit StringContentsSerializer(const std::string& inp) : str(inp) {}

    template<typename Stream>
    void Serialize(Stream& s) const
    {
        for (size_t i = 0; i < str.size(); i++) {
            s << uint8_t(str[i]);
        }
    }

    template<typename Stream>
    void Unserialize(Stream& s)
    {
        str.clear();
        uint8_t c{0};
        while (!s.eof()) {
            s >> c;
            str.push_back(c);
        }
    }
};

BOOST_AUTO_TEST_CASE(iterator_string_ordering)
{
    fs::path ph = m_args.GetDataDirBase() / "iterator_string_ordering";
    CDBWrapper dbw({.path = ph, .cache_bytes = 1 << 20, .memory_only = true, .wipe_data = false, .obfuscate = false});
    for (int x = 0; x < 10; ++x) {
        for (int y = 0; y < 10; ++y) {
            std::string key{ToString(x)};
            for (int z = 0; z < y; ++z)
                key += key;
            uint32_t value = x*x;
            BOOST_CHECK(dbw.Write(StringContentsSerializer{key}, value));
        }
    }

    std::unique_ptr<CDBIterator> it(const_cast<CDBWrapper&>(dbw).NewIterator());
    for (const int seek_start : {0, 5}) {
        it->Seek(StringContentsSerializer{ToString(seek_start)});
        for (unsigned int x = seek_start; x < 10; ++x) {
            for (int y = 0; y < 10; ++y) {
                std::string exp_key{ToString(x)};
                for (int z = 0; z < y; ++z)
                    exp_key += exp_key;
                StringContentsSerializer key;
                uint32_t value;
                BOOST_CHECK(it->Valid());
                if (!it->Valid()) // Avoid spurious errors about invalid iterator's key and value in case of failure
                    break;
                BOOST_CHECK(it->GetKey(key));
                BOOST_CHECK(it->GetValue(value));
                BOOST_CHECK_EQUAL(key.str, exp_key);
                BOOST_CHECK_EQUAL(value, x*x);
                it->Next();
            }
        }
        BOOST_CHECK(!it->Valid());
    }
}

BOOST_AUTO_TEST_CASE(unicodepath)
{
    // Attempt to create a database with a UTF8 character in the path.
    // On Windows this test will fail if the directory is created using
    // the ANSI CreateDirectoryA call and the code page isn't UTF8.
    // It will succeed if created with CreateDirectoryW.
    fs::path ph = m_args.GetDataDirBase() / "test_runner_₿_🏃_20191128_104644";
    CDBWrapper dbw({.path = ph, .cache_bytes = 1 << 20});

    fs::path lockPath = ph / "LOCK";
    BOOST_CHECK(fs::exists(lockPath));
}

BOOST_AUTO_TEST_CASE(dbwrapper_deserialize_failure_is_not_missing)
{
    // P0-086-01: an existing key with a malformed value must throw, not
    // return false (false is "not found" and became TX_CONSENSUS).
    fs::path ph = m_args.GetDataDirBase() / "dbwrapper_deser_fail";
    CDBWrapper dbw({.path = ph, .cache_bytes = 1 << 20, .memory_only = true, .wipe_data = false, .obfuscate = false});
    const uint8_t key{'k'};
    const std::string short_val{"xx"};
    BOOST_REQUIRE(dbw.Write(key, short_val));
    uint256 res;
    BOOST_CHECK_THROW(dbw.Read(key, res), dbwrapper_error);
    uint256 missing;
    BOOST_CHECK(!dbw.Read(uint8_t{'z'}, missing));
}


BOOST_AUTO_TEST_SUITE_END()

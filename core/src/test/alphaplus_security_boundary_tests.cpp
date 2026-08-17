// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <arith_uint256.h>
#include <chainparams.h>
#include <clientversion.h>
#include <common/alphaplus_security.h>
#include <logging.h>
#include <pow.h>
#include <primitives/block.h>
#include <test/util/setup_common.h>
#include <util/fs.h>
#include <util/system.h>
#include <validation.h>

#include <boost/test/unit_test.hpp>

#include <fstream>
#include <iterator>
#include <string>

BOOST_FIXTURE_TEST_SUITE(alphaplus_security_boundary_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(network_name_mainnet)
{
    BOOST_CHECK_EQUAL(FormatAlphaPlusNetworkName("main"), "mainnet");
    BOOST_CHECK_EQUAL(FormatAlphaPlusNetworkName(""), "mainnet");
    BOOST_CHECK_EQUAL(FormatAlphaPlusNetworkName("test"), "testnet");
    BOOST_CHECK_EQUAL(FormatAlphaPlusNetworkName("regtest"), "regtest");
    BOOST_CHECK(IsAlphaPlusMainnetChain("main"));
    BOOST_CHECK(!IsAlphaPlusMainnetChain("regtest"));
}

BOOST_AUTO_TEST_CASE(assumevalid_fail_closed_on_mainnet)
{
    ArgsManager args;
    args.ForceSetArg("-assumevalid", "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    BOOST_CHECK(EnforceAssumevalidMainnetPolicy(args, "main").has_value());
    BOOST_CHECK(!EnforceAssumevalidMainnetPolicy(args, "regtest").has_value());

    ArgsManager unset;
    BOOST_CHECK(!EnforceAssumevalidMainnetPolicy(unset, "main").has_value());
}

BOOST_AUTO_TEST_CASE(insecure_rpc_credentials_rejected)
{
    BOOST_CHECK(IsKnownInsecureRpcCredential("user", "pass"));
    BOOST_CHECK(IsKnownInsecureRpcCredential("rigor", "rigor"));
    BOOST_CHECK(IsKnownInsecureRpcCredential("admin", "password"));
    BOOST_CHECK(!IsKnownInsecureRpcCredential("rigor_ab", "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
    BOOST_CHECK(!IsKnownInsecureRpcCredential("", ""));

    ArgsManager args;
    args.ForceSetArg("-rpcuser", "user");
    args.ForceSetArg("-rpcpassword", "pass");
    BOOST_CHECK(EnforceProductionRpcSecrets(args, "main").has_value());
    BOOST_CHECK(!EnforceProductionRpcSecrets(args, "regtest").has_value());

    ArgsManager defaults;
    defaults.ForceSetArg("-rpcuser", "user");
    defaults.ForceSetArg("-rpcpassword", "pass");
    const auto err = EnforceProductionRpcSecrets(defaults, "main");
    BOOST_REQUIRE(err.has_value());
    BOOST_CHECK(err->original.find("Default or well-known RPC credentials") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(rpc_secret_not_logged)
{
    BOOST_CHECK(RpcDebugLogLeaksSecrets("rpcpassword=secretvalue"));
    BOOST_CHECK(RpcDebugLogLeaksSecrets("rpcuser=user"));
    BOOST_CHECK(RpcDebugLogLeaksSecrets("Generated configuration:\nrpcpassword=abcd"));
    BOOST_CHECK(RpcDebugLogLeaksSecrets("user:xxxx"));
    BOOST_CHECK(RpcDebugLogLeaksSecrets("pass:xxxx"));
    BOOST_CHECK(!RpcDebugLogLeaksSecrets("RPC authentication initialized."));
    BOOST_CHECK(!RpcDebugLogLeaksSecrets("RPC credentials generated successfully."));

    const fs::path datadir = m_path_root / "rpc-secret-leak";
    fs::create_directories(datadir);

    ArgsManager args;
    args.ForceSetArg("-datadir", fs::PathToString(datadir));
    args.ClearPathCache();

    std::string captured;
    auto cb = LogInstance().PushBackCallback([&](const std::string& s) { captured += s; });
    const auto err = EnsureProductionRpcCredentials(args, "main");
    LogInstance().DeleteCallback(cb);
    BOOST_REQUIRE(!err.has_value());

    const std::string user = args.GetArg("-rpcuser", "");
    const std::string password = args.GetArg("-rpcpassword", "");
    BOOST_CHECK(!user.empty());
    BOOST_CHECK(!password.empty());
    BOOST_CHECK_NE(user, "user");
    BOOST_CHECK_NE(password, "pass");

    const fs::path secret_path = datadir / "rpc-auth.conf";
    BOOST_REQUIRE(fs::exists(secret_path));
    std::ifstream in{secret_path};
    const std::string file_contents{(std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()};
    BOOST_CHECK(file_contents.find("rpcuser=" + user) != std::string::npos);
    BOOST_CHECK(file_contents.find("rpcpassword=" + password) != std::string::npos);

    BOOST_CHECK(captured.find(user) == std::string::npos);
    BOOST_CHECK(captured.find(password) == std::string::npos);
    BOOST_CHECK(!RpcDebugLogLeaksSecrets(captured));
    BOOST_CHECK(captured.find("RPC authentication initialized") != std::string::npos);
    BOOST_CHECK(captured.find("RPC credentials generated successfully") != std::string::npos);

    const fs::path debug_log = datadir / "debug.log";
    if (fs::exists(debug_log)) {
        std::ifstream log_in{debug_log};
        const std::string log_contents{(std::istreambuf_iterator<char>(log_in)), std::istreambuf_iterator<char>()};
        BOOST_CHECK(log_contents.find(password) == std::string::npos);
        BOOST_CHECK(!RpcDebugLogLeaksSecrets(log_contents));
    }

    const fs::path fixture_log = m_path_root / "debug.log";
    if (fs::exists(fixture_log)) {
        std::ifstream log_in{fixture_log};
        const std::string log_contents{(std::istreambuf_iterator<char>(log_in)), std::istreambuf_iterator<char>()};
        BOOST_CHECK(log_contents.find(password) == std::string::npos);
        BOOST_CHECK(log_contents.find("rpcpassword=") == std::string::npos);
    }
}

BOOST_AUTO_TEST_CASE(production_build_identity)
{
    const std::string identity = FormatReleaseIdentity();
    BOOST_CHECK(identity.find("NETWORK=mainnet") != std::string::npos);
    BOOST_CHECK(IsCleanGitCommitStamp("1db190b06007"));
    BOOST_CHECK(IsCleanGitCommitStamp("a1b2c3d4e5f6"));
    BOOST_CHECK(!IsCleanGitCommitStamp("1db190b06007-dirty"));
    BOOST_CHECK(!IsCleanGitCommitStamp(""));
}

BOOST_AUTO_TEST_CASE(header_admission_requires_opus_hash)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus();
    CBlockHeader header;
    header.nVersion = 1;
    header.nTime = 1;
    header.nNonce = 1;
    header.nBits = UintToArith256(consensus.powLimit).GetCompact();

    // Legal compact nBits (old anti-DoS shape check) is not enough.
    BOOST_CHECK(!HasValidProofOfWork({header}, consensus));
    BOOST_CHECK(!CheckProofOfWork(header, header.nBits, consensus));

    CBlockHeader genesis = Params().GenesisBlock().GetBlockHeader();
    const auto& live = Params().GetConsensus();
    BOOST_CHECK(HasValidProofOfWork({genesis}, live));
}

BOOST_AUTO_TEST_CASE(production_profile_default_is_development)
{
    // This unit-test binary is a development build: test chains remain available.
    BOOST_CHECK(!ALPHAPLUS_PRODUCTION_MAINNET_BUILD);
    BOOST_CHECK_NO_THROW(CreateChainParams(*m_node.args, CBaseChainParams::REGTEST));
}

BOOST_AUTO_TEST_SUITE_END()

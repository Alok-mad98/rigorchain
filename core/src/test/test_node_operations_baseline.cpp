// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// PHASE C: node operations baseline tests.
// Standalone with -DNODE_OPERATIONS_STANDALONE; Boost otherwise.
// Does not recode consensus formulas. Does not call the Kernel.

#include <node/operations/node_operations_baseline.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace tfs = std::filesystem;

namespace {

bool Expect(const OperationsResult& r, OperationsDecision d, const char* label)
{
    if (r.decision == d) return true;
    std::fprintf(stderr, "FAIL %s: got %d reason=%s\n", label,
                 static_cast<int>(r.decision), r.reason ? r.reason : "");
    return false;
}

bool ExpectReason(const OperationsResult& r, const char* needle, const char* label)
{
    if (r.IsFail() && r.reason && std::strstr(r.reason, needle)) return true;
    std::fprintf(stderr, "FAIL %s: decision=%d reason=%s want substring %s\n", label,
                 static_cast<int>(r.decision), r.reason ? r.reason : "", needle);
    return false;
}

NodeConfigSnapshot MainnetDefaults()
{
    NodeConfigSnapshot s;
    s.network = NodeNetwork::MAIN;
    s.datadir = "/var/lib/rigor";
    s.logdir = "/var/lib/rigor";
    s.server = false;
    s.rpc_cookie_available = true;
    s.listen = true;
    s.maxconnections = 125;
    return s;
}

tfs::path MakeTempDir(const char* tag)
{
    const tfs::path root = tfs::temp_directory_path() / ("rigor-ops-" + std::string(tag));
    std::error_code ec;
    tfs::remove_all(root, ec);
    tfs::create_directories(root);
    return root;
}

void WriteFile(const tfs::path& p, const std::string& body)
{
    tfs::create_directories(p.parent_path());
    std::ofstream out{p};
    out << body;
}

bool TestIllegalConfigFails()
{
    {
        NodeConfigSnapshot s = MainnetDefaults();
        s.datadir.clear();
        if (!Expect(NodeOperationsBaseline::EvaluateConfig(s), OperationsDecision::FAIL_CLOSED, "empty-datadir")) {
            return false;
        }
        if (!ExpectReason(NodeOperationsBaseline::EvaluateConfig(s), "data directory", "empty-datadir-reason")) {
            return false;
        }
    }
    {
        NodeConfigSnapshot s = MainnetDefaults();
        s.mocktime = true;
        if (!Expect(NodeOperationsBaseline::EvaluateConfig(s), OperationsDecision::FAIL_CLOSED, "mocktime")) {
            return false;
        }
    }
    {
        NodeConfigSnapshot s = MainnetDefaults();
        s.assumevalid_set = true;
        if (!Expect(NodeOperationsBaseline::EvaluateConfig(s), OperationsDecision::FAIL_CLOSED, "assumevalid")) {
            return false;
        }
    }
    {
        NodeConfigSnapshot s = MainnetDefaults();
        s.minimumchainwork_set = true;
        if (!Expect(NodeOperationsBaseline::EvaluateConfig(s), OperationsDecision::FAIL_CLOSED, "minwork")) {
            return false;
        }
    }
    {
        NodeConfigSnapshot s = MainnetDefaults();
        s.acceptnonstdtxn = true;
        if (!Expect(NodeOperationsBaseline::EvaluateConfig(s), OperationsDecision::FAIL_CLOSED, "nonstd")) {
            return false;
        }
    }
    {
        NodeConfigSnapshot s = MainnetDefaults();
        s.blockversion_set = true;
        if (!Expect(NodeOperationsBaseline::EvaluateConfig(s), OperationsDecision::FAIL_CLOSED, "blockversion")) {
            return false;
        }
    }
    {
        NodeConfigSnapshot s = MainnetDefaults();
        s.network = NodeNetwork::UNKNOWN;
        if (!Expect(NodeOperationsBaseline::EvaluateConfig(s), OperationsDecision::FAIL_CLOSED, "unknown-net")) {
            return false;
        }
    }
    {
        NodeConfigSnapshot s = MainnetDefaults();
        if (!Expect(NodeOperationsBaseline::EvaluateConfig(s), OperationsDecision::PASS, "mainnet-default")) {
            return false;
        }
    }
    return true;
}

bool TestNetworkIsolation()
{
    {
        NodeConfigSnapshot s = MainnetDefaults();
        s.testnet_flag = true;
        s.regtest_flag = true;
        if (!Expect(NodeOperationsBaseline::EvaluateConfig(s), OperationsDecision::FAIL_CLOSED, "mixed-flags")) {
            return false;
        }
        if (!ExpectReason(NodeOperationsBaseline::EvaluateConfig(s), "not isolated", "mixed-flags-reason")) {
            return false;
        }
    }
    {
        NodeConfigSnapshot s = MainnetDefaults();
        s.network = NodeNetwork::TEST;
        s.testnet_flag = true;
        if (!Expect(NodeOperationsBaseline::EvaluateConfig(s), OperationsDecision::PASS, "testnet-only")) {
            return false;
        }
    }
    {
        NodeConfigSnapshot s = MainnetDefaults();
        s.network = NodeNetwork::REGTEST;
        s.regtest_flag = true;
        s.mocktime = true;
        s.acceptnonstdtxn = true;
        if (!Expect(NodeOperationsBaseline::EvaluateConfig(s), OperationsDecision::PASS, "regtest-debug-ok")) {
            return false;
        }
    }
    {
        NodeConfigSnapshot s = MainnetDefaults();
        s.chain_arg_set = true;
        s.chain_arg = "test";
        s.network = NodeNetwork::MAIN;
        if (!Expect(NodeOperationsBaseline::EvaluateConfig(s), OperationsDecision::FAIL_CLOSED, "chain-mismatch")) {
            return false;
        }
    }
    if (NodeOperationsBaseline::ParseNetwork("main") != NodeNetwork::MAIN) return false;
    if (NodeOperationsBaseline::ParseNetwork("testnet") != NodeNetwork::TEST) return false;
    if (NodeOperationsBaseline::ParseNetwork("regtest") != NodeNetwork::REGTEST) return false;
    if (NodeOperationsBaseline::ParseNetwork("nope") != NodeNetwork::UNKNOWN) return false;
    return true;
}

bool TestDataDirectoryErrors()
{
    {
        DataDirectorySnapshot d;
        d.datadir.clear();
        if (!Expect(NodeOperationsBaseline::EvaluateDataDirectory(d), OperationsDecision::FAIL_CLOSED, "dd-empty")) {
            return false;
        }
    }
    {
        DataDirectorySnapshot d;
        d.datadir = "/tmp/x";
        d.lock_held_by_other = true;
        if (!Expect(NodeOperationsBaseline::EvaluateDataDirectory(d), OperationsDecision::FAIL_CLOSED, "lock")) {
            return false;
        }
        if (!ExpectReason(NodeOperationsBaseline::EvaluateDataDirectory(d), "lock conflict", "lock-reason")) {
            return false;
        }
    }
    {
        DataDirectorySnapshot d;
        d.datadir = "/tmp/x";
        d.chainstate_corrupt = true;
        if (!Expect(NodeOperationsBaseline::EvaluateDataDirectory(d), OperationsDecision::FAIL_CLOSED, "cs-corrupt")) {
            return false;
        }
        if (!ExpectReason(NodeOperationsBaseline::EvaluateDataDirectory(d), "corrupt", "cs-corrupt-reason")) {
            return false;
        }
    }
    {
        DataDirectorySnapshot d;
        d.datadir = "/tmp/x";
        d.requested_network = NodeNetwork::MAIN;
        d.marker_present = true;
        d.marker_network = NodeNetwork::TEST;
        if (!Expect(NodeOperationsBaseline::EvaluateDataDirectory(d), OperationsDecision::FAIL_CLOSED, "net-mismatch")) {
            return false;
        }
        if (!ExpectReason(NodeOperationsBaseline::EvaluateDataDirectory(d), "network type mismatch", "net-mismatch-reason")) {
            return false;
        }
    }
    {
        DataDirectorySnapshot d;
        d.datadir = "/tmp/x";
        d.data_version = 99;
        d.software_data_version = NODE_OPS_DATA_VERSION;
        if (!Expect(NodeOperationsBaseline::EvaluateDataDirectory(d), OperationsDecision::FAIL_CLOSED, "ver-mismatch")) {
            return false;
        }
    }

    const tfs::path tmp = MakeTempDir("datadir");
    {
        const auto snap = NodeOperationsBaseline::InspectDataDirectory(tmp.string(), NodeNetwork::MAIN);
        if (!Expect(NodeOperationsBaseline::EvaluateDataDirectory(snap), OperationsDecision::PASS, "fresh-dir")) {
            tfs::remove_all(tmp);
            return false;
        }
    }
    {
        WriteFile(tmp / "chainstate" / "LOG", "junk");
        auto snap = NodeOperationsBaseline::InspectDataDirectory(tmp.string(), NodeNetwork::MAIN);
        if (!snap.chainstate_corrupt) {
            std::fprintf(stderr, "FAIL expected corrupt chainstate without CURRENT\n");
            tfs::remove_all(tmp);
            return false;
        }
        if (!Expect(NodeOperationsBaseline::EvaluateDataDirectory(snap), OperationsDecision::FAIL_CLOSED,
                    "inspect-corrupt")) {
            tfs::remove_all(tmp);
            return false;
        }
    }
    tfs::remove_all(tmp);

    const tfs::path tmp2 = MakeTempDir("datadir2");
    WriteFile(tmp2 / "chainstate" / "CURRENT", "MANIFEST-000001\n");
    WriteFile(tmp2 / NODE_OPS_NETWORK_MARKER, "test\n");
    {
        auto snap = NodeOperationsBaseline::InspectDataDirectory(tmp2.string(), NodeNetwork::MAIN);
        if (!Expect(NodeOperationsBaseline::EvaluateDataDirectory(snap), OperationsDecision::FAIL_CLOSED,
                    "inspect-net")) {
            tfs::remove_all(tmp2);
            return false;
        }
    }
    if (!NodeOperationsBaseline::WriteDataDirectoryMarkers(tmp2.string(), NodeNetwork::MAIN)) {
        std::fprintf(stderr, "FAIL WriteDataDirectoryMarkers\n");
        tfs::remove_all(tmp2);
        return false;
    }
    {
        auto snap = NodeOperationsBaseline::InspectDataDirectory(tmp2.string(), NodeNetwork::MAIN);
        if (snap.marker_network != NodeNetwork::MAIN || snap.data_version != NODE_OPS_DATA_VERSION) {
            std::fprintf(stderr, "FAIL markers not round-tripped\n");
            tfs::remove_all(tmp2);
            return false;
        }
        if (!Expect(NodeOperationsBaseline::EvaluateDataDirectory(snap), OperationsDecision::PASS, "markers-ok")) {
            tfs::remove_all(tmp2);
            return false;
        }
    }
    tfs::remove_all(tmp2);
    return true;
}

bool TestRpcSecurity()
{
    const auto fail_main = [](const char* m) {
        return NodeOperationsBaseline::EvaluateRpcMethod(NodeNetwork::MAIN, m);
    };
    const auto pass_reg = [](const char* m) {
        return NodeOperationsBaseline::EvaluateRpcMethod(NodeNetwork::REGTEST, m);
    };

    if (!Expect(fail_main("invalidateblock"), OperationsDecision::FAIL_CLOSED, "inv-main")) return false;
    if (!Expect(fail_main("reconsiderblock"), OperationsDecision::FAIL_CLOSED, "rec-main")) return false;
    if (!Expect(fail_main("preciousblock"), OperationsDecision::FAIL_CLOSED, "pre-main")) return false;
    if (!Expect(fail_main("generateblock"), OperationsDecision::FAIL_CLOSED, "gen-main")) return false;
    if (!Expect(fail_main("generatetoaddress"), OperationsDecision::FAIL_CLOSED, "gta-main")) return false;
    if (!Expect(fail_main("setmocktime"), OperationsDecision::FAIL_CLOSED, "mock-rpc-main")) return false;
    if (!Expect(fail_main("getnodehealth"), OperationsDecision::PASS, "health-main")) return false;
    if (!Expect(fail_main("getblockchaininfo"), OperationsDecision::PASS, "gbi-main")) return false;
    if (!Expect(fail_main("submitblock"), OperationsDecision::PASS, "submit-main")) return false;
    if (!Expect(fail_main("sendrawtransaction"), OperationsDecision::PASS, "srt-main")) return false;

    if (!Expect(pass_reg("invalidateblock"), OperationsDecision::PASS, "inv-reg")) return false;
    if (!Expect(pass_reg("generateblock"), OperationsDecision::PASS, "gen-reg")) return false;
    if (!Expect(pass_reg("setmocktime"), OperationsDecision::PASS, "mock-reg")) return false;

    if (NodeOperationsBaseline::ClassifyRpcMethod("invalidateblock") != RpcMethodClass::CONSENSUS_MUTATION) {
        std::fprintf(stderr, "FAIL classify invalidateblock\n");
        return false;
    }
    if (NodeOperationsBaseline::ClassifyRpcMethod("generateblock") != RpcMethodClass::BLOCK_INJECT) {
        std::fprintf(stderr, "FAIL classify generateblock\n");
        return false;
    }

    {
        NodeConfigSnapshot s = MainnetDefaults();
        s.server = true;
        s.rpc_cookie_available = false;
        s.rpc_password_set = false;
        s.rpc_auth_set = false;
        if (!Expect(NodeOperationsBaseline::EvaluateConfig(s), OperationsDecision::FAIL_CLOSED, "no-auth")) {
            return false;
        }
    }
    {
        NodeConfigSnapshot s = MainnetDefaults();
        s.server = true;
        s.rpc_password_set = true;
        s.rpc_user = "user";
        s.rpc_password = "pass";
        if (!Expect(NodeOperationsBaseline::EvaluateConfig(s), OperationsDecision::FAIL_CLOSED, "weak-pass")) {
            return false;
        }
    }
    {
        NodeConfigSnapshot s = MainnetDefaults();
        s.server = true;
        s.rpc_bind = {"0.0.0.0"};
        s.rpc_allowip = {"0.0.0.0/0"};
        if (!Expect(NodeOperationsBaseline::EvaluateConfig(s), OperationsDecision::FAIL_CLOSED, "open-rpc")) {
            return false;
        }
    }
    {
        NodeConfigSnapshot s = MainnetDefaults();
        s.server = true;
        s.rpc_cookie_available = true;
        if (!Expect(NodeOperationsBaseline::EvaluateConfig(s), OperationsDecision::PASS, "cookie-localhost")) {
            return false;
        }
        if (NodeOperationsBaseline::IsOpenRpcWildcard(s)) {
            std::fprintf(stderr, "FAIL default bind must be localhost-restricted\n");
            return false;
        }
    }
    {
        NodeConfigSnapshot s = MainnetDefaults();
        s.rest_enable = true;
        if (!Expect(NodeOperationsBaseline::EvaluateConfig(s), OperationsDecision::FAIL_CLOSED, "rest-main")) {
            return false;
        }
    }
    return true;
}

bool TestLogRedaction()
{
    const std::string a = NodeOperationsBaseline::SanitizeLogLine(
        "rpcpassword=supersecret value");
    if (a.find("supersecret") != std::string::npos) {
        std::fprintf(stderr, "FAIL rpcpassword leaked: %s\n", a.c_str());
        return false;
    }
    if (a.find("***") == std::string::npos) {
        std::fprintf(stderr, "FAIL rpcpassword not redacted: %s\n", a.c_str());
        return false;
    }
    const std::string b = NodeOperationsBaseline::SanitizeLogLine(
        "dump wif=5HueCGU8rMjxEXxiPuD5BDku4MkFqeZyd4dZ1jvhTVqvbTLvyTJ leftover");
    if (b.find("5HueCGU8rMjxEXxiPuD5BDku4MkFqeZyd4dZ1jvhTVqvbTLvyTJ") != std::string::npos) {
        std::fprintf(stderr, "FAIL wif leaked: %s\n", b.c_str());
        return false;
    }
    const std::string hex(128, 'a');
    const std::string c = NodeOperationsBaseline::SanitizeLogLine("sig " + hex);
    if (c.find(hex) != std::string::npos) {
        std::fprintf(stderr, "FAIL hex blob leaked\n");
        return false;
    }
    const std::string d = NodeOperationsBaseline::SanitizeLogLine(
        "signature=aabbccddeeff00112233445566778899");
    if (d.find("aabbccddeeff") != std::string::npos) {
        std::fprintf(stderr, "FAIL signature leaked: %s\n", d.c_str());
        return false;
    }
    if (NodeOperationsBaseline::LooksLikeSecret("rpcpassword=x")) {
        // ok
    } else {
        std::fprintf(stderr, "FAIL LooksLikeSecret rpcpassword\n");
        return false;
    }
    const std::string log = NodeOperationsBaseline::FormatOpsLog(
        OpsLogEvent::CONSENSUS_REJECT, "bad-txns-premature-spend-of-coinbase");
    if (log.find("consensus_reject") == std::string::npos ||
        log.find("bad-txns-premature-spend-of-coinbase") == std::string::npos) {
        std::fprintf(stderr, "FAIL consensus reject not auditable: %s\n", log.c_str());
        return false;
    }
    const std::string leaked = NodeOperationsBaseline::FormatOpsLog(
        OpsLogEvent::FATAL, "rpcpassword=hunter2");
    if (leaked.find("hunter2") != std::string::npos) {
        std::fprintf(stderr, "FAIL FormatOpsLog leaked secret: %s\n", leaked.c_str());
        return false;
    }
    return true;
}

bool TestHealthEndpoint()
{
    NodeHealthSnapshot h = NodeOperationsBaseline::MakeEmptyHealth();
    h.height = 42;
    h.best_block_hash = std::string(64, 'b');
    h.peer_count = 8;
    h.syncing = true;
    h.database_status = DatabaseHealth::OK;
    h.mempool_size = 3;
    h.mempool_bytes = 1200;
    h.kernel_status = KernelHealth::AVAILABLE;
    h.network = NodeNetwork::MAIN;
    const std::string json = NodeOperationsBaseline::FormatHealth(h);
    const char* need[] = {
        "\"height\": 42",
        "\"best_block_hash\":",
        "\"peer_count\": 8",
        "\"syncing\": true",
        "\"database_status\": \"ok\"",
        "\"mempool_size\": 3",
        "\"consensus_kernel_status\": \"available\"",
    };
    for (const char* n : need) {
        if (json.find(n) == std::string::npos) {
            std::fprintf(stderr, "FAIL health missing %s\n%s\n", n, json.c_str());
            return false;
        }
    }
    return true;
}

bool TestUpgradeRecovery()
{
    UpgradeSnapshot u;
    u.data_version = -1;
    u.software_data_version = NODE_OPS_DATA_VERSION;
    if (!Expect(NodeOperationsBaseline::EvaluateUpgrade(u), OperationsDecision::PASS, "fresh-up")) return false;
    u.data_version = NODE_OPS_DATA_VERSION;
    if (!Expect(NodeOperationsBaseline::EvaluateUpgrade(u), OperationsDecision::PASS, "same-up")) return false;
    u.data_version = 2;
    if (!Expect(NodeOperationsBaseline::EvaluateUpgrade(u), OperationsDecision::FAIL_CLOSED, "future-up")) return false;
    u.data_version = NODE_OPS_DATA_VERSION;
    u.would_auto_modify_chainstate = true;
    if (!Expect(NodeOperationsBaseline::EvaluateUpgrade(u), OperationsDecision::FAIL_CLOSED, "auto-mod")) return false;
    const auto hints = NodeOperationsBaseline::BackupPathHints();
    bool saw_wallet = false, saw_cs = false, saw_conf = false;
    for (const auto& h : hints) {
        if (h.find("wallets") != std::string::npos) saw_wallet = true;
        if (h.find("chainstate") != std::string::npos) saw_cs = true;
        if (h.find("rigor.conf") != std::string::npos) saw_conf = true;
    }
    if (!saw_wallet || !saw_cs || !saw_conf) {
        std::fprintf(stderr, "FAIL backup hints incomplete\n");
        return false;
    }
    return true;
}

bool TestIsolationInvariants()
{
    // Operations result has only PASS / FAIL_CLOSED.
    if (static_cast<int>(OperationsDecision::PASS) != 0) return false;
    if (static_cast<int>(OperationsDecision::FAIL_CLOSED) != 1) return false;
    const auto r = NodeOperationsBaseline::EvaluateConfig(MainnetDefaults());
    if (!r.IsPass()) return false;
    return true;
}

} // namespace

#ifdef NODE_OPERATIONS_STANDALONE
int main()
{
    int fails = 0;
    auto run = [&](bool (*fn)(), const char* name) {
        if (!fn()) {
            std::fprintf(stderr, "FAILED %s\n", name);
            ++fails;
        } else {
            std::printf("PASS %s\n", name);
        }
    };
    run(TestIllegalConfigFails, "illegal_config");
    run(TestNetworkIsolation, "network_isolation");
    run(TestDataDirectoryErrors, "data_directory");
    run(TestRpcSecurity, "rpc_security");
    run(TestLogRedaction, "log_redaction");
    run(TestHealthEndpoint, "health_endpoint");
    run(TestUpgradeRecovery, "upgrade_recovery");
    run(TestIsolationInvariants, "isolation");
    if (fails) {
        std::fprintf(stderr, "NODE_OPERATIONS_BASELINE = FAIL (%d)\n", fails);
        return 1;
    }
    std::printf("NODE_OPERATIONS_BASELINE = PASS\n");
    return 0;
}
#else
#include <boost/test/unit_test.hpp>
#include <test/util/setup_common.h>

BOOST_FIXTURE_TEST_SUITE(test_node_operations_baseline, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(illegal_config)
{
    BOOST_REQUIRE(TestIllegalConfigFails());
}

BOOST_AUTO_TEST_CASE(network_isolation)
{
    BOOST_REQUIRE(TestNetworkIsolation());
}

BOOST_AUTO_TEST_CASE(data_directory)
{
    BOOST_REQUIRE(TestDataDirectoryErrors());
}

BOOST_AUTO_TEST_CASE(rpc_security)
{
    BOOST_REQUIRE(TestRpcSecurity());
}

BOOST_AUTO_TEST_CASE(log_redaction)
{
    BOOST_REQUIRE(TestLogRedaction());
}

BOOST_AUTO_TEST_CASE(health_endpoint)
{
    BOOST_REQUIRE(TestHealthEndpoint());
}

BOOST_AUTO_TEST_CASE(upgrade_recovery)
{
    BOOST_REQUIRE(TestUpgradeRecovery());
}

BOOST_AUTO_TEST_CASE(isolation)
{
    BOOST_REQUIRE(TestIsolationInvariants());
}

BOOST_AUTO_TEST_SUITE_END()
#endif

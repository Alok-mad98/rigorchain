// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// PHASE C: node operations baseline evaluator.
// Standalone with -DNODE_OPERATIONS_STANDALONE.
// Does not recode consensus formulas. Does not call the Kernel.
// Does not open wallet files. Does not mutate chainstate.

#include <node/operations/node_operations_baseline.h>

#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace opsfs = std::filesystem;

namespace {

bool IEquals(const char* a, const char* b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        const unsigned char ca = static_cast<unsigned char>(*a);
        const unsigned char cb = static_cast<unsigned char>(*b);
        if (std::tolower(ca) != std::tolower(cb)) return false;
        ++a;
        ++b;
    }
    return *a == *b;
}

std::string Lower(std::string s)
{
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool StartsWithLower(const std::string& s, const char* pfx)
{
    const size_t n = std::strlen(pfx);
    if (s.size() < n) return false;
    for (size_t i = 0; i < n; ++i) {
        if (std::tolower(static_cast<unsigned char>(s[i])) !=
            std::tolower(static_cast<unsigned char>(pfx[i]))) {
            return false;
        }
    }
    return true;
}

bool HostIsWildcard(const std::string& host)
{
    if (host.empty()) return true;
    return host == "0.0.0.0" || host == "::" || host == "[::]" || host == "*";
}

bool CidrIsWildcard(const std::string& spec)
{
    const std::string s = Lower(spec);
    return s == "0.0.0.0/0" || s == "::/0" || s == "0.0.0.0" || s == "::";
}

std::string BindHost(const std::string& bind)
{
    if (bind.empty()) return bind;
    if (bind[0] == '[') {
        const auto end = bind.find(']');
        if (end == std::string::npos) return bind;
        return bind.substr(1, end - 1);
    }
    const auto colon = bind.rfind(':');
    if (colon == std::string::npos) return bind;
    // Distinguish host:port from bare IPv6 (multiple colons).
    if (bind.find(':') != colon) return bind;
    return bind.substr(0, colon);
}

bool FileExists(const opsfs::path& p)
{
    std::error_code ec;
    return opsfs::exists(p, ec);
}

bool DirExists(const opsfs::path& p)
{
    std::error_code ec;
    return opsfs::is_directory(p, ec);
}

bool DirNonEmpty(const opsfs::path& p)
{
    std::error_code ec;
    if (!opsfs::is_directory(p, ec)) return false;
    const auto it = opsfs::directory_iterator(p, ec);
    if (ec) return false;
    return it != opsfs::directory_iterator();
}

std::string ReadTrimmedFile(const opsfs::path& p)
{
    std::ifstream in{p};
    if (!in.is_open()) return {};
    std::string line;
    std::getline(in, line);
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
        line.pop_back();
    }
    return line;
}

bool LooksLikeWif(const std::string& s)
{
    // WIF / extended-key prefixes. Do not treat short operational words as secrets.
    if (s.size() < 50) return false;
    const char c0 = s[0];
    if (c0 != '5' && c0 != 'K' && c0 != 'L' && c0 != 'c' && c0 != '9' &&
        !(s.size() >= 4 && (StartsWithLower(s, "xprv") || StartsWithLower(s, "tprv")))) {
        return false;
    }
    size_t n = 0;
    for (unsigned char c : s) {
        if (std::isalnum(c)) ++n;
        else break;
    }
    return n >= 50;
}

bool HexRun(const char* p, size_t min_len)
{
    size_t n = 0;
    while (p[n] && std::isxdigit(static_cast<unsigned char>(p[n]))) ++n;
    return n >= min_len;
}

} // namespace

NodeNetwork NodeOperationsBaseline::ParseNetwork(const char* chain)
{
    if (!chain || !*chain) return NodeNetwork::MAIN;
    if (IEquals(chain, "main") || IEquals(chain, "mainnet")) return NodeNetwork::MAIN;
    if (IEquals(chain, "test") || IEquals(chain, "testnet") || IEquals(chain, "testnet3")) {
        return NodeNetwork::TEST;
    }
    if (IEquals(chain, "regtest")) return NodeNetwork::REGTEST;
    if (IEquals(chain, "devtest")) return NodeNetwork::DEVTEST;
    return NodeNetwork::UNKNOWN;
}

const char* NodeOperationsBaseline::NetworkName(NodeNetwork n)
{
    switch (n) {
    case NodeNetwork::MAIN: return "main";
    case NodeNetwork::TEST: return "test";
    case NodeNetwork::REGTEST: return "regtest";
    case NodeNetwork::DEVTEST: return "devtest";
    default: return "unknown";
    }
}

bool NodeOperationsBaseline::IsMainnet(NodeNetwork n)
{
    return n == NodeNetwork::MAIN;
}

bool NodeOperationsBaseline::IsInsecureRpcCredential(const std::string& user, const std::string& password)
{
    if (password.empty()) return false;
    const std::string pass = Lower(password);
    static const char* const kWeak[] = {
        "pass", "password", "rpcpassword", "bitcoin", "rigor", "admin",
        "test", "123456", "change-me", "changeme", "secret", "user",
    };
    for (const char* w : kWeak) {
        if (pass == w) return true;
    }
    const std::string u = Lower(user);
    if (!u.empty() && u == pass) return true;
    if (u == "user" && pass == "pass") return true;
    return false;
}

bool NodeOperationsBaseline::IsOpenRpcWildcard(const NodeConfigSnapshot& in)
{
    bool allow_wild = false;
    for (const auto& a : in.rpc_allowip) {
        if (CidrIsWildcard(a)) allow_wild = true;
    }
    if (!allow_wild) return false;
    if (in.rpc_bind.empty()) return true; // allowip wildcard with default bind still means "allow everyone" if bind were honored
    for (const auto& b : in.rpc_bind) {
        if (HostIsWildcard(BindHost(b))) return true;
    }
    // Explicit non-wildcard bind + wildcard allowip: still an open ACL.
    return true;
}

OperationsResult NodeOperationsBaseline::EvaluateConfig(const NodeConfigSnapshot& in)
{
    const int net_flags = static_cast<int>(in.testnet_flag) + static_cast<int>(in.regtest_flag) +
                          static_cast<int>(in.chain_arg_set);
    if (net_flags > 1) {
        return OperationsResult::Fail("network flags not isolated: use at most one of -testnet/-regtest/-chain");
    }
    if (in.network == NodeNetwork::UNKNOWN) {
        return OperationsResult::Fail("unknown network type");
    }
    if (in.chain_arg_set) {
        const NodeNetwork parsed = ParseNetwork(in.chain_arg.c_str());
        if (parsed == NodeNetwork::UNKNOWN) {
            return OperationsResult::Fail("unknown -chain value");
        }
        if (parsed != in.network) {
            return OperationsResult::Fail("network type does not match -chain");
        }
    }
    if (in.datadir.empty()) {
        return OperationsResult::Fail("data directory is empty");
    }
    if (in.maxconnections < 0) {
        return OperationsResult::Fail("invalid P2P -maxconnections");
    }

    const bool main = IsMainnet(in.network);

    if (main && in.mocktime) {
        return OperationsResult::Fail("dangerous debug option -mocktime is not allowed on mainnet");
    }
    if (main && in.acceptnonstdtxn) {
        return OperationsResult::Fail("dangerous debug option -acceptnonstdtxn is not allowed on mainnet");
    }
    if (main && in.assumevalid_set) {
        return OperationsResult::Fail("configuration must not override consensus parameter -assumevalid");
    }
    if (main && in.minimumchainwork_set) {
        return OperationsResult::Fail("configuration must not override consensus parameter -minimumchainwork");
    }
    if (main && in.blockversion_set) {
        return OperationsResult::Fail("dangerous debug option -blockversion is not allowed on mainnet");
    }
    if (main && in.checkpoints_disabled) {
        return OperationsResult::Fail("dangerous debug option -nocheckpoints is not allowed on mainnet");
    }
    if (main && in.rest_enable) {
        return OperationsResult::Fail("unauthenticated REST is not allowed on mainnet");
    }

    if (in.server) {
        const bool has_auth = in.rpc_password_set || in.rpc_auth_set || in.rpc_cookie_available;
        if (!has_auth) {
            return OperationsResult::Fail("RPC authentication is required when -server is enabled");
        }
        if (main && IsInsecureRpcCredential(in.rpc_user, in.rpc_password)) {
            return OperationsResult::Fail("insecure RPC credentials are not allowed on mainnet");
        }
        if (main && IsOpenRpcWildcard(in)) {
            return OperationsResult::Fail("RPC must not be opened to 0.0.0.0/0 on mainnet");
        }
    }

    return OperationsResult::Pass("config ok");
}

OperationsResult NodeOperationsBaseline::EvaluateDataDirectory(const DataDirectorySnapshot& in)
{
    if (in.datadir.empty()) {
        return OperationsResult::Fail("data directory is empty");
    }
    if (!in.datadir_exists) {
        return OperationsResult::Fail("data directory does not exist");
    }
    if (!in.datadir_writable) {
        return OperationsResult::Fail("data directory is not writable");
    }
    if (in.lock_held_by_other) {
        return OperationsResult::Fail("data directory lock conflict");
    }
    if (in.chainstate_corrupt) {
        return OperationsResult::Fail("chainstate data directory is corrupt");
    }
    if (in.block_index_corrupt) {
        return OperationsResult::Fail("block index is corrupt");
    }
    if (in.marker_present && in.marker_network != in.requested_network) {
        return OperationsResult::Fail("network type mismatch");
    }
    if (in.data_version >= 0 && in.data_version != in.software_data_version) {
        return OperationsResult::Fail("data version mismatch; refusing to start (no automatic migration)");
    }
    return OperationsResult::Pass("datadir ok");
}

OperationsResult NodeOperationsBaseline::EvaluateUpgrade(const UpgradeSnapshot& in)
{
    if (in.would_auto_modify_chainstate) {
        return OperationsResult::Fail("upgrade must not automatically modify chainstate");
    }
    if (in.data_version < 0) {
        return OperationsResult::Pass("fresh data version; markers will be written");
    }
    if (in.data_version != in.software_data_version) {
        return OperationsResult::Fail("data version mismatch; refusing to start (no automatic migration)");
    }
    return OperationsResult::Pass("upgrade check ok");
}

RpcMethodClass NodeOperationsBaseline::ClassifyRpcMethod(const char* method)
{
    if (!method || !*method) return RpcMethodClass::UNKNOWN_METHOD;
    if (IEquals(method, "invalidateblock") || IEquals(method, "reconsiderblock") ||
        IEquals(method, "preciousblock")) {
        return RpcMethodClass::CONSENSUS_MUTATION;
    }
    if (IEquals(method, "generate") || IEquals(method, "generatetoaddress") ||
        IEquals(method, "generateblock") || IEquals(method, "generatetodescriptor")) {
        return RpcMethodClass::BLOCK_INJECT;
    }
    if (IEquals(method, "setmocktime") || IEquals(method, "mockscheduler") ||
        IEquals(method, "invokedisallowedsyscall")) {
        return RpcMethodClass::DANGEROUS_DEBUG;
    }
    if (IEquals(method, "submitblock") || IEquals(method, "submitheader") ||
        IEquals(method, "getblocktemplate")) {
        return RpcMethodClass::MINING_SUBMIT;
    }
    if (IEquals(method, "sendrawtransaction") || IEquals(method, "testmempoolaccept")) {
        return RpcMethodClass::SAFE_WRITE;
    }
    return RpcMethodClass::SAFE_READ;
}

OperationsResult NodeOperationsBaseline::EvaluateRpcMethod(NodeNetwork network, const char* method)
{
    const RpcMethodClass cls = ClassifyRpcMethod(method);
    if (!IsMainnet(network)) {
        return OperationsResult::Pass("rpc allowed on non-mainnet");
    }
    switch (cls) {
    case RpcMethodClass::CONSENSUS_MUTATION:
        return OperationsResult::Fail("RPC must not modify consensus state on mainnet");
    case RpcMethodClass::BLOCK_INJECT:
        return OperationsResult::Fail("RPC must not inject blocks on mainnet");
    case RpcMethodClass::DANGEROUS_DEBUG:
        return OperationsResult::Fail("dangerous debug RPC is closed on mainnet");
    case RpcMethodClass::UNKNOWN_METHOD:
        return OperationsResult::Pass("unknown rpc deferred to command table");
    default:
        return OperationsResult::Pass("rpc allowed");
    }
}

DataDirectorySnapshot NodeOperationsBaseline::InspectDataDirectory(const std::string& datadir,
                                                                   NodeNetwork requested)
{
    DataDirectorySnapshot s;
    s.datadir = datadir;
    s.requested_network = requested;
    s.software_data_version = NODE_OPS_DATA_VERSION;
    if (datadir.empty()) {
        s.datadir_exists = false;
        s.datadir_writable = false;
        return s;
    }
    const opsfs::path root{datadir};
    s.datadir_exists = DirExists(root);
    if (!s.datadir_exists) {
        s.datadir_writable = false;
        return s;
    }
    {
        std::error_code ec;
        const auto perms = opsfs::status(root, ec).permissions();
        s.datadir_writable = !ec && ((perms & opsfs::perms::owner_write) != opsfs::perms::none);
        // If permissions bits are inconclusive, try a probe file.
        if (s.datadir_writable) {
            const opsfs::path probe = root / ".ops-write-probe";
            std::ofstream out{probe};
            s.datadir_writable = out.is_open();
            out.close();
            std::error_code rec;
            opsfs::remove(probe, rec);
        }
    }

    const opsfs::path net_marker = root / NODE_OPS_NETWORK_MARKER;
    if (FileExists(net_marker)) {
        s.marker_present = true;
        s.marker_network = ParseNetwork(ReadTrimmedFile(net_marker).c_str());
    }
    const opsfs::path ver_marker = root / NODE_OPS_VERSION_MARKER;
    if (FileExists(ver_marker)) {
        try {
            s.data_version = std::stoi(ReadTrimmedFile(ver_marker));
        } catch (...) {
            s.data_version = -2; // unreadable → treat as mismatch later
        }
    }

    const opsfs::path chainstate = root / "chainstate";
    s.chainstate_present = DirExists(chainstate) && DirNonEmpty(chainstate);
    s.chainstate_has_current = FileExists(chainstate / "CURRENT");
    s.chainstate_corrupt = s.chainstate_present && !s.chainstate_has_current;

    const opsfs::path bindex = root / "blocks" / "index";
    s.block_index_present = DirExists(bindex) && DirNonEmpty(bindex);
    s.block_index_has_current = FileExists(bindex / "CURRENT");
    s.block_index_corrupt = s.block_index_present && !s.block_index_has_current;

    s.wallets_dir_present = DirExists(root / "wallets");
    return s;
}

bool NodeOperationsBaseline::WriteDataDirectoryMarkers(const std::string& datadir, NodeNetwork network)
{
    if (datadir.empty()) return false;
    const opsfs::path root{datadir};
    std::error_code ec;
    opsfs::create_directories(root, ec);
    if (ec && !DirExists(root)) return false;
    {
        std::ofstream net{root / NODE_OPS_NETWORK_MARKER, std::ios::trunc};
        if (!net.is_open()) return false;
        net << NetworkName(network) << '\n';
        if (!net.good()) return false;
    }
    {
        std::ofstream ver{root / NODE_OPS_VERSION_MARKER, std::ios::trunc};
        if (!ver.is_open()) return false;
        ver << NODE_OPS_DATA_VERSION << '\n';
        if (!ver.good()) return false;
    }
    return true;
}

NodeHealthSnapshot NodeOperationsBaseline::MakeEmptyHealth()
{
    NodeHealthSnapshot h;
    h.height = -1;
    h.best_block_hash = std::string(64, '0');
    h.peer_count = 0;
    h.syncing = false;
    h.database_status = DatabaseHealth::MISSING;
    h.mempool_size = 0;
    h.mempool_bytes = 0;
    h.kernel_status = KernelHealth::UNAVAILABLE;
    h.network = NodeNetwork::UNKNOWN;
    return h;
}

const char* NodeOperationsBaseline::DatabaseHealthName(DatabaseHealth s)
{
    switch (s) {
    case DatabaseHealth::OK: return "ok";
    case DatabaseHealth::MISSING: return "missing";
    case DatabaseHealth::CORRUPT: return "corrupt";
    case DatabaseHealth::LOCKED: return "locked";
    case DatabaseHealth::VERSION_MISMATCH: return "version_mismatch";
    case DatabaseHealth::NETWORK_MISMATCH: return "network_mismatch";
    }
    return "unknown";
}

const char* NodeOperationsBaseline::KernelHealthName(KernelHealth s)
{
    return s == KernelHealth::AVAILABLE ? "available" : "unavailable";
}

std::string NodeOperationsBaseline::FormatHealth(const NodeHealthSnapshot& h)
{
    std::ostringstream o;
    o << "{\n";
    o << "  \"height\": " << h.height << ",\n";
    o << "  \"best_block_hash\": \"" << h.best_block_hash << "\",\n";
    o << "  \"peer_count\": " << h.peer_count << ",\n";
    o << "  \"syncing\": " << (h.syncing ? "true" : "false") << ",\n";
    o << "  \"database_status\": \"" << DatabaseHealthName(h.database_status) << "\",\n";
    o << "  \"mempool_size\": " << h.mempool_size << ",\n";
    o << "  \"mempool_bytes\": " << h.mempool_bytes << ",\n";
    o << "  \"consensus_kernel_status\": \"" << KernelHealthName(h.kernel_status) << "\",\n";
    o << "  \"network\": \"" << NetworkName(h.network) << "\"\n";
    o << "}\n";
    return o.str();
}

const char* NodeOperationsBaseline::LogEventName(OpsLogEvent ev)
{
    switch (ev) {
    case OpsLogEvent::STARTUP: return "startup";
    case OpsLogEvent::NETWORK: return "network";
    case OpsLogEvent::SYNC: return "sync";
    case OpsLogEvent::PEER: return "peer";
    case OpsLogEvent::CONSENSUS_REJECT: return "consensus_reject";
    case OpsLogEvent::FATAL: return "fatal";
    }
    return "unknown";
}

bool NodeOperationsBaseline::LooksLikeSecret(const char* s)
{
    if (!s || !*s) return false;
    const std::string t = Lower(s);
    if (t.find("rpcpassword=") != std::string::npos) return true;
    if (t.find("rpcuser=") != std::string::npos) return true;
    if (t.find("rpcauth=") != std::string::npos) return true;
    if (t.find("dumpprivkey") != std::string::npos) return true;
    if (t.find("private_key") != std::string::npos) return true;
    if (t.find("privkey=") != std::string::npos) return true;
    if (t.find("wif=") != std::string::npos) return true;
    if (t.find("xprv") != std::string::npos) return true;
    if (LooksLikeWif(s)) return true;
    return false;
}

std::string NodeOperationsBaseline::SanitizeLogLine(const std::string& line)
{
    std::string out;
    out.reserve(line.size());
    for (size_t i = 0; i < line.size();) {
        const char* rest = line.c_str() + i;
        auto redact_until = [&](const char* key, size_t keylen) {
            if (std::strncmp(rest, key, keylen) != 0 &&
                !(keylen > 0 && IEquals(std::string(rest, rest + std::min(keylen, std::strlen(rest))).c_str(), key))) {
                return false;
            }
            // case-insensitive match of key
            bool match = true;
            for (size_t k = 0; k < keylen; ++k) {
                if (std::tolower(static_cast<unsigned char>(rest[k])) !=
                    std::tolower(static_cast<unsigned char>(key[k]))) {
                    match = false;
                    break;
                }
            }
            if (!match) return false;
            out.append(key);
            i += keylen;
            while (i < line.size() && (line[i] == ' ' || line[i] == '=' || line[i] == ':')) {
                out.push_back(line[i]);
                ++i;
            }
            out.append("***");
            while (i < line.size() && line[i] != ' ' && line[i] != '\n' && line[i] != '\r' &&
                   line[i] != ',' && line[i] != '"' && line[i] != '}') {
                ++i;
            }
            return true;
        };
        if (redact_until("rpcpassword=", 12) || redact_until("rpcuser=", 8) ||
            redact_until("rpcauth=", 8) || redact_until("privkey=", 8) ||
            redact_until("private_key=", 12) || redact_until("wif=", 4) ||
            redact_until("scriptsig=", 10) || redact_until("signature=", 10) ||
            redact_until("witness=", 8) || redact_until("seed=", 5)) {
            continue;
        }
        // Full signature / key hex blobs (64+ hex chars) after sensitive labels already handled.
        // Standalone 128-char hex after "hex:" → redact.
        if ((i == 0 || !std::isalnum(static_cast<unsigned char>(line[i - 1]))) &&
            HexRun(rest, 128)) {
            size_t n = 0;
            while (i + n < line.size() && std::isxdigit(static_cast<unsigned char>(line[i + n]))) ++n;
            out.append("[redacted-hex]");
            i += n;
            continue;
        }
        if (LooksLikeWif(std::string(rest, rest + std::min<size_t>(60, std::strlen(rest))))) {
            size_t n = 0;
            while (i + n < line.size() && std::isalnum(static_cast<unsigned char>(line[i + n]))) ++n;
            out.append("[redacted-key]");
            i += n;
            continue;
        }
        out.push_back(line[i]);
        ++i;
    }
    return out;
}

std::string NodeOperationsBaseline::FormatOpsLog(OpsLogEvent ev, const char* message)
{
    std::string body = message ? message : "";
    body = SanitizeLogLine(body);
    std::ostringstream o;
    o << "[ops:" << LogEventName(ev) << "] " << body;
    return o.str();
}

std::vector<std::string> NodeOperationsBaseline::BackupPathHints()
{
    return {
        "rigor.conf",
        "rpc-auth.conf",
        NODE_OPS_NETWORK_MARKER,
        NODE_OPS_VERSION_MARKER,
        "chainstate/",
        "blocks/",
        "wallets/   (isolated; back up separately from chainstate)",
    };
}

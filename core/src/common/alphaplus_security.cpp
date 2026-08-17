// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <common/alphaplus_security.h>

#include <chainparamsbase.h>
#include <logging.h>
#include <random.h>
#include <tinyformat.h>
#include <util/fs.h>
#include <util/fs_helpers.h>
#include <util/strencodings.h>
#include <util/system.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace {
std::string LowerAscii(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string RandomHex(size_t nbytes)
{
    std::vector<unsigned char> buf(nbytes);
    GetStrongRandBytes(buf);
    return HexStr(buf);
}

bool FileContainsLine(const fs::path& path, const std::string& needle)
{
    std::ifstream in{path};
    if (!in.is_open()) return false;
    std::string line;
    while (std::getline(in, line)) {
        if (line.find(needle) != std::string::npos) return true;
    }
    return false;
}

void RestrictOwnerReadWrite(const fs::path& path)
{
#ifndef WIN32
    std::error_code ec;
    fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace, ec);
    if (ec) {
        LogPrintf("Unable to restrict permissions on %s: %s\n", fs::PathToString(path), ec.message());
    }
#endif
}

bool AppendLine(const fs::path& path, const std::string& line)
{
    const bool existed = fs::exists(path);
    std::ofstream out{path, std::ios::app};
    if (!out.is_open()) return false;
    if (existed) out << '\n';
    out << line;
    if (!out.good()) return false;
    out.close();
    RestrictOwnerReadWrite(path);
    return true;
}
} // namespace

std::string FormatAlphaPlusNetworkName(const std::string& chain)
{
    if (chain.empty() || chain == CBaseChainParams::MAIN) return "mainnet";
    if (chain == CBaseChainParams::TESTNET) return "testnet";
    return chain;
}

bool IsAlphaPlusMainnetChain(const std::string& chain)
{
    return chain.empty() || chain == CBaseChainParams::MAIN;
}

std::optional<bilingual_str> EnforceAssumevalidMainnetPolicy(const ArgsManager& args, const std::string& chain)
{
    if (!IsAlphaPlusMainnetChain(chain)) return std::nullopt;
    if (!args.IsArgSet("-assumevalid")) return std::nullopt;
    return Untranslated(
        "ERROR:\n"
        "-assumevalid is disabled on AlphaPlus mainnet.");
}

bool IsKnownInsecureRpcCredential(const std::string& user, const std::string& password)
{
    if (password.empty()) return false;
    const std::string pass = LowerAscii(password);
    static const char* const kWeakPass[] = {
        "pass", "password", "rpcpassword", "bitcoin", "rigor", "admin",
        "test", "123456", "change-me", "changeme", "secret",
    };
    for (const char* weak : kWeakPass) {
        if (pass == weak) return true;
    }
    const std::string u = LowerAscii(user);
    if (!u.empty() && u == pass) return true;
    if (u == "user" && pass == "pass") return true;
    return false;
}

std::optional<bilingual_str> EnforceProductionRpcSecrets(const ArgsManager& args, const std::string& chain)
{
    if (!IsAlphaPlusMainnetChain(chain)) return std::nullopt;
    const std::string user = args.GetArg("-rpcuser", "");
    const std::string password = args.GetArg("-rpcpassword", "");
    if (IsKnownInsecureRpcCredential(user, password)) {
        return Untranslated(
            "ERROR:\n"
            "Default or well-known RPC credentials are not allowed on AlphaPlus mainnet.\n"
            "Remove rpcuser/rpcpassword from the configuration and restart to generate unique credentials.");
    }
    return std::nullopt;
}

std::optional<bilingual_str> EnsureProductionRpcCredentials(ArgsManager& args, const std::string& chain)
{
    if (!IsAlphaPlusMainnetChain(chain)) return std::nullopt;

    const bool has_user = args.IsArgSet("-rpcuser") && !args.GetArg("-rpcuser", "").empty();
    const bool has_pass = args.IsArgSet("-rpcpassword") && !args.GetArg("-rpcpassword", "").empty();
    const bool has_rpcauth = args.IsArgSet("-rpcauth") && !args.GetArg("-rpcauth", "").empty();
    const bool wants_cookie = args.IsArgSet("-rpccookiefile");
    if (has_user || has_pass || has_rpcauth || wants_cookie) {
        return std::nullopt;
    }

    const fs::path datadir = args.GetDataDirNet();
    if (datadir.empty()) {
        return Untranslated("ERROR:\nUnable to determine datadir for RPC credential generation.");
    }
    if (!fs::exists(datadir) && !TryCreateDirectories(datadir)) {
        return Untranslated(strprintf("ERROR:\nUnable to create datadir %s for RPC credentials.", fs::PathToString(datadir)));
    }

    const fs::path secret_path = datadir / "rpc-auth.conf";
    const std::string user = "rigor_" + RandomHex(8);
    const std::string password = RandomHex(32);

    {
        std::ofstream out{secret_path, std::ios::trunc};
        if (!out.is_open()) {
            return Untranslated(strprintf("ERROR:\nUnable to write RPC credentials to %s.", fs::PathToString(secret_path)));
        }
        out << "# AlphaPlus generated RPC credentials. Do not share. Permissions should be owner-only.\n";
        out << "rpcuser=" << user << "\n";
        out << "rpcpassword=" << password << "\n";
        if (!out.good()) {
            return Untranslated(strprintf("ERROR:\nFailed writing RPC credentials to %s.", fs::PathToString(secret_path)));
        }
    }
    RestrictOwnerReadWrite(secret_path);

    const std::string include_line = "includeconf=rpc-auth.conf";
    const fs::path node_conf = args.GetConfigFilePath();
    if (!FileContainsLine(node_conf, "includeconf=rpc-auth.conf")) {
        if (!AppendLine(node_conf, "# AlphaPlus production RPC secrets (CSW-8)\n" + include_line + "\n")) {
            return Untranslated(strprintf("ERROR:\nUnable to add includeconf to %s.", fs::PathToString(node_conf)));
        }
    }
    // Windows / operator tooling historically uses rigor.conf beside the node conf name.
    const fs::path rigor_conf = datadir / "rigor.conf";
    if (rigor_conf != node_conf && fs::exists(rigor_conf) && !FileContainsLine(rigor_conf, "includeconf=rpc-auth.conf")) {
        AppendLine(rigor_conf, include_line + "\n");
    }

    args.ForceSetArg("-rpcuser", user);
    args.ForceSetArg("-rpcpassword", password);

    const std::string banner =
        "RPC authentication initialized.\n"
        "RPC credentials generated successfully.\n"
        "Stored in " + fs::PathToString(secret_path) + " (do not share).\n";
    tfm::format(std::cout, "%s", banner);
    LogPrintf("%s", banner);
    return std::nullopt;
}

bool IsCleanGitCommitStamp(const std::string& stamp)
{
    if (stamp.empty()) return false;
    return stamp.find("-dirty") == std::string::npos;
}

bool RpcDebugLogLeaksSecrets(const std::string& log_text)
{
    const std::string lower = LowerAscii(log_text);
    static const char* const forbidden[] = {
        "rpcpassword=",
        "rpcuser=",
        "rpcauth=",
        "user:",
        "pass:",
        "pass=",
    };
    for (const char* tok : forbidden) {
        if (lower.find(tok) != std::string::npos) return true;
    }
    if (lower.find("rpcpassword") != std::string::npos) return true;
    if (lower.find("password") != std::string::npos) return true;
    return false;
}

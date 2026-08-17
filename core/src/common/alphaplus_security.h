// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_COMMON_ALPHAPLUS_SECURITY_H
#define BITCOIN_COMMON_ALPHAPLUS_SECURITY_H

#include <util/translation.h>

#include <optional>
#include <string>

class ArgsManager;

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

/** Compile-time production profile: release artifacts are mainnet-only. */
#ifdef RIGOR_PRODUCTION_MAINNET
inline constexpr bool ALPHAPLUS_PRODUCTION_MAINNET_BUILD = true;
#else
inline constexpr bool ALPHAPLUS_PRODUCTION_MAINNET_BUILD = false;
#endif

/** Map chain id (`main`, `test`, …) to the user-facing NETWORK= token. */
std::string FormatAlphaPlusNetworkName(const std::string& chain);

/** True when `chain` is AlphaPlus mainnet. */
bool IsAlphaPlusMainnetChain(const std::string& chain);

/**
 * CSW-2: `-assumevalid` is disabled on AlphaPlus mainnet.
 * Returns an error if the operator attempted an override.
 */
std::optional<bilingual_str> EnforceAssumevalidMainnetPolicy(const ArgsManager& args, const std::string& chain);

/** True for well-known shipped / example RPC secrets (user/pass, etc.). */
bool IsKnownInsecureRpcCredential(const std::string& user, const std::string& password);

/**
 * CSW-8: refuse known-default RPC secrets on mainnet.
 */
std::optional<bilingual_str> EnforceProductionRpcSecrets(const ArgsManager& args, const std::string& chain);

/**
 * CSW-8: on mainnet first start, generate unique rpcuser/rpcpassword when none exist.
 * Writes `<datadir>/rpc-auth.conf` (0600) and includeconf, then ForceSetArg for this process.
 */
std::optional<bilingual_str> EnsureProductionRpcCredentials(ArgsManager& args, const std::string& chain);

/** True when a BUILD_GIT_COMMIT stamp is non-empty and has no `-dirty` suffix. */
bool IsCleanGitCommitStamp(const std::string& stamp);

/**
 * True if log/debug text contains RPC secret material or forbidden forms
 * (`rpcpassword=`, `rpcuser=`, `rpcauth=` secret, `password`, `user:`, `pass:`).
 * Allowed operational phrases such as "RPC credentials generated successfully"
 * do not trip this check.
 */
bool RpcDebugLogLeaksSecrets(const std::string& log_text);

#endif // BITCOIN_COMMON_ALPHAPLUS_SECURITY_H

// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// PHASE C: RIGOR node operations baseline.
//
// Fail-closed evaluator for production node operations:
//   config → datadir → logs → RPC → health → upgrade/restore
//
// This module does not:
//   - modify Consensus Kernel, protocol parameters, issuance, RES, ORU, PoW, ASERT
//   - introduce a second state manager, wallet logic, or consensus formula copy
//   - auto-migrate or auto-repair chainstate
//   - implement a historical compatibility layer (mainnet is not live)
//
// Pipeline:
//   operator config / datadir / RPC method / health snapshot
//           ↓
//   NodeOperationsBaseline::Evaluate*
//           ↓
//   PASS or FAIL_CLOSED
//
// FAIL_CLOSED means the node must not start, or the RPC must not run.
// PASS never means a consensus decision.

#ifndef BITCOIN_NODE_OPERATIONS_NODE_OPERATIONS_BASELINE_H
#define BITCOIN_NODE_OPERATIONS_NODE_OPERATIONS_BASELINE_H

#include <node/operations/operations_result.h>

#include <cstdint>
#include <string>
#include <vector>

/** On-disk operations marker version. No migration layer exists. */
inline constexpr int NODE_OPS_DATA_VERSION = 1;

inline constexpr const char* NODE_OPS_NETWORK_MARKER = "ops-network";
inline constexpr const char* NODE_OPS_VERSION_MARKER = "ops-data-version";

enum class NodeNetwork : uint8_t {
    MAIN = 0,
    TEST = 1,
    REGTEST = 2,
    DEVTEST = 3,
    UNKNOWN = 255,
};

enum class OpsLogEvent : uint8_t {
    STARTUP = 0,
    NETWORK = 1,
    SYNC = 2,
    PEER = 3,
    CONSENSUS_REJECT = 4,
    FATAL = 5,
};

enum class RpcMethodClass : uint8_t {
    SAFE_READ = 0,
    SAFE_WRITE = 1,         // e.g. sendrawtransaction — still enters validation
    MINING_SUBMIT = 2,      // submitblock — still enters ProcessNewBlock
    DANGEROUS_DEBUG = 3,    // setmocktime, mockscheduler
    CONSENSUS_MUTATION = 4, // invalidateblock / reconsiderblock / preciousblock
    BLOCK_INJECT = 5,       // generate / generatetoaddress / generateblock
    UNKNOWN_METHOD = 6,
};

enum class DatabaseHealth : uint8_t {
    OK = 0,
    MISSING = 1,
    CORRUPT = 2,
    LOCKED = 3,
    VERSION_MISMATCH = 4,
    NETWORK_MISMATCH = 5,
};

enum class KernelHealth : uint8_t {
    AVAILABLE = 0,
    UNAVAILABLE = 1,
};

struct NodeConfigSnapshot {
    NodeNetwork network{NodeNetwork::MAIN};
    bool testnet_flag{false};
    bool regtest_flag{false};
    bool chain_arg_set{false};
    std::string chain_arg;
    std::string datadir;
    std::string logdir;
    bool server{false};
    bool rpc_user_set{false};
    bool rpc_password_set{false};
    bool rpc_auth_set{false};
    bool rpc_cookie_available{true}; // cookie auth is generated when password empty
    std::string rpc_user;
    std::string rpc_password;
    std::vector<std::string> rpc_bind;
    std::vector<std::string> rpc_allowip;
    bool listen{true};
    int maxconnections{125};
    bool mocktime{false};
    bool acceptnonstdtxn{false};
    bool assumevalid_set{false};
    bool minimumchainwork_set{false};
    bool blockversion_set{false};
    bool rest_enable{false};
    bool checkpoints_disabled{false};
};

struct DataDirectorySnapshot {
    std::string datadir;
    NodeNetwork requested_network{NodeNetwork::MAIN};
    NodeNetwork marker_network{NodeNetwork::UNKNOWN};
    bool marker_present{false};
    int data_version{-1}; // -1 = missing
    int software_data_version{NODE_OPS_DATA_VERSION};
    bool datadir_exists{true};
    bool datadir_writable{true};
    bool lock_held_by_other{false};
    bool chainstate_present{false};
    bool chainstate_has_current{false};
    bool chainstate_corrupt{false};
    bool block_index_present{false};
    bool block_index_has_current{false};
    bool block_index_corrupt{false};
    bool wallets_dir_present{false};
};

struct NodeHealthSnapshot {
    int32_t height{-1};
    std::string best_block_hash;
    int peer_count{0};
    bool syncing{false};
    DatabaseHealth database_status{DatabaseHealth::MISSING};
    uint64_t mempool_size{0};
    uint64_t mempool_bytes{0};
    KernelHealth kernel_status{KernelHealth::UNAVAILABLE};
    NodeNetwork network{NodeNetwork::UNKNOWN};
};

struct UpgradeSnapshot {
    int data_version{-1};
    int software_data_version{NODE_OPS_DATA_VERSION};
    bool would_auto_modify_chainstate{false};
};

class NodeOperationsBaseline
{
public:
    /** Fail-closed configuration check. Never throws. Never mutates chainstate. */
    static OperationsResult EvaluateConfig(const NodeConfigSnapshot& in);

    /** Fail-closed datadir / chainstate / network-marker check. */
    static OperationsResult EvaluateDataDirectory(const DataDirectorySnapshot& in);

    /** Fail-closed RPC method gate. Mainnet forbids mutation / inject / debug. */
    static OperationsResult EvaluateRpcMethod(NodeNetwork network, const char* method);

    /** Fail-closed upgrade check. Never auto-modifies chainstate. */
    static OperationsResult EvaluateUpgrade(const UpgradeSnapshot& in);

    /** Classify one RPC method. Unknown methods are SAFE_READ (existing table). */
    static RpcMethodClass ClassifyRpcMethod(const char* method);

    static NodeNetwork ParseNetwork(const char* chain);
    static const char* NetworkName(NodeNetwork n);
    static bool IsMainnet(NodeNetwork n);

    /** True if bind/allowip exposes RPC beyond localhost to the whole internet. */
    static bool IsOpenRpcWildcard(const NodeConfigSnapshot& in);

    /** True if credentials are well-known / empty-password-with-user. */
    static bool IsInsecureRpcCredential(const std::string& user, const std::string& password);

    /** Inspect a filesystem datadir. Does not open LevelDB. Does not lock. */
    static DataDirectorySnapshot InspectDataDirectory(const std::string& datadir,
                                                      NodeNetwork requested);

    /** Write ops-network + ops-data-version. Does not rewrite chainstate. */
    static bool WriteDataDirectoryMarkers(const std::string& datadir, NodeNetwork network);

    static NodeHealthSnapshot MakeEmptyHealth();
    static std::string FormatHealth(const NodeHealthSnapshot& h);
    static const char* DatabaseHealthName(DatabaseHealth s);
    static const char* KernelHealthName(KernelHealth s);

    /** Production log line: classified event + sanitized payload. Never logs secrets. */
    static std::string FormatOpsLog(OpsLogEvent ev, const char* message);
    static const char* LogEventName(OpsLogEvent ev);

    /** Redact private keys, RPC secrets, and raw signature material. */
    static std::string SanitizeLogLine(const std::string& line);
    static bool LooksLikeSecret(const char* s);

    /** Backup / restore path names. Informational; does not copy files. */
    static std::vector<std::string> BackupPathHints();
};

#endif // BITCOIN_NODE_OPERATIONS_NODE_OPERATIONS_BASELINE_H

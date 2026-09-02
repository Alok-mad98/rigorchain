// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// PHASE C: production hooks. Does not recode consensus formulas.

#include <node/operations/node_operations_hooks.h>

#include <kernel/cs_main.h>
#include <logging.h>
#include <net.h>
#include <node/context.h>
#include <sync.h>
#include <txmempool.h>
#include <util/fs.h>
#include <util/system.h>
#include <validation.h>

#include <string>

NodeConfigSnapshot FillNodeConfigSnapshot(const ArgsManager& args)
{
    NodeConfigSnapshot s;
    const std::string chain = args.GetChainName();
    s.network = NodeOperationsBaseline::ParseNetwork(chain.c_str());
    s.testnet_flag = args.GetBoolArg("-testnet", false);
    s.regtest_flag = args.GetBoolArg("-regtest", false);
    s.chain_arg_set = args.IsArgSet("-chain");
    s.chain_arg = args.GetArg("-chain", "");
    s.datadir = fs::PathToString(args.GetDataDirNet());
    s.logdir = s.datadir;
    s.server = args.GetBoolArg("-server", false);
    s.rpc_user = args.GetArg("-rpcuser", "");
    s.rpc_password = args.GetArg("-rpcpassword", "");
    s.rpc_user_set = args.IsArgSet("-rpcuser") && !s.rpc_user.empty();
    s.rpc_password_set = args.IsArgSet("-rpcpassword") && !s.rpc_password.empty();
    s.rpc_auth_set = args.IsArgSet("-rpcauth") && !args.GetArg("-rpcauth", "").empty();
    s.rpc_cookie_available = true;
    s.rpc_bind = args.GetArgs("-rpcbind");
    s.rpc_allowip = args.GetArgs("-rpcallowip");
    s.listen = args.GetBoolArg("-listen", true);
    s.maxconnections = static_cast<int>(args.GetIntArg("-maxconnections", 125));
    s.mocktime = args.GetIntArg("-mocktime", 0) != 0;
    s.acceptnonstdtxn = args.GetBoolArg("-acceptnonstdtxn", false);
    s.assumevalid_set = args.IsArgSet("-assumevalid");
    s.minimumchainwork_set = args.IsArgSet("-minimumchainwork");
    s.blockversion_set = args.IsArgSet("-blockversion");
    s.rest_enable = args.GetBoolArg("-rest", false);
    s.checkpoints_disabled = args.IsArgSet("-checkpoints") && !args.GetBoolArg("-checkpoints", true);
    return s;
}

NodeHealthSnapshot FillNodeHealthSnapshot(const node::NodeContext& node)
{
    NodeHealthSnapshot h = NodeOperationsBaseline::MakeEmptyHealth();
    if (node.args) {
        h.network = NodeOperationsBaseline::ParseNetwork(node.args->GetChainName().c_str());
    }
    if (node.connman) {
        h.peer_count = static_cast<int>(node.connman->GetNodeCount(ConnectionDirection::Both));
    }
    if (node.mempool) {
        LOCK(node.mempool->cs);
        h.mempool_size = node.mempool->size();
        h.mempool_bytes = node.mempool->GetTotalTxSize();
    }
    if (node.chainman) {
        h.kernel_status = KernelHealth::AVAILABLE;
        LOCK(::cs_main);
        const CBlockIndex* tip = node.chainman->ActiveTip();
        if (tip) {
            h.height = tip->nHeight;
            h.best_block_hash = tip->GetBlockHash().GetHex();
            h.database_status = DatabaseHealth::OK;
        } else {
            h.height = -1;
            h.database_status = DatabaseHealth::MISSING;
        }
        h.syncing = node.chainman->ActiveChainstate().IsInitialBlockDownload();
    } else {
        h.kernel_status = KernelHealth::UNAVAILABLE;
        h.database_status = DatabaseHealth::MISSING;
    }
    return h;
}

std::optional<std::string> RunNodeOperationsStartupChecks(const ArgsManager& args)
{
    const NodeConfigSnapshot snap = FillNodeConfigSnapshot(args);
    const OperationsResult r = NodeOperationsBaseline::EvaluateConfig(snap);
    if (r.IsFail()) {
        return std::string(r.reason ? r.reason : "node operations config check failed");
    }
    LogPrintf("%s\n", NodeOperationsBaseline::FormatOpsLog(
                          OpsLogEvent::STARTUP,
                          ("node starting network=" + std::string(NodeOperationsBaseline::NetworkName(snap.network)) +
                           " datadir_ok=1")
                              .c_str()));
    return std::nullopt;
}

std::optional<std::string> RunNodeOperationsDataDirectoryChecks(const ArgsManager& args)
{
    const NodeNetwork net = NodeOperationsBaseline::ParseNetwork(args.GetChainName().c_str());
    const std::string datadir = fs::PathToString(args.GetDataDirNet());
    DataDirectorySnapshot snap = NodeOperationsBaseline::InspectDataDirectory(datadir, net);
    const OperationsResult r = NodeOperationsBaseline::EvaluateDataDirectory(snap);
    if (r.IsFail()) {
        return std::string(r.reason ? r.reason : "node operations datadir check failed");
    }
    UpgradeSnapshot up;
    up.data_version = snap.data_version;
    up.software_data_version = NODE_OPS_DATA_VERSION;
    up.would_auto_modify_chainstate = false;
    const OperationsResult ur = NodeOperationsBaseline::EvaluateUpgrade(up);
    if (ur.IsFail()) {
        return std::string(ur.reason ? ur.reason : "node operations upgrade check failed");
    }
    if (!NodeOperationsBaseline::WriteDataDirectoryMarkers(datadir, net)) {
        return std::string("unable to write data directory operations markers");
    }
    LogPrintf("%s\n", NodeOperationsBaseline::FormatOpsLog(
                          OpsLogEvent::STARTUP, "data directory markers verified"));
    return std::nullopt;
}

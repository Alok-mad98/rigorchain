// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// PHASE C: production hooks. Not compiled in NODE_OPERATIONS_STANDALONE tests.
// Fills snapshots from ArgsManager / NodeContext. Does not call Kernel formulas.

#ifndef BITCOIN_NODE_OPERATIONS_NODE_OPERATIONS_HOOKS_H
#define BITCOIN_NODE_OPERATIONS_NODE_OPERATIONS_HOOKS_H

#include <node/operations/node_operations_baseline.h>

#include <optional>
#include <string>

class ArgsManager;
namespace node {
struct NodeContext;
}

NodeConfigSnapshot FillNodeConfigSnapshot(const ArgsManager& args);
NodeHealthSnapshot FillNodeHealthSnapshot(const node::NodeContext& node);

/** Startup config fail-closed. Error string if FAIL_CLOSED. */
std::optional<std::string> RunNodeOperationsStartupChecks(const ArgsManager& args);

/** Datadir fail-closed + write markers. Error string if FAIL_CLOSED. */
std::optional<std::string> RunNodeOperationsDataDirectoryChecks(const ArgsManager& args);

#endif // BITCOIN_NODE_OPERATIONS_NODE_OPERATIONS_HOOKS_H

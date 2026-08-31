// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P069 testnet/regtest runtime canary. Compare-only. Default OFF.
// Does not switch ConnectBlock. PRODUCTION_KERNEL_SWITCHED = FALSE

#ifndef BITCOIN_NODE_KERNEL_RUNTIME_CANARY_H
#define BITCOIN_NODE_KERNEL_RUNTIME_CANARY_H

#include <consensus/convergence_logic.h>
#include <consensus/params.h>
#include <consensus/rigor_supply.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <uint256.h>

#include <string>
#include <vector>

class ArgsManager;
class CChainParams;

static constexpr bool DEFAULT_ENABLE_CONSENSUS_KERNEL_CANARY = false;
inline constexpr const char* KERNEL_RUNTIME_CANARY_MAINNET_FORBIDDEN = "MAINNET_CANARY_FORBIDDEN";
inline constexpr const char* KERNEL_RUNTIME_CANARY_MAINNET_NOT_ARMED = "MAINNET_CANARY_NOT_ARMED";
inline constexpr const char* KERNEL_RUNTIME_CANARY_DISCREPANCY = "CONSENSUS_KERNEL_CANARY_RUNTIME_DISCREPANCY";

/** Testnet always. Mainnet only when PRODUCTION_MAINNET_CANARY_ARMED. */
bool RigorKernelRuntimeCanaryNetworkAllowed(const CChainParams& params);

/** Startup gate. Mainnet + flag requires ARMED; else MAINNET_CANARY_NOT_ARMED. */
bool RigorKernelRuntimeCanaryStartupCheck(const ArgsManager& args, const CChainParams& params, std::string& err);

bool RigorKernelRuntimeCanaryEnabled();

/** ConnectBlock observe hook. No-op when the flag is OFF. Never auto-repairs. */
bool RigorObserveConnectBlockRuntimeCanary(
    int height,
    const uint256& block_hash,
    const uint256& sample_txid,
    const ConvergenceDrawSet& parent_draw,
    const uint256& draw_target,
    const RigorSupplyDeltaInput& supply,
    const std::vector<CScript>& classify_scripts,
    const Consensus::Params& params);

void RigorKernelRuntimeCanarySetLogDir(const std::string& dir);
void RigorKernelRuntimeCanaryResetStats();
int RigorKernelRuntimeCanaryBlocksValidated();
int RigorKernelRuntimeCanaryDiscrepancyCount();
int RigorKernelRuntimeCanaryCrashCount();
int RigorKernelRuntimeCanaryRejectDivergence();
double RigorKernelRuntimeCanaryMaxBlockMicros();

/** Deterministic ConnectBlock-shaped stream (regtest canary soak). */
bool RigorKernelRuntimeCanaryRunBlockStream(int blocks, std::string& err);

#endif

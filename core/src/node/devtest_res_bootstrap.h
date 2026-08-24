// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RIGOR_NODE_DEVTEST_RES_BOOTSTRAP_H
#define RIGOR_NODE_DEVTEST_RES_BOOTSTRAP_H

#include <consensus/amount.h>

class CChainParams;

/**
 * Extra per-coinbase RES paid to -miningresfeeaddr when the mockable-chain
 * Consensus::Params::res_wallet_bootstrap_coinbase field is non-zero.
 * That field is set at chain construction from -reswalletbootstrap (never gArgs
 * in ConnectBlock). Used by functional tests to obtain spendable RES without
 * relying on mempool RES-fee circularity.
 */
CAmount DevTestResWalletBootstrap(const CChainParams& chainparams);

#endif // RIGOR_NODE_DEVTEST_RES_BOOTSTRAP_H

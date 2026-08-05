// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RIGOR_NODE_DEVTEST_RES_BOOTSTRAP_H
#define RIGOR_NODE_DEVTEST_RES_BOOTSTRAP_H

#include <consensus/amount.h>

class CChainParams;

/**
 * Extra per-coinbase RES paid to -miningresfeeaddr when -reswalletbootstrap is set.
 * Only non-zero on mockable test chains (e.g. devtest/regtest). Used by functional tests
 * to obtain spendable RES without relying on mempool RES-fee circularity.
 */
CAmount DevTestResWalletBootstrap(const CChainParams& chainparams);

#endif // RIGOR_NODE_DEVTEST_RES_BOOTSTRAP_H

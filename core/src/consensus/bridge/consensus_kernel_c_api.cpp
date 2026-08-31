// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// C ABI compilation unit. Function bodies live in the domain bridges.
// This file exists so the boundary has a named translation unit.

#ifndef RIGOR_KERNEL_ABI_VERIFY_ONLY
#error "consensus/bridge is verify-only; compile with -DRIGOR_KERNEL_ABI_VERIFY_ONLY"
#endif

#include <consensus/bridge/consensus_kernel_c_api.h>

extern "C" const char* rigor_kernel_c_abi_tag(void)
{
    return "C_ABI_VERIFY_ONLY";
}

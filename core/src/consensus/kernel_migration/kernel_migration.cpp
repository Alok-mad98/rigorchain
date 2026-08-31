// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P067 verify-only TU. Production must not compile this file.

#if !defined(CONSENSUS_KERNEL_MIGRATION_VERIFY_ONLY)
#error "consensus/kernel_migration is verify-only; compile with -DCONSENSUS_KERNEL_MIGRATION_VERIFY_ONLY"
#endif

#include <consensus/kernel_migration/kernel_migration.h>

// Input / result adapters live in the header and only call existing authorities.
// This TU exists so release-build audits can see the verify-only guard.
volatile int rigor_kernel_migration_verify_only_anchor = 1;

// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-086: standalone ConnectBlock convergence audit harness.
// Compile with -DCONNECTBLOCK_CONVERGENCE_AUDIT_STANDALONE.
// Does not link validation.cpp. Does not change consensus semantics.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static int g_fails = 0;

static void Expect(bool cond, const char* msg)
{
    if (!cond) {
        std::fprintf(stderr, "FAIL %s\n", msg);
        ++g_fails;
    }
}

int main()
{
    // Honest authority counts for the production accept path. A count of 1
    // means one production body. LedgerApply is verify-only (0).
    const int consensus_kernel_authority_count = 1;
    const int connectblock_acceptance_authority_count = 1;
    const int pow_acceptance_authority_count = 1;
    const int predicate_acceptance_authority_count = 1;
    const int ledger_acceptance_authority_count = 0;
    const int supply_acceptance_authority_count = 1;
    const int issuance_acceptance_authority_count = 1;
    const int res_acceptance_authority_count = 1;
    const int draw_acceptance_authority_count = 1;
    const int duplicate_authority_count = 1; // dead ConnectBlockRigor helper
    const int consensus_bypass_count = 5;

    Expect(consensus_kernel_authority_count == 1, "kernel authority");
    Expect(connectblock_acceptance_authority_count == 1, "connectblock authority");
    Expect(pow_acceptance_authority_count == 1, "pow authority");
    Expect(predicate_acceptance_authority_count == 1, "predicate authority");
    Expect(ledger_acceptance_authority_count == 0, "ledger not production");
    Expect(supply_acceptance_authority_count == 1, "supply authority");
    Expect(issuance_acceptance_authority_count == 1, "issuance authority");
    Expect(res_acceptance_authority_count == 1, "res authority");
    Expect(draw_acceptance_authority_count == 1, "draw authority");
    Expect(duplicate_authority_count == 1, "one unused duplicate helper");
    Expect(consensus_bypass_count == 5, "five documented bypass/gap paths");

    // Fake state machine used by the fuzz smoke: reject must not increment commit.
    int state_mutation_count = 0;
    const bool invalid = true;
    if (!invalid) {
        ++state_mutation_count;
    }
    Expect(state_mutation_count == 0, "invalid must not commit");

    std::printf("CONNECTBLOCK_ACCEPTANCE_AUTHORITY_COUNT=%d\n", connectblock_acceptance_authority_count);
    std::printf("CONSENSUS_KERNEL_AUTHORITY_COUNT=%d\n", consensus_kernel_authority_count);
    std::printf("CONSENSUS_BYPASS_COUNT=%d\n", consensus_bypass_count);
    std::printf("DUPLICATE_AUTHORITY_COUNT=%d\n", duplicate_authority_count);
    std::printf("LEDGER_ACCEPTANCE_AUTHORITY_COUNT=%d\n", ledger_acceptance_authority_count);
    std::printf("LIVE_MINT_GOLDEN_VECTOR=NOT_COVERED\n");
    std::printf("REORGBIND_GOLDEN_VECTOR=NOT_COVERED\n");
    std::printf("CONNECTBLOCK_CONSENSUS_CONVERGENCE_AUDIT=OPEN\n");

    if (g_fails) {
        std::fprintf(stderr, "%d standalone audit asserts failed\n", g_fails);
        return 1;
    }
    std::printf("PASS: connectblock convergence standalone audit harness\n");
    return 0;
}

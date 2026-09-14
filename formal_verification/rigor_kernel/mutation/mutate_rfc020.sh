#!/usr/bin/env bash
# M105-M107 — RFC-020 leftover issuance pins (frontier re-read).
#
# Dual-supply ORU rewind, and the closed-form cumulative that a
# halving schedule would have turned into an epoch loop.
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# Orphaned ORU flows leak: suffix is applied on the unreverted tip.
run_mutation M105 oru_reorg_rewind_eq Kernel/Chain/ReorgReplay.v \
  '  apply_oru_chain (rollback_oru_chain tip orphans) suffix.' \
  '  apply_oru_chain tip suffix.' \
  Kernel/Chain/ReorgReplay || true

# Surviving ORU pool silently includes the orphan summand.
run_mutation M106 oru_reorg_surviving_pool Kernel/Chain/ReorgReplay.v \
  '      = oru_pool s0 + oru_flow_sum prefix + oru_flow_sum suffix.' \
  '      = oru_pool s0 + oru_flow_sum prefix + oru_flow_sum suffix + oru_flow_sum orphans.' \
  Kernel/Chain/ReorgReplay || true

# Closed form pretends the old 1000-ORU genesis subsidy is still live.
run_mutation M107 oru_subsidy_fold_closed Supply/SupplySchedule.v \
  '    oru_subsidy_fold n = Z.of_nat n * ORU_FLAT_BLOCK_SUBSIDY.' \
  '    oru_subsidy_fold n = Z.of_nat n * (1000 * COIN).' \
  Supply/SupplySchedule || true

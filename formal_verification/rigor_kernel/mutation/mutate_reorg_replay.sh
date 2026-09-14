#!/usr/bin/env bash
# M100-M101 — RFC-020 RES replay (frontier 2.3, RES half).
#
# The hollow res_reorg_replay_eq used to be a definitional drop of the
# orphan argument. These mutants attack the real rewind: skip the
# disconnect, or count orphaned mints in the surviving pool.
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# Orphaned mints leak: suffix is applied on the unreverted tip.
run_mutation M100 res_reorg_rewind_eq Kernel/Chain/ReorgReplay.v \
  '  apply_mint_chain (rollback_mints tip orphans) suffix.' \
  '  apply_mint_chain tip suffix.' \
  Kernel/Chain/ReorgReplay || true

# Surviving pool silently includes the orphan summand.
run_mutation M101 res_reorg_surviving_pool Kernel/Chain/ReorgReplay.v \
  '      = res_pool s0 + mint_event_sum prefix + mint_event_sum suffix.' \
  '      = res_pool s0 + mint_event_sum prefix + mint_event_sum suffix + mint_event_sum orphans.' \
  Kernel/Chain/ReorgReplay || true

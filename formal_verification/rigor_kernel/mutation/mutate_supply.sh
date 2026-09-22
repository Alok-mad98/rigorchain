#!/usr/bin/env bash
# M1, M12, M17 — Supply schedule / aggregate cap / epoch transition
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# Negative-height display destroy cost becomes -1, so
# 0 <= res_destroy_cost_at_height h fails.
# golden_negative_vectors_rejected is M36; it does not read the destroy cost.
run_mutation M1 res_schedule_finite Supply/SupplySchedule.v \
  '  if height <? 0 then 0' \
  '  if height <? 0 then -1' \
  Supply/SupplySchedule || true

# Break the schedule-total identity. Shrinking the cap pin is a
# tautology (x <= x still holds).
run_mutation M12 res_total_issuance_capped Supply/SupplySchedule.v \
  'Definition res_schedule_total : Z := RES_MAX_SUPPLY.' \
  'Definition res_schedule_total : Z := RES_MAX_SUPPLY + 1.' \
  Supply/SupplySchedule || true

# Prefix fold no longer uses epoch_minted; bump the successor instead.
run_mutation M17 supply_after_epochs_eq Supply/SupplyTransition.v \
  '  | S n'\'' => supply_after_epochs n'\''' \
  '  | S n'\'' => supply_after_epochs n'\'' + 1' \
  Supply/SupplyTransition || true

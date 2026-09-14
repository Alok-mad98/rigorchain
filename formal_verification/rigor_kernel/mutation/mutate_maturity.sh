#!/usr/bin/env bash
# M117-M122 — coinbase / RES-mint maturity 100 and spend-side MAX_MONEY
# (frontier expansion Phase C). Dust is not modelled.
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# Depth 99 becomes spendable (maturity constant treated as 0).
run_mutation M117 immature_coinbase_rejected Kernel/Tx/Maturity.v \
  '  COINBASE_MATURITY <=? (spend_h - coin_h).' \
  '  0 <=? (spend_h - coin_h).' \
  Kernel/Tx/Maturity || true

# Off-by-one: spendable only at depth 101.
run_mutation M118 mature_coinbase_at_100 Kernel/Tx/Maturity.v \
  '  COINBASE_MATURITY <=? (spend_h - coin_h).' \
  '  COINBASE_MATURITY <? (spend_h - coin_h).' \
  Kernel/Tx/Maturity || true

# Body RES mint skips the maturity gate.
run_mutation M119 res_mint_uses_same_maturity Kernel/Tx/Maturity.v \
  '  orb (sc_coinbase c) (sc_res_mint c).' \
  '  sc_coinbase c.' \
  Kernel/Tx/Maturity || true

# Every coin needs 100 confirmations, including plain transfers.
run_mutation M120 plain_transfer_no_maturity Kernel/Tx/Maturity.v \
  '       (if requires_maturity c
        then mature spend_h (sc_height c)
        else true).' \
  '       (mature spend_h (sc_height c)).' \
  Kernel/Tx/Maturity || true

# Drop the input MoneyRange check.
run_mutation M121 input_over_max_rejected Kernel/Tx/Maturity.v \
  '  andb (money_range_b (sc_value c))' \
  '  andb true' \
  Kernel/Tx/Maturity || true

# Forget the sum check; only reject out-of-range operands.
run_mutation M122 pool_add_overflow_none Kernel/Tx/Maturity.v \
  '  else MAX_MONEY - running <? addend.' \
  '  else false.' \
  Kernel/Tx/Maturity || true

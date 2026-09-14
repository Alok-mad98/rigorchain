#!/usr/bin/env bash
# M123-M128 — spendability by script class (frontier expansion Phase D).
# No opcode VM. T_MINT is an auth class (VerifyResMintSpend), not false.
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# Converge locks become spendable.
run_mutation M123 converge_unspendable Script/Semantics/Spend.v \
  '| T_CONVERGE => false' \
  '| T_CONVERGE => true' \
  Script/Semantics/Spend || true

# OP_RETURN / null-data become spendable.
run_mutation M124 nulldata_unspendable Script/Semantics/Spend.v \
  '| T_NULL_DATA => false' \
  '| T_NULL_DATA => true' \
  Script/Semantics/Spend || true

# Nonstandard locks become spendable.
run_mutation M125 nonstandard_unspendable Script/Semantics/Spend.v \
  '| T_NONSTANDARD => false' \
  '| T_NONSTANDARD => true' \
  Script/Semantics/Spend || true

# Auth classes ignore a failing signature check.
run_mutation M126 witness_requires_sig_ok Script/Semantics/Spend.v \
  '| T_WITNESS_V0 | T_WITNESS_V1 | T_MINT => sig_ok wit script' \
  '| T_WITNESS_V0 | T_WITNESS_V1 | T_MINT => true' \
  Script/Semantics/Spend || true

# Auth classes reject even a passing signature check.
run_mutation M127 witness_sig_ok_spends Script/Semantics/Spend.v \
  '| T_WITNESS_V0 | T_WITNESS_V1 | T_MINT => sig_ok wit script' \
  '| T_WITNESS_V0 | T_WITNESS_V1 | T_MINT => false' \
  Script/Semantics/Spend || true

# Spend skips classify_script and uses a second ladder.
run_mutation M128 classifier_then_spend Script/Semantics/Spend.v \
  '  spendable_by_class sig_ok (classify_script script) wit script.' \
  '  spendable_by_class sig_ok T_NONSTANDARD wit script.' \
  Script/Semantics/Spend || true

#!/usr/bin/env bash
# M102-M104 — most-work tip selection (frontier 2.3, first OP-R1 slice).
#
# Chainwork is a sum, the heavier suffix wins regardless of length,
# and equal work does not reorg. Each mutant targets one Category A
# theorem.
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# Break the sum identity. The fold stays recursive.
run_mutation M102 chain_work_app Kernel/Chain/ForkChoice.v \
  '    chain_work (xs ++ ys) = chain_work xs + chain_work ys.' \
  '    chain_work (xs ++ ys) = chain_work xs + chain_work ys + 1.' \
  Kernel/Chain/ForkChoice || true

# Longest chain, not most work.
run_mutation M103 "more_work_wins select_by_total_eq_suffix select_suffix_is_a_or_b" Kernel/Chain/ForkChoice.v \
  '  if chain_work b <? chain_work a then a
  else if chain_work a <? chain_work b then b
  else a.' \
  '  if Nat.ltb (length b) (length a) then a
  else if Nat.ltb (length a) (length b) then b
  else a.' \
  Kernel/Chain/ForkChoice || true

# Equal work flips to the challenger — a free reorg.
run_mutation M104 equal_work_keeps_active Kernel/Chain/ForkChoice.v \
  '  else if chain_work a <? chain_work b then b
  else a.' \
  '  else if chain_work a <? chain_work b then b
  else b.' \
  Kernel/Chain/ForkChoice || true

# Always take the first challenger — a lighter cand can win.
run_mutation M113 select_tip_maximal Kernel/Chain/ForkChoice.v \
  '  let wstar := pool_max_work active cands in
  if chain_work active =? wstar then active
  else
    match first_at_work wstar cands with
    | Some s => s
    | None => active
    end.' \
  '  match cands with
  | nil => active
  | s :: _ => s
  end.' \
  Kernel/Chain/ForkChoice || true

# Tie (or already-maximal active) prefers the first challenger.
run_mutation M114 select_tip_keeps_active Kernel/Chain/ForkChoice.v \
  '  if chain_work active =? wstar then active' \
  '  if chain_work active =? wstar then match cands with nil => active | s :: _ => s end' \
  Kernel/Chain/ForkChoice || true

# Heavier challenger is ignored — binary case no longer matches select_suffix.
run_mutation M115 select_tip_binary Kernel/Chain/ForkChoice.v \
  '    | Some s => s' \
  '    | Some s => active' \
  Kernel/Chain/ForkChoice || true

# Selection can return a suffix that is not a candidate.
run_mutation M116 select_tip_total Kernel/Chain/ForkChoice.v \
  '  let wstar := pool_max_work active cands in
  if chain_work active =? wstar then active
  else
    match first_at_work wstar cands with
    | Some s => s
    | None => active
    end.' \
  '  nil.' \
  Kernel/Chain/ForkChoice || true

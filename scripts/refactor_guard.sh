#!/usr/bin/env bash
# RES issuance refactor guard.
#
# Run this at the end of every phase. It answers one question: did the change
# stay inside the RES issuance boundary? Everything it checks is something the
# refactor has no business touching, so any failure means the change escaped.
#
#   ./scripts/refactor_guard.sh          fast legs only  (~1 min)
#   ./scripts/refactor_guard.sh --full   also the kernel critical gate (~13 min)
#
# Establish the baselines this compares against with:
#   python3 scripts/refactor_classify_vectors.py
#   python3 scripts/refactor_snapshot_baseline.py
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

FULL=0
[[ "${1:-}" == "--full" ]] && FULL=1

RC=0
pass() { printf '  \033[32mOK\033[0m   %s\n' "$1"; }
fail() { printf '  \033[31mFAIL\033[0m %s\n' "$1"; RC=1; }
skip() { printf '  \033[33mSKIP\033[0m %s\n' "$1"; }

echo "== L1: golden vector classification is reproducible =="
if python3 scripts/refactor_classify_vectors.py --check >/dev/null 2>&1; then
  pass "classification matches the generator"
else
  python3 scripts/refactor_classify_vectors.py --check || true
  fail "classification drifted; regenerate and review the diff before continuing"
fi

echo "== L2: invariant vectors are byte-identical =="
# The 75 vectors in the invariant group cover sha256d, the FIPS suite, the
# syntactic script->asset classifier, block traces and the replay seed
# expansion. None of them has anything to say about how RES is issued, so a
# single changed bit means the edit reached past the RES boundary.
python3 - <<'PY'
import json, pathlib, sys
sys.path.insert(0, "scripts")
from refactor_classify_vectors import invariant_digest, FV, OUT
stored = json.loads((OUT / "vector_classification.json").read_text())
live = invariant_digest(json.loads((FV / "phase3_golden_vectors.json").read_text()))
if live != stored["invariant_sha256"]:
    print(f"  expected {stored['invariant_sha256']}")
    print(f"  actual   {live}")
    sys.exit(1)
print(f"  {len(stored['invariant'])} vectors pinned at {live[:16]}...")
PY
[[ $? -eq 0 ]] && pass "invariant vector group unchanged" \
               || fail "an invariant vector changed; the edit left the RES boundary"

echo "== L3: Category A theorems intact, unproved surface did not grow =="
if OUTPUT="$(python3 scripts/refactor_snapshot_baseline.py --check 2>&1)"; then
  echo "$OUTPUT" | sed 's/^/  /'
  pass "formal baseline holds"
else
  echo "$OUTPUT" | sed 's/^/  /'
  fail "formal baseline regressed"
fi

echo "== L4: mechanism-independent C++ suites =="
# Deliberately excludes every res_*, convergence_* and sigma/draw suite: those
# are expected to change. What is listed here must not.
SUITES=(
  hash_tests crypto_tests merkle_tests merkleblock_tests
  canonical_hash_input_tests canonical_hash_output_tests
  script_tests script_parse_tests script_segwit_tests script_standard_tests
  scriptnum_tests sigopcount_tests sighash_tests rigor_sighash_vector_tests
  pow_tests rigor_pow_digest_tests
  serialize_tests key_tests key_io_tests
  test_crypto_boundary_security test_pow_boundary_security
  test_script_opcode_security
)
BIN="core/src/test/test_rigor"
if [[ -x "$BIN" ]]; then
  JOINED="$(IFS=,; echo "${SUITES[*]}")"
  if OUTPUT="$(timeout 1800 "$BIN" --run_test="$JOINED" 2>&1)"; then
    pass "${#SUITES[@]} suites green"
  else
    echo "$OUTPUT" | tail -30 | sed 's/^/  /'
    fail "a mechanism-independent suite broke"
  fi
else
  skip "$BIN not built; run make in core/ to arm this leg"
fi

echo "== L5: RFC-019 numbers match the model that produced them =="
# The RFC is the single source of truth for every later phase, so it must not
# be allowed to drift from tools/res_issuance_schedule.py. If a constant moves,
# both move together or this fails.
if OUTPUT="$(python3 tools/res_issuance_schedule.py --check-doc 2>&1)"; then
  echo "$OUTPUT" | sed 's/^/  /'
  pass "RFC-019 agrees with its numeric SSOT"
else
  echo "$OUTPUT" | sed 's/^/  /'
  fail "RFC-019 and tools/res_issuance_schedule.py disagree"
fi

echo "== L6: res_issuance.h agrees with the RFC-019 model =="
# Two independent integer implementations of one specification. The header's
# static_asserts catch a parameter moving; this catches the behaviour diverging
# in a way no assert pins. Both layers were negative-tested when built.
if [[ -f core/src/consensus/res_issuance.h ]]; then
  if OUTPUT="$(python3 formal_verification/tests/test_res_issuance_differential.py \
                 --cases 5000 2>&1)"; then
    echo "$OUTPUT" | sed 's/^/  /'
    pass "C++ and the model agree bit for bit"
  else
    echo "$OUTPUT" | head -12 | sed 's/^/  /'
    fail "res_issuance.h and tools/res_issuance_schedule.py diverged"
  fi
  if [[ -x "$BIN" ]]; then
    if OUTPUT="$("$BIN" --run_test=res_issuance_tests 2>&1)"; then
      pass "res_issuance_tests"
    else
      echo "$OUTPUT" | grep -iE "error|fail" | head -10 | sed 's/^/  /'
      fail "res_issuance_tests"
    fi
  fi
else
  skip "core/src/consensus/res_issuance.h does not exist yet (pre-Phase 2)"
fi

echo "== L7: the lottery surface only ever shrinks =="
# Phase 4's failure mode is not deleting too much -- the compiler catches that
# -- but leaving a draw symbol reachable with no mechanism behind it. So the
# surface is counted, and growth is a hard stop. Deliberately excludes the burn
# symbols: ORU is still destroyed under RFC-019.
if OUTPUT="$(python3 scripts/refactor_draw_inventory.py --check 2>&1)"; then
  echo "$OUTPUT" | sed 's/^/  /'
  pass "draw inventory did not grow"
else
  echo "$OUTPUT" | sed 's/^/  /'
  fail "the lottery surface grew"
fi

echo "== L8: kernel critical gate =="
# run_kernel_critical.sh is not idempotent. ProofSourceManifest.md and
# VersionManifest.md embed a snapshot of `git status` / `git ls-files` taken
# while they are generated, so every run rewrites them to describe whatever
# the tree looked like at that moment. Restore them afterwards, otherwise the
# guard leaves the working tree dirty and the next phase starts from a state
# nobody chose.
SELF_REF=(
  formal_verification/rigor_kernel/release/ProofSourceManifest.md
  formal_verification/rigor_kernel/release/VersionManifest.md
)
if [[ $FULL -eq 1 ]]; then
  if (cd formal_verification/rigor_kernel && ./run_kernel_critical.sh) >/tmp/refactor_guard_kernel.log 2>&1; then
    pass "run_kernel_critical.sh (log: /tmp/refactor_guard_kernel.log)"
  else
    tail -30 /tmp/refactor_guard_kernel.log | sed 's/^/  /'
    fail "run_kernel_critical.sh (log: /tmp/refactor_guard_kernel.log)"
  fi
  git checkout -- "${SELF_REF[@]}" 2>/dev/null \
    && echo "  restored self-referential manifests rewritten by the gate"
else
  skip "pass --full to run it (~13 min)"
fi

echo
if [[ $RC -eq 0 ]]; then
  printf '\033[32mREFACTOR_GUARD=PASS\033[0m\n'
else
  printf '\033[31mREFACTOR_GUARD=FAIL\033[0m — stop and fix before starting the next phase\n'
fi
exit $RC

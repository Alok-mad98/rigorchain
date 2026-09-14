#!/usr/bin/env bash
# Shared mutation helpers. Sourced by mutate_*.sh and run_all_mutations.sh.
# shellcheck disable=SC2034
#
# Frontier item 1.1. The harness used to print
#
#     EXPECTED_FAILURE id=M9 theorem=utxo_soundness
#
# whenever the mutated module failed to compile, whatever the reason.
# The theorem name was a label. A mutant that killed a different lemma,
# or a patch that produced a syntax error, both counted as a confirmed
# kill of whatever name was typed next to the call.
#
# Two changes.
#
# First, a mutant must kill the theorems it names. `coqc` stops at the
# first failing proof, so when an earlier lemma falls first the harness
# admits that lemma and recompiles, repeating until every declared
# theorem has been observed failing or the module compiles. Admitting a
# lemma ASSUMES it, which can only make later proofs easier, so a
# theorem that still fails with every earlier casualty admitted depends
# on the mutated definition itself.
#
# Second, a mutant may name several theorems. One broken definition
# usually invalidates a group of proofs, and duplicating the same edit
# under different ids to claim them one at a time would be bookkeeping,
# not evidence. Each named theorem is verified separately; if any of
# them still proves, the mutant fails.

MUTATION_LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KERNEL_ROOT="$(cd "$MUTATION_LIB_DIR/.." && pwd)"

EXPECT_FAIL_COUNT="${EXPECT_FAIL_COUNT:-0}"
MUTATION_PASS_COUNT="${MUTATION_PASS_COUNT:-0}"
MUTATION_INVALID_COUNT="${MUTATION_INVALID_COUNT:-0}"
MUTATION_LEAK_COUNT="${MUTATION_LEAK_COUNT:-0}"

MUTATION_MAX_ISOLATION_STEPS="${MUTATION_MAX_ISOLATION_STEPS:-40}"
# Set to 1 to report every proof a mutant kills instead of checking the
# declared list. Used by kill_set.sh to derive the coverage map.
MUTATION_KILL_SET_MODE="${MUTATION_KILL_SET_MODE:-0}"

# A full snapshot of every source file, taken once per shell. The
# per-mutation backup should be enough, but a mutation harness that
# silently leaves an edit behind would corrupt every later mutation and
# every proof claim in the tree, so the invariant is checked rather
# than assumed.
MUT_SNAPSHOT="$(mktemp -d)"
tar -C "$KERNEL_ROOT" -cf "$MUT_SNAPSHOT/tree.tar" \
  $(cd "$KERNEL_ROOT" && find . -name '*.v' \
       -not -path './mutation/*' \
       -not -path './certicoq/*') 2>/dev/null

_mut_assert_clean() {
  local id="$1" leaked
  leaked="$(cd "$KERNEL_ROOT" && tar -df "$MUT_SNAPSHOT/tree.tar" 2>/dev/null \
            | sed -n 's/: *Contents differ.*//p' | tr '\n' ' ')"
  [[ -z "$leaked" ]] && return 0
  echo "RESTORE_LEAK id=$id files:$leaked"
  tar -C "$KERNEL_ROOT" -xf "$MUT_SNAPSHOT/tree.tar"
  ( cd "$KERNEL_ROOT" && make -s ) >/dev/null 2>&1 || true
  MUTATION_LEAK_COUNT=$((MUTATION_LEAK_COUNT + 1))
  return 1
}

mutation_replace() {
  local file="$1"
  local old="$2"
  local new="$3"
  python3 - "$KERNEL_ROOT/$file" "$old" "$new" <<'PY'
import pathlib, sys
path, old, new = pathlib.Path(sys.argv[1]), sys.argv[2], sys.argv[3]
text = path.read_text()
if old not in text:
    sys.stderr.write("mutation_replace: pattern not found in %s\n" % path)
    sys.exit(2)
path.write_text(text.replace(old, new, 1))
PY
}

_mut_backup() {
  local rel="$1"
  local dest="$MUT_BAKDIR/$rel"
  if [[ ! -f "$dest" ]]; then
    mkdir -p "$(dirname "$dest")"
    cp "$KERNEL_ROOT/$rel" "$dest"
  fi
}

_mut_restore_all() {
  [[ -d "$MUT_BAKDIR" ]] || return 0
  local f rel stem
  while IFS= read -r -d '' f; do
    rel="${f#"$MUT_BAKDIR"/}"
    cp -f "$f" "$KERNEL_ROOT/$rel"
    # The loop admits proofs in modules other than the mutated one, and
    # those compiled objects now hold axioms. Restoring the source is
    # not enough; drop the objects too.
    stem="${rel%.v}"
    rm -f "$KERNEL_ROOT/${stem}.vo" "$KERNEL_ROOT/${stem}.vos" \
          "$KERNEL_ROOT/${stem}.vok" "$KERNEL_ROOT/${stem}.glob"
  done < <(find "$MUT_BAKDIR" -type f -name '*.v' -print0)
}

# run_mutation <id> <theorem[ theorem...]> <file> <old> <new> <compile_target>
run_mutation() {
  local id="$1"
  local theorems="$2"
  local relfile="$3"
  local old="$4"
  local new="$5"
  local compile_target="$6"

  # Re-measuring one mutation should not require running all of them.
  if [[ -n "${MUTATION_ONLY:-}" && " $MUTATION_ONLY " != *" $id "* ]]; then
    return 0
  fi

  # Enumerate the suite without applying anything. The declaration
  # table is generated from this, so it cannot be written by hand and
  # cannot fall behind the scripts. Bash does the unquoting, which a
  # regex over the scripts got wrong for a third of the calls.
  if [[ -n "${MUTATION_LIST_ONLY:-}" ]]; then
    printf 'MUTATION_DECL\t%s\t%s\t%s\t%s\t%s\n' \
      "$id" "$theorems" "$relfile" "${old//$'\n'/ }" "${new//$'\n'/ }"
    return 0
  fi

  local src_stem="${relfile%.v}"
  local log
  log="$(mktemp)"
  MUT_BAKDIR="$(mktemp -d)"

  echo "== Mutation $id  expected_fail=[$theorems]  file=$relfile =="

  _mut_backup "$relfile"
  if ! mutation_replace "$relfile" "$old" "$new"; then
    _mut_restore_all; rm -rf "$MUT_BAKDIR"; rm -f "$log"
    echo "mutation_passed $id (patch failed to apply)"
    MUTATION_PASS_COUNT=$((MUTATION_PASS_COUNT + 1))
    return 1
  fi

  local -a want=() got=() collateral=()
  read -r -a want <<< "$theorems"

  local rc actual actual_file resolved outcome="" step=0
  while (( step < MUTATION_MAX_ISOLATION_STEPS )); do
    step=$((step + 1))
    rc=0
    (
      cd "$KERNEL_ROOT"
      rm -f "${src_stem}.vo" "${src_stem}.vos" "${src_stem}.vok"
      rm -f "${compile_target}.vo" "${compile_target}.vos" "${compile_target}.vok"
      # Build everything, not just the declared target. Whether a
      # mutant kills a theorem must not depend on which compile target
      # someone wrote next to the call, and a target that stops short
      # of the theorem silently reports it as surviving.
      make -s "${compile_target}.vo" && make -s
    ) >"$log" 2>&1 || rc=$?

    if [[ "$rc" -eq 0 ]]; then
      outcome="compiled"
      break
    fi

    resolved="$(python3 "$MUTATION_LIB_DIR/resolve_failure.py" "$log")"
    actual="$(sed -n 's/^ACTUAL_FAIL=//p' <<< "$resolved")"
    actual_file="$(sed -n 's/^ACTUAL_FILE=//p' <<< "$resolved")"

    if [[ "$actual" == "<PARSE_ERROR>" ]]; then
      outcome="invalid"
      break
    fi
    if [[ -z "$actual" || "$actual" == "<UNKNOWN>" || -z "$actual_file" \
          || "$actual_file" == "<UNKNOWN>" ]]; then
      outcome="unknown"
      break
    fi

    if [[ " ${want[*]} " == *" $actual "* ]]; then
      got+=("$actual")
    else
      collateral+=("$actual")
    fi

    if [[ "$MUTATION_KILL_SET_MODE" != "1" && ${#got[@]} -eq ${#want[@]} ]]; then
      outcome="verified"
      break
    fi

    _mut_backup "$actual_file"
    if ! python3 "$MUTATION_LIB_DIR/admit_proof.py" \
           "$KERNEL_ROOT/$actual_file" "$actual"; then
      outcome="unknown"
      break
    fi
  done
  [[ -n "$outcome" ]] || outcome="unknown"

  _mut_restore_all
  rm -rf "$MUT_BAKDIR"
  _mut_assert_clean "$id" || true
  (
    cd "$KERNEL_ROOT"
    rm -f "${src_stem}.vo" "${src_stem}.vos" "${src_stem}.vok"
    rm -f "${compile_target}.vo" "${compile_target}.vos" "${compile_target}.vok"
    make -s
  ) >/dev/null 2>&1 || true

  if [[ "$MUTATION_KILL_SET_MODE" == "1" ]]; then
    echo "KILL_SET id=$id : ${got[*]} ${collateral[*]}"
    rm -f "$log"
    return 0
  fi

  if [[ "$outcome" == "verified" ]]; then
    echo "EXPECTED_FAILURE id=$id theorem=${want[*]} VERIFIED_FAIL=${got[*]}"
    [[ ${#collateral[@]} -gt 0 ]] && echo "  collateral: ${collateral[*]}"
    EXPECT_FAIL_COUNT=$((EXPECT_FAIL_COUNT + 1))
    rm -f "$log"
    return 0
  fi

  if [[ "$outcome" == "invalid" ]]; then
    echo "mutation_invalid $id (patch broke elaboration, not a proof)"
    tail -n 15 "$log" || true
    MUTATION_INVALID_COUNT=$((MUTATION_INVALID_COUNT + 1))
    rm -f "$log"
    return 1
  fi

  local missing=""
  local t
  for t in "${want[@]}"; do
    [[ " ${got[*]} " == *" $t "* ]] || missing="$missing $t"
  done
  echo "mutation_passed $id (still proves:$missing )"
  [[ ${#collateral[@]} -gt 0 ]] && echo "  collateral: ${collateral[*]}"
  [[ "$outcome" == "unknown" ]] && tail -n 15 "$log"
  MUTATION_PASS_COUNT=$((MUTATION_PASS_COUNT + 1))
  rm -f "$log"
  return 1
}

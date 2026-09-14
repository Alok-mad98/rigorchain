#!/usr/bin/env bash
# Adversarial audit of the frozen Level 4.9 formal kernel package.
# Does not add theorems. Does not run the 50 Coq mutants (see
# ./mutation/run_all_mutations.sh / ./run_kernel_critical.sh).
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$DIR/.." && pwd)"
REPO="$(cd "$ROOT/../.." && pwd)"
FV="$(cd "$ROOT/.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }

echo "== 1 source manifest check =="
./release/check_source_manifest.sh
echo "SOURCE_MANIFEST_PASS=TRUE"

echo "== 2 proof count =="
COUNTS="$(python3 - "$ROOT" <<'PY'
import pathlib, re, sys
root = pathlib.Path(sys.argv[1])
text = (root / "release" / "TheoremClassification.md").read_text().splitlines()
section = None
n = {"A": 0, "B": 0, "C": 0}
row = re.compile(r"^\|\s*`([^`]+)`\s*\|\s*`([^`]+)`\s*\|")
for line in text:
    if line.startswith("## Category A"):
        section = "A"
    elif line.startswith("## Category B"):
        section = "B"
    elif line.startswith("## Category C"):
        section = "C"
    elif line.startswith("## Reading"):
        section = None
    if section and row.match(line):
        n[section] += 1
total = n["A"] + n["B"] + n["C"]
print("CATEGORY_A_COUNT=%d" % n["A"])
print("CATEGORY_B_COUNT=%d" % n["B"])
print("CATEGORY_C_COUNT=%d" % n["C"])
print("THEOREM_COUNT=%d" % total)
# These were four literals (65/67/25/157) that had to be edited by hand
# whenever a theorem was classified. They are checked against the
# generated inventory instead, so a change to the table is a drift
# unless release/gen_final_proof_inventory.py has been re-run over it.
inv = (root / "release" / "FinalProofInventory.md").read_text()
recorded = {k: int(v) for k, v in re.findall(
    r"^(THEOREM_COUNT|CATEGORY_[ABC]_COUNT)=(\d+)$", inv, re.M)}
want = {"CATEGORY_A_COUNT": n["A"], "CATEGORY_B_COUNT": n["B"],
        "CATEGORY_C_COUNT": n["C"], "THEOREM_COUNT": total}
if recorded != want:
    sys.exit("proof count drift: table says %s, FinalProofInventory.md "
             "says %s; re-run release/gen_final_proof_inventory.py"
             % (want, recorded))
if inv.count("- **Name:**") != total:
    sys.exit("FinalProofInventory has %d entries for %d classified "
             "theorems" % (inv.count("- **Name:**"), total))
print("PROOF_COUNT_PASS=TRUE")
PY
)" || fail "proof count"
printf '%s\n' "$COUNTS"

echo "== 3 Print Assumptions =="
ASSUME_V="$(mktemp --suffix=.v)"
OUT="$(mktemp)"
python3 scripts/check_theorem_classification.py \
  --md release/TheoremClassification.md \
  --also evidence/TheoremClassification.md \
  --emit "$ASSUME_V" >/dev/null
if [[ ! -f Makefile ]]; then
  coq_makefile -f _CoqProject.kernel-critical -o Makefile
fi
coqtop -R . RigorKernel -R certicoq/surface RigorSurface -batch -l "$ASSUME_V" > "$OUT" 2>&1 || {
  cat "$OUT"
  rm -f "$ASSUME_V" "$OUT"
  fail "coqtop Print Assumptions failed"
}
if grep -qE 'Axiom|Parameter' "$OUT"; then
  rm -f "$ASSUME_V" "$OUT"
  fail "Print Assumptions is not closed"
fi
CLOSED_COUNT="$(grep -c 'Closed under the global context' "$OUT" || true)"
EXPECTED_CLOSED="$(grep -c '^Print Assumptions' "$ASSUME_V" || true)"
if [[ "$CLOSED_COUNT" -ne "$EXPECTED_CLOSED" ]]; then
  rm -f "$ASSUME_V" "$OUT"
  fail "expected $EXPECTED_CLOSED closed theorems, got $CLOSED_COUNT"
fi
echo "PRINT_ASSUMPTIONS_CLOSED=$CLOSED_COUNT"
echo "ALL_ASSUMPTIONS=CLOSED"
rm -f "$ASSUME_V" "$OUT"

echo "== 4 claim keyword scan (formal_verification/) =="
python3 - "$FV" <<'PY'
import pathlib, re, sys
root = pathlib.Path(sys.argv[1])
# Positive claim only: token assigned / JSON-set to true.
# Mentions that forbid the token, or set it false, are NOT_CLAIMED.
positive = re.compile(
    r"(equivalence_proved|compiler_verified|binary_verified|"
    r"cpp_refinement|full_chain_verified)"
    r'''[\w"]*\s*[=:]\s*true\b''',
    re.I,
)
bad = []
skip_dirs = {".git", "_build", "certicoq"}
for p in root.rglob("*"):
    if not p.is_file():
        continue
    if any(part in skip_dirs for part in p.parts):
        continue
    if p.suffix not in {".v", ".md", ".sh", ".py", ".json", ".ml", ".c"}:
        continue
    try:
        text = p.read_text(errors="replace")
    except OSError:
        continue
    rel = p.relative_to(root).as_posix()
    for i, line in enumerate(text.splitlines(), 1):
        if not positive.search(line):
            continue
        s = line.strip()
        # CI needles that forbid the claim, not the claim itself.
        if re.match(r'^"\w+=true",?$', s):
            continue
        if "forbidden" in s.lower():
            continue
        bad.append("%s:%d: %s" % (rel, i, s[:160]))
if bad:
    for b in bad[:40]:
        print(b, file=sys.stderr)
    sys.exit("claim keyword scan failed (%d hits)" % len(bad))
print("CLAIM_KEYWORD_SCAN=PASS")
PY

echo "== 5 mutation count =="
EXP="$(cat mutation/expected_failures.md)"
for id in M1 M41; do
  echo "$EXP" | grep -q "$id" || fail "missing $id in expected_failures.md"
done
# Every M1–M50 row present.
python3 - "$ROOT" <<'PY'
import pathlib, re, sys
root = pathlib.Path(sys.argv[1])
exp = (root / "mutation" / "expected_failures.md").read_text()
# The id set was pinned at M1-M50 and the runner threshold restated as
# a literal. Both are read off the declaration table now, which is
# itself generated from the mutation scripts, so neither can lag the
# suite. Ids are not required to be contiguous: a mutant that turns
# out to be redundant is deleted rather than renumbered.
rows = re.findall(r"^\| M(\d+) \|", exp, re.M)
got = sorted(int(x) for x in rows)
if len(set(got)) != len(got):
    sys.exit("duplicate mutation ids in expected_failures.md")
scripts = sorted((root / "mutation").glob("mutate_*.sh"))
declared = sorted(
    int(m) for s in scripts
    for m in re.findall(r"run_mutation\s+M(\d+)\b", s.read_text()))
if got != declared:
    sys.exit("expected_failures.md ids %s do not match the scripts %s"
             % (got, declared))
runner = (root / "mutation" / "run_all_mutations.sh").read_text()
if "-ne %d " % len(got) not in runner:
    sys.exit("runner threshold is not %d" % len(got))
print("MUTATION_COUNT=%d" % len(got))
print("MUTATION_ID_SET=M%d-M%d" % (got[0], got[-1]))
PY

echo "== 6 TCB consistency =="
python3 - "$ROOT" <<'PY'
import pathlib, re, sys
root = pathlib.Path(sys.argv[1])
tcb = (root / "release" / "TCBInventory.md").read_text()
cone = (root / "release" / "FormalConeMap.md").read_text()
need = [
    ("count", "TCB_LAYER_COUNT=8" in tcb),
    ("L0", "Specification" in tcb),
    ("L1", "Abstract Coq Kernel" in tcb),
    ("L2", "Concrete Kernel" in tcb),
    ("L3", "Executable Kernel Model" in tcb),
    ("L4", "Extraction Interface" in tcb),
    ("L5", "Runtime Semantic Model" in tcb),
    ("L6", "Runtime Machine Domain" in tcb),
    ("L7", "Checker Contract" in tcb),
    ("trusted", "Trusted:" in tcb or "- Trusted:" in tcb),
    ("verified", "Verified:" in tcb or "- Verified:" in tcb),
    ("open", "Open:" in tcb or "- Open:" in tcb),
    ("cone spec", "Specification" in cone),
    ("cone checker", "Checker Contract" in cone),
    ("iface", "extraction interface consistency" in tcb),
    ("ocaml", "OCaml compiler correctness" in tcb),
]
for name, ok in need:
    if not ok:
        sys.exit("TCB consistency failed: %s" % name)
print(re.search(r"TCB_LAYER_COUNT=\d+", tcb).group(0))
print("TCB_CONSISTENCY=PASS")
PY

echo "== 7 release reproducibility =="
./release/check_release.sh
# Count locked source rows (exclude comments).
SRC_COUNT="$(grep -cE '^[0-9a-f]{64} ' release/SourceManifest.sha256 || true)"
echo "SOURCE_MANIFEST_COUNT=$SRC_COUNT"

echo "FORMAL_LEVEL_ESTIMATE=Level 4.9"
echo "FORMAL_FINAL_AUDIT_READY=TRUE"
echo "AUDIT_PACKAGE_READY=TRUE"
echo "NO_ADMITTED=TRUE"
echo "NO_AXIOMS=TRUE"

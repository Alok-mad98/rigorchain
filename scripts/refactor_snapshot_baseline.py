#!/usr/bin/env python3
"""Snapshot the formal-verification baseline the RES refactor must not regress.

Writes formal_verification/refactor_baseline/formal_baseline.json holding:

  * the Category A / B / C theorem name sets, with the file each lives in
  * the non-claim flag surface in Audit/KernelNonClaims.v
  * the freeze-manifest hashes at the moment of the snapshot

The point is not the counts. It is the *name sets*: after the refactor, every
Category A theorem in this snapshot must either still be proved under the same
name, or appear in an explicit retirement list with a reason. Counts alone let
a theorem silently disappear and be replaced by a different one.

The assumption surface of this kernel is not expressed as Coq `Axiom` or
`Hypothesis` — the G-1 gate in run_kernel_critical.sh bans both outright. It is
expressed as boolean flags in Audit/KernelNonClaims.v, each one naming a
property the kernel explicitly does not prove. That flag set is what P10
(assumptions may only shrink) is actually about, so that is what is recorded.
A flag flipping false -> true, or a new false flag appearing, is a regression.
"""
import argparse
import hashlib
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
KERNEL = ROOT / "formal_verification" / "rigor_kernel"
OUT = ROOT / "formal_verification" / "refactor_baseline"

CLASSIFICATION = KERNEL / "release" / "TheoremClassification.md"
NON_CLAIMS = KERNEL / "Audit" / "KernelNonClaims.v"

MANIFESTS = [
    "formal_verification/genesis_consensus_freeze_manifest.json",
    "formal_verification/release_freeze_manifest.json",
    "formal_verification/consensus_kernel_freeze_manifest.json",
]

ROW = re.compile(r"^\|\s*`([^`]+)`\s*\|\s*`([^`]+)`\s*\|", re.M)
HEADING = re.compile(r"^##\s+Category\s+([ABC])\b", re.M)


def parse_classification(text):
    """Return {category: {theorem: file}} sliced on the ## Category headings."""
    marks = [(m.group(1), m.start()) for m in HEADING.finditer(text)]
    if len(marks) != 3:
        raise SystemExit(f"expected 3 Category headings, found {len(marks)}")
    out = {}
    for i, (cat, start) in enumerate(marks):
        end = marks[i + 1][1] if i + 1 < len(marks) else len(text)
        seg = text[start:end]
        out[cat] = {m.group(1): m.group(2) for m in ROW.finditer(seg)
                    if m.group(1) != "Theorem"}
    return out


FLAG = re.compile(r"^\s*Definition\s+(\w+)\s*:\s*bool\s*:=\s*(true|false)\s*\.", re.M)


def parse_non_claims(path):
    """The boolean non-claim surface: property name -> is it claimed."""
    if not path.is_file():
        raise SystemExit(f"missing {path}; the non-claim surface is the P10 baseline")
    return {m.group(1): m.group(2) == "true" for m in FLAG.finditer(path.read_text())}


def build():
    cls = parse_classification(CLASSIFICATION.read_text())
    manifests = {}
    for rel in MANIFESTS:
        p = ROOT / rel
        manifests[rel] = (hashlib.sha256(p.read_bytes()).hexdigest()
                          if p.is_file() else None)
    return {
        "categories": cls,
        "category_counts": {k: len(v) for k, v in cls.items()},
        "non_claims": parse_non_claims(NON_CLAIMS),
        "freeze_manifests_sha256": manifests,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true",
                    help="compare the live tree against the stored snapshot")
    args = ap.parse_args()

    live = build()
    OUT.mkdir(exist_ok=True)
    target = OUT / "formal_baseline.json"
    payload = json.dumps(live, indent=2, sort_keys=True) + "\n"

    if not args.check:
        target.write_text(payload)
        print(f"wrote {target}")
        for cat, n in sorted(live["category_counts"].items()):
            print(f"  Category {cat}: {n} theorems")
        unproved = sorted(k for k, v in live["non_claims"].items() if not v)
        print(f"  non-claims (properties NOT proved): {len(unproved)}")
        for k in unproved:
            print(f"    {k}")
        return 0

    if not target.exists():
        print(f"FAIL: {target} missing; run without --check first", file=sys.stderr)
        return 1
    base = json.loads(target.read_text())

    rc = 0
    lost = sorted(set(base["categories"]["A"]) - set(live["categories"]["A"]))
    if lost:
        print("FAIL: Category A theorems disappeared without an explicit retirement:",
              file=sys.stderr)
        for name in lost:
            print(f"  {name}  (was in {base['categories']['A'][name]})", file=sys.stderr)
        rc = 1

    moved = sorted(n for n, f in live["categories"]["A"].items()
                   if n in base["categories"]["A"] and base["categories"]["A"][n] != f)
    for name in moved:
        print(f"NOTE: {name} moved {base['categories']['A'][name]} -> "
              f"{live['categories']['A'][name]}")

    base_nc, live_nc = base["non_claims"], live["non_claims"]

    new_gaps = sorted(k for k, v in live_nc.items() if not v and k not in base_nc)
    if new_gaps:
        print("FAIL: new non-claims introduced (violates P10, the unproved surface "
              "may only shrink):", file=sys.stderr)
        for name in new_gaps:
            print(f"  {name}", file=sys.stderr)
        rc = 1

    regressed = sorted(k for k, v in live_nc.items()
                       if not v and base_nc.get(k) is True)
    if regressed:
        print("FAIL: previously claimed properties are now disclaimed:", file=sys.stderr)
        for name in regressed:
            print(f"  {name}", file=sys.stderr)
        rc = 1

    for name in sorted(k for k in base_nc if k not in live_nc):
        print(f"OK: non-claim retired (property has no referent any more): {name}")
    for name in sorted(k for k, v in live_nc.items()
                       if v and base_nc.get(k) is False):
        print(f"OK: non-claim closed (property is now proved): {name}")

    if rc == 0:
        print(f"OK: {len(live['categories']['A'])} Category A theorems intact, "
              f"unproved surface did not grow")
    return rc


if __name__ == "__main__":
    sys.exit(main())

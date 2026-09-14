#!/usr/bin/env python3
"""Replace one proof body with `Admitted.` so compilation can continue.

Used by the mutation harness's isolation loop. `coqc` stops at the
first failing proof, so when a mutant breaks lemma L before theorem T,
the run can only report L and T's mutation-sensitivity is never
established. Admitting L lets the compile proceed to T.

Admitting L *assumes* L, which can only make T easier to prove. So if
T still fails with every earlier casualty admitted, T's proof depends
on the mutated definition directly, not merely through L. That is what
the harness records as a verified kill.

Files are restored by the harness afterwards; nothing here is left in
the tree.
"""
from __future__ import annotations

import pathlib
import re
import sys

DECL = re.compile(
    r"^\s*(?:Global\s+|Local\s+|#\[[^\]]*\]\s*)?"
    r"(?:Theorem|Lemma|Corollary|Remark|Fact|Example|Proposition)\s+"
    r"([A-Za-z_][A-Za-z0-9_']*)"
)
PROOF_START = re.compile(r"^\s*Proof\b")

# Anywhere on the line, not just at its start. Short proofs in this tree
# are routinely written `Proof. vm_compute. reflexivity. Qed.` on one
# line, and an anchored pattern does not match those: the terminator
# search then runs past the end of the proof and takes the *next*
# lemma's Qed, so the replacement deletes that lemma outright. The
# mutant is reported as having broken elaboration rather than as having
# killed anything, which reads as a harness problem and hides whatever
# the mutant actually did.
PROOF_END = re.compile(r"\b(?:Qed|Defined)\s*\.")


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: admit_proof.py <file.v> <theorem>", file=sys.stderr)
        return 2
    path = pathlib.Path(sys.argv[1])
    target = sys.argv[2]
    if not path.is_file():
        return 1

    lines = path.read_text().splitlines(keepends=True)

    decl_at = None
    for i, line in enumerate(lines):
        m = DECL.match(line)
        if m and m.group(1) == target:
            decl_at = i
            break
    if decl_at is None:
        return 1

    start = None
    for i in range(decl_at, len(lines)):
        if PROOF_START.match(lines[i]):
            start = i
            break
    if start is None:
        return 1

    end = None
    for i in range(start, len(lines)):
        segment = lines[i]
        if i == start:
            # Step over the `Proof` token so a one-line proof is closed by
            # its own Qed rather than by the following lemma's.
            segment = segment[PROOF_START.match(segment).end():]
        if PROOF_END.search(segment):
            end = i
            break
    if end is None:
        return 1

    lines[start:end + 1] = ["Proof.\n", "Admitted.\n"]
    path.write_text("".join(lines))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

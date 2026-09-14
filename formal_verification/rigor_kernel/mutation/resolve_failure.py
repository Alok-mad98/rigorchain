#!/usr/bin/env python3
"""Resolve which proof actually failed in a mutant compile log.

The mutation harness used to print

    EXPECTED_FAILURE id=M9 theorem=utxo_soundness

purely because the module failed to compile. The theorem name was a
label, never checked. A mutation that broke a different theorem, or a
patch that produced a syntax error, or a stale .vo, all counted as a
confirmed kill of `utxo_soundness`.

This script reads a compile log, finds the first Coq error location,
and reports the enclosing proof. The harness then requires that name
to match the declared expected-failure theorem, so a mutant has to
kill the theorem it claims to kill.

Prints one of:

    ACTUAL_FAIL=<name>          a proof failed, and this is which one
    ACTUAL_FAIL=<PARSE_ERROR>   the mutant did not type-check at all
    ACTUAL_FAIL=<UNKNOWN>       no location could be resolved
"""
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

LOC = re.compile(r'^File "([^"]+)", line (\d+)', re.M)
DECL = re.compile(
    r"^\s*(?:Global\s+|Local\s+|#\[[^\]]*\]\s*)?"
    r"(Theorem|Lemma|Corollary|Remark|Fact|Example|Proposition|Definition|"
    r"Fixpoint|Instance)\s+([A-Za-z_][A-Za-z0-9_']*)"
)
# A failure inside these means the mutant broke syntax or elaboration,
# not a proof. Those are bad mutants, not confirmed kills.
PARSE_MARKERS = (
    "Syntax error",
    "Unknown interpretation",
    "The reference",
    "was not found in the current environment",
    "Cannot find a physical path",
)


def enclosing_proof(path: pathlib.Path, line: int) -> str | None:
    """Name of the last declaration at or before `line`."""
    try:
        lines = path.read_text().splitlines()
    except OSError:
        return None
    name = None
    for i, text in enumerate(lines[:line], start=1):
        m = DECL.match(text)
        if m:
            name = m.group(2)
    return name


def main() -> int:
    if len(sys.argv) != 2:
        print("ACTUAL_FAIL=<UNKNOWN>")
        return 0
    log_path = pathlib.Path(sys.argv[1])
    try:
        log = log_path.read_text(errors="replace")
    except OSError:
        print("ACTUAL_FAIL=<UNKNOWN>")
        return 0

    m = LOC.search(log)
    if not m:
        print("ACTUAL_FAIL=<UNKNOWN>")
        return 0

    rel, line = m.group(1), int(m.group(2))
    # Coq reports "./Foo/Bar.v" relative to the kernel root.
    path = (ROOT / rel.lstrip("./")).resolve()
    if not path.is_file():
        candidate = ROOT / pathlib.Path(rel).name
        path = candidate if candidate.is_file() else path

    tail = log[m.end():m.end() + 400]
    if any(marker in tail for marker in PARSE_MARKERS):
        print("ACTUAL_FAIL=<PARSE_ERROR>")
        return 0

    name = enclosing_proof(path, line)
    print(f"ACTUAL_FAIL={name}" if name else "ACTUAL_FAIL=<UNKNOWN>")
    try:
        print(f"ACTUAL_FILE={path.relative_to(ROOT).as_posix()}")
    except ValueError:
        print("ACTUAL_FILE=<UNKNOWN>")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

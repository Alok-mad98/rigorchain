#!/usr/bin/env python3
"""Gate: no constant may exist only so that a mutant can flip it.

Frontier item 1.2. Five constants used to sit in the kernel for no
reason other than mutation testing:

    Definition kernel_step_unique     : bool := true.
    Definition executable_step_unique : bool := true.
    Definition runtime_step_unique    : bool := true.
    Definition machine_step_unique    : bool := true.
    Definition concrete_oru_bypass    : Z    := 0.

Each guarded an unreachable branch -- a second constructor of a
transition relation, or an additive term that was always zero. In the
healthy tree they did nothing. Their effect was on the evidence: a
mutant could clear the flag, the alternative branch became reachable,
determinism failed, and the suite recorded a kill. That made
determinism look like a property of the consensus rules. It was not.
The rules were a function either way, and no assignment of them can
make a function non-deterministic.

They are gone. This keeps them from coming back, in any of the three
shapes that make a definition a switch rather than a rule:

  * a nullary boolean whose only uses are as a constructor guard or in
    `unfold ...; discriminate`;
  * a nullary integer defined as 0 or 1 whose only use is as an
    additive term;
  * any constructor premise comparing a nullary constant to a literal.

A constant that does real work -- one the kernel branches on, or that
appears in a statement -- is untouched by all three.
"""
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
SKIP_PARTS = ("mutation", "extracted", "extraction", "evidence", "certicoq")

NULLARY = re.compile(
    r"^Definition\s+([A-Za-z_][A-Za-z0-9_']*)\s*:\s*(?:bool|Z)\s*:=", re.M)
# A constructor premise comparing a constant to a literal.
CONSTRUCTOR_GUARD = re.compile(
    r"^\s+([A-Za-z_][A-Za-z0-9_']*)\s*=\s*(?:true|false|[01])\s*->\s*$", re.M)
ZERO_CONST = re.compile(
    r"^Definition\s+([A-Za-z_][A-Za-z0-9_']*)\s*:\s*Z\s*:=\s*[01]\s*\.", re.M)
ENCLOSING = re.compile(r"^(Inductive|Theorem|Lemma|Corollary|Definition|"
                       r"Fixpoint|Record|Remark|Fact|Example|Proposition)\b",
                       re.M)


def source_files() -> list[pathlib.Path]:
    return [p for p in sorted(ROOT.rglob("*.v"))
            if not any(part in SKIP_PARTS
                       for part in p.relative_to(ROOT).parts)]


def inside_inductive(text: str, at: int) -> bool:
    """True when the nearest enclosing declaration is an Inductive."""
    last = None
    for m in ENCLOSING.finditer(text[:at]):
        last = m.group(1)
    return last == "Inductive"


def additive_uses_only(name: str, files: list[pathlib.Path]) -> bool:
    """True when every mention outside the definition is `+ name`."""
    seen = False
    for path in files:
        for line in path.read_text().splitlines():
            if not re.search(rf"\b{re.escape(name)}\b", line):
                continue
            if re.match(rf"^Definition\s+{re.escape(name)}\b", line):
                continue
            stripped = line.lstrip()
            if stripped.startswith("(*") or stripped.startswith("*"):
                continue
            seen = True
            if not (re.search(rf"\+\s*{re.escape(name)}\b", line)
                    or re.search(rf"unfold\s+[^.]*\b{re.escape(name)}\b", line)):
                return False
    return seen


def main() -> int:
    files = source_files()
    findings: list[str] = []

    for path in files:
        text = path.read_text()
        rel = path.relative_to(ROOT).as_posix()
        nullary = {m.group(1) for m in NULLARY.finditer(text)}
        for other in files:
            nullary |= {m.group(1) for m in NULLARY.finditer(other.read_text())}

        for m in CONSTRUCTOR_GUARD.finditer(text):
            name = m.group(1)
            if name not in nullary or not inside_inductive(text, m.start()):
                continue
            line = text[:m.start()].count("\n") + 2
            findings.append(
                f"{rel}:{line}: constructor guarded by the constant `{name}`; "
                f"a branch only a mutant can reach is not a rule")

        for m in ZERO_CONST.finditer(text):
            name = m.group(1)
            if not additive_uses_only(name, files):
                continue
            line = text[:m.start()].count("\n") + 1
            findings.append(
                f"{rel}:{line}: `{name}` is a constant zero that is only ever "
                f"added; it is a mutation switch, not a kernel rule")

    if findings:
        print("FAIL: mutation-only switches found in the kernel:",
              file=sys.stderr)
        for f in sorted(set(findings)):
            print(f"  {f}", file=sys.stderr)
        print("\nMutate a real rule instead. See mutation/lib.sh.",
              file=sys.stderr)
        return 1

    print("NO_MUTATION_ONLY_SWITCHES=TRUE")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

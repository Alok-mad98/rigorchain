#!/usr/bin/env python3
"""Gate G-1: no unproven step may reach the cone.

The banned constructs are `Admitted`, the `admit` tactic, `Axiom`,
`Parameter`, and the identifiers `refinement_proved` /
`equivalence_proved`. A closed `Print Assumptions` is the real check;
this is the cheap syntactic guard in front of it.

D-16 item 3 is also here: Path B `KernelInput` field assignments
(`ki_utxo_ok :=`, …) are forbidden outside `Kernel/KernelInputBridge.v`,
which is the only `kernel_input_of` constructor.

G-1 used to be a single `grep -nE` over the *.v files, with `Admitted`
unanchored. That matched prose as readily as code, and it eventually
matched the tree's own documentation: Audit/TaskBoundaryMapFlags.v
opens with

    No [Admitted] / [Axiom] / [Parameter].

which is a claim that the file is clean, not a violation of it. The
gate runs under `set -e` with G-1 first, so the whole thing exited
before reaching `make` -- a build error sat unnoticed behind the
false positive for 74 commits.

So the two rules a token check has to follow: read only code, and
match a token only where Coq would treat it as one.

  * Comments are removed before matching, `(* *)` nesting and string
    literals included. Blanks replace them so line numbers still
    point at the source.
  * `Admitted`, `Axiom` and `Parameter` match at statement position
    only -- the start of a sentence, where Coq accepts the command.
  * `admit` matches as a whole word, which is the tactic and never a
    substring of an identifier such as `admit_free`.

certicoq/surface is in scope: it is part of the gated build (its
modules are listed in _CoqProject.kernel-critical and cited by
Kernel/SurfaceCorrespondence.v). The rest of certicoq is a sidecar
and stays out.
"""
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
SKIP_PARTS = (".git", "_build")

STATEMENT = {
    "Admitted": re.compile(r"^[ \t]*Admitted\b", re.M),
    "Axiom": re.compile(r"^[ \t]*Axiom\b", re.M),
    "Parameter": re.compile(r"^[ \t]*Parameter\b", re.M),
}
TOKENS = {
    "admit": re.compile(r"\badmit\b"),
    "refinement_proved": re.compile(r"\brefinement_proved\b"),
    "equivalence_proved": re.compile(r"\bequivalence_proved\b"),
}

# D-16 item 3: Path B KernelInput records in the gated cone may be
# filled only by kernel_input_of. A field assignment is the construction.
KERNELINPUT_FIELDS = (
    "ki_utxo_ok",
    "ki_script_ok",
    "ki_pow_ok",
    "ki_asert_ok",
    "ki_incoming",
    "ki_outgoing",
    "ki_fees",
    "ki_burn",
    "ki_subsidy",
    "ki_res_mint",
)
KERNELINPUT_ASSIGN = re.compile(
    r"\b(" + "|".join(KERNELINPUT_FIELDS) + r")\s*:="
)
KERNELINPUT_CONSTRUCTOR_FILE = "Kernel/KernelInputBridge.v"


def strip_comments(text: str) -> str:
    """Blank out `(* ... *)` comments and string literals.

    Nesting is tracked, because Coq nests. Newlines survive so a hit
    reports the line it was found on.
    """
    out: list[str] = []
    depth = 0
    in_string = False
    i = 0
    n = len(text)
    while i < n:
        ch = text[i]
        if in_string:
            if ch == '"':
                if i + 1 < n and text[i + 1] == '"':  # Coq doubles to escape
                    out.append("  ")
                    i += 2
                    continue
                in_string = False
            out.append("\n" if ch == "\n" else " ")
            i += 1
            continue
        if depth == 0 and ch == '"':
            in_string = True
            out.append(" ")
            i += 1
            continue
        if text.startswith("(*", i):
            depth += 1
            out.append("  ")
            i += 2
            continue
        if depth > 0 and text.startswith("*)", i):
            depth -= 1
            out.append("  ")
            i += 2
            continue
        if depth > 0:
            out.append("\n" if ch == "\n" else " ")
            i += 1
            continue
        out.append(ch)
        i += 1
    return "".join(out)


def source_files() -> list[pathlib.Path]:
    files = []
    for p in sorted(ROOT.rglob("*.v")):
        parts = p.relative_to(ROOT).parts
        if any(x in SKIP_PARTS for x in parts):
            continue
        if parts[0] == "certicoq" and parts[1:2] != ("surface",):
            continue
        files.append(p)
    return files


def main() -> int:
    files = source_files()
    findings: list[str] = []

    for path in files:
        rel = path.relative_to(ROOT).as_posix()
        code = strip_comments(path.read_text())
        for label, pat in list(STATEMENT.items()) + list(TOKENS.items()):
            for m in pat.finditer(code):
                line = code[: m.start()].count("\n") + 1
                findings.append(f"{rel}:{line}: `{label}`")
        if rel != KERNELINPUT_CONSTRUCTOR_FILE:
            for m in KERNELINPUT_ASSIGN.finditer(code):
                line = code[: m.start()].count("\n") + 1
                findings.append(
                    f"{rel}:{line}: free KernelInput field `{m.group(1)} :=` "
                    f"(use kernel_input_of in {KERNELINPUT_CONSTRUCTOR_FILE})"
                )

    if findings:
        print("FAIL: forbidden token present in a .v file:", file=sys.stderr)
        for f in sorted(set(findings)):
            print(f"  {f}", file=sys.stderr)
        print(
            "\nProve it, or move the module out of the gated cone. "
            "Prose about these tokens is fine; the check reads code only. "
            "Path B KernelInput records must be built by kernel_input_of "
            f"in {KERNELINPUT_CONSTRUCTOR_FILE}.",
            file=sys.stderr,
        )
        return 1

    print("OK: no Admitted / admit / Axiom / Parameter / "
          "refinement_proved / equivalence_proved; "
          "no free KernelInput field assignment outside "
          f"{KERNELINPUT_CONSTRUCTOR_FILE}")
    print(f"FORBIDDEN_TOKENS_SCANNED={len(files)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

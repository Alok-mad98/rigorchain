#!/usr/bin/env python3
"""Gate: every Category A theorem is backed by a mutation that kills it.

Frontier item 1.1. Audit finding M-1 counted Category A theorems with
no dedicated mutant by reading the names written next to `run_mutation`.
Those names were never checked against what the mutant actually broke,
so the count was of labels, not of evidence. Re-measuring with the
isolating harness put real coverage at 43 of 65.

This closes the loop from the other side: recompute {Category A} minus
{theorems a mutation is known to kill} and fail when anything is left.
mutation/lib.sh makes the names honest; this makes them cover
Category A.

Deferrals
---------
Some exported theorems are restatements. Their proof is nothing but
`exact` or `apply` of other lemmas, possibly after `intros` and
`split` — `runtime_domain_closed` bundles four range results,
`money_range_safe` forwards to `integer_range_safe`.

No mutation of any definition can make such a proof fail. The harness
admits the broken component first, and an admitted lemma is assumed,
so the deferral still type-checks. A mutant invented for one of these
would be testing Coq's `split`, not the kernel.

They are accepted here on a stricter condition than a mutant would
give. The proof must be verified to contain nothing but deferral
tactics — no rewriting, no arithmetic, no computation — and every
lemma it defers to must itself be covered, following chains as far as
they go. A deferral is covered because its parts are, and the parts
are checked rather than asserted.
"""
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
SKIP_PARTS = ("mutation", "extracted", "extraction", "evidence", "certicoq")

# Tactics that only pass an existing result along. Anything else means
# the proof does work of its own and must be killed by a real mutant.
#
# `unfold` and `exists` are here; `simpl`, `cbn`, `destruct`, `tauto`,
# `rewrite` and `eapply` are not. The line is whether the tactic can
# discharge a goal. `unfold` replaces a name by its definition and
# `exists w` instantiates a binder; both always leave the goal open, so
# a proof that reaches `Qed` through them reached it on the strength of
# the lemmas it applied. The others can close goals, and `simpl`/`cbn`
# followed by `reflexivity` is proof by computation, which is how the
# witness lemmas in the chain cone are proved. Letting them in was
# measured: four names out of eighty-six, in exchange for the reason
# the exemption is allowed at all.
#
# `exists w` only looks like content. Supplying a witness is the work
# in an existence proof, but a step that supplies one and then closes
# with `exact L` has put the work in `L`, which names the same witness
# and is checked on its own. `exists w. vm_compute. reflexivity.` is
# the version that does the work inline, and it is still rejected --
# `vm_compute` is not listed, and a proof with no `exact`/`apply` has
# no components, which is not a deferral either.
DEFERRAL_TACTIC = re.compile(r"""^(?:
      intros?(?:\s+\S.*)?
    | split
    | subst
    | assumption
    | reflexivity
    | unfold\s+.*
    | symmetry
    | exists\s+[A-Za-z_][A-Za-z0-9_']*
      (?:\s*,\s*[A-Za-z_][A-Za-z0-9_']*)*
    | exact\s+[A-Za-z_][A-Za-z0-9_']*
    | apply\s+[A-Za-z_][A-Za-z0-9_']*(?:\s+in\s+[A-Za-z_][A-Za-z0-9_']*)?
  )$""", re.X)
NAME_REF = re.compile(r"(?:exact|apply)\s+([A-Za-z_][A-Za-z0-9_']*)")
# `symmetry` and a multi-witness `exists a, b` are on the list for the
# same reason as the rest of it: neither can close a goal. `symmetry`
# rewrites `a = b` to `b = a` and leaves it open; `exists` supplies
# witnesses and leaves the body to be proved. A proof made only of
# those still has to reach an `exact` or `apply` naming a lemma that is
# checked on its own, or it has no components and is not a deferral.
# `Qed.` is routinely written on the same line as the last tactic. Anchoring
# it to the start of a line reads past the end of the proof and swallows the
# following lemmas, which is how a real deferral gets reported as doing work
# of its own. Frontier 2.4 fixed the same anchor in mutation/admit_proof.py;
# this gate kept it. 244 proofs in the cone use the one-line form.
PROOF_END = re.compile(r"\b(?:Qed|Defined|Admitted)\s*\.")
DECL = re.compile(
    r"^\s*(?:Theorem|Lemma|Corollary|Remark|Fact|Example|Proposition)\s+"
    r"([A-Za-z_][A-Za-z0-9_']*)", re.M)


def source_files() -> list[pathlib.Path]:
    return [p for p in sorted(ROOT.rglob("*.v"))
            if not any(part in SKIP_PARTS for part in p.relative_to(ROOT).parts)]


def strip_comments(text: str) -> str:
    """Blank out `(* ... *)`, keeping offsets and line structure.

    Prose inside a comment reads like source to a regex. `Theorem names
    that the kernel inventory already cites` in Supply/SupplySchedule.v
    is one sentence of documentation, and a scanner that allows leading
    whitespace will emit `Print Assumptions names.` for it. Replacing
    comment bodies with spaces rather than deleting them keeps line and
    column numbers usable for reporting.
    """
    out = list(text)
    depth = 0
    i = 0
    in_string = False
    while i < len(text):
        two = text[i:i + 2]
        if not in_string and two == "(*":
            depth += 1
            out[i] = out[i + 1] = " "
            i += 2
            continue
        if not in_string and two == "*)" and depth:
            depth -= 1
            out[i] = out[i + 1] = " "
            i += 2
            continue
        if depth:
            if text[i] != "\n":
                out[i] = " "
        elif text[i] == '"':
            in_string = not in_string
        i += 1
    return "".join(out)


def read_source(path: pathlib.Path) -> str:
    return strip_comments(path.read_text())


def category_a() -> set[str]:
    text = (ROOT / "release/TheoremClassification.md").read_text()
    start, end = text.find("Category A"), text.find("Category B")
    if start < 0 or end < 0:
        sys.exit("check_mutation_coverage: cannot locate Category A table")
    return {m.group(1)
            for m in re.finditer(r"^\|\s*`([^`]+)`\s*\|", text[start:end], re.M)
            if m.group(1) != "Theorem"}


def declared_targets() -> set[str]:
    named: set[str] = set()
    for script in sorted((ROOT / "mutation").glob("mutate_*.sh")):
        for m in re.finditer(r'run_mutation\s+M\d+\s+("[^"]*"|\S+)',
                             script.read_text()):
            named.update(m.group(1).strip('"').split())
    return named


def collect_proofs() -> tuple[dict[str, str], set[str]]:
    """Map every global proof name to its proof script."""
    proofs: dict[str, str] = {}
    for path in source_files():
        text = read_source(path)
        for m in DECL.finditer(text):
            name = m.group(1)
            tail = text[m.end():]
            stop = PROOF_END.search(tail)
            body = tail[:stop.start()] if stop else tail
            proofs[name] = body[body.find("Proof."):] if "Proof." in body else ""
    return proofs, set(proofs)


def deferral_components(proof: str, known: set[str]) -> list[str] | None:
    """Lemmas a deferral proof forwards to, or None if it is not one."""
    if not proof.strip():
        return None
    body = proof[len("Proof."):] if proof.startswith("Proof.") else proof
    # Destructuring patterns name hypotheses, not lemmas, and the
    # brackets around them would otherwise look like proof structure.
    body = re.sub(r"\bas\s+\[[^\]]*\]", "", body)
    body = re.sub(r"\bas\s+->", "", body)
    body = re.sub(r"\bintros?\b[^.;]*",
                  lambda m: re.sub(r"\[[^\]]*\]", "", m.group(0)), body)
    # Flatten the remaining bracket/bullet structure; it carries no
    # proof content.
    flat = re.sub(r"[\[\]|]", ";", body)
    flat = re.sub(r"^\s*[-+*]+", ";", flat, flags=re.M)
    # A Coq terminator is a period followed by whitespace. Splitting on
    # every period cuts `SurfUTXO.check_utxo` in half and leaves
    # `SurfUTXO` standing alone as a step, which matches no tactic, so
    # any proof naming a module-qualified constant was reported as
    # doing work of its own. The error was one-sided -- it overstated
    # the debt and never excused anything -- but it hid which proofs
    # are deferrals.
    steps = [s.strip() for s in re.split(r";|\.(?=\s|$)", flat) if s.strip()]
    if not steps:
        return None
    for step in steps:
        if not DEFERRAL_TACTIC.match(step):
            return None
    return [n for step in steps for n in NAME_REF.findall(step) if n in known]


def main() -> int:
    cat_a = category_a()
    named = declared_targets()
    proofs, known = collect_proofs()

    resolving: set[str] = set()
    memo: dict[str, bool] = {}

    def covered(name: str) -> bool:
        if name in named:
            return True
        if name in memo:
            return memo[name]
        if name in resolving:          # a cycle proves nothing
            return False
        resolving.add(name)
        components = deferral_components(proofs.get(name, ""), known)
        ok = bool(components) and all(covered(c) for c in components)
        resolving.discard(name)
        memo[name] = ok
        return ok

    failures = []
    deferrals = 0
    for name in sorted(cat_a):
        if name in named:
            continue
        components = deferral_components(proofs.get(name, ""), known)
        if not components:
            failures.append(f"  {name}: no mutation names it, and its proof "
                            f"does real work, so it needs a mutant")
            continue
        missing = [c for c in components if not covered(c)]
        if missing:
            failures.append(f"  {name}: defers to uncovered "
                            f"{', '.join(sorted(set(missing)))}")
            continue
        deferrals += 1

    print(f"CATEGORY_A_TOTAL={len(cat_a)}")
    print(f"CATEGORY_A_KILLED_DIRECTLY={len(cat_a & named)}")
    print(f"CATEGORY_A_COVERED_BY_DEFERRAL={deferrals}")

    if failures:
        print("FAIL: Category A theorems without mutation coverage:",
              file=sys.stderr)
        for line in failures:
            print(line, file=sys.stderr)
        return 1

    print("MUTATION_COVERAGE_CATEGORY_A=COMPLETE")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

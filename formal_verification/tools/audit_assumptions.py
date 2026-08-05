#!/usr/bin/env python3
"""SECURITY-001-F: axiom/admit inventory for the Rigor Coq corpus.

For every top-level named result in every module listed in `coq/_CoqProject`,
run Coq's `Print Assumptions` and record whether the result is closed under the
global context (unconditional) or rests on axioms / admitted lemmas.

This exists because finding F-3 of the pre-mainnet security review found `Qed`
theorems silently inheriting `Admitted` lemmas: `Qed` alone is not evidence that
a result is unconditional.

Usage:
    cd formal_verification && python3 tools/audit_assumptions.py [--json OUT]
    ... --modules 'ReplayReorgCore/*'   audit a subset (substring or glob)
    ... --strict                        exit non-zero if anything rests on axioms

Requires the corpus to be built first (`make` in formal_verification/coq).
"""

from __future__ import annotations

import argparse
import fnmatch
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

FV = Path(__file__).resolve().parents[1]
COQ_DIR = FV / "coq"
COQ_PROJECT = COQ_DIR / "_CoqProject"

# Top-level result keywords whose assumptions are worth auditing.
RESULT_RE = re.compile(
    r"^\s*(Theorem|Lemma|Corollary|Proposition|Remark|Fact|Example)\s+([A-Za-z_][A-Za-z0-9_']*)\s*[:{(]",
    re.MULTILINE,
)
DECLARED_AXIOM_RE = re.compile(
    r"^\s*(Axiom|Parameter|Hypothesis|Variable|Conjecture)\s+([A-Za-z_][A-Za-z0-9_']*)",
    re.MULTILINE,
)


def module_of(rel_path: str) -> str:
    """coq/_CoqProject entry -> logical module name under the `Rigor` root."""
    return "Rigor." + rel_path[:-2].replace("/", ".")


def project_modules() -> list[tuple[str, str]]:
    """Return [(relative .v path, logical module name)] in _CoqProject order."""
    out: list[tuple[str, str]] = []
    for line in COQ_PROJECT.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("-"):
            continue
        if line.endswith(".v"):
            out.append((line, module_of(line)))
    return out


def results_in(path: Path) -> list[str]:
    text = path.read_text(encoding="utf-8")
    # Drop comments so commented-out examples are not mistaken for real results.
    text = re.sub(r"\(\*.*?\*\)", " ", text, flags=re.DOTALL)
    seen: list[str] = []
    for _, name in RESULT_RE.findall(text):
        if name not in seen:
            seen.append(name)
    return seen


def declared_axioms_in(path: Path) -> list[str]:
    text = re.sub(r"\(\*.*?\*\)", " ", path.read_text(encoding="utf-8"), flags=re.DOTALL)
    return [name for _, name in DECLARED_AXIOM_RE.findall(text)]


def audit_module(rel_path: str, module: str, names: list[str]) -> dict[str, dict]:
    """Run `Print Assumptions` for every name in one module."""
    if not names:
        return {}
    lines = [f"Require Import {module}."]
    for n in names:
        # Sentinel lines let us attribute each Coq response to a name.
        lines.append(f'Print Assumptions {module}.{n}.')
    src = "\n".join(lines) + "\n"

    with tempfile.TemporaryDirectory() as tmp:
        vfile = Path(tmp) / "RigorAssumptionProbe.v"
        vfile.write_text(src, encoding="utf-8")
        proc = subprocess.run(
            ["coqc", "-q", "-R", str(COQ_DIR), "Rigor", str(vfile)],
            capture_output=True,
            text=True,
            cwd=str(COQ_DIR),
        )

    if proc.returncode != 0:
        # Fall back to one probe per name so a single bad name cannot hide the rest.
        if len(names) == 1:
            return {
                names[0]: {
                    "module": module,
                    "file": rel_path,
                    "status": "PROBE_ERROR",
                    "assumptions": [],
                    "detail": proc.stderr.strip()[-400:],
                }
            }
        merged: dict[str, dict] = {}
        for n in names:
            merged.update(audit_module(rel_path, module, [n]))
        return merged

    return parse_assumptions(proc.stdout, rel_path, module, names)


def parse_assumptions(
    stdout: str, rel_path: str, module: str, names: list[str]
) -> dict[str, dict]:
    """Split Coq's output into one block per `Print Assumptions` invocation.

    Coq emits either `Closed under the global context` or `Axioms:` followed by
    an indented list, one block per command, in command order.
    """
    blocks: list[list[str]] = []
    current: list[str] | None = None
    for line in stdout.splitlines():
        if line.startswith("Closed under the global context") or line.startswith("Axioms:"):
            if current is not None:
                blocks.append(current)
            current = [line]
        elif current is not None:
            current.append(line)
    if current is not None:
        blocks.append(current)

    out: dict[str, dict] = {}
    for i, name in enumerate(names):
        if i >= len(blocks):
            out[name] = {
                "module": module,
                "file": rel_path,
                "status": "NO_OUTPUT",
                "assumptions": [],
            }
            continue
        block = blocks[i]
        if block[0].startswith("Closed under the global context"):
            out[name] = {
                "module": module,
                "file": rel_path,
                "status": "AXIOM_FREE",
                "assumptions": [],
            }
        else:
            # Inside an `Axioms:` block Coq prints one entry per axiom starting
            # at column 0 (`name : type`), with the type wrapped onto indented
            # continuation lines. Only the unindented lines name an axiom.
            assumptions = []
            for raw in block[1:]:
                if not raw.strip():
                    continue
                if raw[:1].isspace():
                    continue
                assumptions.append(raw.split(":", 1)[0].strip())
            out[name] = {
                "module": module,
                "file": rel_path,
                "status": "DEPENDS_ON_AXIOMS",
                "assumptions": sorted(set(a for a in assumptions if a)),
            }
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", type=Path, default=None, help="write full inventory as JSON")
    ap.add_argument(
        "--modules",
        default=None,
        help="only audit _CoqProject paths matching this glob or substring",
    )
    ap.add_argument(
        "--strict",
        action="store_true",
        help="exit non-zero if any result rests on an axiom or admitted lemma",
    )
    args = ap.parse_args()

    modules = project_modules()
    if args.modules:
        pat = args.modules
        modules = [
            (p, m) for p, m in modules if fnmatch.fnmatch(p, pat) or pat in p
        ]
        if not modules:
            print(f"no _CoqProject entry matches {pat!r}", file=sys.stderr)
            return 2
    inventory: dict[str, dict] = {}
    declared: list[tuple[str, str]] = []

    total = len(modules)
    for i, (rel_path, module) in enumerate(modules, 1):
        path = COQ_DIR / rel_path
        if not path.is_file():
            print(f"[{i}/{total}] MISSING {rel_path}", file=sys.stderr)
            continue
        for ax in declared_axioms_in(path):
            declared.append((rel_path, ax))
        names = results_in(path)
        print(f"[{i}/{total}] {rel_path}: {len(names)} results", file=sys.stderr)
        for name, rec in audit_module(rel_path, module, names).items():
            inventory[f"{module}.{name}"] = rec

    axiom_free = [k for k, v in inventory.items() if v["status"] == "AXIOM_FREE"]
    tainted = {k: v for k, v in inventory.items() if v["status"] == "DEPENDS_ON_AXIOMS"}
    errors = {
        k: v
        for k, v in inventory.items()
        if v["status"] not in ("AXIOM_FREE", "DEPENDS_ON_AXIOMS")
    }

    print()
    print("=" * 72)
    print("RIGOR COQ AXIOM / ADMIT INVENTORY")
    print("=" * 72)
    print(f"Modules audited          : {total}")
    print(f"Top-level results        : {len(inventory)}")
    print(f"  AXIOM_FREE             : {len(axiom_free)}")
    print(f"  DEPENDS_ON_AXIOMS      : {len(tainted)}")
    print(f"  probe errors           : {len(errors)}")
    print(f"Declared Axiom/Parameter : {len(declared)}")
    for rel_path, ax in declared:
        print(f"    {ax}  ({rel_path})")
    print()

    if tainted:
        print("RESULTS THAT ARE NOT UNCONDITIONAL:")
        for k in sorted(tainted):
            v = tainted[k]
            print(f"  {k}")
            print(f"      file: {v['file']}")
            print(f"      rests on: {', '.join(v['assumptions']) or '(unparsed)'}")
    else:
        print("No result depends on an axiom or admitted lemma.")

    if errors:
        print()
        print("PROBE ERRORS (could not determine status):")
        for k in sorted(errors):
            print(f"  {k}: {errors[k]['status']} {errors[k].get('detail','')}")

    if args.json:
        args.json.write_text(
            json.dumps(
                {
                    "summary": {
                        "modules": total,
                        "results": len(inventory),
                        "axiom_free": len(axiom_free),
                        "depends_on_axioms": len(tainted),
                        "probe_errors": len(errors),
                        "declared_axioms": [
                            {"file": f, "name": n} for f, n in declared
                        ],
                    },
                    "results": inventory,
                },
                indent=1,
                sort_keys=True,
            ),
            encoding="utf-8",
        )
        print(f"\nJSON written to {args.json}")

    if errors:
        return 1
    return 1 if (args.strict and tainted) else 0


if __name__ == "__main__":
    sys.exit(main())

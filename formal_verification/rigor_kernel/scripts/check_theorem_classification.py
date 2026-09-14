#!/usr/bin/env python3
"""CI: every (theorem, file) in TheoremClassification.md must exist.

Also emits a Print Assumptions driver from Category A/B/C so
ALL_ASSUMPTIONS=CLOSED cannot drift from the table.

Reverse check (whole cone): every Theorem/Corollary name
must appear in a classification table or in UnclassifiedAllowlist.md.

Category A/B/C name-sets of the canonical table and each --also view
must be identical, so a theorem cannot be classified in one view only
or sit in different categories in the two.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MD = ROOT / "release" / "TheoremClassification.md"
DEFAULT_ALLOWLIST = ROOT / "release" / "UnclassifiedAllowlist.md"
# Audit finding M-2: the reverse scan used to cover only Kernel/ and
# proofs/, leaving 127 theorems in 14 other directories invisible to
# classification governance. It now walks the whole cone.
SKIP_PARTS = ("archive", "_build", "mutation", "extraction", "extracted", "certicoq")

ROW = re.compile(
    r"^\|\s*`([^`]+)`\s*\|\s*`([^`]+)`\s*\|"
)
SECTION = re.compile(r"^##+\s+(Category|Class)\s+([ABC])\b", re.I)
THM = re.compile(r"^(?:Theorem|Corollary)\s+(\w+)", re.M)
ALLOW_NAME = re.compile(r"^`?([A-Za-z_][A-Za-z0-9_]*)`?\s+\S+\.v\b")


def parse_tables(text: str) -> dict[str, list[tuple[str, str]]]:
    current = None
    tables: dict[str, list[tuple[str, str]]] = {"A": [], "B": [], "C": []}
    for line in text.splitlines():
        m = SECTION.match(line.strip())
        if m:
            current = m.group(2).upper()
            continue
        if current is None:
            continue
        row = ROW.match(line)
        if not row:
            continue
        names = row.group(1).strip()
        path = row.group(2).strip()
        for name in re.split(r"\s*/\s*", names):
            name = name.strip()
            if name:
                tables[current].append((name, path))
    return tables


def expand_name(name: str) -> list[str]:
    if "…" in name or "..." in name:
        # Explicit expansions used in older rows.
        if "utxo_extraction_source_eq" in name:
            return [
                "utxo_extraction_source_eq",
                "oru_extraction_source_eq",
                "res_extraction_source_eq",
                "script_extraction_source_eq",
                "pow_extraction_source_eq",
                "asert_extraction_source_eq",
            ]
        return []
    return [name]


def table_names(tables: dict[str, list[tuple[str, str]]]) -> set[str]:
    out: set[str] = set()
    for rows in tables.values():
        for name, _rel in rows:
            for thm in expand_name(name):
                if "*" not in thm:
                    out.add(thm)
    return out


def category_names(tables: dict[str, list[tuple[str, str]]], cat: str) -> set[str]:
    out: set[str] = set()
    for name, _rel in tables[cat]:
        for thm in expand_name(name):
            if "*" not in thm:
                out.add(thm)
    return out


def find_in_file(path: Path, name: str) -> bool:
    if not path.is_file():
        return False
    text = path.read_text()
    if "*" in name:
        prefix = name.split("*", 1)[0]
        suffix = name.rsplit("*", 1)[-1]
        for line in text.splitlines():
            if re.match(r"^\s*(Theorem|Lemma|Corollary|Definition|Example)\s+\S+", line):
                ident = line.split()[1]
                ident = ident.split(":")[0]
                if ident.startswith(prefix) and ident.endswith(suffix):
                    return True
        return False
    return bool(
        re.search(
            rf"^(Theorem|Lemma|Corollary|Definition|Example)\s+{re.escape(name)}\b",
            text,
            re.M,
        )
    )


def check_md(md: Path) -> tuple[dict[str, list[tuple[str, str]]], list[str]]:
    tables = parse_tables(md.read_text())
    errors: list[str] = []
    if not tables["A"] or not tables["B"]:
        errors.append("%s: empty Category A or B table" % md)
    seen = set()
    for cat, rows in tables.items():
        for name, rel in rows:
            for thm in expand_name(name):
                key = (thm, rel)
                if key in seen:
                    continue
                seen.add(key)
                p = ROOT / rel
                if not p.is_file():
                    errors.append("[%s] missing file %s (theorem %s)" % (cat, rel, thm))
                    continue
                if not find_in_file(p, thm):
                    errors.append(
                        "[%s] %s not found in %s" % (cat, thm, rel)
                    )
    return tables, errors


def load_allowlist(path: Path) -> set[str]:
    names: set[str] = set()
    if not path.is_file():
        return names
    for line in path.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#") or line.startswith("|"):
            continue
        if line.startswith("- "):
            line = line[2:].strip()
        m = ALLOW_NAME.match(line)
        if m:
            names.add(m.group(1))
    return names


def iter_scoped_theorems() -> list[tuple[str, str]]:
    found: list[tuple[str, str]] = []
    for p in sorted(ROOT.rglob("*.v")):
        if any(x in p.parts for x in SKIP_PARTS):
            continue
        rel = p.relative_to(ROOT).as_posix()
        for m in THM.finditer(p.read_text()):
            found.append((m.group(1), rel))
    return found


def emit_print_assumptions(tables: dict[str, list[tuple[str, str]]], dest: Path) -> int:
    lines = [
        "(* generated from release/TheoremClassification.md Category A/B/C *)",
        "(* Do not edit by hand. *)",
    ]
    n = 0
    seen = set()
    for cat in ("A", "B", "C"):
        for name, rel in tables[cat]:
            for thm in expand_name(name):
                if "*" in thm:
                    continue
                if thm in seen:
                    continue
                seen.add(thm)
                mod = rel[:-2].replace("/", ".") if rel.endswith(".v") else rel.replace("/", ".")
                lines.append(f"From RigorKernel Require Import {mod}.")
                lines.append(f"Print Assumptions {thm}.")
                n += 1
    dest.write_text("\n".join(lines) + "\n")
    return n


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--md", type=Path, default=DEFAULT_MD)
    ap.add_argument("--also", type=Path, action="append", default=[])
    ap.add_argument("--allowlist", type=Path, default=DEFAULT_ALLOWLIST)
    ap.add_argument("--emit", type=Path, help="write Print Assumptions .v")
    args = ap.parse_args()

    all_errors: list[str] = []
    tables, errors = check_md(args.md)
    all_errors.extend(errors)
    extras: list[dict[str, list[tuple[str, str]]]] = []
    for extra in args.also:
        extra_tables, err = check_md(extra)
        extras.append(extra_tables)
        all_errors.extend(err)

    # Only Category A used to be compared across views. That left B and
    # C free to diverge, and they had: four theorems were classified in
    # the evidence copy and in no other, so the reverse check --- which
    # unions the views --- accepted them while the canonical table did
    # not mention them at all. All three categories are compared now,
    # so a name can no longer be classified in one view only, and a
    # theorem cannot be quietly A here and B there.
    for cat in ("A", "B", "C"):
        canonical = category_names(tables, cat)
        for extra, extra_path in zip(extras, args.also):
            other = category_names(extra, cat)
            missing = sorted(canonical - other)
            extra_only = sorted(other - canonical)
            if missing:
                all_errors.append(
                    "Category %s mismatch vs %s: missing %s"
                    % (cat, extra_path, ", ".join(missing))
                )
            if extra_only:
                all_errors.append(
                    "Category %s mismatch vs %s: extra %s"
                    % (cat, extra_path, ", ".join(extra_only))
                )

    classified = table_names(tables)
    for extra in extras:
        classified |= table_names(extra)
    allow = load_allowlist(args.allowlist)
    if not args.allowlist.is_file():
        all_errors.append("missing allowlist %s" % args.allowlist)
    scoped = iter_scoped_theorems()
    for name, rel in scoped:
        if name in classified or name in allow:
            continue
        all_errors.append(
            "unclassified %s in %s (add to a classification table or %s)"
            % (name, rel, args.allowlist.name)
        )

    if all_errors:
        for e in all_errors:
            print("FAIL:", e, file=sys.stderr)
        return 1

    n_a = sum(len(expand_name(n)) for n, _ in tables["A"])
    n_b = sum(len(expand_name(n)) for n, _ in tables["B"] if "*" not in n)
    print(
        f"THEOREM_CLASSIFICATION_GREP=PASS A={n_a} B={n_b} C={len(tables['C'])} "
        f"CATEGORIES_IDENTICAL=TRUE REVERSE_CHECK=PASS scoped={len(scoped)} "
        f"allowlisted={len(allow)}"
    )

    if args.emit:
        n = emit_print_assumptions(tables, args.emit)
        print(f"PRINT_ASSUMPTIONS_EMITTED={n} path={args.emit}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

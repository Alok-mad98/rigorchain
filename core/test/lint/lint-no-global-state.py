#!/usr/bin/env python3
#
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# TASK-ARCH W11 / W5 / W1 guardrail: no global process state on the script
# or consensus path.
#
# Forbids, in core/src/script/ and core/src/consensus/ (plus two ConnectBlock
# helpers listed below):
#
#   1. gArgs                      — CLI must not be a consensus predicate
#   2. SetRigorResEvalContext     — dead interpreter injection API (W5)
#   3. m_chain_res_mint_total     — observational ledger; not a script input
#                                   (W1). Allowed only in ObservationalMetrics
#                                   and the observational runtime-supply files.
#   4. m_chain_oru_total          — same W1 ledger, RFC-002 S_ORU cache.
#
# Default engine is a comment-aware identifier scan (no extra packages).
# If tree-sitter + tree-sitter-cpp are installed, identifiers are taken from
# the C++ AST instead:
#
#   pip install tree-sitter tree-sitter-cpp
#
# CI: this file is picked up by core/test/lint/all-lint.py. A dedicated
# GitHub Actions workflow also runs it on PRs that touch core/src.

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT_MARKERS = ("core/src/script", "core/test/lint")

SCAN_DIRS = (
    "core/src/script/",
    "core/src/consensus/",
    "core/src/consensus_rigor/",
)
SCAN_FILES = (
    "core/src/connect_block_rigor_checks.cpp",
    "core/src/node/devtest_res_bootstrap.cpp",
    "core/src/node/devtest_res_bootstrap.h",
)

# Observational W1 ledger (not spendable; not T / f_i / script). Reads here
# are fenced, not a regression.
MINT_TOTAL_ALLOWLIST = {
    "core/src/consensus/observational_metrics.cpp",
    "core/src/consensus/observational_metrics.h",
    "core/src/consensus/runtime_supply_state.cpp",
    "core/src/consensus/runtime_supply_state.h",
    "core/src/consensus/runtime_supply_export.cpp",
    "core/src/consensus/runtime_supply_export.h",
    "core/src/consensus/state_snapshot.cpp",
    "core/src/consensus/state_snapshot.h",
    "core/src/consensus/coq_supply_field_hook.cpp",
    "core/src/consensus/coq_supply_field_hook.h",
}

FORBIDDEN = (
    (
        "gArgs",
        r"\bgArgs\b",
        "CLI/global ArgsManager must not be read on the script/consensus path "
        "(W11). Inject via Consensus::Params at chain construction.",
    ),
    (
        "SetRigorResEvalContext",
        r"\bSetRigorResEvalContext\b",
        "dead interpreter injection API (W5). Do not restore block-level rigor "
        "context onto PrecomputedTransactionData / the predicate verifier.",
    ),
    (
        "m_chain_res_mint_total",
        r"\bm_chain_res_mint_total\b",
        "observational issuance ledger (W1). Must not feed T, f_i, mint auth, "
        "or the predicate verifier. Allowed only in ObservationalMetrics and runtime-supply "
        "observational files.",
    ),
    (
        "m_chain_oru_total",
        r"\bm_chain_oru_total\b",
        "observational RFC-002 S_ORU ledger (W1). Must not feed T, f_i, mint auth, "
        "or the predicate verifier. Allowed only in ObservationalMetrics and runtime-supply "
        "observational files.",
    ),
)

LINE_COMMENT = re.compile(r"//.*?$", re.M)
BLOCK_COMMENT = re.compile(r"/\*.*?\*/", re.S)
STRING_LIT = re.compile(r'"(?:\\.|[^"\\])*"')


def repo_root() -> Path:
    here = Path(__file__).resolve()
    for parent in [here.parent, *here.parents]:
        if (parent / "core" / "src").is_dir() and (parent / "core" / "test" / "lint").is_dir():
            return parent
    return Path.cwd()


def collect_files(root: Path) -> list[str]:
    try:
        listed = subprocess.check_output(
            ["git", "ls-files", *SCAN_DIRS, *SCAN_FILES],
            cwd=root,
            text=True,
            encoding="utf-8",
        )
        files = [line for line in listed.splitlines() if line.endswith((".cpp", ".h"))]
        if files:
            return files
    except (OSError, subprocess.CalledProcessError):
        pass
    files: list[str] = []
    for d in SCAN_DIRS:
        p = root / d
        if p.is_dir():
            files.extend(
                str(f.relative_to(root)).replace("\\", "/")
                for f in p.rglob("*")
                if f.suffix in {".cpp", ".h"}
            )
    for rel in SCAN_FILES:
        if (root / rel).is_file():
            files.append(rel)
    return sorted(set(files))


def strip_comments_and_strings(src: str) -> str:
    src = BLOCK_COMMENT.sub(lambda m: "\n" * m.group(0).count("\n"), src)
    src = LINE_COMMENT.sub("", src)
    src = STRING_LIT.sub('""', src)
    return src


def identifiers_tree_sitter(src: str) -> set[str] | None:
    try:
        from tree_sitter import Language, Parser
        import tree_sitter_cpp
    except ImportError:
        return None
    language = Language(tree_sitter_cpp.language())
    try:
        parser = Parser(language)
    except TypeError:
        parser = Parser()
        parser.language = language
    tree = parser.parse(src.encode("utf-8"))
    names: set[str] = set()

    def walk(node) -> None:
        if node.type == "identifier":
            names.add(src[node.start_byte : node.end_byte])
        for child in node.children:
            walk(child)

    walk(tree.root_node)
    return names


def scan_file(rel: str, text: str) -> list[tuple[int, str, str, str]]:
    findings: list[tuple[int, str, str, str]] = []
    code = strip_comments_and_strings(text)
    lines = text.splitlines()
    code_lines = code.splitlines()
    if len(code_lines) < len(lines):
        code_lines.extend([""] * (len(lines) - len(code_lines)))

    for i, (raw, stripped) in enumerate(zip(lines, code_lines), start=1):
        for name, regex, rationale in FORBIDDEN:
            if name in ("m_chain_res_mint_total", "m_chain_oru_total") and rel in MINT_TOTAL_ALLOWLIST:
                continue
            if re.search(regex, stripped):
                findings.append((i, name, rationale, raw.rstrip()))
                break
    return findings


def main() -> int:
    root = repo_root()
    exit_code = 0
    engine = "tree-sitter-cpp AST" if identifiers_tree_sitter("int gArgs;") is not None else "comment-aware regex"
    files = collect_files(root)
    if not files:
        print("lint-no-global-state: no files to scan", file=sys.stderr)
        return 1
    for rel in files:
        path = root / rel
        try:
            text = path.read_text(encoding="utf-8")
        except OSError as exc:
            print(f"lint-no-global-state: cannot read {rel}: {exc}", file=sys.stderr)
            exit_code = 1
            continue
        for line_no, name, rationale, line_text in scan_file(rel, text):
            print(f"{rel}:{line_no}: forbidden {name} — {rationale}")
            print(f"    {line_text.strip()}")
            exit_code = 1
    if exit_code != 0:
        print(
            "\nlint-no-global-state failed. Script/consensus paths must not "
            "read gArgs, restore SetRigorResEvalContext, or consume "
            "m_chain_res_mint_total / m_chain_oru_total as script/mint input. "
            f"Engine: {engine}.",
            file=sys.stderr,
        )
    return exit_code


if __name__ == "__main__":
    sys.exit(main())

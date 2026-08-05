#!/usr/bin/env python3
#
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# TASK-H2 H-4-1 / H-4-2:
#   Static guardrail for the RIGOR consensus directories. Any future
#   commit that re-introduces non-deterministic primitives into the
#   consensus path will fail this lint at PR time.
#
# What this lint forbids (subject to the same authoritative reasoning
# in `core/doc/replay-determinism-checklist.md`):
#
#   D-1  Wall-clock reads in consensus paths
#        GetTime, GetTimeMillis, GetTimeMicros, GetSystemTimeInSeconds,
#        GetMockTime
#
#   D-2  Randomness sources in consensus paths
#        GetRand, GetRandBytes, GetStrongRandBytes, FastRandomContext,
#        InsecureRand
#
#   B-1  Hash-table-ordered containers in consensus paths
#        std::unordered_map, std::unordered_set, std::unordered_multimap,
#        std::unordered_multiset
#
# Scope: matches are reported only inside the consensus directories
# enumerated in `CONSENSUS_DIRS` below. The lint is intentionally a
# textual block-list; it doesn't try to parse C++ semantics, but it does
# pin the symbol names that would have caused TASK-H2 R-3 / R-5 / R-10
# style replay drift had they slipped in unnoticed.

import re
import subprocess
import sys

# Code directories that are part of the consensus path. Adjust here ONLY
# if the project genuinely adds a new consensus-bearing directory; do
# NOT loosen this list to dodge a violation.
CONSENSUS_DIRS = [
    "core/src/consensus/",
    "core/src/consensus_rigor/",
]

# Pattern groups. Each entry: (regex, label, rationale).
FORBIDDEN_PATTERNS = [
    (
        r"\b(GetTime|GetTimeMillis|GetTimeMicros|GetSystemTimeInSeconds|GetMockTime)\s*\(",
        "wall-clock read",
        "checklist D-1: consensus paths must never read wall-clock time",
    ),
    (
        r"\b(GetRand|GetRandBytes|GetStrongRandBytes|InsecureRand|FastRandomContext)\b",
        "randomness source",
        "checklist D-2: consensus paths must never use an RNG",
    ),
    (
        r"std::unordered_(map|set|multimap|multiset)\b",
        "hash-table-ordered container",
        "checklist B-1: consensus state must not be produced by iterating "
        "an unordered_map / unordered_set",
    ),
]

# A line containing one of these markers is intentionally exempt. Use
# sparingly and only on lines that document, not exercise, the symbol.
EXEMPT_MARKERS = (
    "NOT FOR CONSENSUS PATHS",
    "consensus-discipline-allow",
)


def collect_files():
    out = subprocess.check_output(
        ["git", "ls-files", *CONSENSUS_DIRS], text=True, encoding="utf-8"
    )
    return [
        line
        for line in out.splitlines()
        if line.endswith(".cpp") or line.endswith(".h")
    ]


def scan_file(path, compiled_patterns):
    """Return a list of (line_no, pattern_label, rationale, line_text)."""
    findings = []
    try:
        with open(path, "r", encoding="utf-8") as fh:
            for line_no, line in enumerate(fh, start=1):
                stripped = line.rstrip("\n")
                if any(marker in stripped for marker in EXEMPT_MARKERS):
                    continue
                for regex, label, rationale in compiled_patterns:
                    if regex.search(stripped):
                        findings.append((line_no, label, rationale, stripped))
                        break
    except OSError as exc:
        print(f"lint-consensus-discipline: cannot read {path}: {exc}", file=sys.stderr)
    return findings


def main():
    compiled = [
        (re.compile(rx), label, rationale)
        for rx, label, rationale in FORBIDDEN_PATTERNS
    ]
    exit_code = 0
    for path in collect_files():
        for line_no, label, rationale, line_text in scan_file(path, compiled):
            print(f"{path}:{line_no}: forbidden {label} — {rationale}")
            print(f"    {line_text.strip()}")
            exit_code = 1
    if exit_code != 0:
        print(
            "\nlint-consensus-discipline failed. See "
            "core/doc/replay-determinism-checklist.md for the owning rule. "
            "If a match is a documented exception, annotate the line with "
            "either 'NOT FOR CONSENSUS PATHS' or 'consensus-discipline-allow' "
            "and justify the exemption in a code review.",
            file=sys.stderr,
        )
    sys.exit(exit_code)


if __name__ == "__main__":
    main()

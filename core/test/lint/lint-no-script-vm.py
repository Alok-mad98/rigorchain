#!/usr/bin/env python3
#
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# P0-027 R-2: enforce that RIGOR has no script VM.
#
# RIGOR spends are `ScriptPubKey bytes -> ClassifyRigorTxout -> fixed
# predicate -> Ed25519`. Bytes are classified, never dispatched. No byte in
# the namespace names an operation, so there is no evaluator, no operand
# stack, and no conditional-execution state.
#
# This lint exists because that property cannot be asserted from inside the
# code. A helper that returns a constant `false` proves nothing: no future
# commit would make it start returning true, so a unit test calling it can
# never fail. What a future commit *would* do is reintroduce the machinery
# named below, so that is what is pinned here.
#
# Removed by P0-026 / P0-027 and forbidden from returning:
#
#   EvalScript / ExecuteOpcode   the evaluator entry points
#   stacktop / altstacktop / popstack
#                                operand- and alt-stack manipulation
#   ConditionStack / vfExec      IF / NOTIF / ELSE execution state
#   VerifyScript                 Bitcoin's dispatcher; RIGOR uses
#                                VerifyRigorPredicate
#
# Scope is the directories that decide whether coins move. A stack machine
# built in, say, the wallet cannot make an invalid spend valid.

import re
import subprocess
import sys

# Directories on the consensus spend path. Do NOT narrow this list to dodge
# a violation.
SCOPED_DIRS = [
    "core/src/script/",
    "core/src/consensus/",
    "core/src/consensus_rigor/",
]

# Each entry: (regex, label, rationale).
FORBIDDEN_PATTERNS = [
    (
        r"\b(EvalScript|ExecuteOpcode|ExecuteWitnessScript)\b",
        "script evaluator",
        "RIGOR classifies ScriptPubKey bytes; it does not execute them",
    ),
    (
        r"\b(stacktop|altstacktop|popstack)\b",
        "VM operand stack",
        "a fixed predicate reads its witness directly; there is no stack machine",
    ),
    (
        r"\b(ConditionStack|vfExec)\b",
        "conditional execution state",
        "no byte is an instruction, so there is no IF/ELSE to track",
    ),
    (
        r"\bVerifyScript\b",
        "Bitcoin script dispatcher",
        "the consensus entry point is VerifyRigorPredicate",
    ),
]

# A line containing one of these markers is exempt. Use only on lines that
# document the absence of a symbol rather than reintroducing it.
EXEMPT_MARKERS = ("script-vm-allow",)


def collect_files():
    out = subprocess.check_output(
        ["git", "ls-files", *SCOPED_DIRS], text=True, encoding="utf-8"
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
        print(f"lint-no-script-vm: cannot read {path}: {exc}", file=sys.stderr)
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
            "\nlint-no-script-vm failed. RIGOR has no script VM; see "
            "docs/security/RIGOR_OPCODE_NAMESPACE_CLOSURE_P0_027.md. "
            "Reintroducing an evaluator is a constitutional change, not a "
            "refactor. A line that only documents an absent symbol may be "
            "annotated with 'script-vm-allow'.",
            file=sys.stderr,
        )
    sys.exit(exit_code)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Reverse tests for the retired PRODUCTION_KERNEL_SWITCHED lint."""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

_SPEC = importlib.util.spec_from_file_location(
    "lint_kernel_switch",
    ROOT / "tools" / "lint-kernel-switch-single-source.py",
)
_MOD = importlib.util.module_from_spec(_SPEC)
assert _SPEC.loader is not None
_SPEC.loader.exec_module(_MOD)
retired_line_is_violation = _MOD.retired_line_is_violation
iter_scan_files = _MOD.iter_scan_files


def test_makefile_comment_is_not_a_loophole() -> None:
    line = "PRODUCTION_KERNEL_SWITCHED = True  # makefile"
    assert retired_line_is_violation("tools/foo.py", line), line


def test_plain_assignment_is_flagged() -> None:
    line = "PRODUCTION_KERNEL_SWITCHED = True"
    assert retired_line_is_violation("tools/foo.py", line), line


def test_core_src_assignment_is_flagged() -> None:
    line = "inline constexpr bool production_kernel_switched = true;"
    assert retired_line_is_violation(
        "core/src/consensus/kernel_canary/canary_metrics.h", line
    ), line


def test_policy_comment_is_allowed() -> None:
    line = "# retired PRODUCTION_KERNEL_SWITCHED must not be reintroduced"
    assert not retired_line_is_violation("tools/lint-kernel-switch-single-source.py", line)


def test_scan_includes_core_src() -> None:
    paths = {p.relative_to(ROOT).as_posix() for p in iter_scan_files(ROOT)}
    assert any(p.startswith("core/src/") and p.endswith((".h", ".cpp")) for p in paths)
    assert any(p.startswith("docs/") and p.endswith(".md") for p in paths)
    assert "tools/refinement_status.py" in paths


def test_json_key_is_flagged() -> None:
    line = '  "production_kernel_switched": false'
    assert retired_line_is_violation("formal_verification/probe.json", line), line


def test_json_key_uppercase_is_flagged() -> None:
    line = '{"PRODUCTION_KERNEL_SWITCHED": false}'
    assert retired_line_is_violation("docs/probe.json", line), line


def test_comparison_is_not_assignment() -> None:
    line = "if (PRODUCTION_KERNEL_SWITCHED == a) {}"
    assert not retired_line_is_violation("core/src/probe.h", line), line


def test_policy_json_in_policy_file_is_allowed() -> None:
    line = '"production_kernel_switched": "retired, do not reintroduce"'
    assert not retired_line_is_violation("tools/lint-kernel-switch-single-source.py", line)


def test_skip_dirs_treats_libs_as_a_component() -> None:
    skip = _MOD.SKIP_DIRS
    assert "src/.libs" not in skip
    assert "core/src/.libs" not in skip
    assert ".libs" in skip
    assert "logs" in skip


def test_json_glob_is_not_a_fixed_list() -> None:
    json_files = _MOD.iter_json_files(ROOT)
    rels = {p.relative_to(ROOT).as_posix() for p in json_files}
    assert any(p.endswith(".json") and p.startswith("formal_verification/") for p in rels)
    assert "formal_verification/consensus_kernel_single_authority.json" in rels


def main() -> int:
    tests = [
        test_makefile_comment_is_not_a_loophole,
        test_plain_assignment_is_flagged,
        test_core_src_assignment_is_flagged,
        test_policy_comment_is_allowed,
        test_scan_includes_core_src,
        test_json_key_is_flagged,
        test_json_key_uppercase_is_flagged,
        test_comparison_is_not_assignment,
        test_policy_json_in_policy_file_is_allowed,
        test_skip_dirs_treats_libs_as_a_component,
        test_json_glob_is_not_a_fixed_list,
    ]
    failed = 0
    for fn in tests:
        try:
            fn()
            print(f"PASS {fn.__name__}")
        except AssertionError as e:
            print(f"FAIL {fn.__name__}: {e}")
            failed += 1
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

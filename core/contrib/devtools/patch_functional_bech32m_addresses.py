#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# SPDX-License-Identifier: MIT
#
# =============================================================================
# ArdaChain-specific: bulk-fix address types in the functional tests (align to Bech32m / disabled types)
# =============================================================================
#
# Purpose
# -------
# After syncing `test/functional/` from Bitcoin Core upstream, the default
# `getnewaddress()` and `address_type='legacy'|'p2sh-segwit'|'bech32'` are rejected
# by the RPC on ArdaChain. This script does a regex-level bulk replacement over the
# **Python test sources**, rewriting the common patterns to an explicit
# `address_type='bech32m'` or the positional argument `"bech32m"`.
#
# Usage
# -----
#   From the `core/` directory:
#     ./contrib/devtools/patch_functional_bech32m_addresses.py              # write files back
#     ./contrib/devtools/patch_functional_bech32m_addresses.py --dry-run   # only list the files that would change
#     ./contrib/devtools/patch_functional_bech32m_addresses.py --check     # CI: whether the default rules leave nothing to patch
#     ./contrib/devtools/patch_functional_bech32m_addresses.py --aggressive # also rewrite bech32 to bech32m (use with care)
#
# Limitations and follow-up manual steps
# --------------------------------------
# - **By default** `bech32` is left alone (keyword or second positional argument), so that
#   native v0 witness cases and similar remain intact; pass `--aggressive` to convert
#   everything to bech32m (afterwards run the affected functional tests and review the diff).
# - **Nested quotes in f-strings**: writing
#     f'--x={node.getnewaddress(address_type='bech32m')}'
#   is a SyntaxError; use double quotes on the outside instead:
#     f"--x={node.getnewaddress(address_type='bech32m')}"
# - Large cases that depend on several address-type semantics (e.g. parts of `rpc_psbt`,
#   `wallet_importdescriptors`) usually need **additional** manual fixes or a SkipTest on
#   Rigor; this script is no substitute for code review.
# =============================================================================

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# core/test/functional (this file lives in core/contrib/devtools/)
ROOT = Path(__file__).resolve().parents[2] / "test" / "functional"


def patch_text(s: str, *, aggressive_bech32: bool = False) -> str:
    # Keyword address_type (any RPC)
    s = re.sub(r'address_type\s*=\s*["\']legacy["\']', "address_type='bech32m'", s)
    s = re.sub(r'address_type\s*=\s*["\']p2sh-segwit["\']', "address_type='bech32m'", s)
    if aggressive_bech32:
        s = re.sub(r'address_type\s*=\s*["\']bech32["\']', "address_type='bech32m'", s)

    # Positional getnewaddress(label, "type")
    addr_types = r"(?:legacy|p2sh-segwit)"
    if aggressive_bech32:
        addr_types = r"(?:legacy|p2sh-segwit|bech32)"
    s = re.sub(
        rf'getnewaddress\(\s*([^,()]+(?:\([^)]*\))?)\s*,\s*["\']{addr_types}["\']\s*\)',
        r'getnewaddress(\1, "bech32m")',
        s,
    )

    # Bare empty: .getnewaddress()
    s = re.sub(r'\.getnewaddress\(\s*\)', ".getnewaddress(address_type='bech32m')", s)

    # label='coinbase' without trailing address_type
    s = re.sub(
        r'\.getnewaddress\(\s*label\s*=\s*([\'"])coinbase\1\s*\)',
        r".getnewaddress(label='coinbase', address_type='bech32m')",
        s,
    )

    # label.name as sole positional-style (one argument, no comma)
    s = re.sub(
        r'\.getnewaddress\(\s*label\.name\s*\)',
        ".getnewaddress(label.name, address_type='bech32m')",
        s,
    )

    # feature_taproot random address types
    s = s.replace(
        "address_type=random.choice([\"legacy\", \"p2sh-segwit\", \"bech32\"])",
        "address_type='bech32m'",
    )

    return s


def iter_functional_py_files():
    if not ROOT.is_dir():
        return
    for path in sorted(ROOT.rglob("*.py")):
        if path.name == "patch_functional_bech32m_addresses.py":
            continue
        yield path


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="ArdaChain: bulk-fix address_type / getnewaddress() patterns under test/functional/.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print paths that would change; do not write files.",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Exit with code 1 if any file still needs patching (for CI). Implies no writes.",
    )
    parser.add_argument(
        "--aggressive",
        action="store_true",
        help="Also rewrite bech32 (keyword and 2nd positional arg) to bech32m.",
    )
    args = parser.parse_args(argv)

    dry = args.dry_run or args.check
    aggressive = args.aggressive

    if not ROOT.is_dir():
        print(f"Missing directory: {ROOT}", file=sys.stderr)
        return 1

    changed: list[Path] = []
    for path in iter_functional_py_files():
        text = path.read_text(encoding="utf-8")
        new = patch_text(text, aggressive_bech32=aggressive)
        if new != text:
            rel = path.relative_to(ROOT.parent.parent)
            changed.append(rel)
            if not dry:
                path.write_text(new, encoding="utf-8")

    for rel in changed:
        print(rel)

    if args.check and changed:
        mode = "aggressive " if aggressive else ""
        print(
            f"\n--check: {len(changed)} file(s) still match {mode}patch rules. "
            "Run without --check to apply, or fix manually.",
            file=sys.stderr,
        )
        return 1

    if args.dry_run and changed:
        print(f"\n(--dry-run: {len(changed)} file(s) would be modified.)", file=sys.stderr)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

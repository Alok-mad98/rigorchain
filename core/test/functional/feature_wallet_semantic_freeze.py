#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""TASK-B4-003 P7B: wallet semantic freeze guard (regression grep, not a feature test)."""

from pathlib import Path
import re
import subprocess
import sys

REPO_ROOT = Path(__file__).resolve().parents[3]

# Paths scanned for forbidden wallet/UI semantic regressions.
SCAN_ROOTS = [
    REPO_ROOT / "core" / "src" / "wallet",
    REPO_ROOT / "core" / "src" / "qt",
    REPO_ROOT / "core" / "src" / "interfaces",
]

SKIP_DIRS = {"locale", "res", "forms"}

# (description, regex) — match anywhere in scanned file content.
FORBIDDEN_PATTERNS = [
    ("dead RES mempool fee path MinFeeMet_RES", r"\bMinFeeMet_RES\b"),
    ("mixed-asset input blocker reintroduced", r"bad-rigor-mixed-asset-inputs"),
    (
        "RES_FEE_UNIT_SCALE used in fee math (not the deprecated constant definition)",
        r"RES_FEE_UNIT_SCALE\s*[\*/]",
    ),
    ("same-asset fee wording in Qt", r'Transaction fee \(RES\)'),
    ("Taproot/script-path in PSBT RES destination summary", r"Taproot/script-path"),
    ("Bech32m labeled as Taproot in Qt sources", r"Bech32m \(Taproot\)"),
    ("mixed-domain WalletTx credit field", r"struct WalletTx[\s\S]{0,800}\bCAmount credit;"),
    ("mixed-domain WalletTx debit field", r"struct WalletTx[\s\S]{0,800}\bCAmount debit;"),
    ("RES-scaled fee arithmetic res_in - res_out", r"fee\s*=\s*res_in\s*-\s*res_out"),
    ("RES reused/dirty coin-selection guard", r"OutputBlockedByResReuse"),
    (
        "Bitcoin history label Mined in Qt formatTxType",
        r'formatTxType[\s\S]{0,400}tr\("Mined"\)',
    ),
    (
        "Bitcoin history label Sent to in Qt formatTxType",
        r'formatTxType[\s\S]{0,400}tr\("Sent to"\)',
    ),
    (
        "Bitcoin history label Received with in Qt formatTxType",
        r'formatTxType[\s\S]{0,400}tr\("Received with"\)',
    ),
]

# Files allowed to mention deprecated symbols in comments/tests only.
ALLOWLIST_FILES = {
    "fee_domain.h",  # DEPRECATED_RES_FEE_UNIT_SCALE definition
    "wallet_oru_fee_dominance_tests.cpp",
    "WALLET_SEMANTIC_FREEZE.md",
    "feature_wallet_semantic_freeze.py",
}


def iter_source_files():
    for root in SCAN_ROOTS:
        if not root.is_dir():
            continue
        for path in root.rglob("*"):
            if not path.is_file():
                continue
            if path.suffix not in {".cpp", ".h"}:
                continue
            if any(part in SKIP_DIRS for part in path.parts):
                continue
            if path.name in ALLOWLIST_FILES:
                continue
            yield path


def scan_file(path: Path):
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        return [(str(path), f"unreadable: {exc}")]
    hits = []
    for desc, pattern in FORBIDDEN_PATTERNS:
        if re.search(pattern, text):
            hits.append((str(path.relative_to(REPO_ROOT)), desc))
    return hits


def main():
  failures = []
  for path in iter_source_files():
      failures.extend(scan_file(path))

  tx_semantic_h = REPO_ROOT / "core" / "src" / "wallet" / "tx_semantic.h"
  if not tx_semantic_h.is_file():
      failures.append(("core/src/wallet/tx_semantic.h", "missing canonical semantic classifier"))

  if failures:
      print("FREEZE GUARD FAIL — semantic regression patterns detected:", file=sys.stderr)
      for location, reason in failures:
          print(f"  {location}: {reason}", file=sys.stderr)
      sys.exit(1)

  print("FREEZE GUARD PASS — wallet semantic patterns clean")


if __name__ == "__main__":
    main()

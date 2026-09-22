"""RGRPOW01 Compatibility Test Suite v1.0 runner."""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

GROUPS = [
    ("Vectors", "test_hash"),
    ("Serialization", "test_header_serialization"),
    ("Endian", "test_endianness"),
    ("Negative", "test_negative_cases"),
    ("Cross Implementation", "test_cross_implementation"),
    ("RPC Mining", "test_rpc_mining"),
    ("Coinbase Rules", "test_coinbase"),
    ("Stratum", "test_stratum"),
    ("Fuzz", "test_fuzz"),
]


def run_module(mod: str) -> unittest.TestResult:
    loader = unittest.defaultTestLoader
    suite = loader.loadTestsFromName(mod)
    runner = unittest.TextTestRunner(stream=sys.stdout, verbosity=1)
    return runner.run(suite)


def main() -> int:
    print("=" * 32)
    print("RGRPOW01 Compatibility Test")
    print("=" * 32)
    summary: list[tuple[str, str]] = []
    all_ok = True
    for label, mod in GROUPS:
        print(f"\n--- {label} ---")
        result = run_module(mod)
        ok = result.wasSuccessful()
        status = "PASS" if ok else "FAIL"
        if not ok:
            all_ok = False
        summary.append((label, status))
    print()
    print("=" * 32)
    print("RGRPOW01 Compatibility Test")
    for label, status in summary:
        print()
        print(f"{label}:")
        print(status)
    print()
    print("=" * 32)
    return 0 if all_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())

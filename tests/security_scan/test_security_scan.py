#!/usr/bin/env python3
"""Self-test for scripts/security_scan.py. Does not store fixtures in git."""
from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCAN = ROOT / "scripts" / "security_scan.py"


def run_scan(*paths: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCAN), "--paths", *paths],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )


class SecurityScanSelfTest(unittest.TestCase):
    def test_fake_private_key_blocked(self) -> None:
        with tempfile.NamedTemporaryFile(
            mode="w",
            suffix=".txt",
            delete=False,
            encoding="utf-8",
        ) as fh:
            fh.write("-----BEGIN " + "OPENSSH PRIVATE KEY-----\n")
            fh.write("TEST-NOT-A-REAL-KEY\n")
            fh.write("-----END " + "OPENSSH PRIVATE KEY-----\n")
            path = fh.name
        try:
            proc = run_scan(path)
        finally:
            Path(path).unlink(missing_ok=True)
        self.assertEqual(proc.returncode, 1, proc.stdout + proc.stderr)
        self.assertIn("SECURITY CHECK FAILED", proc.stdout)
        self.assertIn("PRIVATE KEY", proc.stdout)

    def test_fake_token_blocked(self) -> None:
        with tempfile.NamedTemporaryFile(
            mode="w",
            suffix=".md",
            delete=False,
            encoding="utf-8",
        ) as fh:
            fh.write("token=" + "ghp_" + "abcdefghijklmnopqrstuvwxyz0123456789\n")
            path = fh.name
        try:
            proc = run_scan(path)
        finally:
            Path(path).unlink(missing_ok=True)
        self.assertEqual(proc.returncode, 1, proc.stdout + proc.stderr)
        self.assertIn("SECURITY CHECK FAILED", proc.stdout)
        self.assertTrue("TOKEN" in proc.stdout or "CREDENTIAL" in proc.stdout)

    def test_normal_documentation_passes(self) -> None:
        with tempfile.NamedTemporaryFile(
            mode="w",
            suffix=".md",
            delete=False,
            encoding="utf-8",
        ) as fh:
            fh.write("# RGRPOW01\n\n")
            fh.write("Public docs may describe the protocol, mining APIs,\n")
            fh.write("and that RPC should bind to 127.0.0.1.\n")
            fh.write("Do not ship default rpcpassword=<GENERATE_AT_DEPLOY>.\n")
            path = fh.name
        try:
            proc = run_scan(path)
        finally:
            Path(path).unlink(missing_ok=True)
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertIn("security_scan: pass", proc.stdout)


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3
"""Wrapper for TASK-005 legal encode(decode) vectors. Not a consensus change."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

TARGET = Path(__file__).resolve().parents[1] / "tests" / "serialization_vectors" / "test_roundtrip.py"
if not TARGET.is_file():
    print("missing", TARGET)
    raise SystemExit(2)
raise SystemExit(subprocess.call([sys.executable, str(TARGET)]))

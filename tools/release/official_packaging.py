#!/usr/bin/env python3
"""Official GitHub-Release artifact names (v0.1.3 layout).

Release engineering only. Does not touch consensus, RFC, Coq, claim flags,
or packed CLIENT_VERSION.
"""
from __future__ import annotations

import re
from pathlib import Path

HEADER = Path(__file__).resolve().parents[2] / "core" / "src" / "clientversion.h"
NAMING_FOLLOWS = "https://github.com/rigorchain/rigorchain/releases/tag/v0.1.3"
PACKED_CLIENT_VERSION = "25.0.0"


def version_from_header(header: Path | None = None) -> str:
    text = (header or HEADER).read_text(encoding="utf-8")
    m = re.search(
        r'RIGOR_RELEASE_VERSION\s*=\s*"v?([0-9]+(?:\.[0-9]+)*)"',
        text,
    )
    if not m:
        raise SystemExit("FAIL: RIGOR_RELEASE_VERSION not found in clientversion.h")
    return m.group(1)


def official_artifacts(ver: str) -> tuple[str, ...]:
    return (
        f"rigor-v{ver}-windows-x86_64.exe",
        f"rigor-v{ver}-windows-x86_64.zip",
        f"rigor-v{ver}-linux-x86_64.tar.gz",
        f"rigor-v{ver}-macos-x86_64.dmg",
        f"rigor-v{ver}-macos-arm64.dmg",
        "SHA256SUMS",
        f"release_v{ver}_manifest.json",
    )


def miner_artifacts(ver: str) -> tuple[str, ...]:
    """Parallel miner product line. Never merged into the wallet SHA256SUMS."""
    return (
        f"rigor-v{ver}-miner-windows-x86_64.exe",
        f"rigor-v{ver}-miner-windows-x86_64.zip",
        f"rigor-v{ver}-miner-linux-x86_64.tar.gz",
        "SHA256SUMS",
        f"release_v{ver}_miner_manifest.json",
    )


def present_without_macos(ver: str) -> tuple[str, ...]:
    return (
        f"rigor-v{ver}-windows-x86_64.exe",
        f"rigor-v{ver}-windows-x86_64.zip",
        f"rigor-v{ver}-linux-x86_64.tar.gz",
    )


def macos_pending(ver: str) -> tuple[str, ...]:
    return (
        f"rigor-v{ver}-macos-x86_64.dmg",
        f"rigor-v{ver}-macos-arm64.dmg",
    )


def is_forbidden_legacy_name(name: str) -> bool:
    return "alphaplus" in name.lower()


if __name__ == "__main__":
    print(version_from_header())

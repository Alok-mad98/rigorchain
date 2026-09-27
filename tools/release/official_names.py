#!/usr/bin/env python3
"""Official rigor-v0.1.3 artifact names. Release engineering only.

Does not touch Consensus Kernel, Transaction Kernel, Ledger, ConnectBlock,
DisconnectBlock, CoinsDB, wallet schema, classifier, PoW, ASERT, protocol
parameters, formal proofs, or Release Guardian stages.
"""
from __future__ import annotations

from pathlib import Path

VERSION = "0.1.3"
VERSION_TAG = f"v{VERSION}"

# Retired AlphaPlus user-download name. Must not be an official 0.1.3 artifact.
OLD_LINUX_ARTIFACT = "rigor-v0.1.2-alphaplus-linux.tar.gz"

OFFICIAL_ARTIFACTS = (
    "rigor-v0.1.3-windows-x86_64.exe",
    "rigor-v0.1.3-windows-x86_64.zip",
    "rigor-v0.1.3-linux-x86_64.tar.gz",
    "rigor-v0.1.3-macos-x86_64.dmg",
    "rigor-v0.1.3-macos-arm64.dmg",
)

SHA256_SIDECARS = tuple(f"{name}.sha256" for name in OFFICIAL_ARTIFACTS)

MATRIX = (
    {
        "target": "windows-x86_64",
        "os_name": "windows",
        "arch": "x86_64",
        "runner": "windows-latest",
        "runner_notes": "GitHub-hosted Windows x86_64",
        "artifacts": (
            "rigor-v0.1.3-windows-x86_64.exe",
            "rigor-v0.1.3-windows-x86_64.zip",
        ),
    },
    {
        "target": "linux-x86_64",
        "os_name": "linux",
        "arch": "x86_64",
        "runner": "ubuntu-latest",
        "runner_notes": "GitHub-hosted Linux x86_64",
        "artifacts": ("rigor-v0.1.3-linux-x86_64.tar.gz",),
    },
    {
        "target": "macos-x86_64",
        "os_name": "macos",
        "arch": "x86_64",
        "runner": "macos-15-intel",
        "runner_notes": "NOT_RELEASE_READY: no forged DMG; wallet DMGs stay on release-wallet-macos.yml",
        "artifacts": ("NOT_RELEASE_READY.txt",),
    },
    {
        "target": "macos-arm64",
        "os_name": "macos",
        "arch": "arm64",
        "runner": "macos-15",
        "runner_notes": "NOT_RELEASE_READY: no forged DMG; wallet DMGs stay on release-wallet-macos.yml",
        "artifacts": ("NOT_RELEASE_READY.txt",),
    },
)

TARGETS = {row["target"]: row for row in MATRIX}


def artifact_for(os_name: str, arch: str, kind: str) -> str:
    if os_name == "windows" and kind == "exe":
        return f"rigor-v{VERSION}-windows-{arch}.exe"
    if os_name == "windows" and kind == "zip":
        return f"rigor-v{VERSION}-windows-{arch}.zip"
    if os_name == "linux" and kind == "tar.gz":
        return f"rigor-v{VERSION}-linux-{arch}.tar.gz"
    if os_name == "macos" and kind == "dmg":
        return f"rigor-v{VERSION}-macos-{arch}.dmg"
    raise ValueError(f"no official name for {os_name} {arch} {kind}")


def is_official_name(name: str) -> bool:
    return name in OFFICIAL_ARTIFACTS


def is_release_build_artifact_name(name: str) -> bool:
 """Names produced by release-build.yml after (real node or NOT_RELEASE_READY)."""
    if is_forbidden_legacy_name(name):
        return False
    if "identity" in name.lower():
        return False
    if name == "NOT_RELEASE_READY.txt":
        return True
    if name in OFFICIAL_ARTIFACTS and not name.endswith(".dmg"):
        return True
    return False


def expected_release_build_names(target: str, out_dir: Path | None = None) -> tuple[str, ...]:
    """Filenames this job must produce for verify/checksum."""
    row = TARGETS[target]
    if out_dir is not None and (Path(out_dir) / "NOT_RELEASE_READY.txt").is_file():
        return ("NOT_RELEASE_READY.txt",)
    return tuple(row["artifacts"])


def is_forbidden_legacy_name(name: str) -> bool:
    return name == OLD_LINUX_ARTIFACT or "alphaplus" in name.lower()

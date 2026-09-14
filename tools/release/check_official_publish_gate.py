#!/usr/bin/env python3
"""Fail-closed gate before GitHub Release upload (Pipeline A).

Rejects identity stubs and files that are not in dist/release/v*/official/.
Does not publish. Does not modify formal_verification/ or consensus.
"""
from __future__ import annotations

import argparse
import tarfile
import zipfile
from pathlib import Path

FORBIDDEN_TOKEN = b"RIGOR_RELEASE_IDENTITY=PASS"
MIN_USER_BYTES = 1_000_000

OFFICIAL_NAMES = (
    "rigor-v0.1.3-windows-x86_64.exe",
    "rigor-v0.1.3-windows-x86_64.zip",
    "rigor-v0.1.3-linux-x86_64.tar.gz",
    "rigor-v0.1.3-macos-x86_64.dmg",
    "rigor-v0.1.3-macos-arm64.dmg",
)


def die(msg: str) -> None:
    print(f"FAIL: {msg}")
    raise SystemExit(1)


def reject_identity(path: Path, blob: bytes) -> None:
    if FORBIDDEN_TOKEN in blob[: min(len(blob), 4096)] or FORBIDDEN_TOKEN in blob:
        die(f"{path.name}: identity stub token present")
    if "identity" in path.name.lower():
        die(f"{path.name}: identity infix is not an official user download")


def check_linux_tar(path: Path, blob: bytes) -> None:
    if len(blob) < MIN_USER_BYTES:
        die(f"{path.name}: {len(blob)} bytes < {MIN_USER_BYTES} (not a node package)")
    with tarfile.open(path, "r:gz") as tf:
        names = tf.getnames()
    if any("identity" in n.lower() for n in names):
        die(f"{path.name}: contains identity member")
    if not any(n.endswith("bin/rigord") or n == "bin/rigord" for n in names):
        die(f"{path.name}: missing bin/rigord")


def check_windows_exe(path: Path, blob: bytes) -> None:
    if len(blob) < MIN_USER_BYTES:
        die(f"{path.name}: {len(blob)} bytes (stub / not NSIS)")
    if blob[:2] != b"MZ":
        die(f"{path.name}: not MZ (not a Windows PE/installer)")
    if b"NullsoftInst" not in blob:
        die(f"{path.name}: missing NullsoftInst (not the official NSIS installer)")


def check_windows_zip(path: Path, blob: bytes) -> None:
    if len(blob) < MIN_USER_BYTES:
        die(f"{path.name}: {len(blob)} bytes")
    with zipfile.ZipFile(path) as zf:
        names = zf.namelist()
        if any("identity" in n.lower() for n in names):
            die(f"{path.name}: contains identity member")
        exe_members = [n for n in names if n.lower().endswith(".exe")]
        if not exe_members:
            die(f"{path.name}: no .exe member")
        sample = zf.read(exe_members[0])
    if sample[:2] != b"MZ":
        die(f"{path.name}: zip exe is not MZ")


def check_dmg(path: Path, blob: bytes) -> None:
    if len(blob) < MIN_USER_BYTES:
        die(f"{path.name}: {len(blob)} bytes (wallet smoke rejects < 1 MiB)")


def main() -> int:
    p = argparse.ArgumentParser(description="Pipeline A publish gate")
    p.add_argument(
        "--official-dir",
        default="dist/release/v0.1.3/official",
        help="only this directory may hold user-facing names",
    )
    args = p.parse_args()
    official = Path(args.official_dir)
    if not official.is_dir():
        die(f"official dir missing: {official} (Pipeline A stage only)")

    # Matrix / CI output must never be this directory.
    if official.name != "official":
        die(f"{official} basename must be 'official' (not linux-x86_64 / from-build)")

    for name in OFFICIAL_NAMES:
        path = official / name
        if not path.is_file():
            die(f"missing official artifact {path}")
        blob = path.read_bytes()
        reject_identity(path, blob)
        if name.endswith(".tar.gz"):
            check_linux_tar(path, blob)
        elif name.endswith(".exe"):
            check_windows_exe(path, blob)
        elif name.endswith(".zip"):
            check_windows_zip(path, blob)
        elif name.endswith(".dmg"):
            check_dmg(path, blob)
        print(f"GATE_OK={name} bytes={path.stat().st_size}")

    print("OFFICIAL_PUBLISH_GATE=PASS")
    print("PIPELINE=A")
    print("USER_FACING_DIR=official")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

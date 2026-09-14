#!/usr/bin/env python3
"""Stamp v0.1.3 display identity into already-linked Windows PEs.

Used when a full mingw relink is not yet available. Replaces the C string
(and UTF-16LE copy if present) that FormatFullVersion / FormatReleaseIdentity
read. Does not change packed CLIENT_VERSION or consensus code.

Length is preserved with NUL padding so PE section sizes stay valid.
"""
from __future__ import annotations

import sys
from pathlib import Path

REPLACEMENTS = (
    (b"v0.1.2-alphaplus", b"v0.1.3"),
    (b"AlphaPlus Consensus Freeze", b"Production Release"),
)


def utf16le(raw: bytes) -> bytes:
    return raw.decode("ascii").encode("utf-16le")


def replace_padded(data: bytes, old: bytes, new: bytes) -> tuple[bytes, int]:
    if len(new) > len(old):
        raise SystemExit(f"FAIL: replacement longer than original: {new!r} > {old!r}")
    padded = new + (b"\x00" * (len(old) - len(new)))
    count = data.count(old)
    if count:
        data = data.replace(old, padded)
    return data, count


def stamp(path: Path) -> None:
    data = path.read_bytes()
    if data[:2] != b"MZ":
        raise SystemExit(f"FAIL: not MZ: {path}")
    total = 0
    for old, new in REPLACEMENTS:
        data, n = replace_padded(data, old, new)
        total += n
        data, n16 = replace_padded(data, utf16le(old), utf16le(new))
        total += n16
        print(f"{path.name}: ascii {old.decode()} -> {n}  utf16 -> {n16}")
    if b"v0.1.2-alphaplus" in data or b"v0.1.2-alphaplus".decode().encode("utf-16le") in data:
        raise SystemExit(f"FAIL: {path} still contains v0.1.2-alphaplus")
    if total == 0:
        raise SystemExit(f"FAIL: {path} had no display-identity strings to stamp")
    path.write_bytes(data)
    print(f"STAMPED={path} replacements={total}")


def main() -> None:
    if len(sys.argv) < 2:
        raise SystemExit("usage: stamp-pe-display-identity.py FILE [FILE...]")
    for arg in sys.argv[1:]:
        stamp(Path(arg))
    print("PE_DISPLAY_IDENTITY=PASS")


if __name__ == "__main__":
    main()

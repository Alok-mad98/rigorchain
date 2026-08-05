#!/usr/bin/env python3
# Pack a finished app-icon PNG into 1024×1024 for Qt :/icons/rigor (no chroma-key, no squircle mask).
# Scales the entire image to fit (no cropping): rounded-rect + margins stay intact.
# Letterboxing: transparent if the source uses alpha; else solid pad from corner luminance
# (dark artwork e.g. splash → black bars; light icons → white bars).
# Usage: python3 build_app_icon.py [source.png]
# Default: _source_appicon.png in this directory.
# Also writes rigor.ico (Qt dir + core/share/pixmaps) for Windows after rigor.png.
# Optionally writes rigor.icns for macOS bundle when `icnsutil` is installed:
#   pip install icnsutil

from __future__ import annotations

import shutil
import sys
import tempfile
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
# HERE = …/core/src/qt/res/icons → core root is four parents up
CORE_ROOT = HERE.parent.parent.parent.parent
SRC = HERE / "_source_appicon.png"
OUT = HERE / "rigor.png"
ICO_OUT = HERE / "rigor.ico"
ICO_SHARE = CORE_ROOT / "share" / "pixmaps" / "rigor.ico"
ICNS_OUT = HERE / "rigor.icns"
CANVAS = 1024


def _opaque_letterbox_pad(im: Image.Image) -> tuple[int, int, int, int]:
    """Solid RGBA for letterboxing when the source has no meaningful transparency."""
    w, h = im.size
    corners = (
        im.getpixel((0, 0)),
        im.getpixel((w - 1, 0)),
        im.getpixel((0, h - 1)),
        im.getpixel((w - 1, h - 1)),
    )
    lumas: list[float] = []
    for p in corners:
        r, g, b = p[0], p[1], p[2]
        lumas.append(0.299 * r + 0.587 * g + 0.114 * b)
    if sum(lumas) / len(lumas) < 90.0:
        return (0, 0, 0, 255)
    return (255, 255, 255, 255)


def pack_icon(src: Path, dst: Path) -> None:
    im = Image.open(src).convert("RGBA")
    uses_alpha = im.getchannel("A").getextrema() != (255, 255)

    w, h = im.size
    scale = min(CANVAS / w, CANVAS / h)
    nw = max(1, int(round(w * scale)))
    nh = max(1, int(round(h * scale)))
    resized = im.resize((nw, nh), Image.Resampling.LANCZOS)

    pad = (0, 0, 0, 0) if uses_alpha else _opaque_letterbox_pad(im)
    canvas = Image.new("RGBA", (CANVAS, CANVAS), pad)
    ox = (CANVAS - nw) // 2
    oy = (CANVAS - nh) // 2
    canvas.paste(resized, (ox, oy), resized)
    canvas.save(dst, "PNG", optimize=True)


def write_macos_icns_from_rigor_png(rigor: Path) -> bool:
    """Build multi-resolution rigor.icns next to rigor.png (requires icnsutil)."""
    try:
        from icnsutil import IcnsFile  # type: ignore[import-untyped]
    except ImportError:
        return False
    im = Image.open(rigor).convert("RGBA")
    layers = (
        ("icp4", 16),
        ("icp5", 32),
        ("icp6", 48),
        ("ic07", 128),
        ("ic08", 256),
        ("ic09", 512),
        ("ic10", 1024),
        ("ic11", 32),
        ("ic12", 64),
        ("ic13", 256),
        ("ic14", 512),
    )
    icns = IcnsFile()
    with tempfile.TemporaryDirectory() as td:
        tdp = Path(td)
        for key, s in layers:
            p = tdp / f"{key}.png"
            im.resize((s, s), Image.Resampling.LANCZOS).save(p, format="PNG")
            icns.add_media(file=str(p))
    icns.write(str(ICNS_OUT))
    return True


def write_windows_icons_from_rigor_png(rigor: Path) -> None:
    """Embed multi-size Windows .ico next to rigor.png and under share/pixmaps (RC + NSIS)."""
    im = Image.open(rigor).convert("RGBA")
    im256 = im.resize((256, 256), Image.Resampling.LANCZOS)
    im256.save(ICO_OUT, format="ICO")
    ICO_SHARE.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(ICO_OUT, ICO_SHARE)


def main() -> int:
    src = Path(sys.argv[1]) if len(sys.argv) > 1 else SRC
    if not src.is_file():
        print(f"Missing: {src}", file=sys.stderr)
        return 1
    pack_icon(src, OUT)
    print(f"Wrote {OUT} ({CANVAS}×{CANVAS}, centered fit, no extra processing)")
    write_windows_icons_from_rigor_png(OUT)
    print(f"Wrote {ICO_OUT} and {ICO_SHARE}")
    if write_macos_icns_from_rigor_png(OUT):
        print(f"Wrote {ICNS_OUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

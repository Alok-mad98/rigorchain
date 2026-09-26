#!/usr/bin/env python3
"""RIGOR Desktop Symbol System v1.0 — 32x32 pixel art, nearest-neighbor 96x96."""

from pathlib import Path

from PIL import Image

OUT = Path(__file__).resolve().parents[1] / "public" / "icons"
SCALE = 3

K = (0, 0, 0, 255)
W = (255, 255, 255, 255)
G = (192, 192, 192, 255)
D = (128, 128, 128, 255)
N = (0, 0, 128, 255)
T = (0, 102, 102, 255)
T2 = (0, 128, 128, 255)
C = (64, 64, 64, 255)
S = (223, 223, 223, 255)
CY = (0, 160, 160, 255)
R = (160, 32, 32, 255)
X = (0, 0, 0, 0)


def canvas():
    return [[X for _ in range(32)] for _ in range(32)]


def rect(px, x, y, w, h, color):
    for yy in range(y, y + h):
        for xx in range(x, x + w):
            if 0 <= xx < 32 and 0 <= yy < 32:
                px[yy][xx] = color


def dot(px, x, y, color):
    if 0 <= x < 32 and 0 <= y < 32:
        px[y][x] = color


def hline(px, x, y, w, color):
    rect(px, x, y, w, 1, color)


def vline(px, x, y, h, color):
    rect(px, x, y, 1, h, color)


def outline(px):
    """1px black outline around opaque pixels — shared 16-bit software edge."""
    solid = {(x, y) for y in range(32) for x in range(32) if px[y][x][3] == 255}
    extra = []
    for x, y in solid:
        for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            nx, ny = x + dx, y + dy
            if 0 <= nx < 32 and 0 <= ny < 32 and px[ny][nx][3] == 0:
                extra.append((nx, ny))
    for x, y in extra:
        px[y][x] = K


def protocol_blocks(px, x, y):
    """Recurring RIGOR mark: three navy/teal blocks."""
    rect(px, x, y, 3, 3, N)
    rect(px, x + 4, y, 3, 3, T)
    rect(px, x + 8, y, 3, 3, N)


def hash_mark(px, x, y, color=N):
    """Pixel hash, not emoji."""
    rect(px, x + 1, y, 2, 8, color)
    rect(px, x + 5, y, 2, 8, color)
    rect(px, x, y + 2, 8, 2, color)
    rect(px, x, y + 5, 8, 2, color)


def paper(px, x, y, w, h):
    rect(px, x, y, w, h, W)
    rect(px, x + w - 4, y, 4, 4, G)
    hline(px, x, y, w, D)


def save(name, px):
    outline(px)
    im = Image.new("RGBA", (32, 32))
    im.putdata([px[y][x] for y in range(32) for x in range(32)])
    out = im.resize((32 * SCALE, 32 * SCALE), Image.NEAREST)
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / name
    out.save(path, "PNG")
    print(path)


def my_computer():
    """CRT terminal with protocol mark on the screen."""
    p = canvas()
    rect(p, 5, 2, 22, 17, D)
    rect(p, 6, 3, 20, 15, G)
    rect(p, 8, 5, 16, 11, T)
    hline(p, 8, 8, 16, T2)
    hline(p, 8, 11, 16, T2)
    protocol_blocks(p, 10, 7)
    rect(p, 22, 13, 1, 2, CY)
    rect(p, 13, 19, 6, 3, G)
    rect(p, 8, 22, 16, 5, D)
    hline(p, 8, 22, 16, S)
    rect(p, 10, 24, 4, 2, T)
    rect(p, 18, 24, 4, 2, N)
    return p


def readme():
    """Protocol introduction: paper + hash + blocks."""
    p = canvas()
    paper(p, 7, 2, 18, 28)
    hash_mark(p, 12, 8, N)
    protocol_blocks(p, 10, 20)
    hline(p, 10, 26, 10, T)
    return p


def documents():
    """Stacked technical pages. Not a yellow folder."""
    p = canvas()
    rect(p, 11, 3, 15, 22, D)
    rect(p, 12, 4, 13, 20, G)
    paper(p, 6, 7, 17, 23)
    protocol_blocks(p, 9, 13)
    hline(p, 9, 19, 11, C)
    hline(p, 9, 22, 11, C)
    hline(p, 9, 25, 7, T)
    return p


def network():
    """Three connected peers: ●──●──●"""
    p = canvas()
    nodes = ((3, 12), (13, 12), (23, 12))
    hline(p, 8, 15, 6, K)
    hline(p, 18, 15, 6, K)
    hline(p, 8, 16, 6, N)
    hline(p, 18, 16, 6, N)
    for x, y in nodes:
        rect(p, x, y, 6, 6, D)
        rect(p, x + 1, y + 1, 4, 4, T)
        rect(p, x + 2, y + 2, 2, 2, N)
    return p


def disc(px, cx, cy, r, color):
    for y in range(cy - r, cy + r + 1):
        for x in range(cx - r, cx + r + 1):
            if (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r:
                dot(px, x, y, color)


def browser():
    """Old window + globe + connected blocks. Not Internet Explorer."""
    p = canvas()
    rect(p, 3, 3, 26, 26, D)
    rect(p, 4, 4, 24, 24, G)
    rect(p, 5, 5, 22, 4, N)
    rect(p, 6, 6, 6, 2, W)
    disc(p, 16, 16, 6, T)
    disc(p, 16, 16, 5, T2)
    vline(p, 16, 10, 13, N)
    hline(p, 10, 16, 13, N)
    hline(p, 12, 13, 9, N)
    hline(p, 12, 19, 9, N)
    rect(p, 6, 24, 5, 3, N)
    rect(p, 13, 24, 5, 3, T)
    rect(p, 20, 24, 5, 3, N)
    hline(p, 11, 25, 2, K)
    hline(p, 18, 25, 2, K)
    return p


def miner():
    """Pickaxe as the primary symbol + small hash/block."""
    p = canvas()
    rect(p, 8, 5, 16, 3, S)
    rect(p, 4, 6, 6, 3, S)
    rect(p, 22, 6, 6, 3, S)
    rect(p, 2, 8, 5, 3, S)
    rect(p, 25, 8, 5, 3, S)
    rect(p, 1, 11, 4, 3, G)
    rect(p, 27, 11, 4, 3, G)
    rect(p, 9, 4, 14, 2, G)
    rect(p, 14, 8, 4, 18, D)
    vline(p, 15, 8, 18, C)
    vline(p, 16, 8, 18, C)
    rect(p, 3, 22, 6, 6, N)
    rect(p, 4, 23, 4, 4, T)
    return p


def downloads():
    """3.5-inch floppy + download arrow."""
    p = canvas()
    rect(p, 6, 3, 20, 26, D)
    rect(p, 7, 4, 18, 24, G)
    rect(p, 21, 3, 5, 8, T)
    rect(p, 8, 5, 13, 8, C)
    rect(p, 9, 6, 11, 6, S)
    hline(p, 10, 8, 9, D)
    rect(p, 8, 16, 16, 10, W)
    vline(p, 15, 17, 5, N)
    vline(p, 16, 17, 5, N)
    rect(p, 12, 20, 8, 2, N)
    rect(p, 13, 22, 6, 2, N)
    rect(p, 14, 24, 4, 1, N)
    return p


def control_panel():
    """System panel + gear + network nodes."""
    p = canvas()
    rect(p, 3, 3, 26, 26, D)
    rect(p, 4, 4, 24, 24, G)
    # 8-tooth gear
    rect(p, 12, 7, 8, 12, N)
    rect(p, 10, 9, 12, 8, N)
    rect(p, 14, 6, 4, 2, N)
    rect(p, 14, 18, 4, 2, N)
    rect(p, 8, 12, 2, 4, N)
    rect(p, 22, 12, 2, 4, N)
    rect(p, 14, 10, 4, 6, G)
    rect(p, 15, 12, 2, 2, T)
    # two connected nodes
    rect(p, 6, 22, 5, 5, T)
    rect(p, 21, 22, 5, 5, T)
    rect(p, 7, 23, 3, 3, N)
    rect(p, 22, 23, 3, 3, N)
    hline(p, 11, 24, 10, K)
    return p


def recycle_bin():
    """Gray bin + broken block."""
    p = canvas()
    rect(p, 10, 3, 12, 3, G)
    rect(p, 8, 6, 16, 2, D)
    rect(p, 9, 8, 14, 16, G)
    hline(p, 9, 8, 14, S)
    vline(p, 12, 10, 12, D)
    vline(p, 16, 10, 12, D)
    vline(p, 20, 10, 12, D)
    rect(p, 8, 24, 16, 2, D)
    # broken block
    rect(p, 18, 18, 9, 8, R)
    rect(p, 20, 19, 3, 3, G)
    rect(p, 24, 22, 2, 3, C)
    dot(p, 19, 21, K)
    dot(p, 22, 24, K)
    return p


def source():
    """Source file + pixel </>."""
    p = canvas()
    paper(p, 5, 2, 22, 28)
    # <
    for i in range(5):
        rect(p, 11 - i, 8 + i, 4, 2, N)
    for i in range(4):
        rect(p, 7 + i, 14 + i, 4, 2, N)
    # /
    rect(p, 15, 16, 3, 2, T)
    rect(p, 16, 14, 3, 2, T)
    rect(p, 17, 12, 3, 2, T)
    rect(p, 18, 10, 3, 2, T)
    # >
    for i in range(5):
        rect(p, 19 + i, 8 + i, 4, 2, N)
    for i in range(4):
        rect(p, 23 - i, 14 + i, 4, 2, N)
    hline(p, 8, 24, 14, T)
    return p


def folder():
    """Teal archive, same family as documents — not Windows yellow."""
    p = canvas()
    rect(p, 3, 10, 26, 16, T)
    rect(p, 3, 8, 11, 4, T2)
    hline(p, 3, 10, 26, S)
    protocol_blocks(p, 10, 15)
    return p


def pdf():
    p = canvas()
    paper(p, 7, 2, 18, 28)
    protocol_blocks(p, 10, 12)
    hline(p, 10, 18, 12, C)
    hline(p, 10, 21, 10, C)
    return p


def wallet():
    p = canvas()
    rect(p, 4, 10, 24, 14, D)
    rect(p, 4, 8, 24, 4, T)
    rect(p, 20, 14, 6, 6, G)
    rect(p, 22, 16, 2, 2, N)
    protocol_blocks(p, 7, 15)
    return p


def node():
    p = canvas()
    rect(p, 6, 6, 20, 20, G)
    rect(p, 7, 7, 18, 18, D)
    rect(p, 9, 9, 14, 6, T)
    protocol_blocks(p, 10, 11)
    rect(p, 9, 18, 4, 3, N)
    rect(p, 14, 18, 4, 3, T)
    rect(p, 19, 18, 4, 3, N)
    return p


def help_icon():
    p = canvas()
    paper(p, 8, 3, 16, 26)
    rect(p, 13, 8, 6, 6, N)
    rect(p, 15, 16, 2, 5, N)
    hline(p, 11, 24, 10, T)
    return p


def text():
    return readme()


ICONS = {
    "my-computer.png": my_computer,
    "readme.png": readme,
    "documents.png": documents,
    "network.png": network,
    "browser.png": browser,
    "miner.png": miner,
    "downloads.png": downloads,
    "control-panel.png": control_panel,
    "recycle-bin.png": recycle_bin,
    "source.png": source,
    "folder.png": folder,
    "pdf.png": pdf,
    "wallet.png": wallet,
    "node.png": node,
    "help.png": help_icon,
    "text.png": text,
}


def main():
    for name, fn in ICONS.items():
        save(name, fn())


if __name__ == "__main__":
    main()

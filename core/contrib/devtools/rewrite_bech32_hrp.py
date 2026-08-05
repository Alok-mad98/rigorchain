#!/usr/bin/env python3
"""Rewrite legacy bech32 HRPs in test sources to match chainparams (P9B naming freeze)."""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "test" / "functional"))
from test_framework.segwit_addr import bech32_decode, bech32_encode  # noqa: E402

HRP_MAP = {"ard": "rg", "tard": "trg", "sard": "srg", "ardrt": "rgrt"}

# Full bech32/bech32m address token (lowercase hrp + 1 + data+checksum)
PAT = re.compile(
    r"\b(ardrt1|tard1|sard1|ard1)([0-9ac-hj-np-z]{6,90})\b"
)


def convert(addr: str) -> str:
    enc, hrp, data = bech32_decode(addr)
    if enc is None:
        return addr
    nh = HRP_MAP.get(hrp)
    if nh is None:
        return addr
    return bech32_encode(enc, nh, data)


def process_text(s: str) -> str:
    def repl(m):
        return convert(m.group(0))

    return PAT.sub(repl, s)


def main():
    exts = {".py", ".json", ".md"}
    dirs = [ROOT / "test" / "functional", ROOT / "test" / "sanity"]
    for d in dirs:
        if not d.is_dir():
            continue
        for p in d.rglob("*"):
            if p.suffix.lower() not in exts:
                continue
            if "rewrite_bech32_hrp.py" in str(p):
                continue
            raw = p.read_text(encoding="utf8")
            new = process_text(raw)
            if new != raw:
                p.write_text(new, encoding="utf8")
                print("updated", p.relative_to(ROOT))


if __name__ == "__main__":
    main()

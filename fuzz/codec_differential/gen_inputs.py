#!/usr/bin/env python3
"""Input generator for the codec differential (1.3).

Emits "id \\t kind \\t hex" lines. Deterministic: the same --seed always
produces the same corpus, so a gate failure is reproducible from the id
alone.

Four families, in increasing distance from the proved territory:

 seed/ the 26 vectors, unchanged
  rnd/   structured random transactions and blocks, built from the wire
         grammar rather than from a decoder, so they explore the value
         space (amounts near 2^63, full-range indices, many in/outputs,
         witness present/absent) instead of the decoder's happy path
  cs/    CompactSize probes: every count that sits on a length-class or
         limit boundary, in canonical and non-canonical encodings
  mut/   byte mutations of the seeds, biased towards length fields

mut/ is the family that finds things. Mutating a length byte of a valid
block lands the input just past a decision boundary, which is where two
implementations that agree everywhere else will disagree.
"""

import argparse
import json
import random
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CASES = ROOT / "tests" / "serialization_vectors" / "cases"

MAX_SIZE = 33554432  # core/src/serialize.h


# ---------------------------------------------------------------- encoding

def cs(n: int) -> bytes:
    """Canonical CompactSize."""
    if n < 0xFD:
        return bytes([n])
    if n <= 0xFFFF:
        return b"\xfd" + n.to_bytes(2, "little")
    if n <= 0xFFFFFFFF:
        return b"\xfe" + n.to_bytes(4, "little")
    return b"\xff" + n.to_bytes(8, "little")


def cs_in_class(n: int, cls: int) -> bytes:
    """CompactSize for n forced into a wider length class than needed.

    cls 1/3/5/9 = total encoded length. Non-canonical whenever the class
    is wider than the canonical one for n.
    """
    if cls == 1:
        return bytes([n])
    if cls == 3:
        return b"\xfd" + n.to_bytes(2, "little")
    if cls == 5:
        return b"\xfe" + n.to_bytes(4, "little")
    return b"\xff" + n.to_bytes(8, "little")


def varbytes(b: bytes) -> bytes:
    return cs(len(b)) + b


def u32(n: int) -> bytes:
    return n.to_bytes(4, "little")


def i64(n: int) -> bytes:
    return (n & ((1 << 64) - 1)).to_bytes(8, "little")


# ------------------------------------------------------- structured random

def rnd_script(rng: random.Random) -> bytes:
    n = rng.choice([0, 0, 1, 22, 25, 34, 71, 100, 252, 253, 300])
    return bytes(rng.getrandbits(8) for _ in range(n))


def rnd_tx(rng: random.Random) -> bytes:
    n_in = rng.choice([0, 1, 1, 2, 3, 5])
    n_out = rng.choice([0, 1, 1, 2, 3, 5])
    # A segwit marker with zero inputs is the dummy-vin encoding; keep it
    # in the mix, it is the branch where the C++ reader backtracks.
    segwit = rng.random() < 0.5

    vin = []
    for _ in range(n_in):
        vin.append(
            bytes(rng.getrandbits(8) for _ in range(32))
            + u32(rng.choice([0, 1, 0xFFFFFFFE, 0xFFFFFFFF]))
            + varbytes(rnd_script(rng))
            + u32(rng.choice([0, 1, 0xFFFFFFFE, 0xFFFFFFFF]))
        )

    vout = []
    for _ in range(n_out):
        amount = rng.choice(
            [0, 1, 546, 5000000000, (1 << 62), (1 << 63) - 1, (1 << 63), (1 << 64) - 1]
        )
        vout.append(i64(amount) + varbytes(rnd_script(rng)))

    body = u32(rng.choice([0, 1, 2, 0xFFFFFFFF]))
    if segwit:
        body += b"\x00\x01"
    body += cs(len(vin)) + b"".join(vin)
    body += cs(len(vout)) + b"".join(vout)
    if segwit:
        for _ in range(n_in):
            k = rng.choice([0, 0, 1, 2, 3])
            body += cs(k)
            for _ in range(k):
                body += varbytes(bytes(rng.getrandbits(8) for _ in range(rng.choice([0, 1, 33, 72]))))
    body += u32(rng.choice([0, 500000, 0xFFFFFFFF]))
    return body


def rnd_block(rng: random.Random) -> bytes:
    header = (
        u32(rng.choice([1, 2, 0x20000000]))
        + bytes(rng.getrandbits(8) for _ in range(32))
        + bytes(rng.getrandbits(8) for _ in range(32))
        + u32(rng.getrandbits(32))
        + u32(rng.getrandbits(32))
        + u32(rng.getrandbits(32))
    )
    n = rng.choice([0, 1, 1, 2, 4])
    return header + cs(n) + b"".join(rnd_tx(rng) for _ in range(n))


# ------------------------------------------------------ compactsize probes

def compactsize_probes():
    """Counts on every boundary, canonical and not.

    Spliced in as a block's transaction count: the header is fixed and
    valid, so whatever the two sides do here is attributable to the
    count alone.
    """
    header = bytes(80)
    interesting = [
        0, 1, 0xFC, 0xFD, 0xFE, 0xFF,
        0xFFFF, 0x10000,
        MAX_SIZE - 1, MAX_SIZE, MAX_SIZE + 1,
        0xFFFFFFFF, 0x100000000, (1 << 64) - 1,
    ]
    for n in interesting:
        for cls in (1, 3, 5, 9):
            if cls == 1 and n > 0xFF:
                continue
            if cls == 3 and n > 0xFFFF:
                continue
            if cls == 5 and n > 0xFFFFFFFF:
                continue
            canonical = cs_in_class(n, cls) == cs(n)
            tag = "canon" if canonical else "noncanon"
            yield (f"cs/vtx-{n}-len{cls}-{tag}", "block", header + cs_in_class(n, cls))


# ------------------------------------------------------------- mutations

def mutate(rng: random.Random, data: bytes) -> bytes:
    if not data:
        return data
    op = rng.choice(
        ["bitflip", "bytefill", "truncate", "extend", "delete", "cslen", "cslen", "cswiden"]
    )
    b = bytearray(data)
    if op == "bitflip":
        i = rng.randrange(len(b))
        b[i] ^= 1 << rng.randrange(8)
    elif op == "bytefill":
        i = rng.randrange(len(b))
        b[i] = rng.getrandbits(8)
    elif op == "truncate":
        return bytes(b[: rng.randrange(len(b))])
    elif op == "extend":
        return bytes(b) + bytes(rng.getrandbits(8) for _ in range(rng.choice([1, 1, 2, 8])))
    elif op == "delete":
        i = rng.randrange(len(b))
        del b[i]
    elif op == "cslen":
        # Nudge a byte that plausibly is a length: small values only.
        candidates = [i for i, v in enumerate(b) if v < 0x30]
        if candidates:
            i = rng.choice(candidates)
            b[i] = max(0, min(255, v_delta(rng, b[i])))
    elif op == "cswiden":
        # Turn a byte into a CompactSize continuation marker.
        i = rng.randrange(len(b))
        b[i] = rng.choice([0xFD, 0xFE, 0xFF])
    return bytes(b)


def v_delta(rng: random.Random, v: int) -> int:
    return v + rng.choice([-2, -1, 1, 2, 16])


# ------------------------------------------------------------------ main

def seeds():
    out = []
    for f in sorted(CASES.glob("*.json")):
        c = json.loads(f.read_text())
        if c.get("raw_block_hex"):
            out.append((c["case_id"], "block", bytes.fromhex(c["raw_block_hex"])))
        elif c.get("raw_tx_hex"):
            out.append((c["case_id"], "tx", bytes.fromhex(c["raw_tx_hex"])))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=20260908)
    ap.add_argument("--mutations", type=int, default=400, help="mutations per seed vector")
    ap.add_argument("--random", type=int, default=2000, help="structured random inputs")
    args = ap.parse_args()

    rng = random.Random(args.seed)
    lines = []

    base = seeds()
    if not base:
        raise SystemExit(f"no seed vectors under {CASES}")

    for cid, kind, data in base:
        lines.append((f"seed/{cid}", kind, data))

    for name, kind, data in compactsize_probes():
        lines.append((name, kind, data))

    for i in range(args.random):
        if i % 2 == 0:
            lines.append((f"rnd/tx/{i}", "tx", rnd_tx(rng)))
        else:
            lines.append((f"rnd/blk/{i}", "block", rnd_block(rng)))

    for cid, kind, data in base:
        for i in range(args.mutations):
            cur = data
            for _ in range(rng.choice([1, 1, 1, 2, 3])):
                cur = mutate(rng, cur)
            lines.append((f"mut/{cid}/{i}", kind, cur))

    out = []
    for cid, kind, data in lines:
        out.append(f"{cid}\t{kind}\t{data.hex()}")
    print("\n".join(out))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Compare the two codec oracles and classify every divergence (1.3).

Reads the shared input file and the two "id status leftover reencoded"
outputs. Agreement is not the interesting output; the divergence set is.
A divergence is only tolerated if it falls into a class that is declared
here *and* characterised by a theorem in
formal_verification/consensus_model/WireCodec.v. Anything else fails.

That is the point of the gate. The three known divergences are proved to
be confined to inputs the consensus layer discards, so they cost nothing.
A fourth divergence would not carry that guarantee, and the way to notice
one is to refuse to let the set grow silently.
"""

import argparse
import sys
from pathlib import Path

MAX_SIZE = 33554432


# ------------------------------------------------- canonical-form parser
#
# Only ever applied to a re-encoding emitted by one of the two oracles,
# never to a fuzzer input. Both oracles emit canonical bytes, so this
# does not need to handle the non-canonical cases the decoders do.

class Reader:
    def __init__(self, b):
        self.b = b
        self.i = 0

    def take(self, n):
        if n < 0 or self.i + n > len(self.b):
            raise ValueError("short read")
        r = self.b[self.i:self.i + n]
        self.i += n
        return r

    def u32(self):
        return int.from_bytes(self.take(4), "little")

    def cs(self):
        v = self.take(1)[0]
        if v < 0xFD:
            return v
        if v == 0xFD:
            return int.from_bytes(self.take(2), "little")
        if v == 0xFE:
            return int.from_bytes(self.take(4), "little")
        return int.from_bytes(self.take(8), "little")


def parse_tx(r):
    r.u32()
    n_in = r.cs()
    segwit = False
    if n_in == 0:
        flag = r.take(1)[0]
        if flag == 1:
            segwit = True
            n_in = r.cs()
        else:
            r.u32()
            return {"n_in": 0, "n_out": 0}
    for _ in range(n_in):
        r.take(32)
        r.u32()
        r.take(r.cs())
        r.u32()
    n_out = r.cs()
    for _ in range(n_out):
        r.take(8)
        r.take(r.cs())
    if segwit:
        for _ in range(n_in):
            for _ in range(r.cs()):
                r.take(r.cs())
    r.u32()
    return {"n_in": n_in, "n_out": n_out}


def decoded_txs(kind, reencoded_hex):
    """The transactions a re-encoding stands for, or None if unparsable."""
    try:
        r = Reader(bytes.fromhex(reencoded_hex))
        if kind == "block":
            r.take(80)
            return [parse_tx(r) for _ in range(r.cs())]
        return [parse_tx(r)]
    except Exception:
        return None


# --------------------------------------------------------- classification

def classify(kind, coq, cpp):
    """Name the divergence class, or None if the two agree."""
    if coq == cpp:
        return None

    coq_status, coq_left, coq_re = coq
    cpp_status, cpp_left, cpp_re = cpp

    # WireCodec.v: cpp_divergence_is_exactly_empty_tx
    if coq_status == "fail" and cpp_status == "ok":
        txs = decoded_txs(kind, cpp_re)
        if txs is not None and any(t["n_in"] == 0 for t in txs):
            return "empty-vin-dummy-flag"
        return "UNDECLARED/cpp-accepts"

    # WireCodec.v: compactsize_model_accepts_above_max. Proved masked at
    # the block level, so observing it here would mean the masking
    # argument does not cover some reachable input.
    if coq_status == "ok" and cpp_status == "fail":
        txs = decoded_txs(kind, coq_re)
        if txs is not None and len(txs) > MAX_SIZE:
            return "max-size"
        return "UNDECLARED/coq-accepts"

    if coq_status == "ok" and cpp_status == "ok":
        if coq_left != cpp_left:
            return "UNDECLARED/leftover"
        return "UNDECLARED/reencoding"

    return "UNDECLARED/status"


DECLARED = {
    "empty-vin-dummy-flag": (
        "C++ reads a zero flag byte after an empty vin and decodes an "
        "empty transaction; the model requires flag == 1. "
        "WireCodec.cpp_divergence_is_exactly_empty_tx"
    ),
    "max-size": (
        "CompactSize count above MAX_SIZE: C++ throws, the model accepts. "
        "WireCodec.max_size_divergence_masked_at_block_level"
    ),
}


# ------------------------------------------------------------------ main

def read_outputs(path):
    out = {}
    for line in Path(path).read_text().splitlines():
        p = line.split("\t")
        if len(p) == 4:
            out[p[0]] = (p[1], p[2], p[3])
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--inputs", required=True)
    ap.add_argument("--coq", required=True)
    ap.add_argument("--cpp", required=True)
    ap.add_argument("--report", help="write diverging cases here")
    args = ap.parse_args()

    kinds = {}
    hexes = {}
    for line in Path(args.inputs).read_text().splitlines():
        p = line.split("\t")
        if len(p) == 3:
            kinds[p[0]] = p[1]
            hexes[p[0]] = p[2]

    coq = read_outputs(args.coq)
    cpp = read_outputs(args.cpp)

    missing = (set(kinds) - set(coq)) | (set(kinds) - set(cpp))
    if missing:
        print(f"FAIL: {len(missing)} inputs produced no verdict, e.g. {sorted(missing)[:3]}")
        return 1

    hist = {}
    rows = []
    for cid in kinds:
        cls = classify(kinds[cid], coq[cid], cpp[cid])
        if cls is None:
            continue
        hist[cls] = hist.get(cls, 0) + 1
        rows.append((cls, cid, kinds[cid], hexes[cid], coq[cid], cpp[cid]))

    total = len(kinds)
    agree = total - len(rows)
    print(f"inputs   {total}")
    print(f"agree    {agree}")
    print(f"diverge  {len(rows)}")
    print()

    if not hist:
        print("no divergences")
    for cls in sorted(hist, key=lambda c: -hist[c]):
        mark = "declared" if cls in DECLARED else "UNDECLARED"
        print(f"  {hist[cls]:6d}  [{mark}] {cls}")
        if cls in DECLARED:
            print(f"          {DECLARED[cls]}")

    if args.report:
        with open(args.report, "w") as f:
            for cls, cid, kind, hx, c, p in sorted(rows):
                f.write(f"{cls}\t{cid}\t{kind}\t{hx}\tcoq={c}\tcpp={p}\n")
        print(f"\nreport   {args.report}")

    undeclared = {c: n for c, n in hist.items() if c not in DECLARED}
    if undeclared:
        print()
        print("FAIL: undeclared divergence classes")
        for cls, n in sorted(undeclared.items()):
            example = next(r for r in rows if r[0] == cls)
            print(f"  {cls}: {n} case(s), e.g. {example[1]}")
            print(f"    kind={example[2]} coq={example[4]} cpp={example[5]}")
            print(f"    {example[3][:160]}")
        print()
        print("Either the model and the C++ genuinely disagree in a new way,")
        print("or the harness changed. Do not widen DECLARED without a")
        print("theorem in WireCodec.v that bounds the new class.")
        return 1

    print()
    print("CODEC_DIFFERENTIAL_UNDECLARED_DIVERGENCE=NO")
    return 0


if __name__ == "__main__":
    sys.exit(main())

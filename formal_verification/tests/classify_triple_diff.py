#!/usr/bin/env python3
"""Classifier differential: extracted Coq ≡ production C++.

Python L1 lottery spec was removed in Batch A. Coverage is unchanged on the
Coq / C++ sides:

  * every length-0/1/2 script (1 + 256 + 65536)
  * length 3–4 with a gate first byte and a 16-byte branch alphabet
  * Coq [classify_nongate_first_byte]: non-gate head ⇒ INVALID for arbitrary tails
  * C++ golden JSON

Empty scriptPubKey is encoded as the stdin line "-" (not a blank line: both
CLIs skip blanks). Not a refinement proof. fuzz ≠ theorem ≠ refinement.
"""
from __future__ import annotations

import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OCAML = ROOT / "extracted" / "classify_cli"
CPP = ROOT / "extracted" / "cpp_classify_cli"

GATES = [0, 1, 2, 3, 4, 20] + list(range(81, 97)) + [106]
BRANCH = [0, 1, 4, 5, 16, 20, 32, 75, 76, 79, 81, 96, 106, 187, 189, 255]


def is_gate(b: int) -> bool:
    return (
        b == 0
        or (1 <= b <= 4)
        or b == 20
        or (81 <= b <= 96)
        or b == 106
    )


def encode_line(spk: bytes) -> str:
    """Wire encoding shared by classify_cli.ml and cpp_classify_cli.cpp."""
    return spk.hex() if spk else "-"


def classify_batch(cli: Path, scripts: list[bytes]) -> list[str]:
    hex_lines = "\n".join(encode_line(s) for s in scripts) + "\n"
    r = subprocess.run(
        [str(cli)],
        input=hex_lines,
        capture_output=True,
        text=True,
        check=True,
    )
    out = [ln.strip() for ln in r.stdout.splitlines() if ln.strip()]
    if len(out) != len(scripts):
        raise SystemExit(f"{cli.name} count {len(out)} != {len(scripts)}")
    return out


def check_pair(spk: bytes, oc: str, cpp: str, where: str) -> None:
    if oc != cpp:
        raise SystemExit(
            f"FAIL {where}: coq={oc} cpp={cpp} hex={spk.hex() or '(empty)'}"
        )


def main() -> int:
    if not OCAML.is_file() or not os.access(OCAML, os.X_OK):
        print(f"FAIL: {OCAML} missing. Rebuild from committed classify_kernel.ml")
        return 1
    if not CPP.is_file() or not os.access(CPP, os.X_OK):
        print(f"FAIL: {CPP} missing. Run bash formal_verification/scripts/build_cpp_classify_cli.sh")
        return 1

    n = 0
    batch: list[bytes] = [b""]
    batch.extend(bytes([a]) for a in range(256))
    batch.extend(bytes([a, b]) for a in range(256) for b in range(256))
    oc = classify_batch(OCAML, batch)
    cpp = classify_batch(CPP, batch)
    for spk, o, c in zip(batch, oc, cpp):
        check_pair(spk, o, c, f"len{len(spk)}")
        if spk and not is_gate(spk[0]) and o != "INVALID":
            raise SystemExit(f"FAIL nongate coq {spk.hex()} -> {o}")
        n += 1
    print(f"length 0-2: {n} scripts coq=cpp")

    gold_path = ROOT.parent / "core" / "src" / "test" / "data" / "rigor_classification_vectors.json"
    gold = json.loads(gold_path.read_text())
    gold_scripts = [bytes.fromhex(v["scriptPubKey"]) for v in gold]
    oc = classify_batch(OCAML, gold_scripts)
    cpp = classify_batch(CPP, gold_scripts)
    for v, o, c in zip(gold, oc, cpp):
        spk = bytes.fromhex(v["scriptPubKey"])
        check_pair(spk, o, c, v["name"])
        if o != v["expected"] or c != v["expected"]:
            raise SystemExit(
                f"FAIL golden C++ vector {v['name']}: coq={o} cpp={c} expected={v['expected']}"
            )
    print(f"C++ golden JSON: {len(gold)} scripts coq=cpp=expected")

    n2 = 0
    structured: list[bytes] = []
    for length in (3, 4):
        rest_len = length - 1

        def rec(prefix: list[int], d: int) -> None:
            if d == 0:
                structured.append(bytes(prefix))
                return
            for b in BRANCH:
                rec(prefix + [b], d - 1)

        for g in GATES:
            rec([g], rest_len)
    oc = classify_batch(OCAML, structured)
    cpp = classify_batch(CPP, structured)
    for spk, o, c in zip(structured, oc, cpp):
        check_pair(spk, o, c, f"struct-len{len(spk)}")
        n2 += 1
    print(f"gate×branch length 3-4: {n2} scripts coq=cpp")

    tails = [b"", b"\x00", b"\xff" * 8, b"\x14" + b"\x11" * 20 + b"\xbd", bytes(range(40))]
    nongate_scripts = [bytes([h]) + t for h in range(256) if not is_gate(h) for t in tails]
    oc = classify_batch(OCAML, nongate_scripts)
    cpp = classify_batch(CPP, nongate_scripts)
    for spk, o, c in zip(nongate_scripts, oc, cpp):
        check_pair(spk, o, c, "nongate")
        if o != "INVALID" or c != "INVALID":
            raise SystemExit(f"FAIL nongate lemma: {spk.hex()} {o} {c}")
    print(f"nongate first byte: {len(nongate_scripts)} scripts all INVALID")

    fp = hashlib.sha256()
    tag = {"ORU": b"\x00", "RES": b"\x01", "NONE": b"\x02", "INVALID": b"\x04"}
    oc_all = classify_batch(OCAML, batch + structured)
    for a in oc_all:
        fp.update(tag[a])
    print(f"fingerprint {fp.hexdigest()}")
    print("classifier differential (extracted Coq = production C++)")
    print("Not a refinement proof. Python L1 spec removed with lottery Batch A.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

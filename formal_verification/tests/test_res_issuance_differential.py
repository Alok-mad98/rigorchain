#!/usr/bin/env python3
"""Differential test: res_issuance.h against the RFC-020 Python SSOT.

Compares the C++ header to tools/res_continuous_price_schedule.py bit for bit
on display destroy cost, mint predicate, cap, min-burn and flat ORU subsidy.

    python3 formal_verification/tests/test_res_issuance_differential.py
    python3 formal_verification/tests/test_res_issuance_differential.py --cases 20000
"""
import argparse
import pathlib
import random
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
SRC = ROOT / "core" / "src"
sys.path.insert(0, str(ROOT / "tools"))

import res_continuous_price_schedule as model  # noqa: E402

HARNESS = r"""
#include <consensus/res_issuance.h>
#include <consensus/rigor_consensus_lock.h>
#include <cstdio>
#include <cstdlib>
#include <cinttypes>

//   p <h>                -> display destroy cost at height h
//   b <h>                -> min burn for one RES atom
//   u <h>                -> ORU subsidy at height h
//   c <res> <burn> <h>   -> mint predicate, 1 or 0
//   s                    -> derived cap
int main() {
    char op;
    while (scanf(" %c", &op) == 1) {
        if (op == 'p') {
            long long h; if (scanf("%lld", &h) != 1) return 2;
            printf("%lld\n", (long long)Consensus::ResDestroyCostAtHeight((int)h));
        } else if (op == 'b') {
            long long h; if (scanf("%lld", &h) != 1) return 2;
            printf("%lld\n", (long long)Consensus::ResMinMintBurnAtoms((int)h));
        } else if (op == 'u') {
            long long h; if (scanf("%lld", &h) != 1) return 2;
            if (h < 0) printf("0\n");
            else if (h == 0) printf("%lld\n", (long long)Consensus::ORU_GENESIS_BLOCK_SUBSIDY);
            else printf("%lld\n", (long long)Consensus::ORU_FLAT_BLOCK_SUBSIDY);
        } else if (op == 'c') {
            long long r, b, h;
            if (scanf("%lld %lld %lld", &r, &b, &h) != 3) return 2;
            printf("%d\n", Consensus::CheckResMintArithmetic(
                (CAmount)r, (CAmount)b, (int)h) ? 1 : 0);
        } else if (op == 's') {
            printf("%lld\n", (long long)Consensus::ResDerivedMaxSupply());
        } else {
            return 2;
        }
    }
    return 0;
}
"""


def build(tmp):
    src = tmp / "harness.cpp"
    src.write_text(HARNESS)
    exe = tmp / "harness"
    cmd = ["g++", "-std=c++20", "-O2", f"-I{SRC}", f"-I{SRC}/config",
           str(src), "-o", str(exe)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stderr, file=sys.stderr)
        raise SystemExit("harness failed to compile against res_issuance.h")
    return exe


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cases", type=int, default=20_000)
    ap.add_argument("--seed", type=int, default=20260912)
    args = ap.parse_args()

    rng = random.Random(args.seed)
    cap = model.res_max_supply_atoms()
    heights = [0, 1, 2, 1000, 52594, 210000, 210001, model.RES_DESTROY_POLE,
               model.RES_DESTROY_POLE + 1, 1 << 20]

    lines, expected, labels = [], [], []

    lines.append("s")
    expected.append(cap)
    labels.append("derived cap")

    for h in heights + [-1]:
        lines.append(f"p {h}")
        expected.append(model.destroy_cost_atoms(h))
        labels.append(f"price({h})")
        lines.append(f"b {h}")
        expected.append(0 if h < 0 else model.min_burn_for_one_atom(h))
        labels.append(f"min_atom_burn({h})")
        lines.append(f"u {h}")
        expected.append(model.block_subsidy_atoms(h))
        labels.append(f"oru_subsidy({h})")

    edges = []
    for h in heights:
        price = model.destroy_cost_atoms(h)
        need = model.min_burn_for_one_atom(h)
        edges += [
            (model.RES_COIN, price, h),
            (model.RES_COIN, price - 1, h),
            (1, need, h),
            (1, need - 1, h),
            (0, 0, h),
            (0, model.MAX_MONEY, h),
            (cap, model.MAX_MONEY, h),
            (cap + 1, model.MAX_MONEY, h),
            (1, 0, h),
        ]
    edges += [(model.RES_COIN, model.RES_DESTROY_COST_GENESIS, -1),
              (1, 0, -1)]

    for r, b, h in edges:
        lines.append(f"c {r} {b} {h}")
        expected.append(1 if model.check_mint(r, b, h) else 0)
        labels.append(f"check(res={r}, burn={b}, h={h})")

    for _ in range(args.cases):
        h = rng.choice([-1, 0, 1, rng.randrange(0, 1 << 20), 210000])
        r = rng.choice([0, 1, model.RES_COIN, cap, cap + 1, rng.randrange(0, 1000)])
        exact = 0
        if h >= 0 and r > 0:
            pole = model.RES_DESTROY_POLE + h
            exact = -(-(r * pole * pole) // (1 << model.RES_DESTROY_SHIFT))
        b = rng.choice([exact, max(0, exact - 1), exact + 1, 0, model.MAX_MONEY])
        lines.append(f"c {r} {b} {h}")
        expected.append(1 if model.check_mint(r, b, h) else 0)
        labels.append(f"check(res={r}, burn={b}, h={h})")

    with tempfile.TemporaryDirectory() as td:
        exe = build(pathlib.Path(td))
        proc = subprocess.run([str(exe)], input="\n".join(lines) + "\n",
                              capture_output=True, text=True)
        if proc.returncode != 0:
            print(proc.stderr, file=sys.stderr)
            raise SystemExit("harness exited non-zero")
        got = proc.stdout.split()

    if len(got) != len(expected):
        raise SystemExit(f"harness returned {len(got)} results, expected {len(expected)}")

    bad = 0
    for label, want, have in zip(labels, expected, got):
        if int(have) != int(want):
            if bad < 20:
                print(f"MISMATCH {label}: model={want} cpp={have}", file=sys.stderr)
            bad += 1

    if bad:
        print(f"\nFAIL: {bad} of {len(expected)} cases disagree", file=sys.stderr)
        return 1
    print(f"OK: {len(expected)} cases agree bit for bit "
          f"({len(heights) + 1} height probes, {len(edges)} boundary cases, "
          f"{args.cases} random triples)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

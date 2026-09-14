#!/usr/bin/env python3
"""Inventory of the lottery surface, so its removal is measured rather than remembered.

Phase 4 of the RES issuance refactor deletes the draw. The failure mode is not
deleting too much -- the compiler catches that -- but deleting too little and
leaving a symbol reachable that no longer has a mechanism behind it. So the
draw is enumerated by symbol here, counted across the tree, and the count is
required to only ever go down.

    python3 scripts/refactor_draw_inventory.py            report
    python3 scripts/refactor_draw_inventory.py --snapshot  record the current counts
    python3 scripts/refactor_draw_inventory.py --check     fail if any count grew

WHAT IS NOT ON THIS LIST MATTERS AS MUCH AS WHAT IS. ORU is still burned under
RFC-019, so OP_CONVERGE, the convergence output, its structural rules and
GetTransactionConvergenceAmount all survive. Only the machinery that turned a
burn into a *chance* is going away. Putting the burn symbols on this list would
make the inventory demand its own deletion, and the guard would be enforcing
the wrong thing.
"""
import argparse
import json
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "core" / "src"
BASELINE = ROOT / "formal_verification" / "refactor_baseline" / "draw_inventory.json"

# Symbols that exist only because issuance was a lottery. Grouped by what part
# of the draw they implement, because the groups die at different moments and a
# flat list would hide which step a leftover belongs to.
DRAW_SYMBOLS = {
    "sigma": [
        "SelectionDigest", "SelectionDigestHash", "ComputeConvergenceHash",
        "ComputeSigmaHash",
    ],
    "candidate_set": [
        "ConvergenceDrawCandidate", "ConvergenceDrawSet", "BuildConvergenceDrawSet",
        "BuildConvergenceDrawSetFromUndo", "BuildBlockConvergenceCandidatesFromUndo",
        "IsConvergenceDrawMember", "BuildConvergenceSelectionContextView",
    ],
    "window": [
        "ConvergenceWindowTail", "TailFromParentDrawSet", "CONVERGENCE_DRAW_WINDOW",
        "CONVERGENCE_DRAW_WINDOW_LOG2",
    ],
    "selection": [
        "ResolveConvergenceDraw", "ResolveDraw", "ConvergenceDrawOutcome",
        "ConvergenceWinnerTieBreakCompare", "GetBlockWinner",
        "GetBlockWinnerFromBlockUndo", "RES_PROBABILITY_TARGET",
        "CONVERGENCE_DRAW_MIN_TXS_FOR_RES",
    ],
    "roots": [
        "ConvergenceDrawEligibleRoot", "ComputeConvergenceDrawCandidateRoot",
        "ComputeConvergenceDrawCandidateRootFromBlockUndo",
        "ComputeConvergenceDrawMerkleRoot", "convergence_root",
    ],
    "coinbase_mint": [
        "CoinbaseHasResMint", "FindBlockPaidConvergenceTxids", "IsResMintScript",
        "ParseResMintScript", "GetResMintReward", "ResRewardForEpoch",
        "RES_REWARD_GENESIS", "ResScheduleSum",
    ],
    "memo_fields": [
        "m_draw_set", "m_paid_winner_txid",
    ],
}

# Files whose whole reason for existing is the draw. Listed separately from the
# symbol counts because "does this file still exist" is a cleaner question than
# "how many times is it mentioned".
DRAW_FILES = [
    "core/src/consensus/convergence_logic.h",
    "core/src/consensus/convergence_logic.cpp",
    "core/src/consensus/convergence_root.h",
    "core/src/consensus/convergence_root.cpp",
    "core/src/consensus/convergence_root_eligibility.h",
    "core/src/consensus/convergence_root_eligibility.cpp",
    "core/src/consensus/convergence_commitment.h",
    "core/src/consensus/convergence_commitment.cpp",
    "core/src/consensus/draw_digest.h",
    "core/src/consensus/rigor_res.h",
    "core/src/consensus/rigor_res.cpp",
    "core/src/consensus/bridge/draw_bridge.cpp",
    # The draw carried two independent 512-bit implementations of the same
    # inequality sigma < T*f, one per draw module. Every non-test caller of
    # either is draw code, so both go: rigor_math.h serves convergence_logic.cpp
    # and convergenceres.cpp, arith_uint512 serves selection.cpp. RFC-019 prices
    # a mint with a 128-bit product instead and needs neither.
    "core/src/consensus/rigor_math.h",
    "core/src/consensus/selection.h",
    "core/src/consensus/selection.cpp",
    "core/src/arith_uint512.h",
    "core/src/arith_uint512.cpp",
]

# Phase 5 closed: the verify-only harness and its standalone TUs now speak
# RFC-019 (RATE, no draw). They remain absent from Makefile.am /
# Makefile.test.include — that is a packaging choice, not a compile debt.
# Do not re-list them as deferred.

# Symbols that look like they belong on the list and must not be added to it.
# Recorded so the reasoning survives the next reader.
DELIBERATELY_KEPT = {
    "GetTransactionConvergenceAmount": "the burn amount; RFC-019 prices against it",
    "GetConvergenceAmount": "same, the consensus-facing spelling",
    "CountConvergenceOutputs": "structural rule: at most one burn output per tx",
    "IsConvergenceScript": "the OP_CONVERGE classifier; the opcode survives",
    "IsConvergenceTx": "still how a burn is recognised",
    "MIN_CONVERGENCE_AMOUNT": "positive-burn floor (one atom); not a mint price",
    "RIGOR_MARKER_CONVERGE": "the opcode itself",
}


def count(symbol):
    """Occurrences of `symbol` as a whole word under core/src, per file."""
    try:
        out = subprocess.run(
            ["rg", "-c", "-w", "--no-heading", "--", symbol, str(SRC)],
            capture_output=True, text=True, check=False).stdout
    except FileNotFoundError:
        sys.exit("rg not found; the inventory relies on it for whole-word counts")
    per_file = {}
    for line in out.splitlines():
        path, _, n = line.rpartition(":")
        if not path or path.endswith(".o"):
            continue
        per_file[str(pathlib.Path(path).relative_to(ROOT))] = int(n)
    return per_file


def build():
    groups = {}
    for group, symbols in DRAW_SYMBOLS.items():
        entry = {}
        for s in symbols:
            per_file = count(s)
            entry[s] = {"total": sum(per_file.values()), "files": len(per_file)}
        groups[group] = entry
    return {
        "groups": groups,
        "group_totals": {g: sum(v["total"] for v in e.values())
                         for g, e in groups.items()},
        "total": sum(sum(v["total"] for v in e.values()) for e in groups.values()),
        "files_present": {f: (ROOT / f).is_file() for f in DRAW_FILES},
        "deliberately_kept": DELIBERATELY_KEPT,
    }


def report(live):
    print("Lottery surface under core/src")
    print("=" * 72)
    for group, entry in live["groups"].items():
        print(f"\n{group}  ({live['group_totals'][group]} references)")
        for s, v in sorted(entry.items(), key=lambda kv: -kv[1]["total"]):
            if v["total"]:
                print(f"  {v['total']:>5} in {v['files']:>3} files   {s}")
            else:
                print(f"  {'gone':>5}                      {s}")
    present = sum(1 for v in live["files_present"].values() if v)
    print(f"\ntotal references : {live['total']}")
    print(f"draw-only files  : {present} of {len(DRAW_FILES)} still present")
    print(f"\ndeliberately kept ({len(DELIBERATELY_KEPT)} symbols, the burn survives):")
    for s, why in sorted(DELIBERATELY_KEPT.items()):
        print(f"  {s:<36} {why}")

    print("\nPhase 5 harness + standalone TUs: RFC-019 aligned (not deferred).")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--snapshot", action="store_true")
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()

    live = build()

    if args.snapshot:
        BASELINE.parent.mkdir(exist_ok=True)
        BASELINE.write_text(json.dumps(live, indent=2, sort_keys=True) + "\n")
        print(f"wrote {BASELINE}: {live['total']} references, "
              f"{sum(1 for v in live['files_present'].values() if v)} files")
        return 0

    if args.check:
        if not BASELINE.exists():
            print(f"FAIL: {BASELINE} missing; run --snapshot first", file=sys.stderr)
            return 1
        base = json.loads(BASELINE.read_text())
        rc = 0
        for group, total in live["group_totals"].items():
            was = base["group_totals"].get(group)
            if was is None:
                print(f"FAIL: new draw group {group} appeared", file=sys.stderr)
                rc = 1
            elif total > was:
                print(f"FAIL: {group} grew {was} -> {total}; the lottery is "
                      f"expanding, not contracting", file=sys.stderr)
                rc = 1
            elif total < was:
                print(f"  {group}: {was} -> {total}")
        for f, present in live["files_present"].items():
            if present and not base["files_present"].get(f, False):
                print(f"FAIL: {f} came back", file=sys.stderr)
                rc = 1
        if rc == 0:
            print(f"OK: lottery surface at {live['total']} references "
                  f"(baseline {base['total']}), never grew")
        return rc

    report(live)
    return 0


if __name__ == "__main__":
    sys.exit(main())

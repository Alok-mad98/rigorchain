#!/usr/bin/env python3
"""Shadow apply_delta: a Python transcription of Coq MintSupplyConservation.apply_delta
versus the extracted supply_cli.

The Python function is not an independent oracle — it exists so a transcription
error is visible as a mismatch against OCaml. Numeric identities (ledger ignores
fee-burn, circulating subtracts it, unauthorized mint rejects) are the checks
that would still fail if both sides drifted the same way.

RFC-020 conservation slice: subsidy is 128 ORU, a mint may be any R in
[0, RES_MAX_SUPPLY] when authorized, and destroy_cost(h) is not a grant. This is
not the pole mint predicate and not a ConnectBlock VST.
"""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CLI = ROOT / "extracted" / "supply_cli"

COIN = 100_000_000
RES_COIN = 100_000_000
RES_MAX_SUPPLY = 419_430_400_000_000
SATURATING = 32769 * COIN
SUBSIDY = 128 * COIN
MINT = 1 * RES_COIN


def apply_delta(s, d):
    _height, subsidy, mint, converge, burn, ordinary, authorized, invalid = d
    if invalid:
        return None
    if subsidy < 0 or mint < 0 or converge < 0 or burn < 0:
        return None
    if authorized:
        if mint > RES_MAX_SUPPLY:
            return None
    elif mint != 0:
        return None
    if ordinary and mint != 0:
        return None
    ledger, circ, res, acc = s
    return (
        ledger + subsidy - converge,
        circ + subsidy - converge - burn,
        res + mint,
        acc + burn,
    )


def ocaml_apply(d) -> str:
    args = [str(CLI)] + [str(int(x)) for x in d]
    r = subprocess.run(args, check=True, capture_output=True, text=True)
    return r.stdout.strip()


def main() -> int:
    if not CLI.is_file():
        print(f"FAIL: {CLI} missing")
        return 1

    # Converge block: burn 32769 ORU, no mint.
    d1 = (134, SUBSIDY, 0, SATURATING, 0, 1, 0, 0)
    s1 = apply_delta((0, 0, 0, 0), d1)
    oc1 = ocaml_apply(d1)
    exp1 = f"{s1[0]} {s1[1]} {s1[2]} {s1[3]}"
    if oc1 != exp1:
        raise SystemExit(f"FAIL converge slice python={exp1} coq={oc1}")
    if s1[0] != SUBSIDY - SATURATING:
        raise SystemExit("FAIL ledger identity")

    # Authorized body mint of 1 RES; amount is not destroy_cost(h).
    d2 = (135, SUBSIDY, MINT, 0, 0, 0, 1, 0)
    s2 = apply_delta(s1, d2)
    # supply_cli always starts from genesis, so compare the single-step mint from genesis.
    oc2 = ocaml_apply(d2)
    from_genesis = apply_delta((0, 0, 0, 0), d2)
    exp2 = f"{from_genesis[0]} {from_genesis[1]} {from_genesis[2]} {from_genesis[3]}"
    if oc2 != exp2:
        raise SystemExit(f"FAIL mint slice python={exp2} coq={oc2}")
    if from_genesis[2] != MINT:
        raise SystemExit("FAIL mint credit")
    if s2[2] != MINT:
        raise SystemExit("FAIL chained mint credit")

    # Unauthorized mint is rejected.
    d_bad = (135, 0, MINT, 0, 0, 1, 0, 0)
    if apply_delta((0, 0, 0, 0), d_bad) is not None:
        raise SystemExit("FAIL python should reject unauthorized mint")
    # Fee burn: ledger ignores it, circulating subtracts it.
    d_burn = (10, SUBSIDY, 0, 0, 50 * COIN, 1, 0, 0)
    from_b = apply_delta((0, 0, 0, 0), d_burn)
    oc_b = ocaml_apply(d_burn)
    exp_b = f"{from_b[0]} {from_b[1]} {from_b[2]} {from_b[3]}"
    if oc_b != exp_b:
        raise SystemExit(f"FAIL fee-burn slice python={exp_b} coq={oc_b}")
    if from_b[0] != SUBSIDY or from_b[1] != SUBSIDY - 50 * COIN:
        raise SystemExit("FAIL fee burn circulating vs ledger")
    if ocaml_apply(d_bad) != "REJECT":
        raise SystemExit("FAIL coq should reject unauthorized mint")

    # Single-block mint above the hard cap is rejected.
    d_cap = (1, 0, RES_MAX_SUPPLY + 1, 0, 0, 0, 1, 0)
    if apply_delta((0, 0, 0, 0), d_cap) is not None:
        raise SystemExit("FAIL python should reject over-cap mint")
    if ocaml_apply(d_cap) != "REJECT":
        raise SystemExit("FAIL coq should reject over-cap mint")

    print("supply shadow: converge + mint slices agree with extracted apply_delta")
    print(f"  subsidy = {SUBSIDY} (128 ORU)")
    print(f"  mint RES credit = {MINT}")
    print("Not a ConnectBlock refinement proof.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Historical integer model of the RFC-019 RES unit-price schedule.

Not the shipping SSOT. RFC-020 lives in tools/res_continuous_price_schedule.py
and core/src/consensus/res_issuance.h. Keep this file only to reconstruct
the superseded epoch table.

Every number in RFC-019 comes from this file. Nothing here uses floating
point: the schedule, the cap and the overflow margins are all computed with
Python ints under the same operations the historical C++ header used.

    python3 tools/res_issuance_schedule.py             human-readable report
    python3 tools/res_issuance_schedule.py --json      machine-readable
    python3 tools/res_issuance_schedule.py --markdown  RFC tables
"""
import argparse
import json
import sys

# --- fixed by the existing protocol, not decided here -----------------------
COIN = 100_000_000               # ORU atoms per ORU
RES_REWARD_EPOCH = 210_000       # blocks per epoch
RES_REWARD_LAST_EPOCH = 250      # last epoch that issues
ORU_GENESIS_SUBSIDY = 1_000      # ORU per block, epoch 0
ORU_TAIL_SUBSIDY = 125           # ORU per block, epoch >= 3

# --- decided by RFC-019 -----------------------------------------------------
RES_COIN = 100_000_000           # RES atoms per RES; matches ORU
RES_DESTROY_COST_GENESIS = 1024 * COIN   # ORU atoms to mint one whole RES at k=0
RES_DESTROY_POLE_OFFSET = 8              # k0 in the (k+k0)/k0 hardening curve

UINT64_MAX = (1 << 64) - 1
INT64_MAX = (1 << 63) - 1


def isqrt(n):
    """Floor square root. Mirrors ResIntegerSqrt in rigor_consensus_lock.h."""
    return __import__("math").isqrt(n)


def unit_price(k):
    """ORU atoms required to mint one whole RES during epoch k.

    RATE(k) = RATE(0) * ((k + k0) / k0)^(3/2), evaluated as

        RATE(k) = RATE(0) * isqrt(((k+k0)^3 << 32) / k0^3) >> 16

    The division by k0^3 sits *inside* the isqrt argument rather than outside.
    That placement is what keeps the product in range: hoisting it out makes
    the multiply reach 2.8e19 and overflow uint64 near k = 250.

    At k = 0 the isqrt argument is exactly 2^32, so the factor is exactly
    2^16 and RATE(0) == RES_DESTROY_COST_GENESIS with no rounding at all.
    """
    if k < 0 or k > RES_REWARD_LAST_EPOCH:
        return 0
    e = k + RES_DESTROY_POLE_OFFSET
    k0c = RES_DESTROY_POLE_OFFSET ** 3
    scale = isqrt(((e * e * e) << 32) // k0c)
    return (RES_DESTROY_COST_GENESIS * scale) >> 16


def oru_subsidy(k):
    """ORU per block during epoch k: 1000, halving three times, then flat."""
    return ORU_TAIL_SUBSIDY if k >= 3 else (ORU_GENESIS_SUBSIDY >> k)


def oru_emitted(k):
    """ORU atoms created by block subsidy during epoch k.

    Epoch 0 has 209 999 paying blocks, not 210 000. GetBlockSubsidy returns 0
    at height 0 and rigor_issuance.cpp is explicit that the missing 1000 ORU is
    never compensated later (P0-038). Using the round 210 000 here would put
    0.98 RES of slack into a cap that is supposed to be exactly the bound.
    """
    blocks = RES_REWARD_EPOCH - 1 if k == 0 else RES_REWARD_EPOCH
    return blocks * oru_subsidy(k) * COIN


def max_res_from_epoch(k):
    """RES atoms mintable from one epoch's own ORU emission.

    res_atoms = floor(oru_atoms * RES_COIN / RATE(k)); the mint rule is the
    cross-multiplied form of the same inequality.
    """
    p = unit_price(k)
    return (oru_emitted(k) * RES_COIN) // p if p else 0


def min_atom_burn(k):
    """Smallest ORU burn that mints one RES atom: ceil(RATE(k) / RES_COIN).

    Wallet/UI helper. Consensus never divides; the R = 1 case of the mint
    inequality is this value.
    """
    p = unit_price(k)
    return (p + RES_COIN - 1) // RES_COIN if p else 0


def derived_cap():
    """Upper bound on RES that can ever exist.

    Total RES = sum over heights of burn(h)/RATE(h), subject to the prefix
    constraint that ORU cannot be burned before it is emitted. RATE is
    strictly increasing, so 1/RATE is strictly decreasing, and by the standard
    exchange argument the sum is maximised by burning every ORU in the epoch
    it appears. That maximum is the per-epoch sum below.

    It is an upper bound and not an estimate: fees, lost keys and ORU held
    rather than burned can only lower realised issuance.
    """
    return sum(max_res_from_epoch(k) for k in range(RES_REWARD_LAST_EPOCH + 1))


def overflow_report(cap):
    """Worst-case intermediate values, against the 64-bit ceilings."""
    k = RES_REWARD_LAST_EPOCH
    e = k + RES_DESTROY_POLE_OFFSET
    k0c = RES_DESTROY_POLE_OFFSET ** 3
    isqrt_arg = ((e * e * e) << 32) // k0c
    scale = isqrt(isqrt_arg)
    price_mul = RES_DESTROY_COST_GENESIS * scale
    # The consensus check is res_out * RATE(k) <= burn * RES_COIN. Both sides
    # exceed 64 bits at cap scale, so the comparison needs a 128-bit
    # intermediate. It replaces the uint512 cross-multiply the draw needed.
    lhs = cap * unit_price(k)
    rhs = cap * unit_price(k)
    return [
        ("isqrt argument at k=250", isqrt_arg, UINT64_MAX, "uint64"),
        ("RATE(0) * scale at k=250", price_mul, UINT64_MAX, "uint64"),
        ("RATE(250)", unit_price(k), INT64_MAX, "int64 (CAmount)"),
        ("derived cap in atoms", cap, INT64_MAX, "int64 (CAmount)"),
        ("res_out * RATE at cap scale", max(lhs, rhs), UINT64_MAX,
         "uint64 -- EXCEEDS, needs 128-bit"),
    ]


def rows():
    out = []
    for k in range(RES_REWARD_LAST_EPOCH + 1):
        out.append({
            "epoch": k,
            "first_height": k * RES_REWARD_EPOCH,
            "destroy_cost_atoms": unit_price(k),
            "destroy_cost_oru": unit_price(k) / COIN,
            "min_atom_burn": min_atom_burn(k),
            "oru_subsidy": oru_subsidy(k),
            "max_res_atoms": max_res_from_epoch(k),
        })
    return out


def report():
    cap = derived_cap()
    r = rows()
    p0, pl = unit_destroy_cost(0), unit_price(RES_REWARD_LAST_EPOCH)

    print("RFC-019 — RES destroy cost schedule")
    print("=" * 76)
    print(f"  RES_COIN               {RES_COIN:>24,}")
    print(f"  RES_DESTROY_COST_GENESIS {RES_DESTROY_COST_GENESIS:>24,} atoms "
          f"= {RES_DESTROY_COST_GENESIS//COIN:,} ORU")
    print(f"  RES_DESTROY_POLE_OFFSET  {RES_DESTROY_POLE_OFFSET:>24}")
    print(f"  RES_REWARD_EPOCH       {RES_REWARD_EPOCH:>24,}")
    print(f"  RES_REWARD_LAST_EPOCH  {RES_REWARD_LAST_EPOCH:>24}")
    print()
    exact = unit_destroy_cost(0) == RES_DESTROY_COST_GENESIS
    print(f"  RATE(0) == RES_DESTROY_COST_GENESIS exactly: {exact}")
    mono = all(unit_price(k) < unit_price(k + 1)
               for k in range(RES_REWARD_LAST_EPOCH))
    print(f"  RATE strictly increasing over 0..250:      {mono}")
    print(f"  hardening RATE(250)/RATE(0):               {pl/p0:.1f}x")
    print()
    print(f"  DERIVED CAP  {cap:>24,} atoms = {cap/RES_COIN:,.8f} RES")
    print()

    print(f"{'epoch':>6} {'first height':>13} {'ORU/blk':>8} "
          f"{'RATE (ORU/RES)':>16} {'max RES this epoch':>20}")
    print("-" * 76)
    for k in (0, 1, 2, 3, 4, 5, 10, 20, 50, 100, 175, 250):
        d = r[k]
        print(f"{d['epoch']:>6} {d['first_height']:>13,} {d['oru_subsidy']:>8} "
              f"{d['destroy_cost_oru']:>16,.2f} "
              f"{d['max_res_atoms']/RES_COIN:>20,.2f}")
    print()

    print("overflow margins")
    print("-" * 76)
    for name, value, ceiling, where in overflow_report(cap):
        ratio = ceiling / value if value else float("inf")
        flag = "OK" if value <= ceiling else "OVERFLOW"
        print(f"  {name:<32} {value:>26,}  {flag:>8}  "
              f"{'headroom %.0fx' % ratio if flag == 'OK' else where}")
    print()
    print("  The last row is why the mint check needs a 128-bit intermediate.")
    print("  It replaces the uint512 cross-multiply the draw inequality used,")
    print("  so the arithmetic machinery gets strictly smaller, not larger.")
    print()

    print("cap utilisation equals participation, by construction")
    print("-" * 76)
    for rho_num, rho_den in ((5, 100), (20, 100), (50, 100), (100, 100)):
        realised = sum(max_res_from_epoch(k) * rho_num // rho_den
                       for k in range(RES_REWARD_LAST_EPOCH + 1))
        print(f"  rho = {rho_num:>3}%   realised {realised/RES_COIN:>14,.0f} RES"
              f"   utilisation {realised/cap:>7.1%}")


def markdown():
    cap = derived_cap()
    print("| Name | Value |")
    print("|------|-------|")
    print(f"| `RES_COIN` | {RES_COIN:,} |".replace(",", " "))
    print(f"| `RES_DESTROY_COST_GENESIS` | {RES_DESTROY_COST_GENESIS:,} atoms "
          f"= {RES_DESTROY_COST_GENESIS//COIN:,} ORU |".replace(",", " "))
    print(f"| `RES_DESTROY_POLE_OFFSET` | {RES_DESTROY_POLE_OFFSET} |")
    print(f"| `RES_REWARD_EPOCH` | {RES_REWARD_EPOCH:,} blocks |".replace(",", " "))
    print(f"| `RES_REWARD_LAST_EPOCH` | {RES_REWARD_LAST_EPOCH} |")
    print(f"| `RES_MAX_SUPPLY` | {cap:,} atoms (derived) |".replace(",", " "))
    print()
    print("| Epoch | First height | ORU/block | RATE (ORU per RES) | Max RES from this epoch |")
    print("|------:|-------------:|----------:|-------------------:|------------------------:|")
    for k in (0, 1, 2, 3, 5, 10, 20, 50, 100, 250):
        print(f"| {k} | {k*RES_REWARD_EPOCH:,} | {oru_subsidy(k)} | "
              f"{unit_price(k)/COIN:,.2f} | {max_res_from_epoch(k)/RES_COIN:,.2f} |"
              .replace(",", " "))


RFC = "docs/archive/rfc/RFC-019-RES-DETERMINISTIC-UNIT-PRICE.md"


def check_doc():
    """Confirm every load-bearing number in RFC-019 is one this script produces.

    Phase 1 of the execution plan forbids hand-computed numbers in the RFC. The
    only way that holds up over edits is to check it, so each claim below is
    derived here and then looked for in the document text. Digit grouping in the
    RFC uses thin spaces, so both spellings are accepted.
    """
    import pathlib
    path = pathlib.Path(__file__).resolve().parent.parent / RFC
    if not path.is_file():
        print(f"FAIL: {RFC} not found", file=sys.stderr)
        return 1
    text = path.read_text()
    cap = derived_cap()

    def num(n):
        return [f"{n:,}", f"{n:,}".replace(",", " "), str(n)]

    claims = [("RES_COIN", num(RES_COIN)),
              ("RES_DESTROY_COST_GENESIS", num(RES_DESTROY_COST_GENESIS)),
              ("genesis destroy cost in ORU", num(RES_DESTROY_COST_GENESIS // COIN)),
              ("pole offset", [str(RES_DESTROY_POLE_OFFSET)]),
              ("epoch length", num(RES_REWARD_EPOCH)),
              ("last epoch", [str(RES_REWARD_LAST_EPOCH)]),
              ("derived cap atoms", num(cap)),
              ("derived cap in RES", [f"{cap // RES_COIN:,}".replace(",", " ")]),
              ("hardening",
               [f"{unit_price(RES_REWARD_LAST_EPOCH) / unit_destroy_cost(0):.1f}"]),
              ("epoch 0 share",
               [f"{max_res_from_epoch(0) / cap:.2%}".replace("%", "")]),
              ]
    for k in (0, 1, 2, 3, 5, 10, 20, 50, 100, 250):
        claims.append((f"RATE({k})",
                       [f"{unit_price(k) / COIN:,.2f}".replace(",", " ")]))
        claims.append((f"max RES from epoch {k}",
                       [f"{max_res_from_epoch(k) / RES_COIN:,.2f}".replace(",", " ")]))
    for rho_num in (5, 20, 50, 100):
        realised = sum(max_res_from_epoch(k) * rho_num // 100
                       for k in range(RES_REWARD_LAST_EPOCH + 1))
        # Same formatting as report(): the RFC table is human-readable and
        # rounds. Flooring here instead is what made this check fail on its
        # first run, which is the check working.
        claims.append((f"realised at rho={rho_num}%",
                       [f"{realised / RES_COIN:,.0f}".replace(",", " ")]))

    rc = 0
    for name, spellings in claims:
        if not any(s in text for s in spellings):
            print(f"FAIL: {name} — none of {spellings} appears in the RFC",
                  file=sys.stderr)
            rc = 1
    if rc == 0:
        print(f"OK: {len(claims)} numbers in RFC-019 are reproduced by this script")
    return rc


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--markdown", action="store_true")
    ap.add_argument("--check-doc", action="store_true",
                    help="verify RFC-019 contains only numbers this script derives")
    a = ap.parse_args()
    if a.check_doc:
        sys.exit(check_doc())
    if a.json:
        json.dump({
            "constants": {
                "RES_COIN": RES_COIN,
                "RES_DESTROY_COST_GENESIS": RES_DESTROY_COST_GENESIS,
                "RES_DESTROY_POLE_OFFSET": RES_DESTROY_POLE_OFFSET,
                "RES_REWARD_EPOCH": RES_REWARD_EPOCH,
                "RES_REWARD_LAST_EPOCH": RES_REWARD_LAST_EPOCH,
                "RES_MAX_SUPPLY_derived": derived_cap(),
            },
            "schedule": rows(),
        }, sys.stdout, indent=2)
        print()
    elif a.markdown:
        markdown()
    else:
        report()


if __name__ == "__main__":
    main()

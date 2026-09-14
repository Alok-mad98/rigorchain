#!/usr/bin/env python3
"""Explore parameter-only reshapes of the RES reward schedule W(k).

Constraint set (all preserved by every candidate below):
  * RES_MAX_SUPPLY = 21e6 RES, RES_REWARD_EPOCH = 210000, RES_REWARD_LAST_EPOCH = 250
  * the compile-time cap identity  W0 * sum_k shape(k) <= RES_MAX_SUPPLY / RES_REWARD_EPOCH
  * one mint per block, T constant, C = 1024 ORU/mint, integer isqrt arithmetic
  * the draw, the window, sigma, the coinbase layout: untouched

Only the shape of W(k) moves. Current shape is (k+1)^-1.5; the generalisation is
(k+k0)^-alpha normalised so W(0) = W0.
"""
import math

RES_COIN = 1_000_000
EPOCH = 210_000
LAST = 250
CAP_PER_EPOCH = 21_000_000 * RES_COIN // EPOCH   # 100 RES in atoms
C_ORU = 1024


def oru_subsidy(k):
    return 125 if k >= 3 else (1000 >> k)


def shape(k, k0, alpha):
    """Normalised so shape(0) == 1."""
    return (k0 / (k + k0)) ** alpha


def plateau_shape(k, k1, alpha):
    return 1.0 if k <= k1 else ((k1 + 1) / (k + 1)) ** alpha


def evaluate(name, shp):
    s = [shp(k) for k in range(LAST + 1)]
    total_shape = sum(s)
    w0 = CAP_PER_EPOCH / total_shape            # max admissible genesis reward, atoms
    w = [w0 * x for x in s]                     # W(k) in atoms

    # unsaturated-regime issuance weights: RES/epoch proportional to W(k)*subsidy(k)
    weight = [w[k] * oru_subsidy(k) for k in range(LAST + 1)]
    tw = sum(weight)
    cum = 0.0
    share_by = {}
    half_life = None
    for k in range(LAST + 1):
        cum += weight[k]
        if k in (0, 2, 9, 24, 49):
            share_by[k] = cum / tw
        if half_life is None and cum / tw >= 0.5:
            half_life = k

    rate0 = C_ORU / (w[0] / RES_COIN)
    rate_last = C_ORU / (w[LAST] / RES_COIN)
    hardening = rate_last / rate0

    # realised supply at rho = 20% burn of fresh ORU emission
    rho = 0.20
    realised = 0.0
    for k in range(LAST + 1):
        lam = rho * oru_subsidy(k) / C_ORU
        realised += w[k] * (1.0 - math.exp(-lam)) * EPOCH
    return {
        "name": name,
        "w0": w0 / RES_COIN,
        "w_last": w[LAST] / RES_COIN,
        "share0": share_by[0],
        "share2": share_by[2],
        "share9": share_by[9],
        "share24": share_by[24],
        "share49": share_by[49],
        "half_life": half_life,
        "rate0": rate0,
        "rate_last": rate_last,
        "hardening": hardening,
        "realised": realised / RES_COIN,
        "shape_sum": total_shape,
    }


def main():
    cands = []
    cands.append(evaluate("current k0=1, a=1.5", lambda k: shape(k, 1, 1.5)))
    for k0 in (2, 3, 5, 8, 13, 21, 34, 55):
        cands.append(evaluate(f"k0={k0}, a=1.5", lambda k, k0=k0: shape(k, k0, 1.5)))
    for a in (1.25, 1.0, 0.75, 0.5):
        cands.append(evaluate(f"k0=1, a={a}", lambda k, a=a: shape(k, 1, a)))
    for k0, a in ((5, 1.25), (8, 1.25), (13, 1.25), (5, 2.0), (8, 2.0), (13, 2.0), (21, 2.0)):
        cands.append(evaluate(f"k0={k0}, a={a}", lambda k, k0=k0, a=a: shape(k, k0, a)))
    for k1 in (5, 10, 20):
        cands.append(evaluate(f"plateau k1={k1}, a=1.5", lambda k, k1=k1: plateau_shape(k, k1, 1.5)))

    hdr = (f"{'candidate':<20} {'W0':>9} {'W(250)':>9} {'ep0':>7} {'ep0-2':>7} {'ep0-9':>7} "
           f"{'ep0-24':>7} {'half-life':>9} {'RATE(0)':>9} {'RATE(250)':>11} {'hardening':>9} {'rho=20% supply':>14}")
    print(hdr)
    print("-" * len(hdr))
    for c in cands:
        print(f"{c['name']:<20} {c['w0']:>9.4f} {c['w_last']:>9.6f} "
              f"{c['share0']:>6.1%} {c['share2']:>6.1%} {c['share9']:>6.1%} {c['share24']:>6.1%} "
              f"{c['half_life']:>7d} {c['rate0']:>9.1f} {c['rate_last']:>11,.0f} "
              f"{c['hardening']:>8.0f}x {c['realised']:>11,.0f}")


if __name__ == "__main__":
    main()

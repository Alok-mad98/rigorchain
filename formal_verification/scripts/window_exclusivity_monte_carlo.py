#!/usr/bin/env python3
"""Offline Monte Carlo of window exclusivity under partial hashrate control.

Documentation / illustration tool. Not a differential-gate step and not a
theorem. Production W is not modified.

Models two independent bounds used to lock W=32:

  1. Attacker authors every block in a window (probability α^W).
  2. No third-party convergence ticket lands in the window (probability e^{-λW}),
     in which case CONVERGENCE_DRAW_MIN_TXS_FOR_RES==1 lets a 1 ORU self-ticket win.

Also reports expected attacker *weight share* of F when honest blocks carry
Poisson(λ) unit tickets and the attacker always inserts one unit ticket.

Usage:
  python3 formal_verification/scripts/window_exclusivity_monte_carlo.py
  python3 formal_verification/scripts/window_exclusivity_monte_carlo.py --trials 20000
"""
from __future__ import annotations

import argparse
import math
import random


def simulate(alpha: float, w: int, lam: float, trials: int, rng: random.Random):
    exclusive = 0
    empty_foreign = 0
    share_sum = 0.0
    for _ in range(trials):
        attacker_blocks = 0
        attacker_w = 0.0
        foreign_w = 0.0
        for _h in range(w):
            if rng.random() < alpha:
                attacker_blocks += 1
                attacker_w += 1.0
            else:
                # Poisson(λ) via Knuth
                k = 0
                L = math.exp(-lam)
                p = 1.0
                while p > L:
                    k += 1
                    p *= rng.random()
                k -= 1
                foreign_w += k
        if attacker_blocks == w:
            exclusive += 1
        if foreign_w == 0:
            empty_foreign += 1
        total = attacker_w + foreign_w
        share_sum += (attacker_w / total) if total else 0.0
    return {
        "p_exclusive": exclusive / trials,
        "p_no_foreign": empty_foreign / trials,
        "e_share": share_sum / trials,
        "alpha_w": alpha**w,
        "exp_neg_lw": math.exp(-lam * w),
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--trials", type=int, default=20000)
    ap.add_argument("--seed", type=int, default=32)
    args = ap.parse_args()
    rng = random.Random(args.seed)

    print(
        f"{'W':>4} {'α':>6} {'λ':>6} {'α^W':>10} {'P[all W]':>10} "
        f"{'e^{-λW}':>10} {'P[no F]':>10} {'E[share]':>10} {'share/α':>8}"
    )
    for w in (8, 16, 24, 30, 32, 36):
        for alpha in (0.10, 0.25, 0.33, 0.50):
            for lam in (0.05, 0.10, 0.25):
                r = simulate(alpha, w, lam, args.trials, rng)
                ratio = r["e_share"] / alpha if alpha else 0.0
                print(
                    f"{w:4d} {alpha:6.2f} {lam:6.2f} {r['alpha_w']:10.3e} "
                    f"{r['p_exclusive']:10.3e} {r['exp_neg_lw']:10.3e} "
                    f"{r['p_no_foreign']:10.3e} {r['e_share']:10.4f} {ratio:8.3f}"
                )
    print()
    print("Reading: share/α > 1 means more than hashrate-proportional weight in F.")
    print("W=16 at α=0.5 still has α^W ≈ 1.5e-5 (hundreds of exclusive windows per")
    print("issuance horizon). W=32 is the smallest power of two with α^W·horizon < 0.05.")
    print("Simulation, not a Coq theorem. W is not changed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

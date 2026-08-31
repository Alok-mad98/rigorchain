#!/usr/bin/env python3
"""P0-045 CA research harness. Not production. Not consensus.

Implements the same cyclic elementary CA as CAPrototype.v.
Used to:
  - lock golden vectors
  - exhibit Rule 90 XOR-linearity (predictability)
  - time CA against SHA256
  - search for counterexamples to the *stated model* only

FUZZ here is counterexample search. It is not a Coq theorem.
CA is not a hash. Passing these checks is not cryptographic evidence.
"""

from __future__ import annotations

import hashlib
import time


RULE30 = 30
RULE90 = 90
RULE110 = 110


def ca_cell(rule: int, left: bool, center: bool, right: bool) -> bool:
    idx = (4 if left else 0) + (2 if center else 0) + (1 if right else 0)
    return bool((rule >> idx) & 1)


def ca_step(rule: int, state: list[bool]) -> list[bool]:
    n = len(state)
    if n == 0:
        return []
    out = []
    for i in range(n):
        left = state[(i + n - 1) % n]
        center = state[i]
        right = state[(i + 1) % n]
        out.append(ca_cell(rule, left, center, right))
    return out


def ca_iter(rule: int, steps: int, state: list[bool]) -> list[bool]:
    cur = list(state)
    for _ in range(steps):
        cur = ca_step(rule, cur)
    return cur


def bits_from_byte(b: int, width: int = 8) -> list[bool]:
    return [bool((b >> (width - 1 - i)) & 1) for i in range(width)]


def bits_to_int(state: list[bool]) -> int:
    v = 0
    for bit in state:
        v = (v << 1) | (1 if bit else 0)
    return v


def xor_states(a: list[bool], b: list[bool]) -> list[bool]:
    return [x ^ y for x, y in zip(a, b)]


def bits_str(state: list[bool]) -> str:
    return "".join("1" if b else "0" for b in state)


# ---------------------------------------------------------------------------
# Golden vectors (locked in CAPrototype.v via vm_compute)
# 8-cell cyclic ring. Encoding: MSB is index 0.
# ---------------------------------------------------------------------------

ZERO8 = bits_from_byte(0b00000000)
SINGLE = bits_from_byte(0b00010000)  # one live cell at index 3
ALT = bits_from_byte(0b10101010)
ONES = bits_from_byte(0b11111111)

GOLDEN = {
    ("zero", RULE30, 1): ZERO8,
    ("zero", RULE90, 1): ZERO8,
    ("zero", RULE110, 1): ZERO8,
    ("single", RULE30, 1): bits_from_byte(0b00111000),
    ("single", RULE90, 1): bits_from_byte(0b00101000),
    ("single", RULE110, 1): bits_from_byte(0b00110000),
    ("alt", RULE30, 1): ALT,  # period-2 is a Rule 30 fixed point on this ring
    ("alt", RULE90, 1): ZERO8,  # Rule 90 annihilates period-2 on even rings
    ("alt", RULE110, 1): ONES,
    ("ones", RULE30, 1): ZERO8,
    ("ones", RULE90, 1): ZERO8,
    ("ones", RULE110, 1): ZERO8,  # 111 -> 0 under Rule 110; not a fixed point
}


def named_state(name: str) -> list[bool]:
    return {"zero": ZERO8, "single": SINGLE, "alt": ALT, "ones": ONES}[name]


def check_golden() -> int:
    fails = 0
    for (name, rule, steps), expected in GOLDEN.items():
        got = ca_iter(rule, steps, named_state(name))
        if got != expected:
            print(f"GOLDEN FAIL {name} rule={rule}: {bits_str(got)} != {bits_str(expected)}")
            fails += 1
    return fails


def check_determinism(trials: int = 256) -> int:
    fails = 0
    for rule in (RULE30, RULE90, RULE110):
        for seed in range(trials):
            s = bits_from_byte(seed)
            a = ca_iter(rule, 16, s)
            b = ca_iter(rule, 16, s)
            if a != b:
                print(f"DETERMINISM FAIL rule={rule} seed={seed}")
                fails += 1
    return fails


def check_length() -> int:
    fails = 0
    for n in range(0, 33):
        s = [False] * n
        if n:
            s[0] = True
        for rule in (RULE30, RULE90, RULE110):
            if len(ca_step(rule, s)) != n:
                print(f"LENGTH FAIL rule={rule} n={n}")
                fails += 1
    return fails


def check_rule90_linear() -> int:
    """Rule 90 is F2-linear: step(a XOR b) = step(a) XOR step(b).

    This is the useful negative result: the rule is too predictable
    to be a mixer, commitment, or hash substitute.
    """
    fails = 0
    for x in range(256):
        for y in range(256):
            a = bits_from_byte(x)
            b = bits_from_byte(y)
            left = ca_step(RULE90, xor_states(a, b))
            right = xor_states(ca_step(RULE90, a), ca_step(RULE90, b))
            if left != right:
                print(f"RULE90 LINEAR FAIL {x:02x} {y:02x}")
                fails += 1
                return fails
    return fails


def check_rule90_single_shift() -> int:
    """On an 8-cell ring a single 1 under Rule 90 splits, then the
    two 1s wrap and cancel. This is algebraic, not cryptographic mixing.
    """
    s = bits_from_byte(0b00010000)
    expect = [
        0b00101000,
        0b01000100,
        0b10101010,
        0b00000000,
    ]
    fails = 0
    cur = s
    for i, e in enumerate(expect, start=1):
        cur = ca_step(RULE90, cur)
        if cur != bits_from_byte(e):
            print(f"RULE90 SHIFT FAIL step {i}: {bits_str(cur)}")
            fails += 1
    return fails


def check_boundaries() -> int:
    fails = 0
    for rule in (RULE30, RULE90, RULE110):
        if ca_step(rule, []) != []:
            print(f"EMPTY FAIL rule={rule}")
            fails += 1
        z = [False] * 16
        if ca_step(rule, z) != z and rule in (RULE30, RULE90, RULE110):
            # bit 0 of these three rules is 0, so all-zero is a fixed point
            if bits_to_int(ca_step(rule, z)) != 0:
                print(f"ALLZERO FAIL rule={rule}")
                fails += 1
    return fails


def check_repeated_execution() -> int:
    s = bits_from_byte(0b01101001)
    fails = 0
    for rule in (RULE30, RULE90, RULE110):
        once = ca_iter(rule, 32, s)
        twice = ca_iter(rule, 16, ca_iter(rule, 16, s))
        if once != twice:
            print(f"ITER SPLIT FAIL rule={rule}")
            fails += 1
    return fails


def bench(seconds: float = 0.4) -> dict[str, float]:
    """Single-thread microbench. Not a hardware study.

    Purpose: cost comparison, not 'CA is fast therefore adopt'.
    """
    payload = bytes(range(32))
    state = [(payload[i // 8] >> (7 - (i % 8))) & 1 == 1 for i in range(256)]

    def count(fn) -> float:
        n = 0
        t0 = time.perf_counter()
        while time.perf_counter() - t0 < seconds:
            fn()
            n += 1
        return n / seconds

    sha256_hz = count(lambda: hashlib.sha256(payload).digest())
    sha256d_hz = count(lambda: hashlib.sha256(hashlib.sha256(payload).digest()).digest())
    ca30_hz = count(lambda: ca_iter(RULE30, 256, state))
    ca90_hz = count(lambda: ca_iter(RULE90, 256, state))
    ca110_hz = count(lambda: ca_iter(RULE110, 256, state))
    ca_then_sha = count(
        lambda: hashlib.sha256(bytes(bits_to_int(ca_iter(RULE30, 256, state)).to_bytes(32, "big"))).digest()
    )
    return {
        "sha256_hz": sha256_hz,
        "sha256d_hz": sha256d_hz,
        "ca30_256x256_hz": ca30_hz,
        "ca90_256x256_hz": ca90_hz,
        "ca110_256x256_hz": ca110_hz,
        "ca30_then_sha256_hz": ca_then_sha,
    }


def main() -> int:
    fails = 0
    fails += check_golden()
    fails += check_determinism()
    fails += check_length()
    fails += check_rule90_linear()
    fails += check_rule90_single_shift()
    fails += check_boundaries()
    fails += check_repeated_execution()

    print("=== P0-045 CA research harness ===")
    print(f"model_checks_failed = {fails}")
    print("rule90_linear = PASS" if fails == 0 else "rule90_linear = see FAIL lines")
    print("note: Rule 90 linearity is an argument AGAINST cryptographic use")
    print("note: CA is not collision-resistant; this harness does not claim that")

    times = bench()
    print("=== single-thread microbench (higher = more iterations/sec) ===")
    for k, v in times.items():
        print(f"{k} = {v:.1f}")
    print("OpuHash is not timed here (64 KiB / 256 ChaCha20+SHA256d rounds).")
    print("A 256-cell / 256-step CA is cheaper than OpuHash and is not a substitute.")

    if fails:
        print("CA_RESEARCH = FAIL")
        return 1
    print("CA_RESEARCH = PASS")
    print("FUZZ = RESEARCH_PASS  # model self-consistency only")
    print("FORMAL_PROOF = SEE CAPrototype.v  # not a RIGOR consensus theorem")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

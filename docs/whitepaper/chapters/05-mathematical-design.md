# Chapter 5 — Mathematical Design

> **Historical RFC — not an active consensus parameter.** This draft chapter
> explains the RFC-014 teaching identity `E = 240`, `2^240 / 2^256 = 1/65536`
> (`C = 65536` ORU). Those figures are **not active consensus parameters**.
> Deterministic Reserve Issuance uses `E = 246`, `C = 1024` ORU per RES mint, expected wins ≈ `n / 1024`.
> Canonical text: bound protocol whitepaper §5.1.

Two constants carry the public mathematics of RIGOR:

| Constant | Domain | Job |
|----------|--------|-----|
| **65536 = 2^16** | Convergence probability | Measurable, hardware-native chance |
| **125 = 5³** | ORU tail subsidy | Permanent miner floor after three halvings |

They belong together: one is the **probability boundary**, the other is the
**convergence point of the subsidy sequence**. Both are integer, checkable,
and independent of anyone’s discretion.

---

## Why 65536

**65536 is 2^16.** It is one of the classic scales of computing: the number
of distinct values in sixteen bits (0 through 65535), the familiar “64K”
boundary of early CPUs, memory pages, and graphics systems. It appears as a
default hash-table size, as the count of TCP/UDP ports, and as a natural
power-of-two length for buffers and indices. Because it is a pure power of
two, it is friendly to bit operations. It is a hardware-native number, not a
round marketing figure.

### Hash space

SHA-256’s output space is `2^256` values. The teaching identity is:

```
2^240 / 2^256  =  1 / 2^16  =  1 / 65536
```

A 256-bit digest, restricted by a 240-bit threshold, is exactly a
**sixteen-bit** fraction of the space. The protocol takes that fraction as
the public probability boundary:

```
p(n ORU)  =  min(1,  n / 65536)     (aggregate identity)
```

One ORU of committed weight corresponds to a **1/65536** chance in that
normalized model. Expected wins scale with committed ORU:

```
expected wins  ≈  n / 65536
```

Under the frozen window (`W = 32`):

```
p_draw(n)  =  n / (65536 · 32)  =  n / 2,097,152
```

The window changes the per-draw chance. It does **not** change the
aggregate `n / 65536` identity.

Saturation at `n ≥ 65536` in the single-draw story means linear probability
reaches 1. That is a probability bound, not a financial promise. Committing
65536 ORU does not “guarantee profit”.

### A note on Knuth

Donald E. Knuth — Turing Award laureate and a founder of modern computer
science — has a well-known affection for 65536. In 1967, stopped at a red
light in Wisconsin, he saw the plate **H65536** on the car ahead and later
wrote that he almost asked the driver whether the number’s meaning was
understood. Decades later he returned to the story in *Mathematical Vanity
Plates* and reconstructed the plate in a program.

RIGOR does not need a celebrity to justify a power of two. The Knuth story
is recorded because 65536 was already a serious constant in computing
before this protocol existed. Choosing it is a decision to use a number
that is **exact, historical, and checkable** — not a number that can be
quietly moved.

**There is no back-room rate. The published aggregate is 1/65536 per ORU
of committed weight.**

---

## Why 125

ORU subsidy is a three-step halving, then a lock:

```
1000  →  500  →  250  →  125
```

| Step | Subsidy | Meaning |
|------|---------|---------|
| Genesis era | 1000 ORU | Initial miner payment |
| First halving | 500 ORU | 1000 / 2 |
| Second halving | 250 ORU | 500 / 2 |
| Terminal floor | **125 ORU** | 250 / 2 — and then **stop** |

Interval: 210,000 blocks. After the third halving, `R(h) = 125` forever.
RFC-001: no further halving is allowed.

**125 is the natural end of that sequence.** It is also a small integer
with an unusually clean identity.

### 125 = 5³

```
5 × 5 × 5  =  125
```

125 is a perfect cube. Among the cubes of 1 through 10
(`1, 8, 27, 64, 125, …`) it is the value the subsidy sequence actually
reaches. The cube root is the integer 5. The tail is not an arbitrary
leftover; it is a **pure third power**.

### Self-describing digits

125 is known as the only positive integer whose **proper divisors**
(`1`, `5`, `25`) all appear as contiguous substrings of its decimal
representation:

```
1 2 5
│   │
1     5
  └── 25
```

- **1** sits at the front
- **5** sits in the middle
- **25** sits at the end

The decimal word “125” displays its own divisor structure. That is a rare
self-describing coincidence. It is not a consensus rule. It is why 125 is
a fitting **symbol** for a floor that must remain complete after repeated
halving.

### Other integer identities (secondary)

These are recorded for the mathematical-design page. They do not change
subsidy consensus.

| Identity | Expression |
|----------|------------|
| Perfect cube | `5³ = 125` |
| Two squares | `10² + 5² = 100 + 25 = 125` |
| Two squares | `11² + 2² = 121 + 4 = 125` |
| Divisors | `1, 5, 25, 125` — a pure prime-power lattice |
| Friedman form | `5^(1+2) = 125` |
| Nearby perfect powers | `121 = 11²`, `125 = 5³`, `128 = 2⁷` |

121, 125, and 128 form a tight cluster of perfect powers. 125 sits in the
middle of that cluster — a small, complete number between a square and a
power of two.

### What 125 means for the protocol

The sequence **1000 → 500 → 250 → 125** is fair because each step is the
same rule: divide by two, using integers, at a fixed height interval. The
sequence **stops** because a medium of exchange that pays miners cannot be
allowed to decay to zero without reopening Chapter 1.

125 is therefore:

- the **arithmetic** end of three halvings of 1000
- the **security-budget** floor for ORU
- a **symbol** of a value that remains whole after reduction

It is not 65536. 65536 is the wide binary world — ports, pages, hash
fractions. 125 is the small cube at which the subsidy is allowed to rest.

**Same commitment as 1/65536:** every step is a published integer rule.
No hidden schedule. No discretionary last reward.

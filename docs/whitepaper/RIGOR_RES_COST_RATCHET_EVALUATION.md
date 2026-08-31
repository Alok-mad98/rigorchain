# RIGOR RES Cost-Ratchet Strength Evaluation

**Task:** `TASK-RIGOR-RES-COST-RATCHET-STRENGTH-EVALUATION-P0-009`  
**Canonical name:** `docs/whitepaper/RIGOR_RES_COST_RATCHET_EVALUATION.md`  
**Date:** 2026-08-23  
**Type:** Technical design evaluation  
**Scope:** RFC-017 RES reward schedule — can the cost ratchet be strengthened?  
**Code changes:** **NONE**  
**Consensus changes:** **NONE**

```
equivalence_claimed = false
documentation_consensus_authority = false
refinement_proved = false
mathematical_documentation_is_code_proof = false
```

This document is a **design evaluation**. It is not a public core whitepaper,
not an RFC, not a consensus change, and not an economic marketing paper. It
does not authorize an implementation change.

It asks one question: **can RIGOR obtain a stronger RES issuance cost ratchet
than RFC-017 delivers, while preserving formal verification, no global state,
deterministic consensus, and UTXO accounting?**

Subject under evaluation:
[`../archive/rfc/RFC-017-SCHEDULED-RES-REWARD.md`](../archive/rfc/RFC-017-SCHEDULED-RES-REWARD.md)
(historical RFC text; the shipped behaviour is the code and the Coq
development, not the RFC).

| Reference | Role |
|-----------|------|
| [`RIGOR_PROTOCOL_IDENTITY.md`](./RIGOR_PROTOCOL_IDENTITY.md) | Protocol identity; no-global-state definition |
| [`RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md`](./RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md) | `R(h)`, `W(k)`, `σ` as shipped |
| [`RIGOR_FORMAL_VERIFICATION_DESIGN.md`](./RIGOR_FORMAL_VERIFICATION_DESIGN.md) | Where the proof stops |
| [`../security/F4_LIFETIME_DECISION.md`](../security/F4_LIFETIME_DECISION.md) | The no-feedback boundary |

---

## 0. Verdict

**Option A — keep RFC-017.**

The evaluation set out to find a stronger mechanism. It did not find one, and
the reason is not a judgement call about complexity budgets. Three independent
results close the question:

1. **The economic premise is void.** RFC-017 §5.2's supply–cost conservation
   law holds for *every* reward schedule. Per-height decay does not create
   scarcity; it selects a different point on the same fixed hyperbola — and
   that exact point is reachable inside RFC-017 by changing one constant
   (`W₀ = 40.000 → 25.729 RES`) with zero proof, arithmetic, or implementation
   change.
2. **The target property is arithmetically unsatisfiable.** Strict per-block
   monotonicity `W(h+1) < W(h)` over RFC-017's horizon requires 52,710,000
   distinct integer values below `W₀ = 40,000,000` atoms. More generally, *any*
   strictly per-block-decreasing integer schedule under the 21M cap terminates
   within ~6,480,740 blocks (~123 years) — 8.1× short of the shipped 1003-year
   horizon. This is not a tuning problem; it is a pigeonhole.
3. **The arithmetic does not fit.** Per-height indexing pushes the largest
   intermediate from 56 bits to 109 bits, requiring 128-bit consensus
   arithmetic and destroying the property that the Coq definition is literally
   the C++ expression.

One genuine defect *was* identified and is documented in §8: the epoch-boundary
reward cliff. It is bounded, it is largest exactly once (height 210,000), and
it is co-located with an ORU subsidy halving that is not proposed for removal —
so smoothing `W` removes at most part of a discontinuity that remains either
way. It does not justify a successor RFC.

Option B (RFC-017B) is **not** recommended. Option C's framing — "the economic
improvement does not justify the consensus complexity" — is too generous: there
is no first-order economic improvement to place on the scale.

---

## 1. RFC-017 as implemented

### 1.1 The shipped schedule

```
k    = ⌊h / RES_REWARD_EPOCH⌋
W(k) = 0                                                   for k > 250
W(k) = (RES_REWARD_GENESIS << 16) / isqrt((k+1)³ << 32)     otherwise
```

| Constant | Value |
|----------|-------|
| `RES_COIN` | 1 000 000 |
| `RES_REWARD_GENESIS` (`W₀`) | 40 000 000 atoms = 40.000000 RES |
| `RES_REWARD_EPOCH` | 210 000 blocks |
| `RES_REWARD_LAST_EPOCH` | 250 |
| `RES_MAX_SUPPLY` | 21 000 000 000 000 atoms |
| `C` (ORU burned per mint, `E = 246`) | 1024 ORU |

Source: `ResRewardForEpoch`, `GetResMintReward`, `ResScheduleSum` in
`core/src/consensus/rigor_consensus_lock.h`; `res_reward_for_epoch` in
`formal_verification/coq/ResSchedule.v`; `res_reward_for_epoch` in
`formal_verification/spec/rigor_consensus_spec.py`.

The cost floor is `C / W(k)`. Because `C` is a protocol constant and `W` is a
protocol function of height alone, the floor rises on a calendar with no
dependence on demand, on ORU emission, or on chain state.

### 1.2 The step is already strict

The task frames the epoch step as a weakness because "210 000 blocks share
identical reward". Two facts qualify that framing before anything else is
evaluated.

First, **RFC-017 is already strictly monotone at every step it has.** A
recomputation of all 250 adjacent pairs finds zero ties: `W(k+1) < W(k)` for
every `k < 250`. The Coq theorem `reward_nonincreasing` states the weak form,
but the schedule realizes the strict one. The ratchet does not stall anywhere
in its domain. (The two-decimal plateau described in RFC-017 §7.2 was removed
when `RES_COIN` became 10⁶.)

Second, **the step is in the floor, not in what anyone pays.** RFC-017 §5.5
gives the realized cost under Poisson arrivals as `λC / (W·(1 − e^(−λ)))`.
Recomputed at 100% burn:

| `k` | λ | floor (ORU/RES) | realized ÷ floor | realized |
|---|---|---|---|---|
| 0 | 0.9766 | 25.6 | 1.567× | 40.1 |
| 1 | 0.4883 | 72.4 | 1.264× | 91.5 |
| 2 | 0.2441 | 133.0 | 1.127× | 149.9 |
| 3 | 0.1221 | 204.8 | 1.062× | 217.6 |
| 10 | 0.1221 | 934.0 | 1.062× | 992.1 |

The realized cost varies continuously with burn intensity *inside* every epoch
and sits 6%–57% above the floor. The intra-epoch price gradient the proposal
seeks already exists; it is supplied by contention against the one-mint-per-block
rule, not by the schedule.

So the accurate statement of the concern is not "the ratchet is flat" but
"the *floor* has 250 steps rather than 52 710 000". §2 asks whether that
granularity has economic content.

---

## 2. Steel-manning the concern: where RES is actually issued

The strongest available form of the task's hypothesis is not about granularity
in the abstract. It is about **where in the schedule the supply actually comes
from.**

At most one mint per block, each destroying `C = 1024` ORU, and ORU emission is
`1000 >> min(k,3)` per block. Mints per epoch are therefore bounded by
`min(210 000, ORU_emitted / C)`. At the physical maximum (100% of emitted ORU
converged):

| `k` | ORU emitted | max mints | `W(k)` RES | RES issued | cumulative share |
|---|---|---|---|---|---|
| 0 | 210 000 000 | 205 078 | 40.000000 | 8 203 120 | **74.49%** |
| 1 | 105 000 000 | 102 539 | 14.142196 | 1 450 127 | 87.66% |
| 2 | 52 500 000 | 51 269 | 7.698004 | 394 669 | 91.25% |
| 3 | 26 250 000 | 25 634 | 5.000000 | 128 170 | 92.41% |
| 9 | 26 250 000 | 25 634 | 1.264911 | 32 425 | 95.43% |
| 24 | 26 250 000 | 25 634 | 0.320000 | 8 203 | 97.49% |
| 99 | 26 250 000 | 25 634 | 0.040000 | 1 025 | 99.32% |
| 250 | 26 250 000 | 25 634 | 0.010058 | 258 | 100.00% |

**Epoch 0 alone issues 74.5% of all RES that will ever exist, at 25.6 ORU/RES —
the lowest cost floor in the chain's history. Epochs 0–2, the first twelve
years, issue 91.2%.**

That is the concern in its sharpest form, and it is real. The single flat
interval where the floor never moves is precisely the interval in which three
quarters of the asset is minted, while 248 of the ratchet's 250 rising steps
govern only the final 8.8% of supply. Stated that way the hypothesis has
genuine force, and §3 tests it rather than dismissing it.

(Total: 11 012 016 RES against 6 877 304 832 ORU destroyed, 52.4% of cap. This
independently reproduces RFC-017 §5.3's 10 983 984 figure to within 0.26%; the
difference is per-epoch flooring of the mint count.)

§3 evaluates whether per-height decay repairs this.

---

## 3. The conservation law: why height decay creates no scarcity

RFC-017 §5.2 states the constraint that governs the entire question:

```
(total RES issued) × (average ORU cost per RES) = (ORU destroyed) ≤ 6 877 500 000
```

The right-hand side is fixed by the ORU subsidy schedule. It is **not a function
of `W`.** Every mint destroys exactly `C` ORU at every height, forever, under
every reward schedule. The reward schedule determines only how much RES that
fixed quantity of destroyed ORU buys.

Applying the ideal continuous curve `W(h) = W₀·(1 + h/210000)^(−3/2)` — same
`W₀`, same time constant, same horizon — to the same ORU-limited mint counts:

| Design | RES issued | average price | ORU destroyed |
|---|---|---|---|
| RFC-017 step `W(k)` | 11 012 016 | 625 ORU/RES | 6 877 304 832 |
| Continuous `W(h)` | 7 083 115 | 971 ORU/RES | 6 877 304 832 |

The product is identical to the last digit. Per-height decay does not make RES
more expensive to produce; it makes each mint pay out less, which is the same
statement viewed from the other side. It moves along the hyperbola, it does not
lift it.

**The decisive consequence:** that destination is already reachable inside
RFC-017. Setting

```
W₀ = 25.729 RES        (admissible: cap requires W₀ ≤ 40.221 RES)
```

reproduces the same total supply and the same average production cost, with

- no change to `ResRewardForEpoch`
- no change to `ResSchedule.v`'s structure
- no change to the overflow analysis
- no new theorem
- one `constexpr` literal and the pinned `static_assert` values

The residual difference between the two designs is second-order: uniform
`W₀` scaling preserves the shape and shifts the level, while continuous decay
redistributes emission from early epochs toward late ones. That redistribution
is smaller than it sounds. Epoch 0's average reward falls from 40.000 to
23.432 RES, but because the same compression applies to every epoch, epoch 0's
*share* of total issuance falls only from 74.49% to 67.84%. Issuance remains
overwhelmingly concentrated in the first four years under either design,
because the concentration is driven by front-loaded ORU emission — 210 000 000
ORU in epoch 0 against 26 250 000 per epoch in the tail — not by the shape of
`W`. Per-height decay does not address the cause.

And §1.2 already showed that the realized price inside epoch 0 is set by
contention (1.567× the floor), not by the floor's slope.

**Finding.** The claim "height decay strengthens the ratchet" does not survive
the conservation law. It is a reparametrisation whose first-order effect is
purchasable with one constant.

---

## 4. Global state (Part 3)

**Answer: height-based decay introduces no global state. `no_global_state`
is preserved.** This is the one part of the proposal that passes cleanly.

The identity paper defines the boundary
([`RIGOR_PROTOCOL_IDENTITY.md`](./RIGOR_PROTOCOL_IDENTITY.md)):

> Monetary architecture is **stateless** in the account-ledger sense:
>
> - no global asset table
> - no account-balance consensus object
> - no explorer database as ledger of record
> - fee burn is not a UTXO and not a wallet balance
>
> Consensus state is the UTXO set plus validation rules. Rebuildable
> indexes do not hold monetary truth.

The test is the *dependency footprint* of the reward function, not its
granularity:

| Form | Reads | Verdict |
|---|---|---|
| `W(⌊h/210000⌋)` — shipped | the validated block's own header height | local |
| `W(h)` — proposed | the validated block's own header height | local |
| `W(realized_mint_rate)` | chain history aggregate | **global state — forbidden** |

`W(h)` and `W(⌊h/E⌋)` read the **same single integer**. Coarsening an integer
by division does not change what was read. Both are pure total functions
`Z → Z`, both are evaluable from the block under validation alone, and both are
computable without the UTXO set, without the tip, and without any accumulator.

One implementation detail is load-bearing and applies identically to both. The
height must be the **block's own** height, not the tip carried in validation
state. The Coq development already enforces this:

```coq
Definition cb_res_mint_amount (b : BlockAbs) : Z :=
  get_res_mint_reward (Z.of_nat (hdr_height (blk_header b))).
```

Source: `formal_verification/coq/ReplayReorgCore/ConnectBlockMint.v`. RFC-017 §6
states the reason: reading the tip would make replaying a block on a different
fork mint a different amount. Per-height decay inherits this requirement
unchanged and does not make it harder to satisfy.

**Part 4 / Property 4 — no consensus feedback loop.** Both `W(k)` and `W(h)`
satisfy this trivially; neither reads realized mint rate, burn history, or
cumulative supply. Neither approaches the F-4 boundary, which forbids
retargeting `RES_PROBABILITY_TARGET` against realized mint rate:

> The protocol **does not** and **will not** (without a Level-5 hard fork) add
> negative-feedback retargeting of `RES_PROBABILITY_TARGET` based on realized
> mint rate.

Source: [`../security/F4_LIFETIME_DECISION.md`](../security/F4_LIFETIME_DECISION.md).

Demand-feedback decay is **rejected** and is not evaluated further. It
introduces new consensus inputs and a grinding surface, reverses F-4, and — per
RFC-017 §1 — produces a *weaker* ratchet than a constant target, because a
demand decline weakens or reverses it. Constant `T` is the strongest property
RFC-017 has; nothing in this evaluation trades it.

---

## 5. Strict monotonicity is arithmetically impossible

The task's Property 1 is strict:

```
h₂ > h₁   ⟹   W(h₂) < W(h₁)
```

This cannot be satisfied. Two independent bounds establish it.

### 5.1 Pigeonhole

A strictly decreasing sequence of non-negative integers over `H` heights
requires `H` distinct values in `[0, W(0)]`, hence `W(0) ≥ H − 1`.

| Quantity | Value |
|---|---|
| Paying heights in the RFC-017 horizon, `H = 251 × 210 000` | 52 710 000 |
| Distinct values available below `W₀` | 40 000 000 |
| Shortfall | 12 710 000 |

Strict per-block decrease is unsatisfiable at `RES_COIN = 10⁶` with
`W₀ = 40 RES`, before any question of curve shape.

### 5.2 The triangular bound — a general result

This is not specific to `W₀ = 40` or to the `(·)^(−3/2)` shape. For *any*
strictly per-block-decreasing positive integer schedule over `H` paying heights,
the values are at least `H, H−1, …, 1`, so

```
Σ_h W(h)  ≥  H(H+1)/2
```

Combined with the cap requirement `Σ_h W(h) ≤ RES_MAX_SUPPLY = 2.1 × 10¹³`:

```
H  ≤  6 480 740 blocks  ≈  30.9 epochs  ≈  123 years
```

RFC-017's horizon is 1003 years. **Any strict per-block ratchet compatible with
the 21M cap must terminate 8.1× sooner.** Strict per-block monotonicity and a
millennium-scale issuance horizon are mutually exclusive under a 21M cap at six
decimals, at any `W₀`, under any curve.

### 5.3 What precision would be required

For the specific `(1 + h/E)^(−3/2)` shape the binding constraint is the tail
slope. At `h = 250E`:

```
|dW/dh| = 7.1563 × 10⁻¹² · W₀_atoms   atoms per block
```

Requiring ≥ 1 atom per block gives `W₀ ≥ 1.3974 × 10¹¹` atoms — at
`W₀ = 40 RES` that means

```
RES_COIN ≥ 3.493 × 10⁹     (~9.5 decimal places)
```

RFC-017 §7.2 chose `RES_COIN = 10⁶` deliberately, to keep RES visually distinct
from ORU's eight decimals while giving a realistic 3–5M supply ~4×10¹² atoms.
Ten decimals inverts that decision and makes the RES atom smaller than the ORU
satoshi-equivalent, for no monetary purpose.

**Finding.** Property 1 as stated is not achievable. Any RFC-017B would have to
weaken it to `W(h+1) ≤ W(h)` — which RFC-017 already provides, and already
realizes strictly at every step it defines (§1.2).

---

## 6. Formal verification impact (Part 4)

### 6.1 What exists today

| Artifact | Current proof method | Cost |
|---|---|---|
| `schedule_terminates` | definitional unfold | trivial |
| `schedule_sum_value` (= 99 450 437) | `vm_compute` over 251 terms | < 1 s |
| `max_issuance_within_cap` | `vm_compute`; `discriminate` | < 1 s |
| `reward_nonincreasing` | `forallb` over 250 pairs, `vm_compute` | < 1 s |
| `cost_floor_nondecreasing_along_schedule` | `Z.div_le_compat_l` on the above | trivial |
| `reward_pin_table` | `vm_compute` on 6 points | trivial |
| `ResScheduleSum() == 99450437` | C++ `constexpr` fold, `static_assert` | compile time |

Source: `formal_verification/coq/ResSchedule.v`;
`core/src/consensus/rigor_consensus_lock.h`.

The entire development is **finite computation**. That is RFC-017 §4.2's
central claim: the cap is not an inductive runtime invariant, it is a
compile-time arithmetic fact. `Print Assumptions` reports *Closed under the
global context* for every theorem.

### 6.2 What survives per-height indexing, and what does not

| Property | Under `W(k)` | Under `W(h)` | Assessment |
|---|---|---|---|
| `deterministic_reward` | pure `Z → Z` | pure `Z → Z` | **preserved** |
| `reward_monotonic` (weak) | 250-pair `vm_compute` | analytic | **improves** — see below |
| `reward_monotonic` (strict) | holds, 0 ties | impossible (§5) | **lost** |
| `finite_supply_bound` | 251-term `vm_compute` | 52 710 000-term fold | **lost as computation** |
| `schedule_terminates` | definitional | definitional | preserved |
| `reward_pin_table` | 6 `vm_compute` points | same | preserved |

**Weak monotonicity actually gets easier.** `∀h, W(h+1) ≤ W(h)` follows
analytically from cube monotonicity, `Z.sqrt_le_mono`, and
`Z.div_le_compat_l` — exactly the chain already used by
`res_reward_for_epoch_le_genesis` in `ResSchedule.v`:

```coq
Lemma res_reward_for_epoch_le_genesis :
  forall k : nat, res_reward_for_epoch k <= RES_REWARD_GENESIS.
Proof.
  ...
  apply Z.le_trans with (m := Z.shiftl RES_REWARD_GENESIS 16 / 65536).
  - apply Z.div_le_compat_l; [ vm_compute; discriminate | lia ].
  ...
Qed.
```

An analytic monotonicity proof would generalize over all `h` without enumeration
and is a genuine, if minor, improvement over the current 250-pair `forallb`.
This is the proposal's one real technical merit.

**The cap bound is where it breaks.** `max_issuance_within_cap` currently
reduces to `vm_compute` because the sum has 251 terms. Under per-height
indexing it has 52 710 000 terms — 210 000× more. `vm_compute` on that is not a
slow proof, it is not a proof: it would allocate tens of gigabytes of `Z` cells
and would not be re-checkable by `coqchk` in any practical time.

The bound would instead have to be established analytically, by a telescoping
or integral-comparison argument of the form

```
Σ_{h≥0} ⌊W₀ · E^{3/2} / (h+E)^{3/2}⌋  ≤  W₀ · E^{3/2} · ∫_{E−1}^{∞} x^{−3/2} dx
                                       ≈  2·W₀·E  =  1.68 × 10¹³  ≤  2.1 × 10¹³
```

The bound is true and even reasonably tight. But formalizing an
integral-comparison argument for `x^(−3/2)` over `Z` with floor square roots and
floor division is a **new proof development with no precedent anywhere in the
current tree**, which is uniformly `vm_compute`-based. It would also need its
own no-overflow side conditions against the 128-bit model of §7.

**Finding.** The proposal converts RIGOR's strongest verification property — the
21M cap as a finite integer computation, independently re-checkable by `coqchk`
— into a hand-written analytic proof. This is a strict downgrade in proof
*character*, not merely in proof effort. RFC-017 §4.2 identifies precisely this
property as what the design bought; per-height decay spends it.

### 6.3 Downstream artifacts

Any change to `W` invalidates, and requires regeneration or re-review of:

- `formal_verification/phase3_golden_vectors.json` — 422 vectors; the
  `SupplyScaffold`, `NegativeCore`, `EdgeCross`, and `ScriptMintNeg` families
  all carry an explicit `"W"` field per height (e.g. XFAM-009 pins
  `h = 209 999 → 40 000 000` against `h = 210 000 → 14 142 196`)
- `core/src/proof/runtime_vector_harness.cpp` — 12 registry entries, of which
  GV-RES-0001/0002/0003, GV-DRAW-0001/0002/0003 and GV-STATE-0002 pin
  `expected_res_mint`
- `formal_verification/tests/test_rfc017_schedule_parity.py` — the tri-implementation
  parity gate (C++/Coq/Python over `k = 0..251`); under per-height indexing the
  exhaustive parity sweep over 52.71M heights becomes a sampling exercise, which
  weakens the parity guarantee itself
- `ResSchedule.v` pins, the two C++ `static_assert`s, and the Python `assert`s

The Rust pool crates (`pool/share-validator/src/res_mint.rs`,
`pool/stratum-server/src/winner.rs`, `pool/mint-engine/src/types.rs`) still
hardcode the pre-RFC-017 constant `RES_REWARD_ATOMS = 10_000`. That is an
existing defect, tracked in RFC-017 §6, and it is orthogonal to this
evaluation — but it would have to be fixed before any schedule change, because a
per-height schedule cannot be approximated by a constant at all.

---

## 7. Determinism and implementation complexity (Part 5)

### 7.1 Floating point, exponentiation, tables

Per-height decay introduces **none** of these, and this should be stated plainly
in the proposal's favour. `isqrt` of a cube is the same primitive at any
indexing granularity. No `pow`, no lookup table, no rounding rule beyond floor
division. On that axis, `W(h)` is exactly as determinism-safe as `W(k)`.

### 7.2 Integer width — the actual obstacle

The obstacle is width, and it is decisive.

| Expression | Largest intermediate | Bits | Fits uint64? |
|---|---|---|---|
| RFC-017, `k = 250`: `(251³) << 32` | 6.79 × 10¹⁶ | 56 | yes, **271.6× headroom** |
| Height form, `h = 52 709 999`: `(h+E)³ << 32` | 6.37 × 10³² | **109** | **no, by 3.45 × 10¹³×** |

RFC-017 §3.1's overflow argument does not merely tighten — it collapses by
thirteen orders of magnitude. Two consequences:

**The numerator does not survive either.** `E³ << 32` is 3.98 × 10²⁵ (86 bits)
and cannot be formed in-line, though this alone is survivable: it is a compile-time
constant and `isqrt(E³ << 32) = 6.31 × 10¹²` does fit, so it could be hardcoded.
What does not survive is the multiplication — `W₀ · isqrt(E³ << 32) = 2.52 × 10²⁰`
(68 bits) overflows `uint64` regardless of how the constant is obtained. The
expression must therefore be restructured to divide before multiplying, which
costs precision (see below), or widened.

Either way, RFC-017 §6's guarantee that "`Z.sqrt` is floor-sqrt and `Z.div`
floor-division on non-negatives, so the Coq definition is *literally* the C++
expression" no longer holds. Coq's `Z` is unbounded and forms these intermediates
without comment; C++ cannot. The two implementations would stop being the same
expression, and the Coq model would need an explicit machine-word model to
retain any correspondence at all.

**A 64-bit-safe reformulation exists but is not atom-exact.** Rewriting as
`q = (E << 32)/(h+E)`, `W = (W₀ · ((q · isqrt(q)) >> 32)) >> 16` keeps every
intermediate under 49 bits, but loses precision at the tail:

| `h` | 64-bit form | exact | error |
|---|---|---|---|
| 0 | 40 000 000 | 40 000 000 | 0 |
| 210 000 | 14 141 845 | 14 142 135 | −0.002% |
| 21 000 000 | 39 062 | 39 407 | −0.875% |
| 42 000 000 | 13 427 | 14 036 | **−4.339%** |
| 52 709 999 | 9 765 | 9 999 | −2.340% |

Worse, this form is **not even strictly decreasing** — `h = 52 500 000` and
`h = 52 709 999` both yield 9 765 — so it fails the property it was introduced
to deliver, while also silently changing the monetary schedule by up to 4.3%.

Atom-exactness and 64-bit width cannot both be had. A real RFC-017B would need
`__int128` (not portable to MSVC, not `constexpr` across toolchains, and needing
its own modelled no-overflow proofs) or a bespoke fixed-point factorisation with
a fresh precision analysis. Either way, "only one literal moves" is gone.

### 7.3 The step-count ceiling

Solving `(k+1)³ · 2³² ≤ 2⁶⁴ − 1` gives `k + 1 ≤ 1625`. This is a hard ceiling
and worth recording as a standalone fact:

> **The unmodified RFC-017 expression can index at most 1 625 steps in 64-bit
> arithmetic.** Per-block granularity is 32 437× beyond that ceiling.

### 7.4 Summary

| Axis | RFC-017 `W(k)` | Height decay `W(h)` |
|---|---|---|
| Arithmetic | uint64, 271.6× headroom | 128-bit, or 4.3% schedule error |
| Floating point | none | none |
| Cap proof | `vm_compute`, 251 terms | analytic integral comparison, new development |
| Monotonicity proof | `vm_compute`, 250 pairs | analytic (**easier**) |
| Coq ≡ C++ expression | literal | broken (wide intermediates, machine-word model needed) |
| Golden vectors | 422, exhaustive parity `k = 0..251` | regenerate; parity becomes sampled |
| `implementation_complexity` | **low** | **high** |
| `formal_proof_complexity` | **low** (finite) | **high** (analytic) |
| `consensus_risk` | **low** | **medium–high** |

---

## 8. The one real defect: the epoch-boundary cliff

The evaluation did find something. It is not the flat interval; it is the
discontinuity at its edge.

| `k → k+1` | `W(k)` RES | `W(k+1)` RES | cliff | RES premium | ORU subsidy |
|---|---|---|---|---|---|
| **0 → 1** | 40.000000 | 14.142196 | **2.8284×** | **25.8578** | 1000 → 500 |
| 1 → 2 | 14.142196 | 7.698004 | 1.8371× | 6.4442 | 500 → 250 |
| 2 → 3 | 7.698004 | 5.000000 | 1.5396× | 2.6980 | 250 → 125 |
| 3 → 4 | 5.000000 | 3.577712 | 1.3975× | 1.4223 | 125 → 125 |
| 9 → 10 | 1.264911 | 1.096405 | 1.1537× | 0.1685 | 125 → 125 |
| 99 → 100 | 0.040000 | 0.039407 | 1.0150× | 0.0006 | 125 → 125 |
| 249 → 250 | 0.010119 | 0.010058 | 1.0061× | 0.0001 | 125 → 125 |

At height 210 000 the same 1024 ORU buys 2.83× less RES than one block earlier.
The premium for a block at height 209 999 over one at 210 000 is 25.86 RES,
about 662 ORU at the epoch-0 floor. This is a bounded incentive discontinuity of
the same family as Bitcoin's halving fee-sniping incentive, and it is the reason
a smoother schedule is worth *evaluating* at all.

Four observations bound its significance.

1. **It is co-located with an ORU halving that stays.** A `static_assert`
   enforces `RES_REWARD_EPOCH == ORU_SUBSIDY_HALVING_INTERVAL`; the ORU subsidy
   drops 1000 → 500 at exactly the same height. Smoothing `W` removes the RES
   component and leaves a 500-ORU cliff at the identical block. The
   discontinuity is not eliminated, only reduced.
2. **It is essentially a one-time event.** By `k = 9` the cliff is 1.15× and the
   premium is 0.17 RES; by `k = 99` it is 1.015× and 0.0006 RES. Only the first
   two or three boundaries carry any magnitude.
3. **Capturing it is not free.** The premium is conditional on winning the
   convergence draw in the replacement block, and requires orphaning an honest
   block from a one-block deficit. It is not a validity issue and creates no new
   consensus rule.
4. **Height is not reorg-malleable.** A block's height is fixed by its chain
   position; there is no way to re-enter a cheaper epoch other than by
   displacing blocks, which is the ordinary reorg calculus.

### 8.1 The middle design, and why it also fails

If the cliff is the concern, the proportionate response is not per-block decay
but a **finer finite step** — which §7.3 shows is bounded at 1 625 steps. Keeping
the RFC-017 curve, time constant, and horizon, and subdividing the epoch by `N`:

| `N` | step (blocks) | steps | largest isqrt arg | 64-bit | cap use | max cliff | cliff @210 000 |
|---|---|---|---|---|---|---|---|
| **1 (shipped)** | 210 000 | 251 | 6.79e16 | yes (271.6×) | **99.45%** | 2.8284× | 2.8284× |
| 2 | 105 000 | 502 | 5.47e17 | yes | 86.17% | 1.8371× | 1.5396× |
| 4 | 52 500 | 1 004 | 4.39e18 | yes | 80.27% | 1.3975× | 1.2218× |
| **6** | 35 000 | 1 506 | 1.48e19 | yes (**1.245×**) | 78.43% | 1.2601× | 1.1394× |
| 7 | 30 000 | 1 757 | 2.35e19 | **no** | — | — | — |

`N = 6` (one step per 35 000 blocks, ~8 months) is the finest 64-bit-safe
subdivision. It is strictly decreasing across all 1 505 pairs, keeps
`schedule_sum` a `vm_compute` over 1 506 terms, cuts the worst cliff from 2.83×
to 1.26×, and reduces the discontinuity at height 210 000 to 1.14×. On the
stated criteria it is the best available design.

It is still not worth doing:

- **Overflow headroom collapses from 271.6× to 1.245×.** RFC-017's headroom is
  what makes the schedule robust to future changes in `W₀`, precision, or
  horizon. At 1.245× any such change is a silent overflow rather than a
  compile-time failure. This trades a large safety margin for a cosmetic
  improvement.
- **Cap utilisation falls from 99.45% to 78.43%**, invalidating RFC-017 §3.2's
  entire `α = 3/2` justification, which turns on the horizon/tail-thickness
  argument and the 99.45% figure.
- **The economics are again a reparametrisation.** `N = 6` yields 7 627 554 RES
  at 902 ORU/RES — product conserved, and reproducible inside RFC-017 by
  `W₀ = 27.706 RES`. Epoch 0's share of issuance falls only from 74.5% to 69.1%.
- **Every downstream artifact in §6.3 must still be regenerated**, for a change
  whose largest observable effect is one softened discontinuity, four years
  after genesis, at a height where the ORU subsidy halves regardless.

---

## 9. Ranking (Part 7)

| Design | Economic strength | Formal verification | Complexity | Recommendation |
|---|---|---|---|---|
| **RFC-017 epoch decay** | baseline; ratchet unconditional, strict at all 250 steps | **excellent** — cap is a 251-term `vm_compute`, `coqchk`-re-checkable, no axioms | **low** — uint64, 271.6× headroom, Coq ≡ C++ literal | **current — keep** |
| Sub-epoch `N = 6` | **no gain** — same conservation hyperbola; = `W₀ 27.706` | good — 1 506-term `vm_compute` survives | medium — headroom 1.245×, full vector regeneration | not worth the margin loss |
| Height decay `W(h)` | **no gain** — same hyperbola; = `W₀ 25.729` | **poor** — cap proof stops being a computation; Property 1 impossible | **high** — 109-bit intermediates or 4.3% schedule error | **reject** |
| Demand feedback decay | **negative** — ratchet becomes demand-dependent and can reverse (RFC-017 §1) | **poor** — new consensus inputs, band invariants | **high** — grinding surface; reverses F-4 | **reject** |

---

## 10. Recommendation

**Option A — keep RFC-017.**

RFC-017 already holds every property the task set out to obtain:

```
cost_floor(k) = C / W(k)          non-decreasing, strict at every step it defines
W = f(block's own height)         no global state
W : Z → Z, integer only           deterministic, bit-identical on every platform
Σ W(k) ≤ cap, 251 terms           machine-checkable finite proof
C independent of demand           no consensus feedback loop
```

The proposed strengthening does not strengthen anything. Its economic effect is
a movement along a conserved hyperbola that one constant already reaches; its
target property is arithmetically unsatisfiable; and its cost is the conversion
of RIGOR's cap guarantee from a finite computation into an analytic proof, plus
128-bit consensus arithmetic.

The epoch-boundary cliff (§8) is a genuine, bounded observation. It is recorded
here rather than actioned. If it is ever revisited, the relevant facts are: the
64-bit step ceiling is 1 625; `N = 6` is the finest safe subdivision; and the
co-located ORU halving means no schedule change can remove the discontinuity at
height 210 000 on its own.

**No RFC-017B is authorized. No implementation change is proposed.**

If greater RES scarcity is ever wanted as a monetary decision — a separate
question from ratchet mechanics — the correct instrument is `W₀`, which is a
single `constexpr` bounded by `W₀ ≤ 40.221 RES`, requires no new theorem, and
preserves every property above. That is a monetary-policy decision requiring its
own authority, and this evaluation neither makes nor recommends it.

---

## 11. Boundaries of this document

This is an evaluation of a design hypothesis against the frozen consensus
constants and the existing Coq development. All figures were recomputed
independently from those constants; where they differ from RFC-017's published
figures the difference is noted (§2).

```
Consensus defines reality.
Explorer displays reality.
```

This document defines neither. It is not the C++ node, not the Coq development,
and not a consensus specification. The schedule is whatever
`core/src/consensus/rigor_consensus_lock.h` computes; the theorems are whatever
`formal_verification/coq/ResSchedule.v` proves about the protocol model.

```
equivalence_claimed = false
documentation_consensus_authority = false
refinement_proved = false
mathematical_documentation_is_code_proof = false
```

**Code impact: NONE**  
**Consensus impact: NONE**

# Genesis difficulty startup-time audit

**Task:** `TASK-RIGOR-GENESIS-DIFFICULTY-STARTUP-TIME-AUDIT`  
**Date:** 2026-08-29  
**Production consensus:** unchanged.

```
VERDICT              = NO_DIFFICULTY_FLAW
GENESIS_TIMESTAMP    = A (ASERT/difficulty); MTP floor only for earliest height-1 time
HEIGHT_1_NBITS       = genesis nBits (all modeled gaps)
FIRST_ASERT_EVAL     = height 2, exponent 0 when height 1 is the parent
ASERT_ANCHOR         = height 1 (not genesis)
COLLAPSE             = false on mainnet
COMPENSATION/FORK    = not proposed
W / T / RES / OPUSHASH / MATURITY = unchanged
```

Evidence: `core/src/test/pow_tests.cpp` (`startup_gap_*`).

## Production path

`GetNextWorkRequired` → (mainnet: not min-diff, not `fPowNoRetargeting`) → `GetNextWorkASERT`.

`CalculateNextWorkRequired` is legacy span math. It is not the live retarget.

```
time_error = (parent_time - anchor_time) - (parent_height - anchor_height) * 600
next      = clamp_±12.5%(anchor_target * 2^(time_error / 43200))
```

Child `nTime` is unused on mainnet. GBT ntime rolling cannot change `nBits`.

| Parent height | Anchor | time_error | Height-1 bits |
|---------------|--------|------------|---------------|
| 0 (genesis) | genesis (`nHeight < 1`) | 0 | copy genesis |
| 1 | height 1 | 0 | copy height 1 = genesis |
| ≥ 2 | height 1 | `(t_parent - t_h1) - (h-1)*600` | ASERT + clamp |

Genesis wall-clock never enters the exponent after height 1 exists.

## Modeled gaps (honest launch)

Miner `UpdateTime`: `nTime = max(MTP+1, now)`. Delay `D` ⇒ height 1 at `genesis+D`, then 10-minute blocks.

| Gap | Height 1 | Height 2 | Height 3 (on schedule) | Counterfactual if genesis stayed the ASERT clock |
|-----|----------|----------|------------------------|--------------------------------------------------|
| 10 min | genesis bits | genesis bits | genesis bits | 1.00× (on time) |
| 1 hour | same | same | same | 1.049× (inside ±12.5%) |
| 1 day | same | same | same | 3.96× → clamp 1.125×/block |
| 7 / 30 / 60 / 180 days | same | same | same | 1.6e4 … 2e108 × → clamp; ~47 steps to powLimit |

A long Genesis→height-1 gap **does not change initial difficulty, does not collapse difficulty, and does not violate the ±12.5% header invariant**. It **delays the first block**.

If genesis is **more than 2 hours in the future**, height 1 is also unmineable (`nTime > MTP` vs `nTime ≤ now+2h`). That is an MTP/future-block gate, not ASERT. Frozen timestamp `1786125600` (2026-08-07 18:00 UTC) is already in the past as of this audit.

## Classification

- **Difficulty / ASERT:** **A** — protocol identity (hash, MTP seed). Mining start for the difficulty clock is **height 1**.
- **Earliest legal height 1:** genesis MTP is a lower bound. That is not an ASERT start time.
- **Not C** for difficulty: a stale genesis clock does not ease or harden the live schedule.

`InitWarning` at lag `> 2τ` (1 day) is launch hygiene for MTP/locktime, not a catch-up-mining kill switch.

## Other networks (not a mainnet flaw)

- **testnet / devtest:** `fPowAllowMinDifficultyBlocks` — a child more than `2*spacing` after the parent gets `powLimit`. A 10-minute pause on testnet *does* drop height 1 to min-difficulty. Mainnet has this flag off.
- **regtest:** `fPowNoRetargeting` — bits never move.
- **slowtest:** ASERT, no min-difficulty — same qualitative result as mainnet.

## If a miner publishes height 1 with `nTime ≈ genesis+1` after a long wall-clock wait

The remaining gap moves to height 1→2. Height 3 then eases by at most +12.5% (ordinary stall). Not genesis-specific. Already covered by the ASERT clamp / `PermittedDifficultyTransition`.

## Correction

None. No consensus change for a follow-up implementation task.

Refreshing `GenesisLockMainnet::TIMESTAMP` would realign MTP with wall-clock but **changes the genesis hash**. It is not required to keep difficulty stable and is out of scope for a difficulty fix.

---

## Re-evaluation under SPEC-008 (SHA3-256 proof-of-work)

**Date:** 2026-08-30. The original verdict above is **not** re-opened; this section
records which of its findings survive the PoW replacement and which are superseded.

### Superseded verdict field

```
OPUSHASH = unchanged     →  superseded: SPEC-008 replaces OpuHash with SHA3-256
```

Every other verdict field stands.

### Findings that are hash-function-independent (survive unchanged)

The entire ASERT analysis is a function of **timestamps and heights only**:

```
time_error = (parent_time - anchor_time) - (parent_height - anchor_height) * 600
next       = clamp_±12.5%(anchor_target * 2^(time_error / 43200))
```

No term references the PoW function. Therefore the anchor-at-height-1 result, the
"Modeled gaps" table, the ±12.5% clamp behaviour, the MTP/future-block gate, and
the per-network differences all carry over verbatim. The 1-day counterfactual
(3.96×) and the 2-hour future-genesis gate were re-derived and are unchanged.

### Findings that depend on the difficulty constants (re-checked, preserved)

`~47 steps from genesis difficulty to powLimit` is the one figure here that reads
the constants. It depends only on the **ratio** between them, not their absolute
values:

| | genesis target | powLimit | ratio | steps at +12.5%/block |
|---|---|---|---|---|
| OpuHash | 2^232 | 2^240 | 2^8 | 47.08 → 48 |
| SPEC-008 §8.2 | 2^223 | 2^231 | 2^8 | 47.08 → 48 |

Both constants scale by the same factor R, so the ratio and this figure are
preserved exactly. **No re-derivation is required.**

### What this audit does *not* cover

The `~10 min at ~28 kH/s OpuHash` calibration is **not** part of this audit; it
lives in the `GenesisLockMainnet::NBITS` comment and in `core/doc/genesis-lockdown.md`.
Its re-derivation is SPEC-008 §8, which measured the new cost ratio directly.
The two documents were previously conflated — this audit models *difficulty
dynamics after launch*, not *time to mine the first block*.

### Test impact

`pow_tests.cpp` `startup_gap_*` cases read `consensus.powLimit` and the genesis
block from chain params rather than hard-coding them, so they follow the new
constants automatically. `startup_gap_audit_does_not_touch_frozen_params` asserts
only ASERT/subsidy/maturity constants, none of which move.

One unrelated assertion in the same file does hard-code a value and must be
updated with the constants:

```cpp
BOOST_CHECK_EQUAL(UintToArith256(consensus.powLimit).bits(), 240);  // → 231
```

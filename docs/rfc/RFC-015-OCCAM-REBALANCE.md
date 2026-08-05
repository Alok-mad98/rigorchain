# RFC-015: Occam Rebalance

**Version:** 1.0 FINAL  
**Status:** FROZEN (mainnet constitutional economics)  
**Freeze date:** 2026-05-26  
**Mode:** Atomic economic simplification  
**Baseline superseded:** `mainnet-freeze-rfc014-2026-05-25` / `4437597197aa713396a123673cc36606346ad523`  
**Release tag:** `mainnet-freeze-rfc015-2026-05-26` (see `MAINNET_PROTOCOL_FREEZE_DECLARATION.md`)

---

## 1. Scope

RFC-015 applies these pre-mainnet economic changes:

1. `RES_REWARD_AMOUNT` changes from `1 RES` to `100 RES` per successful convergence draw.
2. Minimum convergence cardinality changes from `|F| >= 2` to `|F| >= 1`.
3. `MAX_RES_SUPPLY` changes from `27,182,818 RES` to `21,000,000 RES` (Bitcoin-familiar cap; divides evenly by `100.00 RES` mint units).

No other consensus or economic change is authorized by this RFC.

---

## 2. Normative Changes

### 2.1 RES Mint Reward

Old:

```text
RES_REWARD_AMOUNT = 100 atoms = 1.00 RES
```

New:

```text
RES_REWARD_AMOUNT = 10,000 atoms = 100.00 RES
```

The RES display scale is unchanged:

```text
1 RES = 100 atoms
```

### 2.2 Minimum Cardinality

Old:

```text
RES convergence eligibility requires |F| >= 2
```

New:

```text
RES convergence eligibility requires |F| >= 1
```

The minimum economic participation requirement remains:

```text
f_i >= 1
MIN_CONVERGENCE_AMOUNT = 1 ORU
```

### 2.3 Supply Cap

Old:

```text
MAX_RES_SUPPLY = 27,182,818 RES
```

New:

```text
MAX_RES_SUPPLY = 21,000,000 RES
```

At `100.00 RES` per mint, `RES_MAX_SUPPLY % RES_REWARD_AMOUNT == 0`.

### 2.4 Probability Target

RFC-015 does not modify the convergence probability target (see §2.4):

```text
RES_PROBABILITY_TARGET = 2^240
```

The inequality remains:

```text
H_i < RES_PROBABILITY_TARGET * f_i
```

---

## 3. Rationale

### 3.1 Improved Liquidity Formation

RFC-014 produces approximately `100 RES/year` at the `N = 125` convergence equilibrium. RFC-015 keeps the probability law unchanged but raises the per-event reward to `100 RES`, producing approximately `10,000 RES/year` at the same equilibrium.

This improves:

- market depth;
- exchange viability;
- recurring price discovery;
- holder distribution;
- miner and convergence participant feedback.

### 3.2 Removal of Artificial Coordination Rule

The prior `|F| >= 2` rule imposed a coordination threshold outside the mathematical inequality. RFC-015 removes that artificial restriction and lets the draw law operate whenever at least one valid convergence candidate exists.

### 3.3 Economic Participation Preserved

Removing the cardinality threshold does not permit free minting. A candidate still needs a valid ORU convergence transaction with at least `1 ORU` converged, and the candidate still must satisfy the deterministic inequality.

---

## 4. Explicit Non-Changes

RFC-015 does not modify:

- `RES_PROBABILITY_TARGET = 2^240`;
- ASERT;
- ORU tail emission;
- transaction serialization;
- witness serialization;
- RES Mint witness layout;
- Sigma-CR-01;
- `bind_commit` derivation;
- `target_prev` replay-domain binding;
- ConvergenceRoot ordering;
- txid or wtxid semantics;
- burn accounting semantics;
- dynamic issuance;
- adaptive issuance;
- VM, TLV, or witness extensions.

---

## 5. Activation Discipline

RFC-015 is pre-mainnet only. Activation is genesis-reset only. It must not be activated through a fork-height transition, versionbits, dual-rule window, runtime configuration knob, or post-launch migration path.

---

## 6. Mandatory Output Fields

| Field | Value |
|---|---|
| Risk Classification | `HIGH` |
| Consensus Impact | `DIRECT` |
| Replay Impact | `GENESIS-RESET ONLY`; no post-launch replay migration is authorized. |
| Determinism Impact | `NONE`; both changes are fixed constants / fixed branch conditions. |
| Economic Impact | `STRUCTURAL`; expected equilibrium issuance rises from about `100 RES/year` to about `10,000 RES/year`, and singleton convergence can participate. |
| Replay Safety Status | `SAFE ONLY UNDER PRE-MAINNET GENESIS RESET` |
| Serialization Compatibility Status | `UNCHANGED` |
| Minimal Safe Recommendation | Apply exactly these two changes and no others. |

---

## 7. Final Verdict

```text
RFC015-OCCAM-REBALANCE-COMPLETE
```

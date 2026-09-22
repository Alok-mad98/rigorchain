# Consensus lock — Convergence identity

**Status:** documentation of a merge guard, not a consensus rule  
**Date:** 2026-09-15

```
protocol_changes = NONE
consensus_changes = NONE
mathematical_identity_changes = NONE
documentation_consensus_authority = false
```

This policy describes three cheap locks. It is not a consensus rule. It
does not change what a node admits. It does not replace RFC-020 or
RFC-018. It does not replace Coq verification. It does not prove C++
refinement. Changing this file does not change `CheckResMintArithmetic`.

Reproduce locally:

```
bash tools/run-consensus-lock.sh
```

---

## Protected invariant

```
destroy_cost(h) = 1024 · (2^25 + h)^2 / 2^50

        ↓ exact integer-preserving equivalence

R · (2^25 + h)^2 ≤ B << 40
```

`2^50` is the display-curve normalization denominator. It is not a
consensus shift. `2^40` is the integer consensus scaling factor after
`1024 = 2^10` is cancelled. Display rounding/floor do not enter
consensus.

Live C++ form (`core/src/consensus/res_issuance.h`):

```
R > 0  →  R · (RES_DESTROY_POLE + h)^2 ≤ B << RES_DESTROY_SHIFT
R = 0  →  B unconstrained
```

with `RES_DESTROY_POLE = 2^25` and `RES_DESTROY_SHIFT = 40`.

---

## Why it is protected

This identity is the Convergence issuance cost. It is what makes RES
minting a public integer decision. The derived ceiling `2^22` RES is the
closed integral of the same curve.

The lock prevents accidental drift of that identity:

- a different shift
- rounding or floor flowing back into the predicate
- division flowing back into the predicate
- a display helper becoming a consensus input
- a silent change of mathematical meaning

It does not freeze the rest of the engineering tree.

---

## The three locks (and no more)

Keep three independent locks. Do not merge them. Do not add CL004,
CL005, CL006.

```
Consensus Protection
│
├── convergence_identity_test
│
├── convergence_predicate_shape_check
│
└── publication_boundary_check
```

They stay separate because they protect different failure modes:

| Lock | Protects | Why not merged |
|------|----------|----------------|
| `convergence_identity_test` | The mathematical identity: conceptual cost and `B << 40` accept or reject together | A source-shape check can still pass if the integers drift; vectors catch that |
| `convergence_predicate_shape_check` | The arithmetic shape of the mint predicate in `CheckResMintArithmetic` | Vectors can still pass if a new operator is added beside a still-true inequality |
| `publication_boundary_check` | Public narrative: no unscoped “RIGOR is formally verified” | Documentation integrity, not mint arithmetic |

A consensus lock is not “forbid editing the project.” It is “prevent
unintentional damage to core mathematics and protocol identity.”

Future locks, if any, are only for true protocol identities (for
example Convergence identity, PoW header identity, transaction
serialization identity), and must remain few.

---

## What each lock does

| Lock | What it does | What it does not do |
|------|----------------|---------------------|
| `convergence_identity_test` | Integers only: conceptual uncleared cost and `B << 40` give the same accept/reject on fixed vectors, including `h = 0`, observational checkpoints, and `INT32_MAX` | Does not compile C++, does not evaluate `/ 2^50`, does not load a price table, does not run Coq |
| `convergence_predicate_shape_check` | Protects the arithmetic shape of the Convergence mint predicate: multiplication, squaring, scaling shift, and comparison remain explicit. It does not define the entire consensus execution path. 保护 Convergence 铸造判定的核心算术形状：乘法、平方、缩放移位和比较保持显式存在。该锁不定义整个共识执行路径。 Reads only `CheckResMintArithmetic` and the `tx_verify.cpp` call site | Does not scan the whole repository; display helpers with `>> 50` stay out of scope; does not mean consensus may execute only these operators |
| `publication_boundary_check` | Requires `equivalence_claimed = false`, `cpp_formal_equivalence = false`, `documentation_consensus_authority = false` on the public narrative; rejects an unnegated “RIGOR is formally verified” slogan | Documentation integrity only. Not a consensus rule. Not a Coq proof. Not a C++ refinement proof |

Cheap merge guards:

- identity: small integer vectors, seconds
- shape: point source check, seconds
- publication: text scan of the public narrative, seconds

No Coq. No full node build. Ordinary wallet / explorer / miner work does
not pay this tax unless its paths are in the filters below.

Workflows: `.github/workflows/consensus-lock.yml` (identity + shape) and
`.github/workflows/consensus-lock-publication.yml` (publication).
Path-filtered. Existing freeze / Coq / release workflows are unchanged.

---

## What is allowed to change without this lock firing as a protocol event

| Surface | Allowed without RFC? |
|---------|----------------------|
| Documentation (wording, this policy, whitepaper prose) | YES |
| UI | YES |
| Explorer | YES |
| Wallet | YES |

Those surfaces must not claim that documentation is consensus, or that C++
equals the Coq model.

---

## What requires protocol review (RFC)

Changing any of the following is a consensus change and needs the RFC
process — not a silent edit, and not a new consensus lock:

- `2^25` (`RES_DESTROY_POLE`)
- `40` (`RES_DESTROY_SHIFT`)
- ORU subsidy relationship used in the derived ceiling (`128 = 2^7` with `P0 = 2^10`)
- RES atom scaling (`RES_COIN = 10^8`)
- Predicate structure (operators, operands, or replacing `CheckResMintArithmetic`)

Numeric pins for those constants also live in `tools/lint-mainnet-freeze-locks.py`.
This lock does not replace that freeze pin. It protects the **identity
and shape** of Convergence validation.

---

## Required final reading

The lock protects the existing consensus identity.  
It does not introduce a new consensus rule.  
It does not replace RFC governance.  
It does not replace Coq verification.  
It does not prove C++ refinement.  
It only prevents accidental deviation.

# RIGOR CA Consensus Utility Feasibility (P0-045)

**Task:** `TASK-RIGOR-CA-CONSENSUS-UTILITY-FEASIBILITY-P0-045`  
**Date:** 2026-08-30  
**Mode:** RESEARCH DECISION ONLY. Production consensus was not modified.

```
CA_CONSENSUS_UTILITY_STATUS = NO_PROTOCOL_LANDING
CA_SCOPE                    = NONE
CA_PRODUCTION_MODIFIED      = NO

CA_POW_VALUE                = LOW
CA_OPUHASH_VALUE            = REJECT
CA_GBT_VALUE                = REJECT
CA_MINER_VALUE              = REJECT
CA_CONSENSUS_VALUE          = REJECT
CA_CRYPTO_VALUE             = LOW
CA_P2P_VALUE                = REJECT
CA_FORMALIZATION_VALUE      = LOW
CA_FUZZ_VALUE               = LOW

CA_SECURITY_GAIN            = NONE
CA_COMPLEXITY_COST          = HIGH_RELATIVE_TO_GAIN
CA_MAINTENANCE_COST         = UNJUSTIFIED

CA_PRIORITY_VS_EXISTING_FORMALIZATION = BELOW_EXISTING_FORMAL_CLOSURE

FORMAL_PROOF                = TOY_ONLY
FUZZ                        = RESEARCH_PASS
DIFFERENTIAL                = RESEARCH_PASS
BENCHMARK                   = RESEARCH_ONLY

FINAL_VERDICT               = CA_REJECT

CA_PROTOCOL                 = NOT_USED
CA_VISUALIZATION            = OPTIONAL
CA_USE_FOR_ASIC_RESISTANCE  = NO_DEFAULT
CA_OPUHASH                  = REJECT
PRODUCTION_MODIFIED         = NO
```

Honesty lock:

```
refinement_proved     = false
equivalence_claimed   = false
this file is not a C++ refinement of validation.cpp
CA theorems in formal_verification/research/CAPrototype.v
  are not RIGOR consensus theorems
fuzz ≠ theorem ≠ refinement proof
```

---

## 1. Why considered

RIGOR already freezes a short consensus path:

```
Genesis → height → ORU/RES schedule → W=32 → OpuHash → ASERT
  → GBT → miner nonce → submitblock → ConnectBlock → canonical chain
```

Elementary cellular automata (Rule 30 / 90 / 110) are small, deterministic,
integer-only objects. The question is not whether they can be coded. The
question is whether any of them is the unique, simpler, safer solution to a
problem the frozen protocol actually has.

This task does **not** try to invent a use so that CA can be kept.

---

## 2. Code inspection

No CA exists in production or in the live formal cone.

| Surface | Finding |
|---------|---------|
| Consensus lock | `core/src/consensus/rigor_consensus_lock.h` — W, T, OpuHash 64 KiB / 256, ASERT τ, RES/ORU, maturity. No CA field. |
| PoW | `core/src/pow_opus.cpp` — header SHA256d → ChaCha20 fill → 256 mix rounds → SHA256d. Closed and frozen. |
| Mining | `docs/mining/RIGOR_MINING_POW_PIPELINE_P0_042.md` — GBT copy-bytes; miner varies nonce (time only if permitted). |
| State | SPEC-001 / ConnectBlock — UTXO + classified script + monetary slice. Not a grid. |
| Formal live model | `formal_verification/rigor_v01/` — classifier, sighash, mint encoding, supply, reorg, window. `refinement_proved = false`. |
| P2P | P0-030 hardening. Capability bits are not state transitions. |
| Search | No `cellular` / `Rule 30` / `Rule 110` protocol object in the tree. |

Open formal work is C++ refinement of **existing** objects (classifier ∀,
sighash SHA256d/Ed25519, ConnectBlock slice, OpuHash/ASERT, GBT copy-bytes).
None of those holes is “missing a CA”.

---

## 3. Best landing point

**There is no protocol landing point.**

The only honest leftover is optional explorer / educational visualization.
That is not consensus, not GBT, not PoW, and not a reason to keep a CA TASK
open.

```
CA_PROTOCOL      = NOT_USED
CA_VISUALIZATION = OPTIONAL
```

---

## 4. Layer verdicts

### A. PoW / OpuHash

Proposed stack `header → SHA256d → CA → OpuHash → target` adds a third
mixer to a path that already has SHA256d + ChaCha20 + 256 SHA256d mix
rounds.

| Question | Answer |
|----------|--------|
| New security property? | No. CA is not a hash. |
| Only more compute? | Yes, if inserted. |
| Change ASIC/GPU/CPU edge? | Unproven; not a design goal. |
| New hardware path? | Yes — a fixed ECA is an easy circuit. |
| Break OpuHash simplicity? | Yes. |
| Extra verify cost? | Yes, on every header. |
| Extra miner complexity? | Yes. |
| Extra consensus failure surface? | Yes (boundary, indexing, iteration count). |
| Extra formal work? | Yes, and it does not close OpuHash refinement. |
| Worth it? | No. |

```
CA_POW_VALUE     = LOW
CA_OPUHASH_VALUE = REJECT
```

OpuHash is already compute-saturated and parameter-locked. CA does not
repair an OpuHash defect because none was found that CA uniquely fixes.

### B. Consensus state transition

Live transition is:

```
State(h) + Block(h+1) → accept/reject + unique State(h+1)
```

State is UTXO + height-keyed schedules + draw window contents, not a
fixed-width bit ring. Replacing that with `state_n → CA rule → state_{n+1}`
would be a new protocol, not a simplification.

A *sidecar* CA that ignores UTXO and is then hashed would be unused
complexity.

```
CA_CONSENSUS_VALUE = REJECT
```

### C. Cryptographic / commitment auxiliary

CA evolution is not collision resistance, not preimage resistance, and not
a commitment scheme.

Rule 90 is F2-linear (`rule90_linear` in the prototype):  
`step(a XOR b) = step(a) XOR step(b)`. Collisions and structured preimages
are algebraic, not hard.

Rule 30 looks irregular. That is not a proof. Mathematica-style PRNG use
is not a RIGOR security argument.

Any CA-then-SHA256 construction still needs SHA256. The CA prefix is
unjustified mixing.

```
CA_CRYPTO_VALUE    = LOW
CA_SECURITY_GAIN   = NONE
```

### D. Canonical verification layer

A 1D ECA *can* be written in Coq in a few dozen lines. The theorems you
get quickly are:

- determinism (functions are deterministic)
- length preservation (the list does not grow)
- Rule 90 linearity (anti-feature for crypto)

Those are not RIGOR obligations. They do not help classifier refinement,
sighash preimage, ConnectBlock supply, reorg issuance, OpuHash, or ASERT.

```
CA_FORMALIZATION_VALUE = LOW
FORMAL_PROOF           = TOY_ONLY
```

### E. GBT

GBT already ships immutable consensus fields; the miner varies nonce /
permitted time. CA is not a consensus commitment.

```
CA_GBT_VALUE = REJECT
```

### F. Miner

If the miner runs CA and the node re-runs it, that is extra validation
cost for no property. If the miner runs CA and the node ignores it, CA
is not a consensus mechanism.

```
CA_MINER_VALUE = REJECT
```

### G. P2P

P2P anti-spam, scoring, and handshake are already specified (P0-030).
No concrete P2P failure was found whose cheapest fix is a CA challenge.

```
CA_P2P_VALUE = REJECT
```

### H. Explorer / website

A Rule 30 picture on a stats page is allowed. It must not be written into
headers, GBT, P2P, or ConnectBlock.

---

## 5. Rule 30 / 90 / 110

Evaluated as cyclic 8-cell and 256-cell rings (research harness), plus
Coq for the 8-cell goldens and the Rule 90 forall theorem.

| | Rule 30 | Rule 90 | Rule 110 |
|--|---------|---------|----------|
| Size of rule | 8 bits | 8 bits | 8 bits |
| Algebra | no useful closed form used here | `next = left XOR right` | none needed |
| Predictability | visually irregular | **linear / too predictable** | structured; universality unused |
| Formal cost | low | lowest | higher if one chased universality |
| Protocol use | none | none (linearity forbids crypto use) | none (universality ≠ security) |
| Verdict | reject | reject | reject |

Universality of Rule 110 is computational, not cryptographic, not
Byzantine, and not a reason to enlarge a UTXO protocol.

All three rules have bit 0 clear, so the all-zero ring is a fixed point.
All-ones maps to all-zero in one step for all three. Alternating
`10101010` is a Rule 30 fixed point and is annihilated by Rule 90. A
single 1 under Rule 90 on an 8-cell ring splits, wraps, and dies in 4
steps. These are model facts, not security features.

---

## 6. Complexity comparison

Scores are for the **whole RIGOR system** after adding CA, not for CA
in isolation. “CA is a small program” is not the test.

| Metric | NO_CA (today) | CA_ONLY | CA+SHA256 | CA+OpuHash | CA+transition | CA+commitment |
|--------|---------------|---------|-----------|------------|---------------|---------------|
| Consensus complexity | low, frozen | **higher** (new object) | higher | higher | **much higher** | higher |
| Implementation | current PoW/state | new impls | new + hash | new + OpuHash | new state machine | new encoding |
| Verification cost | OpuHash as now | extra | extra | extra | extra per block | extra |
| Mining cost | OpuHash grind | extra or undefined | extra | extra | n/a | n/a |
| ASIC / GPU / CPU | OpuHash as designed | new circuit | new circuit | new circuit | n/a | n/a |
| Formalization | existing cone | toy theorems | toy + hash | toy + OpuHash | invented model | toy |
| Fuzzability | existing targets | easy, low value | easy, low value | easy, low value | easy, low value | easy, low value |
| Determinism | already required | yes | yes | yes | only if frozen rule | yes |
| Attack surface | current | **new** | new | new | new | new (linear Rule 90) |
| Serialization | current | new | new | new | new | new |
| Backward compatibility | n/a | hard fork | hard fork | hard fork | hard fork | hard fork |
| Maintenance | current | ongoing | ongoing | ongoing | ongoing | ongoing |
| Actual security benefit | baseline | **none** | none | none | none | none |

`NO_CA` is the unique row that keeps the system as simple as it is.
Every CA column increases complexity without a compensating security or
formal-closure gain.

---

## 7. What was tested

Isolated research only. Not wired to production proofs, fuzz, or miners.

| Artifact | Role |
|----------|------|
| `formal_verification/research/CAPrototype.v` | cyclic ECA; `ca_state` / `ca_step` / `ca_iter`; determinism; length bound; Rule 90 cell = XOR; `rule90_linear`; 8-cell goldens |
| `formal_verification/research/ca_research.py` | same model; goldens; all-zero / all-one / alt / single-cell; 256×256 Rule 90 linearity; repeat-execution; microbench vs SHA256 / SHA256d |

Commands (research dir):

```
coqc -q CAPrototype.v
python3 ca_research.py
```

Results:

```
FORMAL_PROOF     = TOY_ONLY
  ca_deterministic          Qed   (definitional)
  ca_iter_length_preserved  Qed   (model bound)
  rule90_linear             Qed   (forall equal-length rings)
  golden_*                  Qed   (vm_compute, match Python)
  no RIGOR consensus theorem
  no Axiom / Admitted / Parameter
  not in formal_verification/coq/_CoqProject

FUZZ             = RESEARCH_PASS
  model self-consistency only
  no protocol counterexample (no protocol object)

DIFFERENTIAL     = RESEARCH_PASS
  Coq vm_compute goldens = Python goldens
  no extraction; no C++ production twin
  CA_DIFFERENTIAL = not a production gate

BENCHMARK        = RESEARCH_ONLY
  CPython 256-cell × 256-step ~ 32 Hz
  OpenSSL SHA256 ~ 8e5 Hz, SHA256d ~ 4.6e5 Hz
  numbers are language-mismatched; they do not justify CA
  OpuHash (64 KiB ChaCha20 + 256 mix rounds) was not re-timed
```

No production `rigor_ca` libFuzzer target was added. Expanding fuzz
after `CA_REJECT` would be inventory, not closure.

---

## 8. Scores

Scale: 0 none, 1 weak, 2 moderate, 3 strong. Scored as **value to RIGOR**,
not as elegance of the CA.

| Axis | Score | Note |
|------|------:|------|
| SIMPLICITY | 0 | Adding CA makes the total system less simple |
| SECURITY | 0 | No new security property |
| ROBUSTNESS | 1 | The toy is robust; the protocol would be less so |
| FORMALIZABILITY | 1 | Easy toy; useless for live obligations |
| FUZZABILITY | 1 | Easy to fuzz; fuzz cannot find a protocol bug that is not there |
| PERFORMANCE | 1 | Extra cost, no gain |
| MAINTAINABILITY | 0 | Permanent extra surface |
| PROTOCOL_VALUE | 0 | No real problem solved |

```
CA_NET_VALUE = 0
```

`SECURITY = 0` and `PROTOCOL_VALUE = 0` dominate. High toy-formal scores
would not change the verdict.

---

## 9. Q1–Q9

**Q1.** Does CA solve a real current RIGOR problem?  
**No.**

**Q2.** Is that problem already solved by SHA256 / OpuHash / Coq / existing
consensus?  
**There is no CA-shaped problem.** Hashing, PoW, and state transition
already exist.

**Q3.** Does CA reduce complexity?  
**No. It increases it.**

**Q4.** Can CA form a meaningful Coq theorem?  
**Not a RIGOR one.** `rule90_linear` is meaningful mathematics and an
argument *against* crypto use.

**Q5.** Can fuzz find a real counterexample, rather than more tests?  
**Not of the protocol.** The harness can only check the toy.

**Q6.** Does CA add a consensus attack surface?  
**Yes, if adopted.** Indexing, boundaries, iteration counts, Rule 90
linearity.

**Q7.** Does CA need frozen production parameters to change?  
**Adoption would.** This task did not change them.

**Q8.** Is the long-term maintenance cost worth it?  
**No.**

**Q9.** If CA is deleted tomorrow, does RIGOR lose a real security
property?  
**No.** There is no production CA to delete.

---

## 10. Priority vs existing formalization

Same engineering time is better spent on work that already names a
missing proof:

```
Classifier refinement          (∀ C++ = Coq still open)
Sighash preimage refinement    (SHA256d / Ed25519 VST open)
SHA256 differential refinement
ConnectBlock supply refinement
Reorg issuance refinement
OpuHash / PoW pipeline closure
ASERT closure
GBT / miner copy-bytes
```

```
CA_PRIORITY = BELOW_EXISTING_FORMAL_CLOSURE
```

A CA workstream would interrupt that cone for a decorative primitive.

---

## 11. Next stage

**None.** Do not open a follow-up CA protocol TASK.

```
CA adoption           = NO
production changes    = NONE
CA frozen as unused   = YES
```

Optional later (not a P0, not this tree’s protocol cone): an explorer
animation that is explicitly non-consensus. No header field, no GBT
key, no P2P message, no ConnectBlock predicate.

---

## 12. What remains frozen

Unchanged by this task:

```
production consensus C++
Genesis / Genesis hash
W = 32
T
RES reward schedule
ORU subsidy schedule
RES/ORU halving boundary
maturity
OpuHash (64 KiB / 256 rounds / ChaCha20)
ASERT
block header
GBT
coinbase consensus encoding
Script VM (still absent)
P2P consensus semantics
sighash
Ed25519 ownership
ConnectBlock production semantics
canonical mint encoding
all other formal protocol parameters
```

```
PRODUCTION_MODIFIED = NO
```

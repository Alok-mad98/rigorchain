# Formalization Frontier

Where this package's formal verification currently stops, what it
would take to push each boundary further, and where the cost stops
being worth paying.

Written after the P0-130 independent adversarial audit and the P0-131
repair. Every "not proved" below is a real gap, not a disclaimer.

## 0. What is actually proved today

| Layer | Proved | Not proved |
|---|---|---|
| Domain models (UTXO, ORU, RES, Script class, PoW, ASERT) | Domain invariants; dual-supply rewind; most-work finite-tip selection on `GetBlockProof`; coinbase/RES-mint depth 100; script spendability by class + abstract `sig_ok`; ORU fold = n·128 | That the models match the C++ rules; k-deep / honest-majority (OP-R1); Script VM / Ed25519 |
| Unified kernel | `kernel_sound`; chain induction (2.1) | Reorg k-deep; C++ `ConnectBlock` |
| Concrete / executable | `executable_step_preserves_invariants` | — |
| Runtime machine | `machine_step_refines_executable_step` over a 63-bit checked-word domain, with partiality witnessed | That OCaml `int` behaves like the modelled word |
| Extraction interface | `kernel_call_boundary` is a bijection | That `ExtractedKernel.ml` computes the Coq function |
| Checker contract | `abstract_checker` decides validity, sound and complete | That the shipped checker binary is this function |
| Compiler | — | Everything. `ocamlopt` is trusted |
| Cryptography | `H` is uninterpreted; non-claims are theorems | Nothing about SHA3/Ed25519 security |

The honest summary: **the model is verified; the path from the model
to a running binary is declared, not verified.** That path is the
frontier.

## RFC-020 re-read — what got cheaper, what did not

RFC-020 replaced RFC-019 halving + lottery with a flat 128 ORU
subsidy and a block-local RES predicate
`R · (2²⁵+h)² ≤ B << 40`. That changes the *issuance* cone. It
does not change PoW, ASERT, the codec, extraction, or ConnectBlock.

| Remaining item | Did RFC-020 cheapen it? | Status |
|---|---|---|
| RES rewind / replay-equality | **Yes — weeks, not months.** No 32-block window, no σ, no payment lag | **DONE** (`res_reorg_rewind_eq`, M99–M101) |
| ORU cumulative closed form | **Yes.** Halving fold collapses to `n · 128` | **DONE** (`oru_subsidy_fold_closed`, M107) |
| ORU dual-supply rewind | Not uniquely. Subsidy was already height-only. Cheap because the RES twin exists | **DONE** (`oru_reorg_rewind_eq`, M105–M106) |
| Most-work two-tip selection | No. Fork choice never read the subsidy schedule | **DONE** (`more_work_wins`, M102–M104). Weight is `GetBlockProof` (Phase A). Finite-tip `select_tip` is Phase B (M113–M116) |
| k-deep / honest-majority (OP-R1) | **No.** Confirmation depth is a work race, not an issuance identity. Once a prefix is frozen, ORU/RES now follow for free — that is a corollary, not a cheaper race | **OPEN**, still 2–3 months, new workstream |
| 2.4 0.013% ASERT envelope | No | **Stop.** Needs `coq-interval`; eliminates no split |
| 1.3 codec differential | No | **DONE** |
| 3.1 CertiCoq / 3.2 ConnectBlock | No | 3.1 ENG 4.9 (listed TCB, not Level 5). 3.2 still do not start |

What can still be pushed *inside this cone* after this re-read:
nothing cheap that RFC-020 newly unlocked. The issuance half of 2.3
is closed. k-deep is still the next *2.3* line, and RFC-020 is not
a reason to start it.

Separate from RFC-020: four in-cone rule gaps were worth adding
without touching OP-R1 — `GetBlockProof`, multi-tip max-work,
coinbase/RES maturity + spend-side `MAX_MONEY`, and script
spendability (not a VM). All four landed (M108–M128). Design,
non-goals, and steps: `release/FrontierExpansionPlan.md`. Dust
stays policy.

Do not revive the archived lottery cone to "complete" 2.3.

---

## Tier 1 — weeks, cheap, do these first

Marginal cost is low because the infrastructure already exists.

### 1.1 Close the mutant matrix to 1:1 on Category A — DONE

The starting point was 50 mutants against 65 Category A theorems, and
the names next to `run_mutation` had never been checked against what
the mutant actually broke. Re-measuring with an isolating harness put
real coverage at 43.

What was built: `mutation/lib.sh` now runs an isolation loop. It
compiles, reads the first failing proof out of the log with
`mutation/resolve_failure.py`, admits it with `mutation/admit_proof.py`,
and repeats until the declared theorem itself fails or the module
builds. A mutant is only credited with a kill it can be shown to make.
`mutation/kill_set.sh` reports the full ground-truth kill set.
`scripts/check_mutation_coverage.py` recomputes
`{A theorems} \ {theorems a mutant is known to kill}` and fails when
anything is left, accepting a theorem without a mutant only when its
proof is verified to be pure forwarding and every lemma at the end of
the chain carries a mutant of its own.

Result: 75 mutants, 70 Category A theorems, 67 killed directly and 3
covered by checked deferral chains. Wired into `run_kernel_critical.sh`.

### 1.2 Remove the remaining mutation-only switches — DONE

`kernel_step_unique`, `executable_step_unique`, `runtime_step_unique`
and `concrete_oru_bypass` were constants that existed so a mutant
could flip them, along with the `*_alt_of` functions and `*_alt`
constructors they gated.

All of it is gone. Each apply-relation now has a single constructor
built from the step function, and `kernel_apply_is_graph`,
`executable_apply_is_graph`, `runtime_apply_is_graph` and
`machine_step_is_graph` state that the relation is exactly that
function's graph. Determinism follows from the graph theorem instead
of from a constant, so `machine_step_deterministic` and
`script_step_deterministic` moved from A to C: they are now
consequences of function-ness, and saying so is more honest than
counting them as invariants. M21, M25, M29, M32, M35 and M40 were
retargeted onto real kernel rules.

`scripts/check_no_mutation_switches.py` fails CI if the pattern comes
back — a nullary constant used only as a constructor guard or as an
additive zero.

### 1.3 Differential testing of the extracted codec — DONE

The largest unverified step is Coq → OCaml → binary. It cannot be
*proved* cheaply, but it can be *tested* hard.

Delivered as `fuzz/codec_differential`. The plan above said QuickChick;
that was dropped. QuickChick generates *Coq* values and would have
compared the model against itself, which tests the generator. The
harness instead generates *bytes* and compares the extracted decoder
against the production `CDataStream >> CBlock` path, because the byte
string is the only object both sides agree exists.

```
bash fuzz/codec_differential/run.sh
```

- `ExtractCodec.v` extracts `decode_block_from`, `decode_transaction_from`
  and the encoders to OCaml. No `Extract Constant`, and deliberately no
  `ExtrOcamlNatInt`: mapping `nat` onto machine `int` hands arithmetic to
  OCaml, and whether that hand-off is faithful is part of what is being
  probed. The extracted decoder adds no arithmetic axioms. This is only
  affordable because of 2.2b.
- The C++ side drives the real deserialiser, not a transcription of it.
  `DecodeHexBlk` is not called: it collapses "decoded exactly" and
  "decoded with bytes to spare" into one `bool`, and that distinction is
  the subject of divergence 1.
- Inputs: the 26 `tests/serialization_vectors` cases, CompactSize
  boundary probes, structured random transactions and blocks, and byte
  mutations of the corpus biased towards length fields. Seeded, so a
  failure reproduces from the case id.

**It found a third model/C++ divergence.** `UnserializeTransaction`
re-reads vin and vout only when the flag byte following an empty vin is
nonzero, so the ten bytes `version || 00 || 00 || locktime` deserialise
to a transaction with no inputs and no outputs — and C++ re-serialises
them unchanged, so on that side it is canonical.
`decode_transaction_from` requires the flag to be exactly 1 and rejects.
1,868–1,964 hits per 134,065 inputs, across three seeds, and every
single divergence was this one shape.

`WireCodec.v` now carries the characterisation rather than the bare
observation: `strict_implies_cpp_tx`,
`cpp_divergence_is_exactly_empty_tx` (everything C++ accepts and the
model rejects has no inputs, no outputs, no witness),
`cpp_divergence_excluded_by_transaction_ok` (all of it fails
`transaction_ok`, which requires a nonempty vin), and
`cpp_divergence_nonvacuous` (the ten bytes, by evaluation). So the gap
is real but confined to transactions the consensus layer discards
anyway — a bound that holds *because* `transaction_ok` demands a
nonempty vin, and that would stop holding if that were relaxed.

`compare.py` fails on any divergence outside the declared set, so the
set cannot grow silently. It is meant to be expensive to widen: a fourth
divergence carries none of the guarantees the first three do.

This does **not** raise the maturity level. Testing is not proof and the
package should not pretend otherwise. What changed is that the
model/C++ gap is now falsifiable instead of asserted, and the harness
immediately grew the known divergence set from two to three.

Not covered: `MAX_SIZE` (divergence 2) stays proof-only. Unmasking it
needs a count above 33,554,432 backed by enough bytes to decode, so
roughly a 33 MB input, and the extracted decoder builds unary `nat` list
indices that will not survive that.

### 1.4 Script classifier totality and partition — DONE

The starting point was worse than "incomplete". `classify_total` read
`exists t, classify_script s = t`, witnessed by `classify_script s`
and closed by `reflexivity`; `classify_type_total` said the same at
the asset level; `classify_authority_free` read
`classify_asset s = classify_asset s`; `classify_deterministic` read
`classify_script s = classify_script s`. Four statements that hold of
any function whatsoever, including one that answers `T_MINT` for every
script.

Totality is only informative with a range, so both directions are now
proved. `classify_total` gives range membership in the six declared
types and `classify_range_exact` exhibits a witness script for each,
which rules out the degenerate classifier that answers `T_NONSTANDARD`
everywhere. `classify_type_total` does the same over the four asset
classes.

Partition needed the same care. Stated over the fibres of
`classify_script` it is automatic, because the classifier is a
function. The content is one level down, in the recognizers: the
ladder consults mint, then converge, then null-data, and if those
shapes could overlap the ORDER of the ladder would be deciding the
class. `mint_first_byte`, `converge_first_byte` and
`null_data_first_byte` pin each shape's first byte; `mint_not_converge`,
`mint_not_null_data` and `converge_not_null_data` show the three bytes
differ; `shape_hits_le_one` counts them; `ladder_order_immaterial` and
`no_shape_is_nonstandard` close the partition. The witness step is the
deliberate exception and `witness_ladder_terminal` says so.

Authority-freedom now says what it was meant to say: `mint_script_of p`
is a mint script for every 20-byte `p`, so no key and no allowlisted
address moves a script between classes. `classify_deterministic` was
deleted; `classify_extensional` is the version with content.

The A/B line was drawn by what a mutant can kill. The range results,
the three disjointness results and `mint_script_of_is_mint` are
Category A with mutants M70–M76. `shape_hits_le_one`,
`mint_shape_classifies`, `classify_payload_independent` and
`classify_authority_free` forward to those, so no mutation of any
definition can make their proofs fail; they are Category B with a
pointer to where the content is.

`check_script_sound` and the `KI_Script` branch of the kernel
specification carried the tautology bundle into the kernel's own
contract. They now carry the asset range and the hit count.

---

## Tier 2 — months, high value per unit cost

### 2.1 Chain-level induction — DONE

Every consensus theorem here used to be about a single step. The
property users care about is about a chain. Two modules close the
kernel half of that gap: `Kernel/Chain/ChainTransition.v` for the
abstract layer and `Kernel/Chain/ChainRefinement.v` for the stack
above it.

The load-bearing decision was where acceptability gets evaluated, and
it is worth stating plainly because the wrong choice would have been
invisible. `input_ok s i` is not a property of `i`: it reads
`oru_stock (ks_oru s)` and `res_cap (ks_res s)`, so what a block may
spend depends on what the blocks before it did. Writing the chain
hypothesis as `Forall (input_ok s0) is` would have checked every block
against the state at genesis — far easier to satisfy, and false of any
real chain. `chain_ok` walks the running state instead. M77 makes
exactly that mistake and `kernel_chain_is_graph` stops proving, so the
choice is now defended by a mutant rather than by a comment.

On the abstract layer, `kernel_chain_preserves_invariants` carries the
six invariants across a run of arbitrary length by induction on the
run, and `kernel_chain_states_invariant` strengthens that to every
intermediate state rather than the tip alone, which is what a node
actually serves. `reachable_preserves_invariants` restates it over
reachability. Conservation becomes a real accounting result once it is
about a chain: `chain_oru_conservation` and `chain_no_hidden_creation`
say the tip stock differs from the start stock by exactly the summed
declared flows, so a hidden per-block creation of any size fails once
the chain is long enough. `chain_res_cap_fixed` rules out the chain
that raises its own ceiling block by block — a single step cannot see
that, since it respects whatever cap it is handed.

On the refinement stack, `concrete_chain_commute` and
`executable_chain_commute` iterate the single-step commutations, and
`machine_chain_refines_executable_chain` gives the simulation over
bounded words with partiality carried through: it does not claim the
machine finishes the run, it claims a run the machine does finish is
the run the model computes. `machine_chain_sound` composes the four
into the sentence the package could not previously say — a completed
machine run decodes, projects onto the abstract run, and satisfies the
consensus invariants there.

Vacuity was the obvious way for all of this to be worthless, so both
directions are proved rather than assumed. `chain_ok_inhabited` and
`chain_ok_inhabited_with_issuance` give acceptable runs of every
length, including ones that move money;
`machine_chain_completes_on_idle` does the same for the machine guard,
so `machine_chain_sound` is non-empty at every chain length and not
only for the empty chain. In the other direction
`chain_ok_rejects_overspend` and `machine_chain_refinement_is_partial`
show the hypotheses reject things. The sharpest of these is the pair
`chain_ok_rejects_overspend` / `chain_ok_accepts_funded_spend`: the
identical input is refused in one and accepted in the other, and the
only difference is the block before it. That is the property a
`Forall (input_ok s0) is` formulation would have silently lost.

The A/B line follows 1.4: A is what a mutant can kill. Twelve
statements qualify and M77–M87 kill them, with
`reachable_preserves_invariants` the one deferral whose components the
coverage gate checks. The forwarding results and the non-vacuity
witnesses are Category B, matching the existing precedent of
`machine_refinement_is_partial` and `checker_is_not_constant`. M88 and
M89 cover two Category B witnesses anyway, since they were cheap and
they are what an auditor would attack first.

Cost: as estimated. Benefit: this was the single largest gap between
what is proved and what "the consensus rules are correct" would mean,
for the kernel state. It does NOT close reorganisation, fork choice,
or block relay, and `Audit/KernelNonClaims.v` still says so.

### 2.2 Byte-level serialization — DONE

`decode_encode_identity` is over an abstract list. Real consensus bugs
live in the byte encoding — non-canonical varints, length prefixes,
malleability.

Method: model the wire format over `list byte`, prove
`decode (encode x) = Some x` **and** the canonicity direction
`decode b = Some x -> encode x = b`, which is what rules out
malleability. The second direction is the hard and valuable one.

Cost: 1–2 months.

**Result.** 29 theorems in `consensus_model/BlockCodec.v` and the new
`consensus_model/WireCodec.v`, all `Qed`, all `Closed under the global
context`. The block layer had the roundtrip direction only; the
canonicity direction is now closed by `encode_decode_block_from`,
`encode_decode_block`, `block_encoding_injective` and
`encode_block_injective`, none of which assume `block_ok` — they
constrain the bytes, which is all a node has on arrival.

Four codec-layer corpus cases became theorems rather than test
expectations: truncation (`decode_header_needs_80`), non-canonical
transaction count (`decode_block_rejects_noncanonical_tx_count`),
superfluous witness records (`decode_rejects_superfluous_witness`) and
trailing bytes (`decode_block_rejects_trailing_bytes`). Non-vacuity is
witnessed rather than assumed: `block_ok` is inhabited, the decoder
accepts something and rejects something, and the encoder is not
constant.

Two findings came out of it.

`block_ok_of_decode` was on the plan and turned out to be **false**.
`decode_does_not_imply_block_ok` proves it: `transaction_ok` requires a
nonempty output list, `decode_legacy_tail` accepts an output count of
zero. `block_ok` therefore has to keep being carried as a hypothesis.
The header layer does satisfy its predicate (`header_ok_of_decode`).

The codec model **does not evaluate**. `two16`/`two32`/`two64` are `nat`
powers of 256 and amounts go through `Z.to_nat` into unary `nat`, so
`vm_compute` overflows above roughly `10^5` and a real coinbase amount
of `5 * 10^9` satoshi is out of reach. Every proof here is written to
route around this through the ordering lemmas instead of computation,
which works, but it blocks 1.3: differential testing of an extracted
decoder against C++ on the real corpus is not currently possible.
Fixing it means moving the codec's numeric representation off unary
`nat`, and that should be scheduled before 1.3, not after.

The C++ boundary is modelled explicitly rather than assumed away.
`WireCodec.v` carries `decode_block_wire`, a mirror of `DecodeHexBlk`'s
prefix semantics, and `wire_divergence_is_exactly_trailing` proves the
two decoders differ on precisely the inputs with bytes left over and
nowhere else. `wire_is_malleable` exhibits the malleability that C++
admits and `block_encoding_injective` rules out for the strict decoder.
The `MAX_SIZE` divergence runs the other way and is shown unreachable at
the block layer by `max_size_divergence_masked_at_block_level`. That
these two Coq functions mirror the C++ ones is a TCB assumption, stated
as such, and pinned by the frozen SHA-256 of `core_read.cpp` and
`serialize.h` in the gate.

The codec cone was previously outside that gate — neither compiled by it
nor listed in its theorem set. It is inside now, and the gate's
theorem-name matching was anchored, because `Theorem encode_decode_block`
is a prefix of `Theorem encode_decode_block_from` and the old
`str.split` matching would have checked the wrong proof.

### 2.2b Codec numerics off unary nat — DONE

2.2 left the codec proved but not runnable, which is a real gap: a model
that cannot be evaluated cannot be compared against C++, and its
non-vacuity witnesses have to be hand-built from values small enough to
avoid the arithmetic. Wire scalars were `nat`. A `nat` of `5 * 10^9`,
which is one 50 BTC output, is a five-billion-node term, and `two32` /
`two64` were `nat` powers of 256 that could not be built at all.

Wire scalars are now `N`. Bytes stay `nat` — they are below 256 and
changing them would have pulled `bytes32`, `UTXOState`, `ValidationModel`
and `KernelInputV2` into the refactor for no gain. What moved is the
scalar carried by a fixed-width little-endian field, the CompactSize
count, `op_index`, and the `two16` / `two32` / `two64` bounds. `Z` record
fields are unchanged; conversion happens at the record boundary through
`Z.to_N` / `Z.of_N`.

A count read off the wire is `N` and can be up to `2^64`. Turning one
into a unary index would have reintroduced the whole problem, so
`take_n_N` and `decode_vec_N` check the count against the remaining input
length first. This is not a behaviour change and is not asserted to be
one: `take_n_N_spec` and `decode_vec_N_spec` prove each guarded decoder
*equal* to its unguarded counterpart, on the grounds that a count larger
than the remaining length was already going to fail. Establishing that
needed a new fact per element type — every element costs at least one
byte — which is `dec_consumes` in the vector section.

Results:

- All 143 proof obligations across `ByteModel`, `CompactSizeModel`,
  `TransactionCodec`, `BlockCodec` and `WireCodec` re-established. No
  theorem was weakened or dropped.
- `codec_runs_on_full_range_values`: a block with a 50 BTC output and
  full-range `op_index` and `sequence` round-trips, proved by evaluation.
- `oversized_count_rejected_by_evaluation`: a claimed transaction count
  of `2^32 - 1` is rejected by evaluation instead of diverging.
- `take_n_N_spec`, `decode_vec_N_spec`: the guards are semantics-
  preserving, so every canonicality and rejection theorem from 2.2 holds
  of the decoder that actually runs.
- The witness machinery from 2.2 that existed only to dodge unary
  arithmetic — `small_lt_two32`, `small_lt_two64`, the one-conjunct-at-a-
  time splitting that avoided a stack overflow in `witness_tx_ok` —
  collapsed to `lia`.
- Four new theorems added to the gate.

The blast radius stayed inside the codec cone plus `op_index` in
`TransactionModel`. `SerializationBridge`, `KernelInputConstruction`,
`ValidationPipeline` and the rest of `consensus_model` needed no edits.

This unblocked 1.3, which has since been done and immediately found a
third model/C++ divergence.

### 2.3 Reorg and fork-choice safety — PART (RFC-020 issuance DONE)

Prove that the invariant survives a reorganization: rolling back `n`
blocks and applying a different branch lands in a state satisfying the
same invariant, and that fork choice is a total order.

RFC-020 closed the RES half. A mint is a function of the confirming
block’s own body (`R`, `B`, `h`). There is no 32-block candidate
window, no σ, and no one-block payment lag. `Kernel/Chain/ReorgReplay.v`
proves the rewind, not a definitional drop of the orphan argument:

- `res_reorg_rewind_eq` — disconnect orphans, then apply the new
  suffix, equals never applying the orphans. M100.
- `res_reorg_surviving_pool` — surviving `res_pool` is start + prefix
  + suffix; orphans are not a summand. M101.
- `res_reorg_orphans_change_pool` — a positive orphan mint is visible
  if it is not disconnected, so the rewind is not `x = x`.
- `res_reorg_replay_eq` — same-height special case on `res_tx` lists,
  now via the rewind rather than by ignoring the argument. M99.

Category-C lottery modules stay in archive; they are not deleted.

The first ORU / PoW slice is also in. `Kernel/Chain/ForkChoice.v`
is a finite-tip most-work rule, not a longest-chain rule:

- `chain_work_app` — accumulated work is the sum of the increments.
  M102.
- `more_work_wins` — the heavier suffix is selected, regardless of
  length. M103.
- `equal_work_keeps_active` — equal work keeps the already-active
  suffix. M104.
- `longer_need_not_win` — one block of work 3 beats two of work 1,
  so the rule is not `x = x` for “longest wins”.

Work increments are `GetBlockProof` (`~2^256 / (target+1)` after
SetCompact guards): `block_proof_eq_div` (M108) and
`chain_work_uses_block_proof` (M112). Phase A of
`release/FrontierExpansionPlan.md` is done. Among a finite set of
suffixes, `select_tip` picks a maximal-work tip and a tie keeps
the active one (`select_tip_maximal` / `select_tip_keeps_active` /
`select_tip_binary` / `select_tip_total`, M113–M116). Phase B is
done. Not `arith_uint256` bit-exact, not `ActivateBestChain`.
Common-prefix work cannot flip the verdict (`prefix_work_cancels`).

Phase C is coinbase / RES-mint depth 100 and spend-side
`MAX_MONEY` (`immature_coinbase_rejected` et al., M117–M122).
Phase D is spendability by script class with abstract `sig_ok`
(`converge_unspendable` / `witness_requires_sig_ok` et al.,
M123–M128): converge / null-data / nonstandard are unspendable;
witness v0 / v1 and mint require the abstract check. Not a VM,
not Ed25519 unforgeability, not hooked into `valid_tx`.

The ORU twin is also in: `oru_reorg_rewind_eq` / `oru_reorg_surviving_pool`
(M105–M106), and the RFC-020 closed form `oru_subsidy_fold_closed`
(M107) — folding heights 0..n is `n · 128`, which is the identity
the C++ observational cumulative now uses.

What remains is the expensive half of OP-R1: a k-deep common-prefix
bound under an honest-majority assumption, and C++
`ActivateBestChain`. RFC-020 does not shrink that race. OP-R1 stays
OPEN.

Cost of what is left: still 2–3 months on the k-deep / honesty side.

### 2.4 ASERT numeric error bound — DONE, and smaller than it looked

The entry above asked for two things. The second one was already
done: `factor_range`, `factor_mono`, `clamp_to_prev_bounds` and
`asert_next_wf` in `proofs/Pow_Correctness/ASERTCorrectness.v` already
proved the factor sits in `[65536, 131072)` and that the clamps land
the target in `[1, pow_limit]` with no hypothesis on the timestamps.
Reading the roadmap entry rather than the file would have produced a
second copy of all of it; the first draft of this work did exactly
that before the duplication was noticed.

What was actually missing was everything about the *intermediate*
polynomial, which is where the C++ does two unchecked casts:

```cpp
const __int128 poly = P1*e + P2*e*e + P3*e*e*e + (1<<47);
const uint64_t factor = static_cast<uint64_t>(poly >> 48)
                      + static_cast<uint64_t>(ASERT_RADIX);
```

Three Category A theorems, all with `Print Assumptions` closed:

- `asert_poly_pos` — the cubic is strictly positive on its whole
  domain, so the unsigned cast cannot wrap. A negative polynomial
  would not produce a small target, it would produce an enormous one,
  and nothing downstream flags it.
- `asert_poly_fits_uint64` — and it stays under `2^64`, clearing that
  ceiling by under one part in 10^5 at the top of the domain.
- `factor_wrap_continuous` — `factor 65536 = 2 * factor 0`, exactly.
  The exponent is split into a shift count and a remainder; this is
  the statement that the two pieces agree where they meet, so the
  piecewise target has no jump as the remainder wraps.

Plus `asert_poly_exceeds_int64` in Category C: at the top of the
domain the polynomial exceeds `2^63`. That is a constraint on
implementations rather than an invariant of the model, and it records
that the `__int128` promotion is load-bearing rather than defensive.
An `int64_t` evaluation would be signed overflow, it would need a
remainder near 65535 to reach it, and it would therefore survive
ordinary testing and fire on the first sufficiently drifted timestamp.

**Not proved: the 0.013% bound itself.** Measured by exhaustive
evaluation outside Coq the worst relative deviation from
`65536 * 2^(e/65536)` is 0.0117% at e = 57176, so the figure quoted for
aserti3-2d holds here — as a measurement, not a theorem. Proving it
needs a certified enclosure of `2^x` over a continuum, which means
coq-interval; that is not in this toolchain and the stdlib reals give
no tight envelope alone. Monotonicity and the exact endpoints do not
pin the interior, and evaluation cannot reach it either: a per-point
integer certificate is `a^65536 <= 2^e * 65536^65536`, comparing
numbers over a million bits wide, at 65536 points.

That gap is worth stating precisely rather than apologising for. The
deviation from `2^x` is a property of the approximation, and every
node evaluates the same cubic — the polynomial *is* the rule, not an
implementation of some other rule. A poor approximation would mean
difficulty tracks intended spacing slightly imprecisely; it would not
split the chain. The split risks are in the integer arithmetic around
it, and those are what the theorems above cover. Judged by the
stopping criteria in this document, the unproved half eliminates no
consensus-splitting bug class, and it is the half that costs a month.

The mutants are M90–M94 in `mutation/mutate_asert_numeric.sh`, and
running them changed the proofs. The first version evaluated the
polynomial's endpoints in lemmas of their own. Every coefficient
mutant killed those lemmas first, the isolation loop admitted them,
and both cast-safety theorems then proved against the admitted — now
wrong — values. They were killed by no mutant at all while looking
fully covered. Inlining the evaluated endpoints into the proofs that
need them fixed it. The kill sets are derived with
`MUTATION_KILL_SET_MODE=1`, not predicted.

Deriving them also surfaced a harness bug worth recording, in the same
family as 1.1. `mutation/admit_proof.py` matched `Qed.` only at the
start of a line, so for the one-line `Proof. vm_compute. reflexivity.
Qed.` form used throughout this tree it ran past the end of the proof
and took the *next* lemma's `Qed`, deleting that lemma. The mutant was
then reported as having broken elaboration rather than as having
killed anything — a harness complaint that reads like noise and hides
the result. Fixed, with both forms covered.

---

## Tier 3 — years, where cost starts to bite

### 3.1 Verified extraction and a verified compiler

This closes the largest declared gap
(`EXTRACTION_FORMAL_PROOF=source_identity_only`,
`verified_compiler_in_current_tcb = false`).

Two routes:

- **CertiCoq → CompCert.** CertiCoq compiles Gallina to C with a
  correctness proof; CompCert compiles C to assembly with a
  correctness proof. Composing them gives a proof from the Coq
  function to machine code. Cost: 6–12 months, and CertiCoq's
  supported Gallina fragment will force model rewrites.

- **`coq-of-ocaml` round-trip.** Write the runtime in OCaml, lift it
  back into Coq, prove the lifted version equals the model. Cheaper
  (3–6 months) but only covers the OCaml source, leaving `ocamlopt`
  trusted — so it does not reach level 5.

Recommendation: CertiCoq only if a genuine level-5 claim is required.
Otherwise Tier 1.3 (differential testing) buys most of the practical
assurance at a twentieth of the cost — as it did: 1.3 cost days and
turned up a divergence that two rounds of reading the C++ had missed.

**Plan written.** The gate, principles, inviolables, compile
surface, and Phase 0–8 steps are
`release/CertiCoqCompCertPlan.md`. Phase 0–8 are done (sidecar,
`PIN_VERIFY=PASS`, isolated `certicoq/surface` + Category B
`surface_check_*_eq`, 8.19 `OBSERVE_AGREE=PASS`, seven CertiCoq
Clight units, sidecar `ccomp` + named GC `PHASE6=PASS`, Phase 7
listed TCB `PHASE7=PASS` with pipeline **not** instantiated,
Phase 8 document freeze with the **narrow** sidecar sentence,
Phase 9 Level 5 attempt **Abort** — `clightgen` AST exists,
`transf_c_program` did not compute, Codegen still unproved).
This is not 3.2, not `ConnectBlock`, and not a Level 5
sentence about `rigord`. Maturity stays **4.9**. The first
artifact is the six `check_*` functions via a Coq 8.19 sidecar
(cone stays 8.15.0), boxed `Z`, CertiCoq Clight, then CompCert.
`fragment_checker.c` stays rehearsal and is observationally
stale (RFC-019 `draw_ok`). Public flags stay false.

 Closed Clight → Cminor on six SHA-pinned checkers.
TASK 3.1A recorded `COMPILER_CLOSED_ROUTE=BLOCKED_BY_TCB`.
TASK 3.1B froze `unlift` and proved a one-step Clight
`Sset`/`Sassign` simulation under `unlift_env_related`; it did
**not** Close function- or `check_oru`-level simulation
(`store_params` is ill-formed on both official
`function_entry1` and `function_entry2`). Audit:
`audit/formal_bridge/unlift_semantic_simulation_v1.md`.
This is still not Level 5.

### 3.2 C++ `ConnectBlock` refinement

TASK 3.2A (boundary audit, 2026-09-13): production IR is
`RigorConsensusKernelConnectInput`, not Coq
`UnifiedTransition.KernelInput`. Every Coq field is MISSING as a
C++ bridge. Named next theorem:
`cpp_kernelinput_of_connect_observables_refines_coq_kernelinput`.
Byte decode remains `NOT PROVED` (trailing / `MAX_SIZE`).
`CPP_KERNELINPUT_EQUIVALENCE` stays false. Audit:
`audit/formal_bridge/cpp_kernelinput_refinement_boundary_v1.md`.
Do not reopen 3.1.

TASK 3.2B (Coq-side projection, 2026-09-13): pins the **right-hand**
target `coq_connect_observables_project_to_kernelinput` from audit
record `ConnectBlockObservables` onto frozen `KernelInput`.
Projection equations Qed, Closed. C++ left half still MISSING.
`COQ_SIDE_PROJECTION = PROVED`.
`CPP_KERNELINPUT_EQUIVALENCE` / `REFINEMENT_PROVED` stay false.
Level 5 is not claimed. Do not start the C++ side. Do not reopen 3.1.

TASK 3.2C (C++ observables mapping, 2026-09-13): pins
`CppConnectBlockObservables` and candidate
`cpp_connect_observables_related` (Coq-to-Coq map Qed, Closed).
`CPP_SIDE_OBSERVABLES_MAPPING = DEFINED`. Production ConnectBlock
does not inhabit that record. First left-half hole:
`CPP_INCOMING_OUTGOING_EXTRACTION` (no ConnectBlock-scope incoming /
outgoing locals). Full 3.2A theorem still NOT PROVED. Do not reopen 3.1.

TASK 3.2D (incoming/outgoing extraction, 2026-09-13): pins
`cpp_incoming_outgoing_of_body` and
`cpp_incoming_outgoing_related` (Qed, Closed).
`CPP_INCOMING_OUTGOING_EXTRACTION = DEFINED`.
C++ `CheckTxInputs` does not inhabit the per-tx `Z` pair.
Fees / burn / mint remain unrefined. Do not reopen 3.1.

TASK 3.2E (CppTxOruFlow interface, 2026-09-13): pins
`cpp_checktxinputs_inhabits_flows` and
`flows_imply_incoming_outgoing` (Qed, Closed).
`CPP_TX_ORU_FLOW_INTERFACE = DEFINED`.
`REAL_CPP_INHABIT = NOT PROVED`. Do not reopen 3.1.

TASK 3.2F (inhabit statement, 2026-09-13): pins
`real_cpp_checktxinputs_inhabits_flows` as a [Prop]
(`REAL_CPP_INHABIT_STATEMENT = DEFINED`). That [Prop] is
**not** proved. ASSUMPTION_1 decode and ASSUMPTION_2 UTXO
stay NOT PROVED. Do not reopen 3.1.

TASK 3.2G (decode domain, 2026-09-13): pins
`STRICT_VALID_BLOCK_DOMAIN` (Defined) and
`cpp_block_decode_refines_coq_decode` (Stated, not proved).
Trailing / `MAX_SIZE` / dummy-vin divergences stay documented.
Do not reopen 3.1.

TASK 3.2H (UTXO view representation, 2026-09-13): pins
`cpp_utxo_view_related` / `cpp_coin_related` and
`cpp_utxo_representation_refines` (Stated, not proved).
`CPP_UTXO_REPRESENTATION_REFINEMENT = STATED (NOT PROVED)`.
Does not prove inhabit or decode. Do not reopen 3.1.

TASK 3.2I (named accumulators, 2026-09-13): pins
`CppNamedAccumulators` and
`cpp_named_accumulators_refines` (Stated, not proved).
`CPP_NAMED_ACCUMULATORS_REFINEMENT = STATED (NOT PROVED)`.
Fees / burn / res_mint / subsidy only. Do not reopen 3.1.

TASK 3.2J (bool flags, 2026-09-13): pins
`cpp_bool_flags_abstract_refines` (Stated, not proved).
`CPP_BOOL_FLAGS_ABSTRACTION = STATED (NOT PROVED)`.
All ten KernelInput fields now have a C++ source or
abstraction statement. Do not reopen 3.1.

TASK 3.2K (boundary freeze, 2026-09-13):
`INTERFACE_PHASE = COMPLETE`. `PROOF_PHASE = NOT STARTED`.
Roadmap: `audit/formal_bridge/CPP_KERNELINPUT_PROOF_ROADMAP_v1.md`.
Does not prove any C++ relation. Do not reopen 3.1.

TASK 3.3A (decode proof, 2026-09-13):
`CPP_BLOCK_DECODE_REFINEMENT = PARTIAL`.
Coq-side of the domain is Closed; C++ `operator>>` inhabit
remains. `REFINEMENT_PROVED` stays false. Do not start 3.3B here.

TASK 3.3A2 (operator-shift inhabit, 2026-09-13):
`remaining_operator_shift_matches_coq_decode` still **PARTIAL**.
Tight stream witness + split lemmas Closed. C++ `Unserialize`
inhabit remains. Decode stays PARTIAL. Do not start 3.3B.

TASK 3.3A3 (foundational decode residuals, 2026-09-13):
Obligations 4/2/3/1 still **PARTIAL**. Coq CAmount / header
representation Closed. C++ integer-LE / `Unserialize` inhabit
remains. Decode stays PARTIAL. Do not start 3.3B.

TASK 3.3A4 (integer/hash primitives, 2026-09-13):
2A/2B/2C/4A/4B still **PARTIAL**. Coq LE specs and MSB-clear
lemmas Closed. C++ `ser_readdata32/64` inhabit remains.
Decode stays PARTIAL. Do not start tx body or 3.3B.

TASK 3.3A5 (leaf freeze / TCB decision, 2026-09-13):
`DECODE_LEAF_RESIDUALS = FROZEN`. Route **TCB** (narrow;
no Coq `Axiom`). Main decode theorem **not** closed.
`REFINEMENT_PROVED` stays false. Do not start 3.3B.

TASK 3.3A6 (compositional lift under leaf TCB, 2026-09-13):
`DECODE_MAIN_THEOREM_UNDER_LEAF_TCB = STILL_PARTIAL`.
Coq spec composition of header / nValue / tx / CBlock
structure Closed. Leaf premises do not inhabit `tos_block`.
Remaining non-TCB: CompactSize, CScript, vector,
`UnserializeTransaction`, CBlock `SERIALIZE_METHODS`,
production-bool inhabit. Decode stays PARTIAL.
`REFINEMENT_PROVED` stays false. Do not start 3.3B.

TASK 3.3A7 (CompactSize / CScript / vector, 2026-09-13):
three residuals still **PARTIAL**. Coq `ReadCompactSize`
control flow, script = CS + `take_n`, vector = CS +
`decode_vec_N` Closed. C++ inhabit remains. 8/16-bit reads
are CompactSize sub-residuals, not new TCB.
`REFINEMENT_PROVED` stays false. Do not start tx control
flow or 3.3B.

TASK 3.3A8 (tx control flow / CBlock composition, 2026-09-13):
four residuals still **PARTIAL**. Coq `UnserializeTransaction`
control flow = `decode_transaction_from`; header field order
= `decode_header`; CBlock = header then vector of txs. C++
inhabit remains. Main theorem **STILL_PARTIAL**.
`REFINEMENT_PROVED` stays false. Do not start production-bool
inhabit or 3.3B.

TASK 3.3A9 (final decode production inhabit, 2026-09-13):
two residuals still **PARTIAL**. Coq: production-bool ≡ tight;
tight ∧ loose-projection ⇒ main. C++ `operator>>` inhabit of
`tos_block` / `osc_block` remains. Main theorem **STILL_PARTIAL**.
`REFINEMENT_PROVED` stays false. Do not start 3.3B.

TASK 3.3A10 (freeze decode boundary, 2026-09-13):
`CPP_BLOCK_DECODE_REFINEMENT = PARTIAL (FROZEN)`.
Main theorem **STILL_PARTIAL**. Open inhabit 1–9 unchanged.
`NEXT_ACTIVE = 3.3B_UTXO_REPRESENTATION`. UTXO proofs **not**
started. `REFINEMENT_PROVED` stays false.

TASK 3.3B (UTXO representation, 2026-09-13):
`CPP_UTXO_REPRESENTATION_REFINEMENT = PARTIAL`. Coq snapshot /
prefix / ORU-sum Closed. C++ `CCoinsViewCache` inhabit remains.
Decode stays FROZEN. Do not start 3.3C.
`REFINEMENT_PROVED` stays false.

Prove the shipped C++ implements the kernel model.

Method: VST (Verifiable C) or Frama-C/WP on the actual sources. This
requires the C++ to be written for verification; on an existing
codebase, expect to spend most of the effort on separation-logic
specs for data structures that were never designed for it.

Cost: 2–4 engineer-years. This is where the curve turns sharply.

### 3.3 Verified cryptographic primitives

Replace abstract `H` with a verified SHA3/Keccak implementation
(HACL*, fiat-crypto) and verified Ed25519.

Cost: 6–12 months of integration if the verified implementations are
adopted as-is; much more if they must be re-verified against this
project's calling conventions.

Note carefully: this proves the *implementation* matches the
*specification* of the hash. It does **not** prove collision
resistance, which is not a provable statement — see Tier 4.

---

## Tier 4 — where cost exceeds any plausible benefit: stop here

These are listed so that "push formalization to its limit" has a
defined limit rather than an open-ended one.

| Target | Why it is out of scope |
|---|---|
| Verifying the Coq kernel itself (MetaCoq / Coq-in-Coq) | Multi-year; removes one trusted component and adds another. `coqc` is trusted by essentially all users of proof assistants, and the marginal risk it carries is far below the risk in the C++ node |
| CPU / hardware semantics | Requires a verified processor model (e.g. a formal ISA plus silicon equivalence). Not obtainable for commodity hardware |
| Cryptographic security proofs (collision resistance, discrete log) | **Not provable.** These are complexity-theoretic assumptions, not theorems. `kernel_does_not_assume_sha3_security` is already a theorem in this package and should stay that way |
| Network / P2P correctness under adversarial scheduling | Would require a distributed-systems model with an adversarial scheduler (Ivy, TLA+, or ITrees). Cost comparable to 3.2, and the consensus kernel is not where partition bugs live |
| Verifying the build system, OS, and toolchain supply chain | Reproducible builds are engineering, not proof. Diminishing returns immediately |

---

## Stopping criteria

Do not decide "further or not" by enthusiasm. Use these:

1. **Marginal assurance per engineer-month.** Estimate, before
   starting, how many *plausible consensus-splitting bug classes* the
   work eliminates. Tier 1 and 2 items each eliminate at least one.
   Tier 4 items eliminate none that are not already dominated by a
   cheaper risk.

2. **The dominant-risk test.** If a proposed proof effort removes a
   risk that is *smaller than* a risk you are still accepting
   elsewhere, do the cheaper one first. Verifying `coqc` while
   `ConnectBlock` is unverified fails this test.

3. **The claim test.** Every completed item must change a line in
   `release/FinalClaimsBoundary.md` or `release/OpenProperties.md`. If
   it changes neither, it produced no auditable assurance.

4. **The mutation test.** Every new Category A theorem must come with
   a mutant that breaks it. A theorem no mutant can break is a theorem
   that was not worth proving.

5. **Level honesty.** Level 5 requires 3.1. Nothing in Tier 1 or 2
   reaches it, and no amount of Tier 1/2 work should be described as
   if it does.

## Recommended order

```
1.1  mutant matrix to 1:1              1 week       DONE
1.2  remove determinism switches       1 week       DONE
1.4  classifier totality               1 week       DONE
2.1  chain-level induction             1-2 months   DONE  <- was the largest single gain
2.2  byte-level serialization          1-2 months   DONE
2.2b codec numerics off unary nat      3-4 weeks    DONE  <- unblocked 1.3
1.3  differential testing              2-3 weeks    DONE  <- found divergence 3
2.4  ASERT error bound                 1 month      PART  <- casts and wrap
                                                          done; the 0.013%
                                                          bound needs
                                                          coq-interval
2.3  reorg safety                      RFC-020 issuance DONE           PART
                                       (RES+ORU rewind, n·128 fold,
                                       most-work). k-deep / honesty
                                       unchanged, 2-3 mo (OP-R1)        <- do not start
2.5  FrontierExpansionPlan             GetBlockProof DONE (M108–M112). DONE
     (in-cone; not OP-R1)              multi-tip DONE (M113–M116).
                                       maturity 100 DONE (M117–M122).
                                       script spend DONE (M123–M128)
                                       (not VM; not dust; not
                                       valid_tx hook)
--- RFC-020 does not unlock another cheap 2.x item ---
--- 2.5 landed (A–D); it does not close 2.3 ---
--- decision point: is a level-5 claim required? ---
3.1  CertiCoq + CompCert               6-12 months   ENG 4.9
     (gate: release/CertiCoqCompCertPlan.md;
      Phase 0–8 DONE; Phase 9 ABORT; Phase 10
      ABORT_LEVEL5 / PARTIAL — CPS exp without
      pipeline.v; no mem Clight; no §10.
 Flags stay false. : Clight→Cminor
      Closed on six checkers. TASK 3.1A:
      COMPILER_CLOSED_ROUTE=BLOCKED_BY_TCB
      (compile_switch Parameter). TASK 3.1B:
      unlift one-step Sset/Sassign under an
      explicit relation; store_params is stuck
      on official Clight1/Clight2 entries;
      check_oru function simulation NOT PROVED.
      TASK 3.1C: unlift_function_core
      forward_simulation = NOT_FEASIBLE_WITHIN_SCOPE
      (12 Scalls + one function_entry;
      free_list needs SimplLocals-scale inject).
      Do not read as Level 5 DONE.)
3.2  C++ refinement                    2–4 years
     TASK 3.2A: construction gap named.
     TASK 3.2B: Coq-side projection
     target relation PROVED (Closed).
     TASK 3.2C: C++ observables mapping DEFINED.
     TASK 3.2D: incoming/outgoing extraction DEFINED.
     TASK 3.2E: CppTxOruFlow interface DEFINED.
     TASK 3.2F: inhabit [Prop] DEFINED.
     TASK 3.2G: STRICT_VALID_BLOCK_DOMAIN DEFINED;
     decode relation STATED (NOT PROVED).
     TASK 3.2H: UTXO representation STATED (NOT PROVED).
     TASK 3.2I: named accumulators STATED (NOT PROVED).
     TASK 3.2J: bool flags STATED (NOT PROVED).
     TASK 3.2K: interface phase COMPLETE;
     proof phase NOT STARTED.
     TASK 3.3A: decode PARTIAL (Coq-side Closed;
     C++ operator>> remaining).
     TASK 3.3A2: operator-shift inhabit still PARTIAL
     (tight stream witness Closed; Unserialize remaining).
     TASK 3.3A3: four residuals still PARTIAL
     (CAmount/header Coq repr Closed; C++ inhabit remaining).
     TASK 3.3A4: integer/hash primitives still PARTIAL
     (LE spec + MSB-clear Closed; ser_readdata remaining).
     TASK 3.3A5: decode leaves FROZEN as TCB-classified
     reader facts; main theorem NOT CLOSED.
     TASK 3.3A6: leaf-TCB composition lift STILL_PARTIAL
     (spec composition Closed; inhabit remaining).
     TASK 3.3A7: CompactSize/script/vector Coq composition
     Closed; C++ inhabit PARTIAL; main theorem STILL_PARTIAL.
     TASK 3.3A8: tx control flow / CBlock Coq composition
     Closed; four inhabit residuals PARTIAL; main theorem
     STILL_PARTIAL.
     TASK 3.3A9: production-bool / loose operator>> Coq
     inhabit-glue Closed; C++ inhabit PARTIAL; main theorem
     STILL_PARTIAL.
     TASK 3.3A10: decode FROZEN at PARTIAL; next active 3.3B
     (`cpp_utxo_representation_refines`); UTXO proofs not
     started.
     TASK 3.3B: UTXO representation PARTIAL (snapshot/sum
     Coq Closed; C++ cache inhabit remaining). Decode stays
     FROZEN. Do not start 3.3C.
     REFINEMENT_PROVED stays false.
--- stop ---


The decision point matters. Everything above it improves the model and
its governance and is worth doing. Everything below it buys a
different *kind* of assurance — about the binary rather than the
design — at ten to a hundred times the cost per bug class. That is the
point where cost stops justifying advancement for this project.

# Codec differential (roadmap 1.3)

Runs the same bytes through the extracted Coq decoder and through the C++
one, and fails if they disagree in a way that is not already bounded by a
theorem.

```bash
bash fuzz/codec_differential/run.sh
CODEC_DIFF_SEED=7 CODEC_DIFF_MUTATIONS=4000 CODEC_DIFF_RANDOM=30000 \
  bash fuzz/codec_differential/run.sh
```

## Why this exists

Every theorem in `formal_verification/consensus_model` is about a Coq
term. The node runs a C++ binary. Two steps sit in that gap and no proof
covers either:

1. Coq model → C++ implementation. Not provable here at all; it is a
   claim about a C++ program.
2. Coq source → OCaml → machine code. Provable only with a verified
   compiler, which is out of scope.

Testing does not close either step and this harness does not raise the
maturity level. What it does is make the first step *falsifiable*: if the
model and the implementation disagree on some byte string, the harness
can find that byte string, and it did.

## Design

| Piece | Role |
|-------|------|
| `ExtractCodec.v` (in `consensus_model/`) | Extracts the decoder to OCaml. No `Extract Constant`, no `ExtrOcamlNatInt` |
| `oracle_main.ml` | Coq side: hex in, `status / leftover / re-encoding` out |
| `codec_cpp.cpp` | C++ side: same contract, driving the production `CDataStream >> CBlock` path |
| `gen_inputs.py` | Deterministic input generator, four families |
| `compare.py` | Pairs the verdicts and classifies every divergence |
| `run.sh` | All of the above |

Two decisions are load-bearing.

**`ExtrOcamlNatInt` is not used.** Mapping `nat` onto machine `int` would
hand arithmetic to OCaml, and whether that hand-off is faithful is part
of what is being probed. `nat`, `N` and `Z` all extract as Coq datatypes,
so the extracted decoder adds no arithmetic axioms. This costs about two
orders of magnitude in speed and is only affordable at all because
roadmap 2.2b moved wire scalars off unary `nat`; before that the
extracted decoder could not run on a realistic amount.

**The C++ side is not a transcription.** It deserialises through the same
`CDataStream >> CBlock` that `DecodeHexBlk` drives. A transcription would
only ever test the transcription. `DecodeHexBlk` itself is not called
because it collapses "decoded with bytes to spare" and "decoded exactly"
into one `bool`, and that distinction is the entire subject of divergence
1.

Both sides report leftover bytes separately from the status, so a single
run answers for both the strict decoder (`ok` with leftover 0) and the
wire decoder (`ok`), rather than baking the choice into the harness.

## Input families

| Prefix | What it is |
|--------|-----------|
| `seed/` | The 26 `tests/serialization_vectors` cases, unchanged |
| `cs/` | CompactSize counts on every length-class and limit boundary, canonical and non-canonical |
| `rnd/` | Structured random transactions and blocks built from the wire grammar, exploring amounts near 2^63, full-range indices, witness present and absent |
| `mut/` | Byte mutations of the seeds, biased towards length fields |

`mut/` is the family that finds things: mutating a length byte of a valid
block lands the input just past a decision boundary, which is where two
implementations that agree everywhere else disagree.

Generation is seeded, so a failure is reproducible from the case id alone.

## Result

At 134,065 inputs per seed, every divergence falls into one class, and it
is a class the harness found rather than one that was known in advance:

```
empty-vin-dummy-flag
```

`UnserializeTransaction` re-reads vin and vout only when the flag byte
following an empty vin is nonzero. A zero flag consumes neither, so the
ten bytes `version || 00 || 00 || locktime` deserialise to a transaction
with no inputs and no outputs — and C++ re-serialises them unchanged, so
on that side it is even canonical. `decode_transaction_from` requires the
flag to be exactly 1 and rejects.

The model is the stricter side. `WireCodec.v` now carries the
characterisation:

- `strict_implies_cpp_tx` — everything the model accepts, C++ accepts
- `cpp_divergence_is_exactly_empty_tx` — everything C++ accepts and the
  model rejects has no inputs, no outputs and no witness
- `cpp_divergence_excluded_by_transaction_ok` — all of it fails
  `transaction_ok`, which requires a nonempty vin
- `cpp_divergence_nonvacuous` — the ten bytes, by evaluation

So the divergence is real but confined to transactions the consensus
layer discards regardless. That is a bound, not a dismissal: it holds
because `transaction_ok` demands a nonempty vin, and it would stop
holding if that ever changed.

## The gate

`compare.py` fails on any divergence outside `DECLARED`. Do not widen
`DECLARED` without a theorem in `WireCodec.v` that bounds the new class —
the list is meant to be expensive to grow, because a fourth divergence
carries none of the guarantees the first three do.

## What this does not show

- Nothing about C++ is proved. That `codec_cpp.cpp` exercises the same
  path as the node is an assumption in the trusted base.
- Agreement on 134k inputs is not agreement. The input distribution is
  the generator's, not an adversary's.
- `MAX_SIZE` (divergence 2) is not reached here. Unmasking it needs a
  count above 33,554,432 backed by enough bytes to decode, so roughly a
  33 MB input; the extracted decoder builds unary `nat` list indices and
  will not survive that. The masking argument for it stays proof-only
  (`max_size_divergence_masked_at_block_level`).

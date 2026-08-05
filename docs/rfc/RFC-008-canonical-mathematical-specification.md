# RFC-008: Canonical Mathematical Specification

**Version:** 1.5 (Sigma-PoW-01; Sigma-CR-01 superseded, Conv-Decouple-01 + A-7 Single-Path, Leaf-Freeze-01 superseded)
**Status:** Consensus-Critical, Pre-Mainnet
**Scope:** RFC-001 ~ RFC-007 Unique Interpretation Standard for All Formulas

**This document MUST be interpreted under RFC-000-FINAL Constitutional Nomenclature**

> **v1.5 changelog (2026-08-03)**: SECURITY-001-A / A-8, codename Sigma-PoW-01.
> **§3.2, §3.2.1, §3.2.2 and §5 are rewritten and Sigma-CR-01 is superseded.** σ slot
> `[64..95]` is the parent block hash `B_{h-1}` taken verbatim; `bind_commit` no longer
> exists, the bind depth `k = 10` no longer exists, and `target_prev` is no longer a σ
> input. `target_prev` remains a PCORB mint witness field, checked against the parent
> header's `nBits` by `ConnectBlock`, so §12 and the witness layout are unaffected. The
> draw also moves one block later: the block at height `h` pays the winner of the
> convergence set committed by block `h-1`, and the `height` inside σ is `h`. The 100-byte
> total length, the field offsets, the inequality shape and `RES_PROBABILITY_TARGET` are
> all unchanged. The reason is that under the previous rule both `txid_i` and `C` were
> chosen by the producer of the paying block, so the draw could be re-rolled offline at
> about 13 ms of one CPU core per guaranteed win; every σ input is now committed by an
> already-mined block. The cross-difficulty replay domain that Sigma-CR-01 closed stays
> closed, because `B_{h-1}` is a hash over the parent header and that header contains its
> own `nBits`. Chain-invalidating; ships with the A-5/A-6/A-7 reset, pre-mainnet, no hard
> fork on a live network. See `docs/security/SECURITY_001_A8_REPORT.md`.
>
> **v1.4 changelog (2026-08-03)**: SECURITY-001-A / A-7. The witness v1 RES **script path** is
> removed; a RES output has exactly one spending path, the 2-item Ed25519 key path, whose signature
> commits to the spending transaction. The PCORB **mint** witness is reduced from eleven pushes to
> **six** — the proof fields alone. Deleted: the σ-domain Ed25519 signature (`wit[6]`) and its
> verifying key (`wit[7]`), the leaf sentinel (`wit[8]`), and both reserved placeholders (`wit[9]`,
> `wit[10]`). Consequently **§16 (Leaf-Freeze-01) is superseded** and `RFC008_ERRATUM_RESERVED_ZERO.md`
> is moot; the "11-item … unchanged" line in the v1.2 entry below no longer describes current
> consensus. σ, its 100-byte preimage, `bind_commit`, the inequality shape and `RES_PROBABILITY_TARGET`
> are all **unchanged** — σ is simply no longer signed. The signature was removed because its
> verifying key was supplied by the witness itself, so any freshly generated keypair satisfied it; the
> script path was removed because its authorization covered σ rather than the spending transaction,
> which let a broadcast claim be re-pointed by anyone. Chain-invalidating; ships with the A-5/A-6
> reset, pre-mainnet, no hard fork on a live network. See `docs/security/SECURITY_001_A7_REPORT.md`.
>
> **v1.3 changelog (2026-05-25)**: Conv-Decouple-01 (RFC-014). The RES
> eligibility inequality RHS `T` is now the immutable constitutional constant
> `RES_PROBABILITY_TARGET = 2^240`. It is NO LONGER derived from `SetCompact(target_prev)`.
> `target_prev` retains its role inside `bind_commit` (Sigma-CR-01) for replay-domain
> separation ONLY. The 100-byte σ preimage layout, witness layout, wire formats,
> and the inequality SHAPE (`H_i < T · f_i`) are bit-identical; only the *source*
> of `T` changes. Activation is genesis-only (pre-mainnet, no hard fork).
>
> **v1.2 changelog (2026-05-12)**: Adds §16 *ConvergeProofLeafScript Freeze
> (Leaf-Freeze-01)*. The RNG/PCORB witness leaf is permanently frozen to
> `OP_1` (1 byte, `0x51`). The previously-shipped 34-byte form
> `OP_1 || push32(uint256::ONE)` was implementation contamination and is
> permanently forbidden. No hard fork: mainnet is not yet live.
>
> **v1.1 changelog (2026-05-12)**: Sigma input slot `[64..95]` is now `bind_commit`
> rather than the raw `B_{h-k}`. `bind_commit` cryptographically commits the
> parent header difficulty target `target_prev` (parent `nBits`) into σ, closing
> the cross-difficulty replay vector identified in
> `audit_sigma_replay_domain_separation.md`. The 100-byte total length and
> deterministic LE serialization are preserved unchanged. No hard fork required:
> mainnet is not yet live.

---

## 0. Absolute Principle (MUST)

All mathematical expressions in this protocol MUST be interpreted ONLY according to this RFC.

Any deviation, reinterpretation, simplification, or optimization is a consensus violation.

---

## 1. Mathematical Domain Definition (Domain Separation)

### 1.1 Three Major Domains (Must Be Isolated)

1. **Sigma Domain (σ)**:
   - Hash outputs
   - Randomness
   - Selection

2. **Arithmetic Domain**:
   - Integer arithmetic
   - Comparisons
   - Multiplication

3. **Asset Domain**:
   - ORU
   - RES
   - Fees / Mint Cost (f_i)

### Mandatory Rules
- Sigma values MUST NEVER be interpreted as asset values
- Asset values MUST NEVER be used as hash input except explicitly defined
- Arithmetic MUST NEVER mix domains implicitly

---

## 2. Basic Data Types (Canonical Types)

### 2.1 Definition

```
bytes32  := exactly 32 bytes (no interpretation)
uint256  := 256-bit unsigned integer
uint512  := 512-bit unsigned integer (for multiplication only)
uint32   := 4-byte unsigned integer (little-endian encoding)
uint64   := 8-byte unsigned integer (little-endian encoding)
```

**Implementation**: All types implemented in codebase including `arith_uint512` for 512-bit arithmetic.

### Forbidden
- No floating point
- No signed integer
- No implicit casting
- No big-endian reinterpretation

---

## 3. Concatenation Rules

### 3.1 Unique Legal Concatenation

```
A || B := byte-wise concatenation
```

### 3.2 SIGMA Input (Must Be Strictly Consistent)

**Input Buffer (100 bytes EXACTLY) — Sigma-PoW-01:**
```
[0..31]   = txid_i (32 bytes)
[32..63]  = Convergence Root C (32 bytes)
[64..95]  = B_{h-1} (32 bytes)            ← Sigma-PoW-01: parent block hash, verbatim
[96..99]  = LE32(height)                  ← payout height h
```

### 3.2.1 The Seed Field (Sigma-PoW-01)

```
seed := B_{h-1}
```

Where:
- `B_{h-1}` is the hash of the parent block, exactly 32 bytes, taken verbatim from the
  `hashPrevBlock` field of the header being validated (`pindex->pprev->GetBlockHash()` in
  implementation terms).
- `txid_i` and `C` are the candidate transaction id and the convergence root **committed by
  block `h-1`**, not by the block being validated. See §5.4.
- `height` is the height of the block that pays the RES mint (`h`), serialized as
  little-endian uint32.
- `Hash256` is the canonical double-SHA-256 defined in §4.
- Concatenation `||` is byte-wise as defined in §3.1.

### Mandatory Constraints
- Total length MUST be exactly 100 bytes. Any deviation MUST cause rejection.
- The seed MUST be placed at offset 64 verbatim. Hashing it again, folding any other value
  into it, or substituting an ancestor at any depth other than 1 is a consensus violation.
- No field of the block being validated may enter σ. The producer of block `h` has no σ
  input at all.

### 3.2.2 Justification (Why the Seed Is the Parent Block Hash)

Every σ input must be expensive for a block producer to vary, because σ decides who mints
RES. Fixed inputs the producer already knows are not obstacles; the *variable* inputs are
what matter. Under the previous layout `txid_i` and `C` were both drawn from the block being
assembled, so a producer could regenerate its own convergence transaction, recompute σ, and
publish only a variant that satisfied the inequality. At `RES_PROBABILITY_TARGET` the
per-attempt probability for a minimum convergence is `2⁻¹⁶`, so a guaranteed win cost about
65 536 double-SHA-256 evaluations, roughly 13 ms of one core, against a ten-minute block
interval.

Sourcing `txid_i` and `C` from block `h-1` and seeding on that block's hash makes every
distinct σ assignment require a distinct valid block `h-1`, that is one full proof-of-work
solution. `C` and each candidate txid are committed by the parent's merkle root, hence by
its header; `B_{h-1}` is the output of the parent's proof of work and cannot be predicted
while the set is still being chosen.

The cross-difficulty replay domain that Sigma-CR-01 was written to close remains closed by
construction: `B_{h-1}` is a hash over the parent header, and that header contains its own
`nBits`, so the parent's compact target is committed more strongly than the previous explicit
fold achieved.

Superseded by SECURITY-001-A8 (Sigma-PoW-01): slot `[64..95]` previously held
`bind_commit := Hash256(B_{h-k} || LE32(target_prev))` with `k = 10`, and §3.2.1 previously
specified that derivation. `bind_commit`, the bind depth `k`, and `target_prev` as a σ input
are all deleted. `target_prev` remains a PCORB mint witness field and is still checked
against the parent header's `nBits`; it simply does not feed σ. The audit reasoning behind
the superseded Sigma-CR-01 form is recorded in `audit_sigma_replay_domain_separation.md`,
which is now a historical document.

---

## 4. Hash Function Definition (Hash256)

### 4.1 Definition

```
Hash256(x) = SHA256(SHA256(x))
```

### Forbidden
- Single SHA256
- Keccak
- Blake
- Truncated hash

---

## 5. Sigma (σ) Definition (Unique Randomness — Sigma-PoW-01)

### 5.1 Definition

```
σ_i = H_i  :=  Hash256( txid_i || C || B_{h-1} || LE32(h) )
```

`B_{h-1}` MUST be the parent block hash, verbatim. `C` and `txid_i` MUST come from the
convergence set committed by block `h-1`. `h` MUST be the payout height. See §3.2.1.

### 5.2 Semantics

```
H_i ∈ [0, 2^256 - 1]
```

### 5.3 Grinding Cost Guarantee (Sigma-PoW-01)

Every σ input other than the payout height is committed by block `h-1`: `txid_i` and `C` by
its merkle root, and `B_{h-1}` by its proof of work. Therefore:

```
one distinct σ assignment  ⇒  one distinct valid block h-1
```

that is, one full proof-of-work solution per attempt. The residual is that whoever mines
block `h-1` may discard a block whose draw outcome it dislikes, at the cost of a full block
reward — the same residual any proof-of-work lottery carries.

The parent's compact difficulty target is committed transitively, because `B_{h-1}` is a hash
over the parent header and the header contains `nBits`. Substituting a different difficulty
context therefore changes σ, which is the property Sigma-CR-01 was written to obtain.

### Forbidden
- Interpreting H_i as float
- Normalizing H_i
- Dividing H_i
- Hashing or folding `B_{h-1}` before placing it at offset 64
- Sourcing `txid_i` or `C` from the block that pays the mint
- Passing a caller-supplied seed or height to draw resolution (see §5.4)

### 5.4 Draw Timing (Sigma-PoW-01)

The draw resolved at height `h` runs over the convergence set `F` committed by block `h-1`:

```
block h-1  commits F and C  (sealed by its merkle root, then by its proof of work)
block h    pays the winner  (coinbase CVGE output commits the eligible-subset root;
                             body carries the RES mint)
```

The payout height is always `source_height + 1` and the seed is always the source block's
hash. In the implementation, `ResolveConvergenceDraw` takes only the draw set — it has no
height parameter and no seed parameter — so no caller can express resolving a set against a
context it did not come from.

Superseded by SECURITY-001-A8 (Sigma-PoW-01): §5 previously defined σ over `bind_commit` and
required `target_prev` inside it, and the block at height `h` drew over the convergence
transactions in its own body and paid the mint in that same block. The old §5.3 domain
separation guarantee, stated over tuples `(txid, C, B_{h-k}, h, T)`, is subsumed by §5.3 as
written above.

---

## 6. Core Inequality (Unique Consensus Rule)

### 6.1 Definition

```
H_i < T · f_i
```

**Conv-Decouple-01 (RFC-014) source of `T`:**

```
T := RES_PROBABILITY_TARGET = 2^240   (constitutional constant; uint256)
```

`T` is NOT a function of `target_prev`, `nBits`, ASERT state, or any header
field. It is identical across all networks (mainnet/testnet/devtest/regtest/signet/slowtest).
`target_prev` is **not** a σ input under Sigma-PoW-01; it remains a PCORB witness field
checked against the parent header by `ConnectBlock` (see §5 historical note / SECURITY-001-A8).

### 6.2 Computation Rules

**Left side:**
```
H_i → uint256
```

**Right side:**
```
T · f_i → computed in uint512
```

### Critical
Comparison MUST be exact integer comparison
NO division
NO floating point
NO scaling

---

## 7. Multiplication Rules (Critical)

### 7.1 Must Use 512-bit

```
uint512 product = (uint512)T * (uint512)f_i
```

### Forbidden
- uint256 overflow
- modulo arithmetic
- truncation

**Implementation**: Uses `arith_uint512` (512-bit) for multiplication, fully compliant with RFC-008.

---

## 8. Winner Selection (No Division)

### 8.1 Defined Order

```
i ≺ j  iff  H_i * f_j < H_j * f_i
```

### 8.2 Implementation

```
Left  = (uint512)H_i * f_j
Right = (uint512)H_j * f_i
```

### Forbidden
- H_i / f_i
- float comparison

---

## 9. Equivalence Explanation (Theory vs Implementation)

**Mathematical Expression (allowed in papers):**
```
argmin(H_i / f_i)
```

**Implementation Expression (only legal):**
```
H_i * f_j < H_j * f_i
```

### Rule
Division form MUST NEVER appear in implementation

---

## 10. Boundary Conditions (Edge Cases)

### 10.1 f_i = 0
MUST reject transaction

### 10.2 Empty Set
If F is empty:
```
C = 32 zero bytes
No winner
```

### 10.3 Single Element
If only one candidate:
```
It MUST still satisfy H_i < T · f_i
```

---

## 11. Determinism Requirements

All nodes must guarantee:
```
Same input →
Same C →
Same H_i →
Same winner
```

### Forbidden
- Random seed
- Time-based entropy
- Local variation

---

## 12. Forbidden Optimizations (Very Critical)

**CURSOR Common Errors (Must Be Hardcoded)**
- "Optimized" to float comparison
- Using double
- Early division
- Using 256-bit truncation
- Modifying concatenation order
- Automatic endian conversion
- Using library default big integers (unverified bit width)

---

## 13. Machine Verifiability

Must satisfy:
- All operations are finite
- All inputs are fixed-length
- All outputs are deterministic
- No undefined behavior

---

## 14. Final Lock (Consensus Lock — Sigma-PoW-01)

The following are immutable post-mainnet:
1. Hash input structure: **100 bytes EXACTLY** in the order
   `txid (32) || C (32) || B_{h-1} (32) || LE32(height)`.
2. Slot `[64..95]` is the **parent block hash** `B_{h-1}` taken **verbatim**
   (no inner hash; no `bind_commit`; no `target_prev` in the σ preimage).
3. Draw timing: block `h` pays the draw over the set committed by block `h-1`
   (deferred draw / SECURITY-001-A8).
4. Hash function: double SHA-256 (`Hash256(x) = SHA256(SHA256(x))`).
5. Inequality: `H_i < T · f_i` computed in `uint512`.
6. Comparison method: cross multiplication `H_i * f_j < H_j * f_i`.
7. Integer-only arithmetic. No floating point. No signed integer.
8. Endianness: little-endian for `height` (4 bytes) only among the σ inputs;
   all 32-byte fields are byte-for-byte transparent. PCORB still carries
   little-endian `target_prev` (4 bytes) as a witness field, not a σ input.

---

## 16. ConvergeProofLeafScript Freeze (Leaf-Freeze-01)

> **SUPERSEDED — SECURITY-001-A / A-7.** This entire section is no longer operative. The leaf script
> push was removed from the PCORB witness, and `ConvergeProofLeafScript()` /
> `ConvergeProofLeafHash()` were deleted from `consensus/res_proof.cpp` along with the tapleaf freeze
> test. The leaf existed only to serve the witness v1 RES **script path**, which A-7 removed: a RES
> output now has exactly one spending path, the 2-item Ed25519 key path. The script path's only
> authorization was `VerifyConvergeProof` over σ, which commits to nothing about the spending
> transaction, so a broadcast claim could be copied by anyone into their own transaction spending
> the same output.
>
> The section is retained below as the record of what was frozen and why. The prohibitions in §16.4
> — no payload-bearing leaves, no BIP341 tapleaf semantics, no MAST, no control blocks — remain
> binding as constitutional doctrine even though there is no longer a leaf to which they attach.
> Current PCORB layout (6 pushes, no leaf, no signature): `consensus/res_proof.h`.

### 16.1 Frozen Definition (MUST)

```
ConvergeProofLeafScript := OP_1
```

Canonical serialization:

```
0x51
```

Canonical size:

```
1 byte EXACTLY
```

Any leaf bytes that do not match the single byte `0x51` MUST cause validation
to fail with `SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH` in
`script/interpreter.cpp`. This is a strict consensus rule, not a policy
preference.

### 16.2 Frozen Leaf Hash

`ConvergeProofLeafHash` is the canonical double-SHA-256 (Hash256) of the
leaf bytes:

```
ConvergeProofLeafHash := Hash256( ConvergeProofLeafScript )
                      = Hash256( 0x51 )
                      = 953ccfa596a6c6d39e5980194539124fdcff116a571455a212baed811f585ee0
```

This value is byte-for-byte equal to the constant computed at compile time
from `consensus/res_proof.cpp::ConvergeProofLeafHash()`, and is pinned in
`test/res_proof_math_tests.cpp::res_proof_tapleaf_bip341_vector_and_encoding`.

### 16.3 Rationale

The leaf used by the RNG/PCORB witness path is a *sentinel*, not a script
program. Its role is exclusively domain separation inside the script-path
witness layout — it carries no payload, no MAST commitment, no tag.

The earlier implementation form

```
CScript() << OP_1 << ToByteVector(uint256::ONE)
```

(34 bytes: `0x51 || 0x20 || 32 bytes of uint256::ONE`) was implementation
contamination, traceable to an early Taproot-shaped prototype that was
never promoted to a normative specification. It was:

- **Not** specified in any RFC.
- **Violating** the RIGOR "Script as Asset" constitution by introducing
  payload-bearing tapleaf semantics resembling BIP341 / MAST.
- **Conflicting** with existing test expectations that asserted
  `leaf.size() == 1u`.
- **Not** consensus-frozen.

Leaf-Freeze-01 eliminates this contamination *before* mainnet activation.

### 16.4 Permanent Prohibitions (MUST NOT)

The following are PERMANENTLY forbidden in RIGOR consensus from v1.2 onward:

1. **Payload-bearing leaf forms.** No `OP_X || push(...)` constructions.
   The leaf MUST contain exactly one byte.
2. **Taproot / BIP341 tapleaf semantics.** No leaf version byte, no tagged
   hashing (`TapLeaf` / `TapBranch`), no control block parsing.
3. **MAST-style script commitments.** No script tree, no Merkle path, no
   branch selection.
4. **Variable / parameterised leaves.** The leaf is a single global
   constant. There is no per-transaction, per-output, or per-asset variant.
5. **Leaf-versioning soft forks.** Any future change to
   `ConvergeProofLeafScript` is a hard fork that requires a new RFC and a
   new section in this document. v1.2 is the terminal version of this
   constant for the foreseeable mainnet horizon.

### 16.5 Implementation Consumers (Informative)

The following call sites read `ConvergeProofLeafScript()` and MUST be
updated together with §16.1 if and only if a future RFC supersedes this
freeze:

- `consensus/res_proof.cpp::ConvergeProofLeafScript()` — single source of
  truth.
- `consensus/res_proof.cpp::ConvergeProofLeafHash()` — derived constant.
- `script/interpreter.cpp` — byte-equality check at the V1 RES native
  script-path entry (`SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH` on deviation).
- `wallet/wallet.cpp`, `wallet/rpc/rpcspend.cpp`, `script/sign.cpp` —
  witness-stack constructors push the leaf bytes via
  `emplace_back(leaf.begin(), leaf.end())`. They are length-agnostic and
  therefore require *no code change* under Leaf-Freeze-01; they will
  automatically emit the 1-byte form.

### 16.6 Determinism & Replay

`ConvergeProofLeafScript` is a static `const CScript`. It is initialised
exactly once at program load and its bytes are fixed for the entire
process lifetime. There is no input data, no randomness, no system clock
involvement. Replay across runs, across nodes, and across rebuilds is
byte-exact deterministic by construction.

The Hash256 in §16.2 is therefore the canonical "golden" leaf hash for
the entire protocol lifetime; any deviation in any consumer implies
either:

(a) tampering with `res_proof.cpp`, or
(b) drift in `CHash256` / SHA-256 — itself a consensus violation.

---

## 15. Revision History

### v1.4 — 2026-08-03 — A-7 Single-Path (SECURITY-001-A)

**Scope:** Remove the witness v1 RES script path and the σ-domain signature from the mint witness.

**Normative changes:**
- PCORB mint witness: eleven pushes → **six** (`convergence_txid`, `convergence_amount_sat`,
  `bind_hash`, `height`, `target_prev`, `convergence_root`), sizes 32 / 8 / 32 / 8 / 4 / 32.
- §16 (Leaf-Freeze-01) **superseded**: the leaf push is deleted and `ConvergeProofLeafScript()` /
  `ConvergeProofLeafHash()` are removed from the source.
- `RFC008_ERRATUM_RESERVED_ZERO.md` **moot**: there is no 11-item count to correct and no
  `wit[9]` / `wit[10]` to freeze. F-WX-08 / F-WX-09 / F-WX-10 / F-WX-11 are void.
- A witness v1 RES output is spendable only over the 2-item Ed25519 key path.

**Non-changes (invariants preserved):**
- σ input layout (§3.2, §5) and the 100-byte preimage: unchanged.
- σ-CR-01 `bind_commit` and `target_prev`'s role within it: unchanged.
- Inequality shape `H_i < T · f_i` and `T = RES_PROBABILITY_TARGET`: unchanged.
- Field order and sizes of the six surviving proof fields: unchanged.
- All other RFC sections: unchanged.

**Compatibility:**
- Chain-invalidating. Merges with the A-5/A-6 chain reset; no separate reset, no live-network hard
  fork, since mainnet is not yet live.
- Implementation: `consensus/res_proof.{h,cpp}`, `consensus/res_mint_witness.{h,cpp}`,
  `script/interpreter.cpp`, `policy/policy.cpp`.
- Test: `core/src/test/res_single_spend_path_tests.cpp` asserts that an 11-item stack is rejected by
  both relay policy and script verification, and that the key path still relays. The tapleaf freeze
  test is deleted with the leaf helpers.

### v1.2 — 2026-05-12 — Leaf-Freeze-01 (Pre-Mainnet Cleanup) — **superseded by v1.4**

**Scope:** Permanently freeze `ConvergeProofLeafScript` to the minimal
sentinel `OP_1` (1 byte, `0x51`).

**Normative changes:**
- §16 added: full freeze of leaf bytes, leaf hash, and prohibitions.
- §14 final lock implicitly extended (leaf form is now immutable post-v1.2).

**Non-changes (invariants preserved):**
- σ input layout (§3.2, §5): unchanged.
- σ-CR-01 `bind_commit`: unchanged.
- 11-item RNG/PCORB witness stack layout: unchanged (see `RFC008_ERRATUM_RESERVED_ZERO.md` for items 9–10 MUST_BE_ZERO). — *Accurate as of v1.2; superseded by v1.4, which reduced the mint witness to six pushes and made the erratum moot.*
- All other RFC sections: unchanged.

**Compatibility:**
- Pre-mainnet. No activation flag. Treat v1.2 as the only valid layout
  from genesis forward.
- Implementation: `core/src/consensus/res_proof.cpp` line replaced
  (`CScript() << OP_1 << ToByteVector(uint256::ONE)` → `CScript() << OP_1`).
- Test: `core/src/test/res_proof_math_tests.cpp` asserts `leaf.size() == 1u`
  and pins `Hash256(0x51)` as the golden leaf hash.
- No witness-stack constructor changes required; all current consumers
  push the leaf bytes verbatim via `leaf.begin()` / `leaf.end()`.

### v1.1 — 2026-05-12 — Sigma-CR-01 (Pre-Mainnet Hardening)

**Scope:** Cryptographic binding of `target_prev` into σ to close cross-difficulty replay.

**Normative changes:**
- §3.2 buffer slot `[64..95]` redefined from raw `B_{h-k}` to `bind_commit`.
- §3.2.1 added: `bind_commit := Hash256(B_{h-k} || LE32(target_prev))`.
- §5.1 σ definition refactored to use `bind_commit`.
- §5.3 added: explicit domain-separation guarantee statement.
- §14 final lock updated to enumerate `bind_commit` and `target_prev`
  endianness.

**Non-changes (invariants preserved):**
- Total σ input length: still **100 bytes EXACTLY**.
- Deterministic serialization order: unchanged.
- `B_{h-k}` selection rule (k = 10): unchanged.
- All other RFC sections (concatenation, hash, inequality, multiplication,
  winner selection, boundary conditions): unchanged.

**Compatibility:**
- Pre-mainnet. No activation flag, no BIP-9 versionbits required. Treat v1.1
  as the only valid layout from genesis forward.
- Golden vectors regenerated: `formal_verification/golden_vectors.json` v1.1
  and `formal_verification/rigor_golden_vectors.json` v1.1.
- Coq model updated: `formal_verification/coq/Sigma.v`.

**Rationale and discarded alternatives:**
See `audit_sigma_replay_domain_separation.md`. Notably,
`LE32(height XOR target_prev)` was rejected as cryptographically unsound
(entropy collapse on `height = target_prev`, distinguisher attacks via
algebraic structure of XOR).

### v1.0 — 2026-04-26 — Initial Lock
- Original σ input: `txid || C || B_{h-k} || LE32(h)` (replay vulnerability,
  superseded by v1.1).

---

**END OF RFC-008 v1.1**

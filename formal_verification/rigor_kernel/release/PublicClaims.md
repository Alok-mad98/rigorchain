# Public Claims

Wording an auditor (or a public statement) may and may not use.

## Allowed — TRUE

| Claim | Meaning |
|---|---|
| critical consensus invariants formally verified | Category-A Coq kernel theorems are Closed |
| TCB / extraction boundary documented | Stop is Coq validators; compiler/runtime outside |
| extraction source identity | The six extracted names are definitionally the Coq validators (`EXTRACTION_FORMAL_PROOF=source_identity_only`) |

Required qualifier: **inside a Coq consensus kernel model, with
explicit extraction and runtime trust boundaries.**

Allowed public phrase:

> Formally verified critical consensus invariants
> inside a Coq consensus kernel model,
> with explicit extraction and runtime trust boundaries.

Shorter equivalent (same meaning, same boundary):

> formally verified critical consensus invariants
> (consensus kernel only)

## Forbidden — FALSE

| Claim | Status |
|---|---|
| full bitcoin verification | FALSE |
| full node verification | FALSE |
| network verification | FALSE |
| wallet verification | FALSE |
| crypto proof | FALSE |
| C++ implementation equivalence | FALSE |
| full-chain equivalence | FALSE |
| Coq proof covers OCaml compiler / full extraction refinement | FALSE |
| Coq proof covers binary artifact | FALSE |
| theorem-to-binary refinement | FALSE |
| executable binary semantic preservation | FALSE |
| compiler correctness | FALSE |

## Research sidecar (not a public claim)

An engineering path exists: CertiCoq 0.9+8.19 generated boxed-`Z`
Clight for the six `check_*` functions and `validate_kernel`;
sidecar `ccomp` 3.13 compiled that C. That is **not** a Coq proof
covering the binary artifact. Correspondence theorems were not
instantiated. Checking-TCB flags stay false. Maturity stays 4.9.
Narrow wording: `release/FinalClaimsBoundary.md` (research sidecar
note). Do **not** read this as
`Coq proof covers binary artifact = TRUE`.

Machine-checked flags in `Audit/KernelNonClaims.v`:

```
cpp_equivalence = false
full_chain_equivalence = false
node_correctness = false
network_correctness = false
wallet_correctness = false
crypto_security = false
kernel_execution_verified = false
extraction_equivalence = false
extraction_source_identity = true   (* Coq term identity only *)
extraction_full_equivalence = false
binary_semantic_equivalence = false
compiler_correctness = false
```

`EXTRACTION_FORMAL_PROOF=source_identity_only`
`EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE`
`PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE`

There is **no production-code refinement proof**.

## Differential verification status

The extracted OCaml checkers are an executable specification of the
Coq validators. A production-logic **shadow** of the comparable
fragment is compared on golden, invalid, and extra vectors.

`MATCH` means the two *implementations agreed* on that vector. It is
**not** an independent correctness assertion, **not** a C++ /
ConnectBlock refinement, and **not** a full extraction refinement.

When a vector carries `expect` (`VALID` / `INVALID`), that absolute
verdict is checked separately. `AUTO` or a missing `expect` is
agreement-only.

Map: `release/RefinementMapping.md`
Report: `evidence/DiffVerifyReport.md`
Run: `scripts/diff_verify.sh`

Coverage includes golden, INVALID-001…005, extra structured / random
vectors, and tagged gap vectors (`powLimit` stand-in, SetCompact
sign/overflow, unclamped `exp_q_raw` vs `±ASERT_EXP_CLAMP`). Count is
regenerated in the report.
MATCH remains fragment agreement only.

One-click auditor replay: `release/independent_audit_replay.sh`.

## Crypto assumption inventory

All hash / keccak / signature / PoW-digest assumptions used by this
cone are declared in `Kernel/CryptoAssumptions.v`. They are
assumptions, not security proofs.

`crypto_security = false`
`keccak_implementation_verified = false`
`production_crypto_refinement = false`

## Verified compiler trust root

The intended path is Coq checkers → CompCert-C / Clight → `ccomp`.
A research sidecar generated CertiCoq Clight and compiled it with
sidecar `ccomp` 3.13. That path is **not switched** into the
checking TCB (`verified_compiler_in_current_tcb = false`).
Correspondence theorems were not instantiated.

A hand-written C99 fragment rehearsal (`scripts/verified_compiler_path.sh`)
can compile `verified_compiler/fragment_checker.c` with `ccomp` if
present, otherwise untrusted `gcc`, and replay INVALID-001…005.
That rehearsal is **not** a Coq-extract refinement.
Success token: `C_FRAGMENT_REHEARSAL=PASS`.

Full binary semantic equivalence is **still not claimed**.

`COMPILER_CORRECTNESS=false`
`BINARY_SEMANTIC_EQUIVALENCE=false`
`VERIFIED_COMPILER_FORCED_SWITCH=FALSE`

Map: `release/VerifiedCompilerPath.md`

## Open properties

Properties that remain unproved — liveness, fairness, network
liveness / finality, k-deep / honest-majority fork-choice (OP-R1;
RES rewind and most-work selection are separate in-model identities),
full extraction refinement,
production refinement, crypto security, compiler / binary
equivalence — are listed in `release/OpenProperties.md`.
Listing them is not a proof.

P0-120 added two **local** Coq-model safety corollaries
(`apply_block_accepted_inputs_unique` /
`apply_block_rejects_duplicate_input`,
`res_mint_bounded_by_genesis`; Category B). A follow-up proved the
schedule-side aggregate `res_total_issuance_capped` (Category A):
`RES_REWARD_EPOCH * Σ_{k=0..250} W_epoch(k) ≤ RES_MAX_SUPPLY`.
These do **not** change the public claim and do **not** close reorg,
`S_RES` rejection, refinement, or compiler correctness.

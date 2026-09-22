# Claims Boundary

**Allowed public wording:** Formally verified critical consensus
invariants inside a Coq consensus kernel model, with explicit
extraction and runtime trust boundaries.

No artifact in this cone may be cited as C++ implementation equivalence,
full-chain equivalence, implementation refinement, extraction
refinement, binary semantic preservation, or cryptographic security.

## Proved (Coq kernel model)

The following are proved inside `formal_verification/rigor_kernel/`,
over the Coq models. They are kernel invariants, not node equivalence
and not binary equivalence.

| Area | What is proved |
|---|---|
| Supply schedule invariant | `W(h)` is finite and non-increasing on `h >= 0`; mint bound is `W(h)`, not a running `S_RES` reject |
| ORU conservation | `ORU_input = ORU_output + fee`; `new_supply = old + subsidy + fees - burn`; fee is ORU-denominated; burn is RES-transfer only |
| RES issuance bounds | Positive RES creation requires coinbase, a valid mint shape, `mint_amount <= W h`, and a draw-eligibility **hypothesis** |
| Script classification properties | Recognizer branches are pairwise exclusive; unsupported witness does not fall through to mint/converge. Totality / function-ness lemmas are class B |
| PoW target comparison | Digest-as-integer above target is rejected over abstract `H`. `pow_valid_deterministic` is a class-B function-ness lemma, not a digest implementation proof |
| ASERT deterministic model | `asert_next_target` is a function of the parent/anchor inputs; `exp_q` uses `Z.quot`; `num_shifts` uses `Z.div`; compact round-trip is a lower bound, not an identity |
| Refinement / TCB boundary | The proof covers Spec → Coq → validator. Extraction is outside the proof boundary. `kernel_boundary_closed` is a class-C documentation marker; it is **not** a CompCert, extraction, or C++ refinement |

## Generated (not proved)

| Area | Status |
|---|---|
| Extracted kernel binary | Six boolean validators extract to OCaml. Source identity is proved (`extraction_source_equivalence`). `extracted-kernel-checker` / `rigor-kernel-checker` are generated test artifacts. Full OCaml / compiler equivalence is **not** claimed |
| Golden-vector run | Test harness evidence only. Not theorem-to-binary refinement |

Audit flags in `Audit/KernelNonClaims.v`:

| Flag | Value |
|---|---|
| `binary_kernel_artifact` | `true` (generated test artifact exists) |
| `kernel_execution_verified` | `false` |
| `extraction_equivalence` | `false` |
| `binary_semantic_equivalence` | `false` |
| `kernel_refinement_boundary` | `true` (boundary documented; stop is Coq validators) |
| `tcb_defined` | `true` |
| `binary_artifact_trace` | `true` (test trace recorded; not a proof) |
| `cpp_equivalence` | `false` |
| `full_chain_equivalence` | `false` |
| `node_correctness` | `false` |
| `network_correctness` | `false` |
| `wallet_correctness` | `false` |
| `crypto_security` | `false` |

## Not proved

The following are **out of scope**. Citing any of them from this cone
is a claim-discipline violation.

| Non-claim | Why it is open |
|---|---|
| CPP implementation equivalence | No refinement from `core/src/**` to the Coq model |
| Full node equivalence | No whole-node or ConnectBlock simulation |
| Full-chain / reorg / chain-selection semantics | Single-transition kernel only |
| Extraction semantic preservation | No extraction refinement theorem |
| Binary semantic equivalence | No compiler / runtime / integer-width proof |
| SHA3 security | Collision / preimage resistance is not proved; PoW uses abstract `H`. Inventory: `Kernel/CryptoAssumptions.v` |
| Signature security | Ed25519 is not modelled as a cryptographic assumption set. Inventory: `Kernel/CryptoAssumptions.v` |
| Production-code refinement | Differential verification of a comparable fragment only. No Coq-to-C++ refinement (`release/RefinementMapping.md`) |
| Network consensus liveness | No P2P, mempool, or liveness theorem |
| Miner fairness | No game-theoretic or hashrate fairness theorem |
| RES ownership assignment | `draw_won → owner` is explicitly not claimed (C-4); eligibility is a hypothesis |
| Runtime rejection behavior | In particular, C++ ConnectBlock does **not** reject on observational `S_RES`; no runtime-rejection equivalence |
| Implementation refinement | No `refinement_proved` / `equivalence_proved` flag exists in this cone |
| Wallet security | Wallet format and signing policy are out of scope |
| Network security | Relay, eclipse, and eclipse-resistance are out of scope |
| Classifier ≡ `rigor_classify.cpp` | Model-internal properties only (C-7) |
| Liveness / fairness / finality / reorg | Not modelled. Inventory: `release/OpenProperties.md` |
| Verified compiler / binary equivalence | Documented rehearsal only. `release/VerifiedCompilerPath.md` |

## Forbidden claim tokens

Do not use these labels for this package:

- `CPP_EQUIVALENCE`
- `FULL_CHAIN_EQUIVALENCE`
- `IMPLEMENTATION_REFINEMENT`
- `CRYPTO_SECURITY`
- `WALLET_SECURITY`
- `NETWORK_SECURITY`
- `EXTRACTION_EQUIVALENCE`
- `BINARY_SEMANTIC_EQUIVALENCE`
- `THEOREM_TO_BINARY_TRACE`

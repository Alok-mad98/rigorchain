# CertiCoq + CompCert Plan (Frontier 3.1)

Technical design for closing the **verified extraction + verified
compiler** gap. This is the gate document. Coding of the pipeline
starts only at Phase 1 §5 after the Phase 0 lock below is accepted.

Companion: `FormalizationFrontier.md` §3.1 (why this is expensive),
`VerifiedCompilerPath.md` (what is runnable today),
`CompilerBoundary.md` (what is still OPEN),
`FinalClaimsBoundary.md` (what may be said),
`OpenProperties.md` OP-E1 / OP-B1 (what must stay listed until Closed),
`LevelDefinition.md` (what Level 5 means).

This is **not** 3.2 / VST / `ConnectBlock`, not OP-R1, not a C++
refinement, and not a reason to describe the production node as
verified.

---

## 0. Why this workstream exists

The cone is at maturity **4.9**. Level 5 requires two facts this
package explicitly does not have:

1. **Extraction correctness** — `kernel_call_boundary` is a
   bijection onto declared names. That is interface consistency.
   It is not a proof that `ExtractedKernel.ml` computes the Coq
   function. `EXTRACTION_FORMAL_PROOF=source_identity_only`.
2. **Verified compiler** — `verified_compiler_in_current_tcb = false`.
   The default path is Coq 8.15.0 → inductive-`Z` OCaml → untrusted
   `ocamlopt`.

Those are OP-E1 and OP-B1. They are the largest declared gap
between the model and a running binary. Tier 1.3 (codec
differential) already bought the cheap half of that assurance.
3.1 buys a *different kind*: a proof about a compiled checker,
not more tests.

The Frontier priced this at **6–12 months** and said: start only if
a genuine Level 5 claim is required. This document assumes that
decision has been made, and then refuses every shortcut that would
make the claim false.

### 0.1 What is true on this tree today

| Fact | Where |
|---|---|
| Six checkers extract to OCaml with inductive `Z`/`nat`; no `ExtrOcamlNatInt` / `ExtrOcamlZInt` on the faithful path | `extracted/Extract.v` |
| Source identity only | `extraction_source_equivalence` |
| Runtime machine is a 63-bit *non-negative* checked-word domain, partial on overflow | `Runtime/MachineWord.v` (`word_max = 2^62-1`) |
| Machine refines the executable model when the step succeeds | `machine_step_refines_executable_step` |
| CompCert / CertiCoq are absent from the checking TCB | `VerifiedCompilerBoundary.v` |
| A hand-written C99 fragment can replay INVALID-001…005 under `gcc` (or `ccomp` if present) | `verified_compiler/fragment_checker.c` |
| That fragment is **not** extracted, and it is **stale**: `check_res` still demands `coinbase && draw_ok` (RFC-019). Current `RESValidator.check_res` demands `negb coinbase` and has no draw bit | RFC-020 |
| ASERT intermediates need more than signed 64-bit | `asert_poly_exceeds_int64`; rehearsal `asert_llong_overflow` |
| Coq of the cone is **8.15.0** | `VersionManifest.md` |
| Released CertiCoq 0.9+8.19 needs **Coq 8.19** and **CompCert 3.13.1** | opam `coq-certicoq.0.9+8.19` |

### 0.2 What 3.1 will actually close

A **compiled checker artifact** whose Gallina source is the six
`check_*` functions (plus `validate_kernel`), compiled by CertiCoq
to Clight, then by CompCert to assembly, with:

- a stated correspondence from those Gallina functions to the
  generated Clight (CertiCoq theorem, instantiated, not gestured);
- CompCert's existing Clight-to-assembly theorem applied to that
  Clight;
- INVALID-001…005 still `INVALID` on the resulting binary;
- the CertiCoq GC, FFI, OS, and CPU listed as remaining TCB.

That is Level 5 **for that artifact**. It is not Level 5 for
`rigord`, `ConnectBlock`, or the C++ node.

---

## 1. Principles

These are design rules. Breaking one is a reason to stop and amend
this file, not a reason to keep coding.

**P1. Same checkers, new compiler.** The compile surface is the
existing `check_utxo` / `check_oru` / `check_res` / `check_script`
/ `check_pow` / `check_asert` and `validate_kernel`. 3.1 does not
invent a seventh consensus rule and does not revive RFC-019.

**P2. Generator, not transcription.** The C / Clight that CompCert
sees must be produced from the Coq checkers by CertiCoq (or an
equivalent generator whose correspondence is a Coq theorem).
`fragment_checker.c` stays a rehearsal. It must never become the
compile target, and it must never be “fixed up” to look like
CertiCoq output.

**P3. Sidecar first, cone later.** The 8.15.0 critical cone remains
the auditor-replay TCB until a later, separate decision upgrades
it. 3.1 does **not** begin by rewriting `_CoqProject.kernel-critical`
onto Coq 8.19. The compile surface is ported into a sidecar
toolchain. Correspondence back to the 8.15 checkers is an explicit
obligation, not an implicit “it looks the same”.

**P4. Boxed `Z` first.** The faithful extract kept inductive `Z`
because native-int hand-off is part of what is being proved. The
first CertiCoq compile uses CertiCoq's boxed big-integer runtime,
not CompCert `long long`. A later bounded-integer compile is
allowed only where a theorem says every intermediate fits. ASERT
is not in that set today (`asert_poly_exceeds_int64`,
`asert_llong_overflow`).

**P5. Partiality is content.** Overflow, out-of-range words, and
decode failure are real. A total “it always returns bool” wrapper
that swallows `None` is a new checker and is forbidden.

**P6. Flags move last.** `COMPILER_CORRECTNESS`,
`BINARY_SEMANTIC_EQUIVALENCE`, `verified_compiler_in_current_tcb`,
and `EXTRACTION_FORMAL_PROOF` stay at today's values until Phase 7
has Closed theorems *and* `Print Assumptions` is Closed (or lists
only the already-accepted GC / FFI / `H` / `sig_ok` parameters).

**P7. Claim test.** Every completed phase that wants to be cited
must change `FinalClaimsBoundary.md` or `OpenProperties.md`. A
phase that only adds scripts and a green CI job produced
engineering, not assurance.

**P8. Mutation test.** Every new Category A theorem comes with a
mutant that kills it. A theorem no mutant can break is Category B
or C, not A.

**P9. Dominant-risk honesty.** Closing 3.1 does not make
`ConnectBlock` smaller. Do not describe the compiled checker as
if it were the node. The dominant remaining consensus-split risk
after 3.1 is still C++ (3.2), not `ocamlopt`.

**P10. One compile surface.** Do not grow the surface mid-flight
(codec, `select_tip`, `Spend.v`, `apply_block`, maturity). Those
are later increments with their own plan. The first Closed 3.1
artifact is the six checkers.

---

## 2. Inviolable conditions

Stop the workstream and amend this file if any of the following
is about to be violated. These are not style preferences.

| ID | Condition |
|---|---|
| I-1 | Do not flip `COMPILER_CORRECTNESS` or `BINARY_SEMANTIC_EQUIVALENCE` to true, or `verified_compiler_in_current_tcb` to true, until **both** (a) a generator-correspondence or CertiCoq instantiation and (b) a CompCert compile of *that generated* Clight are Closed |
| I-2 | Do not claim C++ / `ConnectBlock` / `rigord` / full-node / network / wallet / crypto-security as a result of 3.1 |
| I-3 | Do not replace the faithful OCaml extract as the **default** auditor executable until Phase 7 is Closed. Dual-path is required until then |
| I-4 | Do not vendor official INRIA CompCert sources into this repository if the license forbids it. Pin a CI-installable or licensed `ccomp` / `coq-compcert` instead |
| I-5 | Do not use a second hand-written C file as the “extract”. `fragment_checker.c` is rehearsal only and is already observationally stale |
| I-6 | Do not upgrade the 8.15 critical cone as a side-effect of making CertiCoq run. Cone upgrade is a separate, later decision |
| I-7 | Do not compile proof terms, Category C documentation modules, mutation harnesses, or `Audit/KernelNonClaims.v` |
| I-8 | Do not hide the CertiCoq GC, primitive-int runtime, or FFI inside a “verified binary” sentence. They remain TCB and must be named |
| I-9 | Do not start 3.2, OP-R1, a Script VM, dust-as-consensus, or Ed25519 EUF-CMA inside this workstream |
| I-10 | Do not mark Frontier 2.3 as DONE. Issuance half stays DONE; k-deep stays OPEN |
| I-11 | Do not introduce `Extract Constant`, `ExtrOcamlNatInt`, or `ExtrOcamlZInt` on the compile path |
| I-12 | Do not weaken INVALID-001…005. After every compile, those five records still reject |
| I-13 | Do not compile ASERT (or any other checker) onto signed 64-bit machine integers while `asert_poly_exceeds_int64` / `asert_llong_overflow` remain live. Boxed `Z` or an explicit 128-bit model only |
| I-14 | Do not claim CPU / ISA / OS / linker correctness. Tier 4 stays stopped |
| I-15 | Do not describe CertiCoq as “fully verified Gallina-to-C” without stating which pipeline theorems are instantiated and which runtime objects stay trusted. CertiCoq's published pipeline is not a blank cheque |
| I-16 | `equivalence_claimed` / `refinement_proved` / `cpp_equivalence` stay `false`. 3.1 does not close OP-R2 or OP-P1 |
| I-17 | Do not revive the archived lottery cone or the RFC-019 `draw_ok` bit to make the stale C fragment match |
| I-18 | CakeML / HOL rewrite is out. `coq-of-ocaml` is out (it cannot reach Level 5). This workstream is CertiCoq → CompCert only |

---

## 3. Conditions that must be satisfied

Two lists. The first is the **start gate** (already in
`OpenProperties.md` OP-B1 / `VerifiedCompilerPath.md`). The second
is the **Level-5 close gate**.

### 3.1 Start gate (all required before Phase 1 coding)

1. This document is accepted as the 3.1 SSOT.
2. A written refinement statement exists: named Gallina functions,
   input/output types, and the compiled entry points. (This file
   §4 is that statement until a Coq module replaces it.)
3. Toolchain pin chosen: **Coq 8.19.x + CertiCoq 0.9+8.19 +
   CompCert 3.13.1 + MetaCoq 1.3.1+8.19 + Equations 1.3+8.19**,
   as a sidecar, not as a cone upgrade. Recorded in
   `release/CertiCoqToolchain.md` when Phase 1 lands.
4. CompCert license path decided: academic/CI-installable `ccomp`
   **or** `coq-compcert` used only as a Coq library plus a
   separately licensed binary. Decision written down. No silent
   vendoring.
5. Integer strategy locked: boxed `Z` for the first Closed
   artifact (P4, I-13).
6. Resource: a dedicated workstream (not a one-script increment).
   Estimate in §11.
7. Public flags remain at today's values until the close gate.

### 3.2 Close gate (all required before any Level 5 sentence)

1. CertiCoq generates Clight from the six checkers. The generator
   is CertiCoq, not a human.
2. That Clight compiles with CompCert `ccomp` (or the CompCert
   Coq backend applied to the same AST).
3. A Coq theorem, or a CertiCoq theorem instantiated at these
   functions, states the Gallina–Clight correspondence. Remaining
   gaps are listed by name, not implied empty.
4. CompCert's Clight-to-assembly theorem applies to the generated
   file (or the gap is listed).
5. INVALID-001…005 reject on the compiled binary. Positive smokes
   that the Coq checkers accept still accept.
6. Observational agreement with the faithful OCaml extract on a
   pinned vector set (the existing differential corpus plus the
   five invalids). Any divergence is either a listed integer-model
   gap or a blocker.
7. GC / FFI / prim-int objects are named in `TCBInventory.md`.
8. `Print Assumptions` on every new Category A theorem is Closed
   or lists only accepted parameters (`H`, `sig_ok`, GC axiom if
   one is introduced — and if one is introduced it is Category C
   documentation, not hidden).
9. New Category A theorems have mutants.
10. `FinalClaimsBoundary.md` and `OpenProperties.md` OP-E1 / OP-B1
    are edited to the *restricted* Closed sentence in §10.
    `COMPILER_CORRECTNESS` becomes true **only** for the compiled
    checker artifact, with the qualifier in the same paragraph.
11. The 8.15 cone still replays (`run_kernel_critical.sh`,
    `check_source_manifest.sh`). 3.1 did not regress it.
12. Maturity may move from 4.9 to 5 **only** with the qualifier
    “compiled six-checker artifact; GC/FFI/OS/CPU trusted; not
    the node”. A bare “Level 5” is forbidden.

### 3.3 What satisfaction does *not* include

Closing 3.1 does **not** require: OP-R1, 3.2, codec extraction,
`select_tip` in the binary, `Spend.v` hooked into `valid_tx`,
Ed25519, keccak, `arith_uint256` bit-exact, Coq-in-Coq, or a
verified processor.

---

## 4. Compile surface (normative)

### 4.1 In

| Gallina | File | Why it is first |
|---|---|---|
| `check_oru` | `Kernel/ORUValidator.v` | Boolean inequalities; smallest arithmetic |
| `check_utxo` | `Kernel/UTXOValidator.v` | `valid_tx_b`; lists + `money_range` |
| `check_pow` | `Kernel/PowValidator.v` | Target compare + compact bound; digest is already a `Z` |
| `check_asert` | `Kernel/ASERTValidator.v` | `Z.quot` / `exp_q_raw`; **boxed `Z` only** |
| `check_res` | `Kernel/RESValidator.v` | RFC-020 mint inequality; `RATE`, `shiftl`, poles |
| `check_script` | `Kernel/ScriptValidator.v` | Classifier ladder on `list byte` |
| `validate_kernel` | `Kernel/KernelInterface.v` | Dispatcher; must call the same six |

Plus the **minimal import cone** those definitions need
(records, `Z` ops, `classify_script`, `compact_encode/decode`,
`RATE` / `res_destroy_pole_sq`, `valid_tx_b`). Proof modules are
imported only when a definition lives there. Theorems are not
compiled.

### 4.2 Out (first Closed artifact)

| Out | Why |
|---|---|
| Entire `_CoqProject.kernel-critical` | Proofs, mutants, non-claims |
| `Kernel/Chain/*`, `select_tip`, `GetBlockProof` | Rule work; not the extracted checkers |
| `Script/Semantics/Spend.v` | Standalone; not hooked into `check_script` |
| `Kernel/Tx/Maturity.v` | Not in `check_*` |
| Codec (`WireCodec.v`, `ExtractCodec.v`) | Separate extract; 1.3 already tests it |
| Runtime machine / 63-bit step | Compile target is the checkers, not `machine_step_of`. The word model informs the *later* bounded-int increment |
| `fragment_checker.c` | Hand-written, stale, not a generator |
| Legacy `extraction/ExtractKernel.v` | Uses `ExtrOcamlNatInt` / `ExtrOcamlZInt` |
| C++ `ConnectBlock`, keccak, Ed25519 | 3.2 / OP-C1 / OP-S1 |

### 4.3 Suggested compile order

```
check_oru
  → check_utxo
  → check_pow
  → check_asert          (boxed Z; do not lower to int64)
  → check_res
  → check_script
  → validate_kernel
```

Each step produces Clight, then a `ccomp` object, then the
INVALID / smoke vectors that apply to that checker. Do not wait
for all six before the first `ccomp`. Do not claim Level 5 until
all six plus the dispatcher are in.

### 4.4 I/O types (refinement statement)

Inputs are the existing Coq records (`UTXOState`/`tx`,
`ORUState`/`kernel_tx`, `RESState`/`res_tx`, `ScriptState`,
`PowHeader`, `ASERTInput`). Output is `bool`.

No new ABI is designed in Phase 1–4 beyond whatever CertiCoq
generates for those inductives. A C driver that constructs the
records from vectors is **trusted glue**, listed in the TCB, and
is not the checker.

### 4.5 Relation to the 63-bit machine

`MachineWord.word_max = 2^62-1` is a Coq-model bound that makes
runtime refinement non-vacuous. It is **not** CompCert's
`int64` semantics (signed, wrapping). Do not identify them.

A later increment may compile a *bounded* checker that fails
when a field leaves `wf_word`, and prove it equal to the boxed-`Z`
checker on `wf_word` inputs. That increment is out of the first
Closed artifact. It must not be smuggled in to “make CompCert
faster”.

---

## 5. Target architecture

```
8.15 cone (unchanged auditor TCB)
    check_*  ==[Phase 2/3 correspondence]==  sidecar Gallina check_*
                                                    |
                                            CertiCoq (boxed Z)
                                                    ↓
                                                 Clight
                                                    |
                                              CompCert ccomp
                                                    ↓
                                              assembly + GC + FFI
                                                    |
                                         trusted vector driver
                                                    ↓
                                         compiled checker binary
```

Two compilers remain in play until Phase 7:

| Path | Role |
|---|---|
| Faithful OCaml extract + `ocamlopt` | Default auditor executable; source identity only |
| CertiCoq → CompCert | New path; not default until close gate |

`coq-of-ocaml` and CakeML are not on this diagram.

### 5.1 What “composition” means here

Informally:

```
Gallina check_*  ≈_CertiCoq  Clight  ≈_CompCert  assembly
```

Formally, Phase 7 must exhibit *theorems*, not this picture.
Expected shape (names indicative):

- `certicoq_check_oru_correct` — if CertiCoq's pipeline theorem
  applies to `check_oru`, instantiate it.
- `compcert_clight_of_check_oru` — the generated Clight is the
  program CompCert compiles.
- `compiled_check_oru_rejects_invalid002` — evaluation / replay,
  not a substitute for the two above.

If a CertiCoq pass used by this fragment has no published
correctness theorem, **list the pass as TCB**. Do not paper over
it by citing the CompCert theorem twice.

### 5.2 Remaining TCB after a successful close

| Still trusted | Why |
|---|---|
| Coq 8.15.0 (cone) and Coq 8.19 (sidecar) | Two checkers of two source trees until a cone upgrade |
| CertiCoq GC (`gc_stack.o` / `gc.o`) | Not produced by CompCert from our Gallina |
| CertiCoq FFI / prim-int objects if linked | Runtime, not the checker |
| Vector driver / `strtoll`-class parsing | Glue |
| OS, linker, CPU / ISA | Tier 4 |
| Abstract `H`, `sig_ok` | OP-C1 unchanged |
| Production C++ | OP-P1 / 3.2 unchanged |

CompCert *as applied to the generated Clight* leaves the TCB.
`ocamlopt` leaves the TCB **of this artifact only**. It stays in
the TCB of the default OCaml extract until that extract is retired
(not required by 3.1).

---

## 6. Phases

Each phase has a reason, files, exit criteria, and a stop list.
Ship a phase only when its exit criteria hold. Shipping Phase 4
without Phase 7 is fine and is the expected mid-point. Shipping a
half-proved Phase 7 is not.

```
0  Decision lock                         this document
1  Toolchain sidecar                     weeks
2  Isolate compile surface               2–4 weeks
3  8.19 port + observational equality    3–6 weeks
4  CertiCoq → Clight (first checker)     4–8 weeks
5  Six checkers + dispatcher             4–8 weeks
6  CompCert compile + link GC            3–6 weeks
7  Correspondence + composition          2–4 months
8  Differential, mutants, claims         3–5 weeks
```

Phases 4–6 may overlap per checker (ORU through CompCert before
script has Clight). Phase 7 does not start in earnest until at
least one checker has a CompCert binary. Phase 8's claim edit
waits for all six.

### Phase 0 — Decision lock — THIS DOCUMENT

**Reason.** The Frontier called 3.1 a decision point. Starting
without a lock is how `fragment_checker.c` becomes a fake extract.

**Exit.** This file exists; principles / inviolables / start gate
are accepted; Frontier §3.1 points here; no pipeline code yet.

**Stop.** Do not install CertiCoq “to see” before Phase 1 records
the pin. Do not edit `KernelNonClaims.v` flags.

### Phase 1 — Toolchain sidecar

**Reason.** CertiCoq 0.9+8.19 does not load on Coq 8.15.0. Forcing
the cone onto 8.19 in order to try a plugin is how auditor replay
breaks.

**Files (new):**

| Role | Path |
|---|---|
| Pin | `release/CertiCoqToolchain.md` |
| Image / opam | `certicoq/opam.pinned` or CI dockerfile under `certicoq/ci/` |
| Probe | `certicoq/scripts/toolchain_probe.sh` |

**Pinned versions (normative, amend only by editing this table):**

| Component | Version |
|---|---|
| Sidecar Coq | 8.19.x (`>= 8.19` and `< 8.20`) |
| CertiCoq | 0.9+8.19 |
| CompCert (Coq lib) | 3.13.1 (`coq-compcert`) |
| CompCert binary | `ccomp` matching 3.13.1, licensed/CI |
| MetaCoq | 1.3.1+8.19 (erasure + safechecker plugins) |
| Equations | 1.3+8.19 |
| OCaml | `>= 4.13` (host already has 4.13.1) |
| C compiler for GC | `clang` (`conf-clang`) |

CertiRocq / Rocq 9.1 is **not** the first pin. It is a later
migration if 8.19 CertiCoq becomes unmaintained. Do not jump to
Rocq 9.1 to look current.

**Steps.**

1. Write the license decision (I-4). If `ccomp` cannot be
   obtained, stop: `coq-compcert` alone does not produce a binary.
   **DONE** — `L-RESEARCH-SIDECAR-2026-09-12` in
   `release/CertiCoqToolchain.md`. CompCert 3.13.1 is INRIA
   Non-Commercial; not vendored; production/mainnet shipping
   blocked until an AbsInt agreement. Host `ccomp` is ABSENT;
   that is a step-2 install, not a reason to take §1.5 fallbacks.
2. Build a reproducible sidecar (container or opam switch) that
   is **not** on `PATH` for `run_kernel_critical.sh`.
   **DONE** — this host: local `OPAMROOT=certicoq/.opam-root`,
   switch `rigor-certicoq-8.19` (Coq 8.19.2, `ccomp` 3.13,
   CertiCoq 0.9+8.19). Docker definition exists; daemon was
   down. Cone `/usr/bin/coqc` remains 8.15.0. Host `ccomp`
   remains ABSENT. `SIDECAR_BUILD=PASS`.
3. `toolchain_probe.sh` prints versions and
   `CERTICOQ=PRESENT` / `CCOMP=PRESENT` / `CONE_COQ=8.15.0`.
   **DONE** — `certicoq/scripts/toolchain_probe.sh` exits
   `TOOLCHAIN_PROBE=PASS`. Sidecar resolved by absolute path;
   host `PATH` unchanged. Recorded: Coq 8.19.2, `ccomp` 3.13,
   CertiCoq `Plugin/CertiCoq.vo`. Cone remains 8.15.0.
4. Compile a CertiCoq hello-world (`Definition f := true`) to
   Clight and through `ccomp`. This is not a kernel claim.
   **DONE** — `certicoq/hello/Hello.v` +
   `certicoq/scripts/compile_hello.sh`. CertiCoq wrote
   `Hello.f.c`; sidecar `ccomp` 3.13 produced `hello.s` /
   `hello.o` (`CCOMP_HELLO=PASS`, `HELLO_WORLD=PASS`).
   Generated files stay under `certicoq/generated/` (gitignored).
   Not a kernel claim. `COMPILER_CORRECTNESS=false`.
5. Record hashes of the pinned tarballs / opam URLs.
   **DONE** — `certicoq/pins.sha512` +
   `certicoq/scripts/verify_pins.sh`. Seven tarballs downloaded
   to `certicoq/generated/tarballs/` (gitignored). Each digest
   matched the published opam checksum (`PIN_VERIFY=PASS`,
   2026-09-12). OCaml 4.14.2 is sha256 in opam; local sha512
   recorded in `release/CertiCoqToolchain.md` §2. CompCert
   archive not committed (I-4). `HOST_CCOMP=ABSENT`,
   `CONE_COQ=8.15.0`, `COMPILER_CORRECTNESS=false`.

**Exit.** Probe is green on a clean machine; hello-world Clight
exists; cone 8.15 replay still green; `CertiCoqToolchain.md`
lists every pin.
**DONE** (2026-09-13) — `./run_kernel_critical.sh` on `/usr/bin/coqc`
8.15.0, `HOST_CCOMP=ABSENT`,
`FORMAL_KERNEL_PHASE_FINAL_CLOSURE=PASS`. Sidecar stays off the
cone `PATH`. `COMPILER_CORRECTNESS=false`.

**Stop / do not.** Do not change `VersionManifest.md` Coq 8.15.0.
Do not add CertiCoq to `_CoqProject.kernel-critical`. Do not
commit CompCert sources if the license forbids it.

### Phase 2 — Isolate the compile surface

**Reason.** CertiCoq will force Gallina rewrites (Frontier). Those
rewrites must not land in the 8.15 proof cone first. Isolation
makes the blast radius visible.

**Files (new):**

| Role | Path |
|---|---|
| Surface `_CoqProject` | `certicoq/surface/_CoqProject` |
| Copy or thin wrappers | `certicoq/surface/Kernel/*.v` |
| Import inventory | `certicoq/surface/IMPORTS.md` |

**Steps.**

1. List every `.v` that the six `check_*` definitions
   *definitionally* depend on (not every file they `Require` for
   a theorem). Write `IMPORTS.md`. Expected core: the six
   validators, `KernelInterface`, ORU/RES/UTXO/script/pow/ASERT
   *definitions*, `SupplySchedule` RATE/poles, `CompactBits`,
   `ScriptClassifier`, `UTXOSoundness.valid_tx_b`.
2. Copy or `-R`-share those files into `certicoq/surface/` so the
   sidecar can compile them without the rest of the cone.
3. If a definition lives in a proof file (`valid_tx_b` in
   `UTXOSoundness.v`), extract the **definition** into a surface
   module and leave the theorems in the cone. Do not duplicate
   Category A proofs in the sidecar.
4. Prove, *in the 8.15 cone*, that the surface names are still
   the cone names (`surface_check_oru = check_oru`, etc.). These
   are the Phase 2 correspondence theorems. Category B is enough
   if they are `reflexivity`; they become A if the copy can drift.

**Exit.** Sidecar builds the surface on 8.15 *or* the copy is
shown equal by `reflexivity` / pointwise `bool` equality.
`IMPORTS.md` lists every file. No CertiCoq rewrite yet.

**Stop / do not.** Do not “simplify” `check_res` to the stale C
fragment. Do not drop `negb coinbase`. Do not add `draw_ok`.

**DONE** (2026-09-13) — `certicoq/surface/` is a self-contained
definition slice (`RigorSurface`, 8.15 `SURFACE_BUILD=PASS`).
`IMPORTS.md` lists every definitional dependency. Definitions
that lived in proof files (`valid_tx_b`, `kernel_tx`, `res_tx`)
were extracted; Category A proofs stay in the cone. Cone
`Kernel/SurfaceCorrespondence.v` proves pointwise `bool`
equality after `of_cone` (`surface_check_*_eq`,
`surface_validate_kernel_eq`, Category B). `_CoqProject.kernel-critical`
has `-R certicoq/surface RigorSurface` only — no CertiCoq plugin.
`check_res` keeps `negb coinbase` and has no `draw_ok`.
`COMPILER_CORRECTNESS=false`.

### Phase 3 — Coq 8.19 port and observational equality

**Reason.** The sidecar is 8.19. The cone is 8.15. Syntax and
stdlib can differ. The port is a compiler-host change, not a
rule change.

**Files.** Same surface, compiled by 8.19 `coqc`.

**Steps.**

1. Build `certicoq/surface` under Coq 8.19. Fix only what 8.19
   rejects (deprecations, `Require` paths, universe issues).
2. For each checker, keep a Coq evaluation witness that the
   8.19 function agrees with the 8.15 result on the INVALID and
   smoke records (`vm_compute` / `native_compute`). That is
   observational, not a cross-version metatheorem — state it as
   such.
3. If a rewrite is required for 8.19, prove `check_*_8_19 x =
   check_*_8_15 x` *inside one toolchain* by transporting the
   definition, or record an explicit gap per checker.
4. Re-run cone 8.15 critical. It must not have changed.

**Exit.** Six checkers + dispatcher build on 8.19; observational
witnesses for INVALID-001…005; documented list of 8.19-only
edits.

**Stop / do not.** Do not “fix” ASERT to fit `long long`. Do not
enable `ExtrOcamlZInt`. Do not merge the 8.19 tree over the cone.

**DONE** (2026-09-13) — sidecar `coqc` 8.19.2 built the same
surface into `certicoq/generated/surface-8.19/`
(`SURFACE_819_BUILD=PASS`). No Gallina rewrite
(`certicoq/surface/PORT_8_19.md`: 8.19-only edits = none).
`Observe.v` `vm_compute` closed INVALID-001…005 and the six
positive smokes on both 8.15.0 and 8.19.2
(`OBSERVE_8_15=PASS`, `OBSERVE_8_19=PASS`,
`OBSERVE_AGREE=PASS`). That is host evaluation, not a
cross-version metatheorem. Cone 8.15 `SurfaceCorrespondence`
still Closed; `/usr/bin/coqc` remains 8.15.0;
`HOST_CCOMP=ABSENT`. Boxed `Z`. No `ExtrOcamlZInt`.
`COMPILER_CORRECTNESS=false`.

### Phase 4 — CertiCoq fragment audit and first Clight

**Reason.** CertiCoq compiles a Gallina fragment. Typeclasses,
some well-founded recursion, axioms on the computational path,
and universe tricks fail. The audit is the rewrite list.

**Files (new):**

| Role | Path |
|---|---|
| Audit | `certicoq/surface/FRAGMENT_AUDIT.md` |
| CertiCoq command | `certicoq/surface/ExtractCertiCoq.v` |
| Generated Clight (build artifact) | `certicoq/generated/` (gitignored except a SHA pin) |

**Audit checklist (every checker):**

- No axiom on the computational path.
- No `Extract Constant`.
- Recursion is structural `Fixpoint` or a form CertiCoq accepts.
- Records / inductives are first-order enough for erasure.
- `Z` operations used (`quot`, `shiftl`, `mul`, compares) are
  listed; they go through boxed `Z`.
- Script classifier fuel `Fixpoint` is accepted or rewritten
  with an equivalent structural recursor *and* a proof.

**Steps.**

1. Fill the audit against CertiCoq 0.9+8.19, not against memory.
2. Rewrite only the surface. Every rewrite has a pointwise
   equality to the pre-rewrite function (`bool` extensional).
3. `CertiCoq Compile check_oru` (or current command name) to
   Clight. First success is ORU.
4. Inspect the Clight: it must mention a GC runtime, not a
   hand-written `check_oru` body copied from `fragment_checker.c`.
5. Pin the generated file hash in `certicoq/generated/SHA256` as
   a build output, not as a hand-edited C source.

**Exit.** `FRAGMENT_AUDIT.md` complete; `check_oru` Clight exists
from CertiCoq; pointwise equalities for any rewrite; cone
untouched.

**Stop / do not.** Do not commit generated C as if it were
source-of-truth. Do not link `gcc` and call that CompCert. Do
not skip the audit because ORU “looks simple”.

**DONE** (2026-09-13) — `FRAGMENT_AUDIT.md` filled against
CertiCoq 0.9+8.19 (sidecar `coqc` 8.19.2), not memory. Surface
Gallina was not rewritten (no pointwise equality required).
`CertiCoq Compile -O 0 check_oru` produced
`generated/clight-oru/RigorSurface.Kernel.ORU.check_oru.c`
(`ORU_CLIGHT=PASS`). Clight includes `gc_stack.h` /
`thread_info` / `garbage_collect` and boxed `BinInt.Z`
(`Z.leb` / `Z.eqb` / `Z.compare`); it is not the five-argument
`long long` body in `fragment_checker.c`
(`CLIGHT_NOT_FRAGMENT=TRUE`). Hash pin:
`certicoq/generated/SHA256` (build output; generated `.c` stays
gitignored). Remaining checkers inventoried, not compiled
(Phase 5). Cone `/usr/bin/coqc` remains 8.15.0;
`HOST_CCOMP=ABSENT`; `PHASE6_CCOMP_NOT_RUN=true`.
`COMPILER_CORRECTNESS=false`.

### Phase 5 — Remaining checkers to Clight

**Reason.** Level 5 for “the six checkers” is false if only ORU
compiled.

**Steps.** Walk §4.3. After each checker: Clight exists; rewrite
equalities hold; the checker still rejects its INVALID vector
under Coq evaluation.

Script is last: the classifier is the most likely fragment
break. If CertiCoq rejects the fuel `Fixpoint`, rewrite with a
proved-equivalent recursor; do not replace classification with
a stub that always returns `T_NONSTANDARD`.

**Exit.** Seven Clight units (six + dispatcher) or one linked
unit containing all seven names. Audit updated per rewrite.

**Stop / do not.** Do not drop `check_script` to close early and
then call it six checkers. A documented “five-checker artifact”
is allowed only if OP-B1 is edited to say five, and script stays
OPEN. Prefer not to.

**DONE** (2026-09-13) — CertiCoq 0.9+8.19 accepted all seven
names with no surface rewrite (`PHASE5_CLIGHT=PASS`). Units:
`clight-oru` (Phase 4) plus `clight-utxo` / `pow` / `asert` /
`res` / `script` / `validate`. Fuel `Fixpoint is_push_only_n`
compiled; not stubbed. ASERT stays boxed `Z`
(`ASERT_BOXED_Z=TRUE`, `Z.quot` in Clight). RES has
`rt_coinbase` and no `draw_ok`. Every unit includes
`gc_stack.h`; none resembles `fragment_checker.c`.
`Observe.v` still rejects INVALID-001…005 and the new
`invalid_script` on 8.15.0 and 8.19.2
(`OBSERVE_AGREE=PASS`). Pin: `certicoq/generated/SHA256`.
Cone `/usr/bin/coqc` remains 8.15.0; `HOST_CCOMP=ABSENT`;
`PHASE6_CCOMP_NOT_RUN=true`. `COMPILER_CORRECTNESS=false`.

### Phase 6 — CompCert compile and GC link

**Reason.** Clight is not a binary. CompCert is the verified
*C* compiler. Using `gcc` on CertiCoq C is the current
rehearsal, one level up, and must not be sold as 3.1.

**Files.**

| Role | Path |
|---|---|
| Build | `certicoq/scripts/ccomp_link.sh` |
| Driver (trusted) | `certicoq/driver/vector_driver.c` |
| Report | `evidence/CertiCoqCompCertReport.md` |

**Steps.**

1. Confirm generated C is CompCert-C / Clight (CertiCoq's
   backend is chosen for this). If CertiCoq emits C that
   `ccomp` rejects, that is a blocker, not a reason to switch
   to `gcc` and keep the name CompCert.
2. `ccomp` each unit. Link the CertiCoq GC (`gc_stack.o` or
   `gc.o` as the pipeline requires). The GC object is TCB.
3. Trusted driver reads the same vector records the OCaml
   harness uses (INVALID-001…005 and the positive smokes).
4. Script prints `CCOMP=PRESENT`, `GC=LINKED`,
   `COMPILER_CORRECTNESS=false` until Phase 7.
5. If `ccomp` is missing in CI, the job is `SKIP` / `ABSENT`,
   not `PASS`. The rehearsal pattern in
   `verified_compiler_path.sh` (`VERIFIED_COMPILER=ABSENT` +
   `C_FRAGMENT_REHEARSAL=PASS`) must not be copied here as a
   success token.

**Exit.** A binary exists, produced by `ccomp` from CertiCoq
Clight, linked with the named GC, rejecting INVALID-001…005.
Report names every linked object.

**Stop / do not.** Do not fall back to `gcc` and leave
`COMPILER_CORRECTNESS` discussion unchanged. A `gcc` binary of
CertiCoq C is an engineering smoke, recorded as
`CERTICOQ_GCC_SMOKE=PASS`, never as CompCert.

**DONE** (2026-09-13) — sidecar `ccomp` 3.13 compiled all seven
Phase 5 units plus `phase6_run` (`CCOMP_UNIT_*=PASS`). GC
`gc_stack.o` and prims built with `/usr/bin/clang` (TCB).
Trusted `certicoq/driver/vector_driver.c` linked with
`phase6.o` + named GC/prims/`libm`. Binary rejected
INVALID-001…005 and accepted the six smokes
(`PHASE6_VECTORS=PASS`, `PHASE6=PASS`). Report:
`evidence/CertiCoqCompCertReport.md`. `CERTICOQ_GCC_SMOKE=not_run`.
Cone `/usr/bin/coqc` remains 8.15.0; host `ccomp` ABSENT.
`COMPILER_CORRECTNESS=false`.

### Phase 7 — Correspondence and composition

**Reason.** A binary that agrees on five vectors is 1.3-quality
testing. Level 5 is theorems.

**Files (new):**

| Role | Path |
|---|---|
| Correspondence | `certicoq/proofs/CertiCoqCorrespondence.v` |
| Composition | `certicoq/proofs/CompiledCheckerSoundness.v` |
| Flags (still false until Closed) | extend `Kernel/VerifiedCompilerBoundary.v` only when Closed |

**Minimum theorems (names indicative):**

| Name | Statement | Class |
|---|---|---|
| `surface_check_eq_cone` (per checker) | sidecar function equals cone `check_*` | A if a mutant can break the copy; else B |
| `rewrite_check_eq` (per rewrite) | post-CertiCoq-fragment function equals pre-rewrite | A if the rewrite is non-trivial |
| `certicoq_applies_to_check_*` | pipeline theorem instantiated, or listed missing pass | A or explicit TCB row |
| `compcert_applies_to_generated_clight` | CompCert theorem applied to the generated program | A or explicit TCB row |
| `compiled_driver_not_the_checker` | documentation: driver is TCB | C |
| `we_do_not_claim_node_from_compiled_checker` | `node_correctness = false` still | C (already exists; re-cite) |

**Steps.**

1. Instantiate what CertiCoq actually proves for this pipeline
   version. Read the theorems; do not summarise the website.
2. For each unpublished pass, add a TCB row, not a comment in
   a proof.
3. Apply CompCert to the generated Clight AST if the toolchain
   allows; otherwise state that `ccomp` was used on that file
   and CompCert's theorem covers `ccomp` for CompCert-C.
4. Compose only what both sides cover. The composed sentence
   is the strongest public sentence allowed in §10.
5. `Print Assumptions` on every new A/B theorem.

**Exit.** Correspondence file compiles; assumptions closed or
listed; OP-E1 / OP-B1 can be edited to the restricted Closed
wording. Flags still false until the claim edit in Phase 8.

**Stop / do not.** Do not treat CertiCoq's existence as the
correspondence theorem. Do not hide a missing pass. Do not
claim `BINARY_SEMANTIC_EQUIVALENCE=true` for `rigord`.

**DONE** (2026-09-13) — honest listed-pass close, not a
pipeline instance. Read CertiCoq 0.9+8.19 in-tree:
`LambdaANF/toplevel_theorems.v` has per-pass `correct` /
`correct_cc`; `Codegen/*.v` has **no** `Theorem`;
`print_Clight` is an **Axiom**. None of those were
instantiated on `check_*`. CompCert's AST theorem was not
applied to the generated C (`ccomp` was used as a binary).
Files: `certicoq/proofs/CertiCoqCorrespondence.v`,
`CompiledCheckerSoundness.v`, `TCB_PASSES.md`. Cone 8.15
`PHASE7=PASS`, six `Print Assumptions` **Closed**.
`PHASE7_PIPELINE_INSTANTIATED=false`. Maturity stays **4.9**.
OP-E1 / OP-B1 stay open. Flags stay false. I-16:
`refinement_proved` stays false.

### Phase 8 — Differential, mutants, claims, freeze

**DONE** (2026-09-13) — document freeze only. Narrow sidecar
sentence (not the §10 instantiated-theorem paragraph). No new
Category A, so no M129+ and no `EXPECTED_FAILURE` bump.
`PHASE7_PIPELINE_INSTANTIATED=false`. Maturity stays 4.9.
Flags stay false. Source lock includes surface / proofs /
driver / scripts / `generated/SHA256` (not generated Clight).

**Reason.** Housekeeping is how this cone stays honest. Same bar
as M99–M128.

**Steps.**

1. Differential: compiled binary vs faithful OCaml extract on
   the existing kernel vector set. Fail on unexplained
   divergence. Integer-model gaps already named
   (`asert_llong_overflow`) are not surprises; they must not
   appear on the boxed-`Z` path. If they do, the compile used
   machine ints and Phase 4/6 is wrong.
2. Mutants for every new Category A theorem. New script under
   `mutation/mutate_certicoq_surface.sh`. Wire
   `run_all_mutations.sh`. Expected count rises by the new
   EXPECTED_FAILURE rows.
3. Dual tables: `TheoremClassification.md` and `evidence/` copy.
4. `inventory_metadata.json` → `gen_final_proof_inventory.py`.
5. `gen_expected_failures.py`.
6. `release/freeze_source_manifest.sh` — include the surface
   *source* and the pin files; do not lock generated Clight as
   if it were hand-written Gallina.
7. Claims: §10 sentence only. Edit
   `FinalClaimsBoundary.md`, `OpenProperties.md` OP-E1 / OP-B1,
   `TCBInventory.md`, `PublicClaims.md`, `LevelDefinition.md`
   (Level 5 qualifier), `FormalizationFrontier.md` §3.1
   (DONE with qualifier), `VerifiedCompilerPath.md`,
   `CompilerBoundary.md`, `VersionManifest.md` (sidecar
   versions; cone Coq stays 8.15.0 unless a later upgrade).
8. `Audit/KernelNonClaims.v`: add *restricted* true flags if
   needed (`compiled_checker_compcert = true`) **and** keep
   `compiler_correctness` either false or renamed so it cannot
   be read as “the node compiler”. Prefer new names over
   flipping the old global flag from false to true.

**Exit (taken).** Claim wording is the **narrow** sidecar
sentence in `FinalClaimsBoundary.md`, not the §10 paragraph
that treats correspondence as instantiated. OP-E1 / OP-B1 stay
**Not proved**. No new Category A ⇒ mutation count unchanged.
Source lock re-frozen. Flags stay false. Maturity stays 4.9.

**Stop / do not.** Do not delete `we_do_not_claim_*` theorems.
Do not set `cpp_equivalence = true`. Do not raise
`NATIVE_BINARY_VERIFIED` for `rigord`.

### Phase 9 — Level 5 close attempt (reopen correspondence)

**Reason.** Phase 0–8 closed the *engineering* path. The user
asked to attempt the §3.2 Level 5 gate. This phase tries to
instantiate what is published. It does **not** relax I-1 or
§10: Codegen without a theorem, or `print_Clight` as Axiom,
is an **Abort**, not a Level 5 badge.

**Files (new or extended, sidecar only):**

| Role | Path |
|---|---|
| CompCert AST attempt | `certicoq/proofs/CompCertAST.v` (8.19) |
| LambdaANF attempt | `certicoq/proofs/LambdaANFInstance.v` (8.19) |
| Outcome | `certicoq/proofs/PHASE9_RESULT.md` |
| Plan / TCB | this file; `TCB_PASSES.md` |

**Steps.**

1. Rebuild Phase 5 Clight and Phase 6 `ccomp` link in a
   sidecar **subshell**. Parent `PATH` stays cone 8.15.0;
   host `ccomp` stays ABSENT.
2. `clightgen` the generated `check_oru` C. Try
   `transf_c_program p = OK tp` then
   `transf_c_program_correct` in sidecar Coq 8.19. If
   `transf_c_program` does not compute, record the gap;
   do not treat a `ccomp` exit code as the theorem.
3. Try published LambdaANF lemmas on `check_oru`. Do not
   `Require Compiler.pipeline` (that imports
   `Axiom print_Clight`). Do not write a new Codegen proof.
4. Exit:

   - **Closed** only if Gallina–Clight *and* Clight–assembly
     are `Print Assumptions` Closed on `check_oru`, with no
     unpublished Codegen / `print_Clight` Axiom in the
     assumptions. Then §10 sentence, new flag *names*,
     maturity **5 (compiled checker artifact only)**.
   - **Abort** if any load-bearing gap remains. Maturity
     stays 4.9. Public flags stay false. Partial theorems
     may remain in `certicoq/proofs/`; they are not cone
     Category A unless a mutant exists.

**DONE** (2026-09-13) — **Abort.** `clightgen` produced a
`Csyntax.program` of `check_oru` C; `vm_compute` of
`transf_c_program` timed out at 120s (not applied).
LambdaANF `toplevel_theorems` loads without `pipeline.v`;
no `exp` of `check_oru`. Codegen still has no `Theorem`.
`PHASE9=ABORT`. Maturity stays 4.9. Flags stay false.
Detail: `certicoq/proofs/PHASE9_RESULT.md`.

**Stop / do not.** Do not waive I-1 / §10 to take Level 5
while Codegen is TCB. Do not flip cone
`compiler_correctness`. Do not vendor CompCert. Do not
start 3.2 / OP-R1 / CakeML.

### Phase 10 — partial pipeline (no Level 5)

**Reason.** After Phase 9 Abort, attempt the five partial
steps: in-memory Clight (no `print_Clight`); CompCert on
that Clight; export `exp` without `pipeline.v`; follow
upstream Codegen; take §10 only if both sides Closed.

**DONE** (2026-09-13) — **Abort Level 5, partial progress.**
Erase / LambdaBoxLocal / LambdaANF CPS `vm_compute` to
`Ret` without `Compiler.pipeline`. Full `compile_LambdaANF`
timed out (600s). CPS→`compile_Clight` is `Err`
`translate_funs`. No in-memory `Clight.program`. CompCert
not applied. CPS `exp` exists; `well_scoped` / `correct`
not Closed. Codegen still has no `Theorem`. §10 not
written. Maturity stays 4.9. Flags stay false.
Detail: `certicoq/proofs/PHASE10_RESULT.md`.

**Stop / do not.** Same as Phase 9. Do not treat a CPS
`exp` or an `Err` string as Gallina–Clight. Do not treat
a timeout as `OK`.

---

## 7. Integer policy (cross-cutting)

This is the load-bearing technical decision. It is repeated so
it cannot be “optimised” in Phase 6.

| Layer | Integer | Allowed in first artifact? |
|---|---|---|
| Cone / sidecar Gallina | Unbounded `Z` | Yes (source) |
| CertiCoq runtime | Boxed `Z` (GC) | **Yes — required** |
| CompCert `long long` | Signed 64-bit | **No**, except a later increment with a fit theorem |
| OCaml `int` / 63-bit word | `word_max = 2^62-1` | Model only; not identified with CompCert ints |
| C++ `__int128` | ASERT poly | Not CompCert; not in 3.1 |

Already-proved constraints that make machine-int ASERT false:

- `asert_poly_exceeds_int64` — cubic exceeds `2^63` on the
  domain. C++ `__int128` is load-bearing.
- `asert_llong_overflow` — `te = -2^50`, `tau = 2^50`: Coq
  `Z.quot` is defined; `te * 65536` overflows signed 64-bit.

A later “bounded compile” increment may:

1. prove a checker’s intermediates lie in a stated range; or
2. compile a partial checker that returns `None` on overflow
   and prove it agrees with boxed `Z` when `Some`.

That increment needs its own plan. It is not a Phase 6 tweak.

---

## 8. Non-goals (do not sneak these in)

| Out | Why |
|---|---|
| 3.2 VST / Frama-C of `validation.cpp` | Different program, 2–4 years |
| OP-R1 / k-deep / honest majority | No hashrate model |
| OP-P1 / OP-R2 C++ refinement | 3.1 is Gallina → asm, not Gallina → C++ |
| Script VM / Ed25519 / sighash | OP-S1 / OP-C1 |
| Codec Clight | Separate extract; schedule after six checkers if ever |
| `select_tip` / maturity / `Spend.v` in the binary | Not in `check_*` |
| Dust / mempool | Policy |
| CakeML / HOL | New cone |
| `coq-of-ocaml` | Leaves `ocamlopt` trusted |
| Coq-in-Coq / MetaCoq of `coqc` | Tier 4 |
| CPU / seL4 / verified OS | Tier 4 |
| Flipping global `compiler_correctness` without a qualifier | Overclaim |
| Using `gcc` success as CompCert success | Rehearsal pattern |
| Upgrading the auditor cone to 8.19/Rocq 9.1 as step 1 | Breaks replay |
| Treating `fragment_checker.c` as almost-CertiCoq | Stale, hand-written |

---

## 9. Housekeeping (every phase that adds a theorem)

Same bar as M99–M128:

1. `Qed`, no `Admitted`.
2. `Print Assumptions` Closed, or only accepted parameters,
   listed.
3. New Category A ⇒ a mutant that kills that theorem.
4. Dual tables stay in lockstep.
5. Inventory generator + expected-failure generator.
6. `freeze_source_manifest.sh` when publishing source.
7. Frontier / claims wording honest. No “the binary is
   verified” without “which binary”.
8. `equivalence_claimed = false`, `refinement_proved = false`,
   `cpp_equivalence = false` stay as they are.

Suggested mutant ID block (assign at implementation; do not
gap-fill by inventing kills):

| Phase | Provisional |
|---|---|
| 2–3 surface equality | M129–M134 (one per checker if A) |
| 4–5 fragment rewrites | M135– |
| 7 correspondence | assign after the theorem exists |

---

## 10. What may be said after close

Allowed addition, and no more:

> The six Coq kernel checkers (`check_utxo`, `check_oru`,
> `check_res`, `check_script`, `check_pow`, `check_asert`) and
> `validate_kernel`, restricted to the CertiCoq-supported
> fragment and compiled with boxed `Z`, generate Clight via
> CertiCoq and assembly via CompCert. The Gallina–Clight
> correspondence is the instantiated CertiCoq theorem (plus any
> named remaining pass listed as TCB). The Clight–assembly
> correspondence is CompCert's. The CertiCoq garbage collector,
> FFI, OS, and CPU remain trusted. This is not `ConnectBlock`,
> not `rigord`, not C++ equivalence, and not a proof that the
> production node is this artifact.

Still forbidden:

- the full node / C++ / `rigord` is verified;
- OP-R1 / k-deep is closed;
- the Script VM is formalized;
- dust is a consensus rule;
- bare “Level 5” without the artifact qualifier;
- `refinement_proved` / `cpp_equivalence`;
- `ocamlopt` is verified (it is not; it is bypassed for this
  artifact);
- SHA3 / Ed25519 security;
- CPU / ISA correctness.

OP-E1 becomes: **Closed for the compiled six-checker artifact
(boxed `Z`, named TCB residues); open for the OCaml extract and
for any other function.**

OP-B1 becomes: **Closed for CompCert-on-CertiCoq-Clight of that
artifact; open for `ocamlopt`, `gcc`, and the node.**

Maturity: **5 (compiled checker artifact only)** or stay **4.9**
if Phase 7 had to list a load-bearing unpublished CertiCoq pass
as TCB. In that second case, do not take the Level 5 number.
Honesty over the badge.

---

## 11. Cost and staffing

Frontier: 6–12 months. This split is the working budget. It is
not a promise that the lower bound holds if CertiCoq rejects
the classifier or if `ccomp` cannot consume the generated C.

| Phase | Calendar (one proof engineer) |
|---|---|
| 0 lock | days (this file) |
| 1 toolchain | 1–3 weeks (license + image dominate) |
| 2 surface isolation | 2–4 weeks |
| 3 8.19 port | 3–6 weeks |
| 4 first Clight | 4–8 weeks (audit + first rewrite) |
| 5 remaining Clight | 4–8 weeks |
| 6 `ccomp` + GC | 3–6 weeks |
| 7 correspondence | 2–4 months |
| 8 housekeeping | 3–5 weeks |
| **Total** | **~6–12 months** |

Phase 7 is the variance. If CertiCoq's theorems do not
instantiate cleanly, the honest outcome is a compiled artifact
plus a listed TCB pass, maturity still 4.9, OP-B1 still open
for “compiler correctness”, and a narrower Closed line
“CertiCoq generated this Clight; CompCert compiled it; the
CertiCoq pass P is trusted”. That is still worth doing. It is
not Level 5.

Review: one independent reader of the claim edit (Phase 8)
who is not the implementer. Same bar as P0-130.

---

## 12. Risks and fallbacks

| Risk | Why it is real here | Fallback (honest) |
|---|---|---|
| Coq 8.15 ≠ 8.19 drift | Whole cone pinned at 8.15 | Sidecar + observational witnesses; do not force cone upgrade |
| CertiCoq rejects classifier / `RATE` | Frontier warned about fragment rewrites | Proved-equivalent rewrite; or drop that checker from the artifact and say so |
| Generated C is not CompCert-C | CertiCoq can emit gcc-oriented C | Blocker. `gcc` smoke only; no CompCert claim |
| CompCert license / no `ccomp` in CI | Already a documented blocker | Stop Phase 6; do not pretend `coq-compcert` compiled a binary |
| ASERT / RES overflow on machine ints | Already theorems | Boxed `Z` only |
| CertiCoq correctness incomplete | Pipeline theorems may not cover every pass | List the pass as TCB; do not take Level 5 |
| Stale `fragment_checker.c` used as target | RFC-019 `draw_ok` still in the file | Never the target (I-5, I-17) |
| Claim inflation | Level 5 language is tempting | §10 sentence; claim scan |
| Cone upgrade “while we are here” | 8.19/Rocq 9.1 looks cleaner | Forbidden (I-6) until a separate plan |

If Phase 4 cannot produce Clight for even `check_oru` on the
pinned toolchain, **stop**. Do not switch to CakeML or
hand-written C. Amend this file or abandon 3.1.

If Phase 7 cannot instantiate CertiCoq's theorem, **keep the
binary as an engineering artifact**, keep flags false, and
write the listed-pass TCB row. That is a successful
*engineering* phase and a failed *Level 5* close. Both must
be sayable.

---

## 13. Sequence (do not reorder)

```
0  lock this document
1  pin sidecar (8.19 + CertiCoq 0.9 + CompCert 3.13.1)
2  isolate six-checker surface; equality to cone
3  port surface to 8.19; observational INVALID witnesses
4  fragment audit; CertiCoq Clight of check_oru
5  Clight of the remaining checkers + dispatcher
6  ccomp + named GC; no gcc-as-success
7  instantiate correspondence; list missing passes
8  differential; mutants; restricted claim; freeze  DONE
   (document freeze; listed TCB; not Level 5)
9  Level 5 close attempt (CompCert AST + LambdaANF;
   Abort unless both I-1 sides Closed)

do not start: 3.2, OP-R1, VM, dust, cone Coq upgrade, CakeML
```

1 before 2 (no surface work on an unpinned plugin).
2 before 3 (port the isolated surface, not the cone).
4 before 6 (CompCert compiles CertiCoq output, not a sketch).
7 before any flag flip (I-1).
8 last (claims are the product).

---

## 14. Gate to start coding

This document is the gate.

Implementation starts at **Phase 1 step 1** (write the CompCert
license decision and `CertiCoqToolchain.md`).

If a step would require a hand-written C extract, a `gcc`
success token sold as CompCert, a global
`compiler_correctness = true` without §10, a cone upgrade to
get the plugin to load, an int64 ASERT, or a Level 5 sentence
about `rigord`, stop and amend this file first.

Phase 0 is accepted when this paragraph is the working rule
and Frontier §3.1 cites this path.

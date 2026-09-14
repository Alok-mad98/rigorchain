# CertiCoq + CompCert Toolchain Pin

Phase 1 step 1 of `release/CertiCoqCompCertPlan.md` (I-4).

This file is the **license decision** and the **version pin**.
It is not a proof, not a binary, and not a TCB shrink.

`COMPILER_CORRECTNESS=false`
`BINARY_SEMANTIC_EQUIVALENCE=false`
`verified_compiler_in_current_tcb = false`
`VERIFIED_COMPILER_FORCED_SWITCH=FALSE`
`EXTRACTION_FORMAL_PROOF=source_identity_only`

---

## Status

| Item | Status |
|---|---|
| Phase 1 step 1 — license decision (I-4) | **DONE** (this file, 2026-09-12) |
| Phase 1 step 2 — sidecar image / opam switch | **DONE** (local OPAMROOT, 2026-09-12; Docker daemon was down) |
| Phase 1 step 3 — `toolchain_probe.sh` | **DONE** (`TOOLCHAIN_PROBE=PASS`, 2026-09-12) |
| Phase 1 step 4 — hello-world Clight + `ccomp` | **DONE** (`HELLO_WORLD=PASS`, 2026-09-12) |
| Phase 1 step 5 — locally downloaded tarball hashes | **DONE** (`PIN_VERIFY=PASS`, 2026-09-12) |
| Phase 1 exit — cone 8.15 `run_kernel_critical.sh` | **DONE** (`FORMAL_KERNEL_PHASE_FINAL_CLOSURE=PASS`, 2026-09-13) |
| Phase 2 — isolate six-checker surface | **DONE** (`SURFACE_BUILD=PASS`, `surface_check_*_eq` Category B, 2026-09-13) |
| Phase 3 — 8.19 port + observational equality | **DONE** (`SURFACE_819_BUILD=PASS`, `OBSERVE_AGREE=PASS`, 2026-09-13) |
| Phase 4 — fragment audit + first Clight (`check_oru`) | **DONE** (`ORU_CLIGHT=PASS`, `FRAGMENT_AUDIT.md`, 2026-09-13) |
| Phase 5 — remaining checkers + dispatcher to Clight | **DONE** (`PHASE5_CLIGHT=PASS`, seven units, 2026-09-13) |
| Phase 6 — `ccomp` + named GC + vector binary | **DONE** (`PHASE6=PASS`, `PHASE6_VECTORS=PASS`, 2026-09-13) |
| Phase 7 — correspondence / listed TCB passes | **DONE** (`PHASE7=PASS`, pipeline **not** instantiated, 2026-09-13) |
| Phase 8 — differential / claims / freeze | **DONE** (document freeze, narrow sentence, 2026-09-13) |
| Phase 9 — Level 5 close attempt | **ABORT** (`PHASE9=ABORT`, I-1 / §10 held, 2026-09-13) |
| Phase 10 — partial pipeline | **ABORT_LEVEL5** (`PHASE10=ABORT_LEVEL5`, `PHASE10_PARTIAL=true`, 2026-09-13) |
| TASK-018 — Closed Clight → CompCert probe | **PART** (`CLIGHT_TO_CMINOR_CLOSED=PASS`, Asm / Level 5 **not** taken, 2026-09-13) |
| TASK 3.1A — Asm boundary audit | **BLOCKED_BY_TCB** (`compile_switch` residual; no `closed_compcert_check_oru`, 2026-09-13) |
| TASK 3.1B — unlift Clight simulation | **PART** (one-step under relation; function/`check_oru` simulation not proved; no Level 5, 2026-09-13) |
| TASK 3.1C — unlift_function_core FS audit | **NOT_FEASIBLE_WITHIN_SCOPE** (no hybrid entry needed; whole-program inverse SimplLocals required; no Level 5, 2026-09-13) |

Decision id: **`L-RESEARCH-SIDECAR-2026-09-12`**.

Cite this id from later Phase 1 scripts. Do not silently pick a
different CompCert, a Coq Platform “open source” subset, or a
newer CertiRocq/Rocq 9.1 stack.

---

## 1. License decision (binding)

### 1.1 Chosen path

**Research sidecar under the INRIA Non-Commercial License.**

Phase 1–7, and any auditor replay of the 3.1 *research* artifact,
use public CompCert **3.13.1** (`coq-compcert` + the `ccomp` that
package’s `make install` produces) installed from the upstream
tarball into a sidecar that is **not** this git tree and **not**
on the `PATH` of `run_kernel_critical.sh`.

CertiCoq **0.9+8.19** is MIT and is installed the same way
(opam / sidecar), not vendored.

This is path **L-RESEARCH-SIDECAR**. The rejected alternatives
are listed in §1.5.

### 1.2 Why this is the only honest first path

Facts on this host (2026-09-12):

| Probe | Result |
|---|---|
| Cone `coqc` | **8.15.0** (`/usr/bin/coqc`, OCaml 4.13.1) |
| `ccomp` | **ABSENT** |
| `opam` | **ABSENT** |
| CertiCoq plugin | **ABSENT** |
| `clang` | PRESENT (Ubuntu 14.0.0) — GC build only |
| Host `ocamlc` | 4.13.1 |

Facts about the software:

| Component | License | Consequence |
|---|---|---|
| CompCert 3.13.1 compiler (`ccomp`, verified backend) | **INRIA Non-Commercial License Agreement** (or a separate AbsInt commercial contract) | Public release is for educational, research, personal, or evaluation use **only**. Commercial use is prohibited. Redistribution of CompCert or derivatives must keep those same terms |
| `coq-compcert` 3.13.1 (opam) | Same: `license: "INRIA Non-Commercial License Agreement"` | Installing the opam package is not a relicensing. It still builds and installs `ccomp` (`./configure … -clightgen` + `make install`) |
| Dual-licensed CompCert files (`cfrontend/Clight.v`, `lib/`, `common/`, …) | INRIA NC **or** LGPL-2.1+ | The LGPL subset is **not** the verified C compiler. It does not replace `ccomp` |
| Coq Platform `*~open_source` CompCert subset | A reduced tree without the verified backend | **Forbidden as a CompCert stand-in** |
| CertiCoq 0.9+8.19 | MIT | Install from upstream; do not pretend it includes CompCert rights |
| Equations 1.3+8.19 | LGPL-2.1-only | Sidecar install |
| MetaCoq 1.3.1+8.19 | MIT | Sidecar install |

I-4 forbids vendoring official INRIA CompCert sources into this
repository. The public license also forbids granting recipients
rights that the NC agreement does not grant. Committing the
CompCert tree (or a built `ccomp`) would be a distribution of a
non-free compiler under a Bitcoin-style public repo, which is
exactly the thing I-4 exists to prevent.

`coq-compcert` **theories alone** do not produce a binary. Phase 6
requires the `ccomp` that the same 3.13.1 build installs. If that
binary cannot be obtained under §1.3, Phase 6 is `CCOMP=ABSENT`,
not `PASS`.

### 1.3 What this decision authorizes

Allowed, as **research / evaluation / education**:

1. Download CompCert 3.13.1 and CertiCoq 0.9+8.19 from the pinned
   URLs in §2 into a **local or CI sidecar** (opam switch or
   container), not into `git add`.
2. Build `ccomp` and the CertiCoq plugin there.
3. Compile the six-checker surface to Clight and through `ccomp`
   for the 3.1 research artifact.
4. Replay that artifact as *evidence that the research path ran*,
   with the NC notice printed by the probe / link script.
5. Cite CompCert’s published Clight-to-assembly theorem as
   applying to CompCert-C, once Phase 7 instantiates it.

### 1.4 What this decision forbids

| Forbidden | Why |
|---|---|
| `git add` of CompCert sources, `ccomp`, or a packed opam switch | I-4; NC redistribution terms |
| Shipping a `ccomp`-produced checker as a **production / mainnet / commercial** binary | NC license: commercial use needs AbsInt |
| Putting sidecar `ccomp` / Coq 8.19 on the cone `PATH` | Cone TCB stays 8.15.0 + faithful OCaml extract |
| Treating Coq Platform open-source CompCert as `ccomp` | Different program |
| Treating `gcc` on CertiCoq C as CompCert | Plan I-5 / Phase 6 |
| Flipping `COMPILER_CORRECTNESS` or `verified_compiler_in_current_tcb` because this file exists | This file is a pin, not a theorem |
| Using CompCert 3.14+ / 3.17+ / 3.18 “because it is newer” | CertiCoq 0.9+8.19 pins **3.13.1** |
| Jumping to CertiRocq / Rocq 9.1 in Phase 1 | Plan Phase 1: not the first pin |

### 1.5 Rejected alternatives (this step)

| Alternative | Why rejected now |
|---|---|
| **L-VENDOR** — copy AbsInt/CompCert into the tree | License + I-4 |
| **L-ABSINT-COMMERCIAL** — buy an AbsInt Usage Agreement and use that `ccomp` for a shippable binary | Not obtained. Required **before** any production/mainnet use of a CompCert-compiled checker. Not a Phase 1 blocker for research |
| **L-THEORIES-ONLY** — install `coq-compcert` Coq files, skip `ccomp` | Plan Phase 1 step 1: stop; no binary |
| **L-GCC-AND-CALL-IT-COMPCERT** | Overclaim |
| **L-OPEN-SOURCE-SUBSET** — Coq Platform `~open_source` CompCert | No verified backend |
| **L-UPGRADE-CONE-TO-8.19** — make CertiCoq load on the auditor cone | I-6 |

### 1.6 Production / commercial follow-up (not Phase 1)

RIGOR is a live-protocol tree. Formalizing a checker is research.
**Shipping** that checker as the binary operators run for
consensus gain is the commercial line the NC license draws.

Until an AbsInt Software Usage Agreement (or a later written
re-decision that a specific use is still non-commercial) exists:

- the compiled 3.1 artifact stays a **research evidence binary**;
- it is not the default node executable;
- `PublicClaims.md` / `FinalClaimsBoundary.md` must not say the
  production node is CompCert-compiled;
- procurement of AbsInt is a **separate** decision, recorded by
  amending this file (new decision id), not by a comment in a
  build script.

Contact for that later path: AbsInt (`sales@absint.com` /
`info@absint.com`), as stated on https://compcert.org/download.html.
This paragraph is not a purchase and not a license.

### 1.7 CI rule (when step 2 exists)

If CI installs CompCert:

1. Install from the §2 URL and checksum, in an ephemeral or
   cached sidecar, never as a committed blob.
2. Print `COMPCERT_LICENSE=INRIA_NON_COMMERCIAL` and
   `L-RESEARCH-SIDECAR-2026-09-12` in the log.
3. Missing `ccomp` ⇒ `CCOMP=ABSENT` / job skip or fail-closed
   for Phase 6, never `COMPILER_CORRECTNESS` theatre.
4. Do not publish `ccomp` as a downloadable product of this
   repository.

---

## 2. Version pin (normative)

Amend this table only by editing this file. Versions are those
CertiCoq 0.9+8.19 declares, not “latest CompCert”.

| Component | Pin | License | Upstream source | Published checksum | Local witness (2026-09-12) |
|---|---|---|---|---|---|
| Sidecar Coq | **8.19.2** | LGPL-2.1 | https://github.com/coq/coq/releases/download/V8.19.2/coq-8.19.2.tar.gz | sha512 `91bc81530fa4f6498961583ad51eac5001f139881788b88e360a866ad8e2a6e2c5bce86d1a580ab4cd4782bf49d48318767df82471ce33ba3ac143e5569ad33c` | MATCH, 7678311 bytes |
| CompCert / `coq-compcert` / `ccomp` / `clightgen` | **3.13.1** | INRIA Non-Commercial | https://github.com/AbsInt/CompCert/archive/v3.13.1.tar.gz | sha512 `828a2f700e32400fc9d3ad2265b67194c48b10fef7f4b6bc1ed5a35d556088cc7defb697faf38c8b871b5b55e58e05f563e827255d2fc9bb964ac5f2cc1024d0` | MATCH, 2798512 bytes |
| CertiCoq | **0.9+8.19** | MIT | https://github.com/CertiCoq/certicoq/releases/download/v0.9+8.19/certicoq-0.9-8.19.tar.gz | sha512 `633a4786adf9d24a5db0ecf1f61a28d659cb7ae994369a259cc7c3d65d9f039669e24dfa55d8c4ec96a9f5a20c7a3ecc52e02ec4be671430bcdccfbec9577250` | MATCH, 1320120 bytes |
| Equations | **1.3+8.19** | LGPL-2.1-only | https://github.com/rocq-prover/equations/archive/refs/tags/v1.3-8.19.tar.gz | sha512 `1b966b6a9fa9fbc23e46dd1238e72f326e674ce1b5c4fee1b8fb274d21cba46a5680d3bd40d0c5aa32b240f2a8c1bb14c4db4576978cd59038905e79c89e4458` | MATCH, 1102852 bytes |
| MetaCoq (erasure + safechecker plugins) | **1.3.1+8.19** | MIT | https://github.com/MetaCoq/metacoq/releases/download/v1.3.1-8.19/v1.3.1-8.19.tar.gz | sha512 `ff4c46e4a86aebfda0673f1dbea0d27751007b103dfe686657fb29237ff24abb119daa41ee4e921be6772686350be81a07a558fb9ff44e619f93d6b6ffde4c26` | MATCH, 4000111 bytes |
| `coq-ext-lib` | **0.12.1** | BSD-2-Clause (package) | https://github.com/coq-community/coq-ext-lib/archive/refs/tags/v0.12.1.tar.gz | sha512 `4c2beeaf7967304b03f0126a0754de268098aa7acc8d5ebb827b19c28cbf243c9c9c839c00b30320ea2b3fa8ae86c6de15e85232df72b0e8bf8b9794ec55d364` | MATCH, 84299 bytes |
| Sidecar OCaml | **4.14.2** (`coq-compcert` 3.13.1 rejects OCaml 5) | LGPL | https://github.com/ocaml/ocaml/archive/4.14.2.tar.gz | opam publishes sha256 `c2d706432f93ba85bd3383fa451d74543c32a4e84a1afaf3e8ace18f7f097b43` | MATCH sha256; local sha512 `61bfa7961aae02458210f10865a5703f7a50a9be2452379e820cf2dfac2c6037da553c31835145e50a776880bdeb2ec78cd7a3147d65f418e7b3e593534e8a04`; 5505119 bytes |
| `clang` (CertiCoq GC) | host or image; recorded 14.0.0 on this machine | LLVM | system | n/a | n/a |

Cone (unchanged; not a sidecar pin):

| Component | Pin | Role |
|---|---|---|
| Auditor Coq | **8.15.0** | `_CoqProject.kernel-critical` |
| Auditor OCaml | **4.13.1** | faithful extract + `ocamlopt` |

`coq-compcert` 3.13.1 accepts Coq `>= 8.12` `< 8.21` and is built
with `-ignore-coq-version` in the opam file. CertiCoq still
requires Coq **8.19**. The sidecar is 8.19, not 8.15 and not 8.20.

Phase 1 step 5 locally downloaded each §2 tarball into
`certicoq/generated/tarballs/` (gitignored; I-4) and compared
`openssl dgst` to the published opam checksums in
`certicoq/pins.sha512`. Result: **`PIN_VERIFY=PASS`**
(2026-09-12). Archives are not committed. Replay:
`certicoq/scripts/verify_pins.sh`.

---

## 3. How the sidecar must sit relative to the cone

```
/usr/bin/coqc          8.15.0     ← run_kernel_critical.sh
sidecar opam switch    8.19 + CertiCoq + ccomp 3.13.1
                       ← Phase 1 scripts only; not exported
                         into the cone PATH
```

Cone walks (`G-1`, source lock, classification, mutation-switch
scan, mutation snapshot) skip the `certicoq/` directory. The
sidecar is not cone source; its CompCert/MetaCoq trees contain
`Axiom` / `Parameter` and would overflow the mutation `tar`
argument list.

Step 2 (next) must:

- install `opam` if needed **without** replacing `/usr/bin/coqc`;
- create a named switch (suggested: `rigor-certicoq-8.19`);
- keep `eval $(opam env)` inside `certicoq/scripts/*` only.

---

## 4. Flags (unchanged by this file)

```
COMPILER_CORRECTNESS=false
BINARY_SEMANTIC_EQUIVALENCE=false
NATIVE_BINARY_VERIFIED          (not raised)
VERIFIED_COMPILER_FORCED_SWITCH=FALSE
verified_compiler_in_current_tcb = false
EXTRACTION_FORMAL_PROOF=source_identity_only
COMPCERT_LICENSE=INRIA_NON_COMMERCIAL
COMPCERT_PATH=L-RESEARCH-SIDECAR-2026-09-12
CCOMP=ABSENT                    (host PATH; sidecar switch has 3.13)
CERTICOQ=ABSENT                 (host PATH; sidecar has 0.9+8.19)
CONE_COQ=8.15.0
SIDECAR_BACKEND=local
SIDECAR_SWITCH=rigor-certicoq-8.19
SIDECAR_COQ=8.19.2
SIDECAR_CCOMP=PRESENT           (only under OPAMROOT, not exported)
```

---

## 5. Next step

Phase 9 **Aborted** (`PHASE9=ABORT`). Phase 10 **Aborted
Level 5** with partial progress (`PHASE10=ABORT_LEVEL5`):
CPS `exp` without `pipeline.v`; no mem Clight; no §10.
I-1 and §10 held. Maturity stays 4.9. Flags stay false.
Do not start 3.2 / OP-R1 / CakeML / a cone Coq upgrade.
Do not take Level 5.

If a later rebuild cannot obtain `ccomp` 3.13.1 under §1.3,
**stop** and amend this file. Do not fall back to §1.5.

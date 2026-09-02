# RIGOR GitHub Actions Repair (P0-081)

**Task:** `TASK-CI-REPAIR-GITHUB-ACTIONS-INFRASTRUCTURE-P0-081`  
**Date:** 2026-09-02  
**Nature:** CI infrastructure only. This TASK does not modify Consensus Kernel,
Transaction Kernel, Ledger, Coq theorems, refinement proofs, authority
manifests, or consensus parameters.

```
CI_STATUS            = PASS   (local re-run of official gates)
COQ_ADMITTED         = 0
COQ_AXIOMS           = 0
REFINEMENT_PROVED    = TRUE
CONSENSUS_KERNEL_MODIFIED = NO
```

---

## 1. Cause

Two independent GitHub Actions failures, both environmental.

### 1.1 Docker pull / `ContainerId`

Jobs `coq-assumptions`, `kernel-gates` (workflow
`consensus-kernel-integrity.yml`) and `coq-protocol-core` (workflow
`formal-verification-gate.yml`) used job-level:

```yaml
container:
  image: coqorg/coq:8.15.0
  options: --user root
```

**`coqorg/coq:8.15.0` is not a published Docker Hub tag.** The `coqorg/coq`
8.15 family (queried 2026-09-02) publishes `8.15`, `8.15.2`, and OCaml /
flambda / native variants of **8.15.2 only**. There is no `8.15.0` tag.

GitHub-hosted runners therefore:

1. Run `docker pull coqorg/coq:8.15.0`
2. Fail with **Docker pull failed with exit code 1**
3. Never create a container, so `ContainerId` is null
4. Surface the runner bug/limitation:
   **Value cannot be null. (Parameter 'ContainerId')**

Because this happens **before any job step**, the job cannot print a useful
error, cannot skip remaining gates cleanly, and cannot record Coq / OCaml
versions. A Hub outage or rate-limit on a *valid* tag would fail the same way:
job-level `container:` is not recoverable.

Secondary Docker facts from the audit:

| Check | Result |
|-------|--------|
| `container:` | Three jobs (removed) |
| `services:` | None |
| docker login / private registry | None |
| pull policy | GitHub default (always pull job container) |
| image tag | `8.15.0` — **does not exist** |
| `latest` | Not used (and now forbidden by helper) |
| architecture | Hub 8.15 images are `linux/amd64` only; would have been incompatible with arm64 runners even if the tag existed |

Coq is only needed as a pinned **8.15.0** toolchain. The image was not
providing protocol state. It is replaced by `ubuntu-latest` + opam.

### 1.2 Node.js 20 deprecation

All workflows used `actions/checkout@v4` and `actions/setup-python@v5`.
Those actions still declare the Node 20 runtime. GitHub runners warn:

> Node.js 20 actions are deprecated.

Node 20 reached EOL in April 2026. Runners default to Node 24 (June 2026)
and remove Node 20 (September 2026). Official replacements:

| Action | Was | Now | Runtime |
|--------|-----|-----|---------|
| `actions/checkout` | v4 | v6 | Node 24 |
| `actions/setup-python` | v5 | v6 | Node 24 |
| `actions/cache` | unused | v5 | Node 24 |

---

## 2. Modified files

**Changed**

- `.github/workflows/consensus-kernel-integrity.yml`
- `.github/workflows/formal-verification-gate.yml`
- `.github/workflows/no-global-state-lint.yml`
- `formal_verification/scripts/run_differential_gate.sh`
  (JSON lockfiles compared semantically; `.v` / `.inc` still byte-compared.
  No Coq source, theorem, or vector content changed.)

**Added (CI only)**

- `.github/actions/setup-coq/action.yml`
- `tools/ci/ci-environment-preflight.sh`
- `tools/ci/setup-coq-8.15.0.sh`
- `tools/ci/print-reproducibility.sh`
- `tools/ci/docker-pull-or-fail.sh`
- `tools/ci/test_docker_failure_contract.sh`
- `docs/ci/RIGOR_GITHUB_ACTIONS_REPAIR_P0_081.md` (this file)

**Not modified:** Consensus Kernel C++, Transaction Kernel, Ledger, any `.v`
file, refinement proofs, authority manifests, consensus parameters.

---

## 3. Fix

### A. Workflow audit

Three workflow files under `.github/workflows/`. No other official gates use
`container:`, `services:`, docker login, or a registry mirror.

### B. Remove unreliable Docker dependency

Coq jobs now run on **`ubuntu-latest`** with a composite action that:

1. Caches `~/.opam` via `actions/cache@v5` (key includes OS / arch / image
   version + `coq-8.15.0` + `ocaml-4.13.1`)
2. Installs **opam 2.2.1** (pinned GitHub release binary, x86_64 or arm64)
3. Creates switch `rigor-coq-8.15.0-ocaml-4.13.1`
4. `opam install coq.8.15.0`
5. Refuses Ubuntu apt `coq` and refuses `:latest` tags

This matches the historical local pin (`Coq 8.15.0`, `ocamlopt 4.13.1` in
`formal_verification/production_consensus_kernel_build.json`). Coq 8.15.x
scripts (`run_protocol_core_coq.sh`, extraction) keep accepting 8.15.x;
the installer additionally requires the **8.15.0** patch.

If a future job must pull an image, it must call
`tools/ci/docker-pull-or-fail.sh <image:pinned-tag>` — never job-level
`container:`.

### C. Container failure handling

Job-level `container:` is forbidden. Preflight
(`tools/ci/ci-environment-preflight.sh`) detects `GITHUB_ACTIONS` +
`/.dockerenv` and exits immediately with:

```
CI_DOCKER_ENVIRONMENT_FAILURE
```

`tools/ci/docker-pull-or-fail.sh` prints the same token when:

- the image is missing / untagged / `:latest`
- `docker` is not on `PATH`
- `docker pull` fails
- `docker create` returns an empty `ContainerId`

A failed step stops the job: later gates do not run. The `if: failure()`
integrity step still emits `CONSENSUS_KERNEL_INTEGRITY_FAILED`.

`tools/ci/test_docker_failure_contract.sh` is run on `authority-lock` so the
token cannot regress silently.

### D. Action upgrades

Every `uses:` of checkout / setup-python / cache in `.github/` now targets
Node 24 majors (`@v6` / `@v6` / `@v5`).

### E. Reproducibility block

Every job runs `tools/ci/print-reproducibility.sh` after toolchain setup and
prints:

```
RUNNER_OS
RUNNER_ARCH
RUNNER_KERNEL
RUNNER_DISTRO
COMPILER_VERSION
COQ_VERSION
OCAML_VERSION
PYTHON_VERSION
DOCKER_IMAGE
DOCKER_IMAGE_DIGEST
```

When Docker is not used (current official gates):

```
DOCKER_IMAGE=not_used
DOCKER_IMAGE_DIGEST=not_used
```

Successful jobs also print `CI_STATUS=PASS`. Coq jobs additionally echo
`COQ_ADMITTED=0`, `COQ_AXIOMS=0`; `kernel-gates` echoes
`REFINEMENT_PROVED=TRUE`.

The differential gate used `cmp` on `coq_harness_index.json`. The committed
file is `json.dump(..., indent=2)` plus a trailing newline; the generator
writes `indent=1` with no trailing newline. The **vector set is identical**
(433 rows). Byte-compare would fail on every GitHub runner after Docker was
removed. The gate now uses semantic JSON equality for `.json` lockfiles only.

---

## 4. Reproducibility information (local re-run)

Recorded on the repair host while re-running the three official gates.
GitHub-hosted `ubuntu-latest` will print its own block in the job log
(typically Ubuntu 24.04, g++ 13, Python 3.10 from `setup-python@v6`, Coq
8.15.0 / OCaml 4.13.1 from the opam switch).

```
RUNNER_OS=Linux
RUNNER_ARCH=x86_64
RUNNER_KERNEL=6.6.87.2-microsoft-standard-WSL2
RUNNER_DISTRO=Ubuntu 22.04.5 LTS
COMPILER_VERSION=g++ (Ubuntu 11.4.0-1ubuntu1~22.04.3) 11.4.0
COQ_VERSION=The Coq Proof Assistant, version 8.15.0
OCAML_VERSION=4.13.1
PYTHON_VERSION=Python 3.10.12
DOCKER_IMAGE=not_used
DOCKER_IMAGE_DIGEST=not_used
```

Pinned CI toolchain:

| Component | Pin |
|-----------|-----|
| Coq | 8.15.0 (opam `coq.8.15.0`) |
| OCaml | 4.13.1 |
| opam | 2.2.1 |
| Python (GHA) | 3.10 |
| Docker image | not used |

---

## 5. Gate re-run

Commands (same as the GitHub jobs, without the runner wrapper):

```bash
bash tools/ci/test_docker_failure_contract.sh
# authority-lock: python lints + SKIP_COQ=1 governance (see workflow)
python3 tools/check-coq-consensus-assumptions.py
bash formal_verification/run_consensus_kernel_refinement_gate.sh
SKIP_TEST_RIGOR=1 bash formal_verification/scripts/run_differential_gate.sh
bash formal_verification/run_transaction_kernel_refinement.sh
```

Local re-run on this host (2026-09-02). GitHub `gh run` was not readable
(`Forbidden`); hosted-runner evidence will appear on the next push.

| Job | Result | Evidence |
|-----|--------|----------|
| CI docker-failure contract | PASS | `CI_DOCKER_FAILURE_CONTRACT=PASS` (empty / untagged / `:latest` all print `CI_DOCKER_ENVIRONMENT_FAILURE`) |
| authority-lock | PASS | `AUTHORITY_LOCK=PASS` `CI_STATUS=PASS` |
| coq-assumptions | PASS | `FORMAL_ASSUMPTION_GATE=PASS` `COQ_ADMITTED=0` `COQ_AXIOMS=0` `COQ_PARAMETERS=0`; Print Assumptions: Closed under the global context |
| kernel-gates | PASS | `CONSENSUS_KERNEL_REFINEMENT_EXECUTION_CLOSED` `P056_KERNEL_BOUNDARY_REFINEMENT_PROVED=true` `differential gate: PASS` `TRANSACTION_KERNEL_REFINEMENT=CLOSED` `CI_STATUS=PASS` |

```
CI_STATUS            = PASS
COQ_ADMITTED         = 0
COQ_AXIOMS           = 0
REFINEMENT_PROVED    = TRUE
```

This is a CI environment repair, not a protocol modification.

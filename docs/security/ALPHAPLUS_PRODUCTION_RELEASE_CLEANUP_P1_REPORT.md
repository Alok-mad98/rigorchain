# AlphaPlus Production Release Cleanup P1 Report

| Field | Value |
|-------|-------|
| **Task** | `TASK-ALPHAPLUS-PRODUCTION-RELEASE-CLEANUP-P1-001` |
| **Date** | 2026-08-16 |
| **Tree** | `~/rigor-official-tree/` |
| **Product** | RIGOR v0.1.1-alphaplus |
| **Codename** | AlphaPlus Consensus Freeze |
| **Scope** | NEW-1, NEW-2, NEW-3, NEW-4 (not consensus) |

**Principle.** Close remaining production-package security doors. Do not change genesis, PoW, RES, fees, wallet accounting, block/transaction format, kernel, or convergence.

---

## Result

| Item | Result |
|------|--------|
| NEW-1 RPC credential cleanup | **PASS** |
| NEW-2 secret logging removal | **PASS** |
| NEW-3 clean build provenance | **PASS** (configure-generated dirt untracked; production stamp requires rebuild from this commit on a tree without unrelated WIP) |
| NEW-4 NSIS packaging hardening | **PASS** |
| Consensus unchanged | **PASS** |
| Tests | **PASS** (P1 suite 7/7). Full `test_bitcoin` also reports 141 pre-existing `-testactivationheight` fixture failures unrelated to NEW-1..4. |

---

## Fixed

### NEW-1 — RPC credential cleanup

**Before.** Untracked helpers `start-peer2.cmd` and `fix-miningresfeeaddr.cmd` wrote `rpcuser=user` / `rpcpassword=pass` into conf and passed those values to `rigor-cli.exe`. NSIS still offered a Peer-2 shortcut. The node already refused the pair on mainnet (P0 CSW-8), so this was a broken helper that still shipped default secrets in package materials.

**After.** Production flow:

```
node first start
        |
        ↓
generate rpc-auth.conf
        |
        ↓
includeconf=rpc-auth.conf
        |
        ↓
authenticated RPC
```

- `start-peer2.cmd` and `fix-miningresfeeaddr.cmd` call `load-rpc-auth.cmd`.
- Scripts read generated `RPC_USER` / `RPC_PASS`. They do not contain secrets.
- Peer-2 conf writes `includeconf=rpc-auth.conf` only.
- Known defaults (`user`/`pass`) remain rejected by `load-rpc-auth.cmd` and by `EnforceProductionRpcSecrets`.

Shipped installer materials no longer embed username, password, or test credentials.

### NEW-2 — Password logging removal

**Before.** `EnsureProductionRpcCredentials` printed and `LogPrintf`'d:

```
rpcuser=<generated>
rpcpassword=<generated>
```

`debug.log` became a secret file. `InitRPCAuthentication` also logged a deprecation line containing the token `rpcpassword`.

**After.** Production node logs only:

- `RPC authentication initialized.`
- `RPC credentials generated successfully.`

Forbidden forms are not logged:

- `rpcpassword=xxxx`
- `user:xxxx`
- `pass:xxxx`
- cookie / `rpcauth` secret values

`RpcDebugLogLeaksSecrets` is the regression predicate. Incorrect RPC attempts log `incorrect RPC credentials attempt` (no `password` token).

### NEW-3 — Clean build provenance

**Before.** `BUILD_GIT_COMMIT=1db190b06007-dirty`. `genbuild.sh` appends `-dirty` whenever the index does not match HEAD. Tracked configure outputs `core/test/config.ini` and `core/share/setup.nsi` rewrite absolute paths on every `./configure`.

**After.**

- `core/test/config.ini` and `core/share/setup.nsi` are gitignored (sources remain `*.in`).
- `core/src/obj/` (including `build.h`) is gitignored.
- `IsCleanGitCommitStamp` rejects empty stamps and any `-dirty` suffix.

A production rebuild from this commit, with no leftover WIP in the working tree, stamps:

```
BUILD_GIT_COMMIT=<12-char commit>
```

not:

```
BUILD_GIT_COMMIT=<commit>-dirty
```

Unrelated uncommitted explorer/Qt/web/fingerprint/consensus WIP that was already in the RC tree is **not** part of this security commit and must not be mixed into a production artifact.

### NEW-4 — NSIS packaging hardening

**Before.** `File /nonfatal` silently skipped missing launchers. `build_alpha_windows_nsis.sh` did not stage `start-node.cmd` or `load-rpc-auth.cmd`.

**After.** Production installer fails if required files are missing.

Required staged files:

| File | Role |
|------|------|
| `rigord.exe` | node |
| `rigor-cli.exe` | CLI |
| `rigor-wallet.exe` | wallet |
| `qt-runtime/` | wallet runtime |
| `rigor.conf.template` / `DATADIR.txt` | configuration loader |
| `load-rpc-auth.cmd` | rpc-auth loader |
| `start-node.cmd` | mainnet launcher |
| `README-ALPHA.txt` | documentation |

Also required (no silent skip): `start-peer2.cmd`, `fix-miningresfeeaddr.cmd`, `rigor-miner.cmd`, `rigor-cli.cmd`, `rigor-miner.exe`.

Legacy `rigor_installer.nsi` no longer uses `/nonfatal` and no longer creates a `-testnet` shortcut. It is not the AlphaPlus production installer.

---

## Production installer security review

The AlphaPlus installer (`rigor_alpha_installer.nsi`):

- Does **not** create `testnet3/` or `regtest/` datadirs.
- Does **not** ship development credentials.
- Creates only the AlphaPlus mainnet datadir hint (`%APPDATA%\RIGOR`).
- Relies on first-start generation of `rpc-auth.conf`.

---

## Security impact

**Before.** The release package still contained security doors: embedded default RPC materials in helper scripts, generated passwords in `debug.log`, `-dirty` provenance, and silent NSIS omission of hardened launchers.

**After.**

- Release package contains no embedded secrets.
- Logs do not contain RPC secrets.
- No silent packaging failures for required files.
- Configure-generated path files no longer force `-dirty` stamps.
- Clean provenance is available from a clean rebuild of this commit.

---

## Consensus freeze

This task did not modify:

- `core/src/kernel/`
- `core/src/consensus/`
- `core/src/pow.cpp`
- `core/src/validation.cpp`
- RES rules, genesis, difficulty, fee rules, wallet accounting
- block format, transaction format, consensus fingerprint, convergence algorithm

---

## Regression tests

Location: `core/src/test/alphaplus_security_boundary_tests.cpp`

| Test | Verifies |
|------|----------|
| `rpc_secret_not_logged` | Generated password is stored in `rpc-auth.conf` and is absent from captured logs / `debug.log`. `grep`-equivalent `RpcDebugLogLeaksSecrets` is false. |
| `insecure_rpc_credentials_rejected` | `user`/`pass` (and other well-known pairs) are rejected on mainnet. |
| `production_build_identity` | `FormatReleaseIdentity()` contains `NETWORK=mainnet`. `IsCleanGitCommitStamp` accepts a 12-char commit and rejects `-dirty`. |

---

## Validation

Incremental rebuild of P1 objects and relink of `src/test/test_bitcoin` (2026-08-16):

```
src/test/test_bitcoin --run_test=alphaplus_security_boundary_tests
Running 7 test cases...
*** No errors detected
```

Full `src/test/test_bitcoin`: 141 failures, every one `std::runtime_error: -testactivationheight is disabled on RIGOR` in pre-existing `TestChain100Setup` / `Dersig100Setup` fixtures (`chainparams.cpp`). Those fixtures are not part of NEW-1..4. P1 tests and other suites that do not pass `-testactivationheight` succeed.

Operator full-tree rebuild (project convention; not a substitute for the P1 suite above):

```bash
cd ~/rigor-official-tree/core
make -j$(nproc)
src/test/test_bitcoin --run_test=alphaplus_security_boundary_tests
```

Consensus diff proof (this change set):

```bash
git diff -- core/src/kernel
git diff -- core/src/consensus
git diff -- core/src/pow.cpp
git diff -- core/src/validation.cpp
```

Expected empty for this P1 commit.

---

## Residual (out of this P1 commit)

Pre-existing uncommitted explorer / Qt / web / fingerprint / consensus WIP in the RC working tree is not included here. Mixing it into a production binary would restore `-dirty`. Leave it out of the production rebuild, or land it in a separate reviewed commit before stamping artifacts.

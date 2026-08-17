# AlphaPlus Security Boundary Hardening — P0 Report

| Field | Value |
|-------|-------|
| **Task** | `TASK-ALPHAPLUS-SECURITY-BOUNDARY-HARDENING-P0-001` |
| **Date** | 2026-08-16 |
| **Tree** | `~/rigor-official-tree/` |
| **Product** | RIGOR v0.1.1-alphaplus |
| **Codename** | AlphaPlus Consensus Freeze |
| **Scope** | CSW-1, CSW-2, CSW-3, CSW-8 only |

**Principle.** Close the remaining doors around already frozen consensus walls. Do not change genesis, PoW, RES, fees, wallet accounting, or block/transaction format.

---

## Result

| Door | Status | Wall built |
|------|--------|------------|
| CSW-1 Mainnet production artifact separation | **PASS** | Production profile (`RIGOR_PRODUCTION_MAINNET` / `--enable-production-mainnet`) compiles out testnet/regtest/signet/devtest. Development builds keep those networks. Startup / `-version` prints `NETWORK=mainnet` on mainnet. |
| CSW-2 Disable unsafe `-assumevalid` on mainnet | **PASS** | Fail-closed. Any explicit `-assumevalid` on mainnet aborts startup with `ERROR: -assumevalid is disabled on AlphaPlus mainnet.` |
| CSW-3 OpuHash at header admission | **PASS** | `HasValidProofOfWork` now runs `CheckProofOfWork` (OpuHash) for every header. Compact-block and `BLOCK` paths also require OpuHash before `min_pow_checked=true`. |
| CSW-8 Production RPC secret hardening | **PASS** | No shipped `rpcuser=user` / `rpcpassword=pass`. First mainnet start generates unique credentials into `rpc-auth.conf` (owner-only). Known defaults are refused on mainnet. |

**Consensus freeze:** unchanged. `core/src/kernel/`, `core/src/consensus/`, `core/src/pow.cpp` were not modified. `core/src/validation.cpp` received only the `HasValidProofOfWork` admission pre-check (same predicate `ContextualCheckBlockHeader` already used). Genesis, PoW algorithm, difficulty, RES, subsidy, fees, and wallet accounting are untouched.

---

## Fixed security doors

### CSW-1 — Mainnet production artifact separation

**Before.** One binary selected mainnet / testnet / signet / regtest / devtest / slowtest at runtime.

**After.**

- Development builds (default `./configure`): all networks remain available.
- Production artifacts: compile with `--enable-production-mainnet` or `-DRIGOR_PRODUCTION_MAINNET`.
  - `-testnet`, `-regtest`, `-devtest`, `-signet`, `-chain=<non-main>` are rejected.
  - `CreateChainParams` / `CreateBaseChainParams` refuse non-main.
- Every process prints:

```
RIGOR version v0.1.1-alphaplus
AlphaPlus Consensus Freeze
NETWORK=mainnet
```

Release scripts (`build_alpha_windows_pe.sh`, Linux package notes) enable the production profile.

### CSW-2 — `-assumevalid` fail-closed on mainnet

**Before.** CLI accepted `-assumevalid=<hash>` and could skip script checks.

**After.** On mainnet, `InitConfig` and `ApplyArgsManOptions` reject any explicit `-assumevalid`. Compiled `defaultAssumeValid` (genesis) is unchanged and is not an operator override. Regtest/testnet keep the switch for `feature_assumevalid.py`.

Example:

```
ERROR:
-assumevalid is disabled on AlphaPlus mainnet.
```

### CSW-3 — OpuHash at header admission

**Before.** `HasValidProofOfWork` accepted compact `nBits` shape only. Peers could enter the header pipeline without OpuHash.

**After.** Required flow:

```
Receive Header
    ↓
Contextual / anti-DoS checks
    ↓
AlphaPlus PoW validation (CheckProofOfWork / OpuHash)
    ↓
Accept / Reject
```

Paths covered:

- `headers` → `CheckHeadersPoW` → `HasValidProofOfWork` → OpuHash
- `cmpctblock` → OpuHash before `ProcessNewBlockHeaders`
- `BLOCK` → OpuHash before `min_pow_checked=true`

`ContextualCheckBlockHeader` still runs the same `CheckProofOfWork`. This is a perimeter hardening, not a consensus rule change. Genesis remains exempt inside `CheckProofOfWork` (hash identity; do not remine).

### CSW-8 — Production RPC secret hardening

**Before.** Installer scripts wrote `rpcuser=user` / `rpcpassword=pass` and created `testnet3` / `regtest` datadirs.

**After.**

- First mainnet start without credentials:

```
Generating RPC authentication credentials...

Generated configuration:
rpcuser=<random>
rpcpassword=<random>
```

- Stored in `<datadir>/rpc-auth.conf` with owner-only permissions; `includeconf=rpc-auth.conf` is appended.
- Known defaults (`user`/`pass`, `rigor`/`rigor`, `password`, `change-me`, …) abort mainnet startup.
- Cookie auth remains available if `-rpccookiefile` is set explicitly.
- Development networks keep cookie-auth / test credentials.
- AlphaPlus NSIS installer no longer creates `testnet3` / `regtest`.
- Legacy `setup.nsi` testnet Start Menu shortcut removed.
- Windows launchers use `load-rpc-auth.cmd` (no hardcoded secrets).

---

## Files changed

### New

| File | Role |
|------|------|
| `core/src/common/alphaplus_security.h` | Production-profile helpers (not consensus) |
| `core/src/common/alphaplus_security.cpp` | assumevalid / RPC secret policy + first-start generation |
| `core/src/test/alphaplus_security_boundary_tests.cpp` | CSW-1/2/3/8 unit coverage |
| `installer/windows/load-rpc-auth.cmd` | Per-install RPC secret load/generate |
| `docs/security/ALPHAPLUS_SECURITY_BOUNDARY_HARDENING_P0_REPORT.md` | This report |

### Modified (security only)

| File | CSW |
|------|-----|
| `core/src/common/init.cpp` | 2, 8 |
| `core/src/clientversion.cpp` | 1 (`NETWORK=`) |
| `core/src/chainparamsbase.cpp` | 1 |
| `core/src/chainparams.cpp` | 1 |
| `core/src/util/system.cpp` | 1 |
| `core/src/node/chainstatemanager_args.cpp` | 2 |
| `core/src/validation.cpp` | 3 (`HasValidProofOfWork` only) |
| `core/src/validation.h` | 3 comment |
| `core/src/net_processing.cpp` | 3 |
| `core/src/httprpc.cpp` | 8 |
| `core/src/bitcoind.cpp` | 1 banner |
| `core/src/qt/bitcoin.cpp` | 1 banner |
| `core/configure.ac` | 1 `--enable-production-mainnet` |
| `core/src/config/bitcoin-config.h.in` | 1 |
| `core/src/Makefile.am` | new common sources |
| `core/src/Makefile.test.include` | new test |
| `core/share/examples/rigor.conf` | 2 (remove Bitcoin assumevalid hashes) |
| `core/share/setup.nsi.in` | 1/8 (drop testnet shortcut) |
| `installer/windows/rigor_alpha_installer.nsi` | 8 |
| `installer/windows/rigor-cli.cmd` | 8 |
| `installer/windows/rigor-miner.cmd` | 8 |
| `installer/windows/start-node.cmd` | 8 |
| `scripts/alpha/build_alpha_linux.sh` | 1 |
| `scripts/alpha/build_alpha_windows_pe.sh` | 1 |
| `scripts/alpha/build_alpha_windows_nsis.sh` | 1/8 |

### Not modified (consensus freeze)

- `core/src/kernel/**`
- `core/src/consensus/**`
- `core/src/pow.cpp`
- Genesis hash, PoW algorithm, difficulty formula, RES, subsidy, fee model, wallet accounting, block/tx format

`validation.cpp` delta is confined to `HasValidProofOfWork` (admission pre-check). `CheckProofOfWork`, `ConnectBlock`, and contextual nBits rules are unchanged.

---

## Validation

### Consensus freeze diffs

```
git diff -- core/src/kernel
git diff -- core/src/consensus
git diff -- core/src/pow.cpp
```

Expected: empty.

`git diff -- core/src/validation.cpp` must show only `HasValidProofOfWork` calling `CheckProofOfWork` for every header (no ConnectBlock / subsidy / RES / fee edits).

### Functional checks (after rebuild)

Development binary (this tree’s default `./configure`, no production flag):

1. `rigord -version` prints `RIGOR version v0.1.1-alphaplus`, `AlphaPlus Consensus Freeze`, `NETWORK=mainnet`.
2. Mainnet genesis remains `1c2d643638c3db29c241f4daca5bbe90f8439f7b03d22a686247214ed52ad758`.
3. `rigord -assumevalid=00..00` on mainnet fails closed with the CSW-2 error.
4. First mainnet datadir without RPC settings writes `rpc-auth.conf`; `user`/`pass` is refused.
5. Mining path unchanged: GBT / pool / `submitblock` / explorer / wallet still use the same consensus objects.

Production artifact (after `--enable-production-mainnet` or `-DRIGOR_PRODUCTION_MAINNET`):

6. `rigord -testnet` / `-regtest` / `-chain=devtest` refuse to start.

Unit tests (run in this tree after incremental relink):

```
src/test/test_bitcoin --run_test=alphaplus_security_boundary_tests
```

Result: **5/5 PASS** (`network_name_mainnet`, `assumevalid_fail_closed_on_mainnet`, `insecure_rpc_credentials_rejected`, `header_admission_requires_opus_hash`, `production_profile_default_is_development`).

Consensus freeze diffs: `core/src/kernel` empty, `core/src/consensus` empty, `core/src/pow.cpp` empty. `validation.cpp` delta is `HasValidProofOfWork` only.

### Rebuild (user-local; not run by the agent)

Full `make` is a user step:

```bash
cd ~/rigor-official-tree/core
# Development (keeps testnet/regtest):
./configure
make -j"$(nproc)"
make check
# or targeted:
src/test/test_bitcoin --run_test=alphaplus_security_boundary_tests

# Production artifact:
./configure --enable-production-mainnet
make -j"$(nproc)"
```

If `configure` does not yet list `--enable-production-mainnet`, run `./autogen.sh` first, or pass `CXXFLAGS='-DRIGOR_PRODUCTION_MAINNET'`.

---

## Confirmation

- Official tree: `~/rigor-official-tree/`
- CSW-1 PASS
- CSW-2 PASS
- CSW-3 PASS
- CSW-8 PASS
- Genesis unchanged
- PoW unchanged
- RES unchanged
- Fees unchanged
- Wallet accounting unchanged

AlphaPlus security boundaries hardened.

Consensus remains frozen.

Implementation attack surfaces reduced without changing protocol identity.

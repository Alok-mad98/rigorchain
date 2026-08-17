# AlphaPlus Security Boundary Re-Audit

| Field | Value |
|-------|-------|
| **Task** | `TASK-ALPHAPLUS-SECURITY-BOUNDARY-RE-AUDIT-001` |
| **Date** | 2026-08-16 |
| **Tree** | `~/rigor-official-tree/` (official AlphaPlus source tree) |
| **Branch** | `mainnet-release-candidate-001` |
| **HEAD** | `1db190b0600747b5bc238aaa05bf4ba193a37cfe` — `security: harden AlphaPlus P0 security boundaries` |
| **HEAD~1** | `1a799f06ffdac771df7b86bf61a073ffcee9d0eb` — `release: finalize AlphaPlus miner compatibility baseline` |
| **Product** | RIGOR v0.1.1-alphaplus |
| **Codename** | AlphaPlus Consensus Freeze |
| **Mode** | Audit only — no code change, no commit, no push, no tag, no merge |
| **Prior audit** | `TASK-ALPHAPLUS-ARCHITECTURE-SECURITY-BOUNDARY-AUDIT-001` |
| **Hardening** | `TASK-ALPHAPLUS-SECURITY-BOUNDARY-HARDENING-P0-001` |

**Principle.** A wall cannot be bypassed, silently overridden, or left to developer assumption. A door accepts unsafe input, allows a hidden bypass, or depends on trust.

**Final verdict: PARTIAL (remaining priority P1)**

The four P0 doors targeted by hardening (CSW-1, CSW-2, CSW-3, CSW-8) are closed on the committed perimeter. Consensus identity is unchanged. No new P0 door was introduced. Additional P1 doors remain (known Wave-1 surfaces plus packaging leftovers in this tree).

---

## Scope and method

All inspection was performed inside `~/rigor-official-tree/`. No second clone. No file copies for inspection. No modification of consensus, wallet, pool, explorer, or installer **code**. This report file is the only write produced by the task.

Commands required by the task:

```
git status
git diff HEAD~1 -- core/src/kernel
git diff HEAD~1 -- core/src/consensus
git diff HEAD~1 -- core/src/pow.cpp
git diff HEAD~1 -- core/src/validation.cpp
```

`git diff HEAD~1` without a second commit mixes the hardening commit with the dirty working tree. Classification below uses `git diff HEAD~1 HEAD` for the hardening commit and `git status` / `git diff HEAD` for the actual on-disk tree.

Live checks on this tree’s development `core/src/rigord` (configure without `--enable-production-mainnet`):

```
./src/rigord -version
./src/rigord -assumevalid=<random_hash>
./src/test/test_bitcoin --run_test=alphaplus_security_boundary_tests
```

Plus first-start RPC generation and known-default rejection against temporary datadirs.

---

## Previous audit findings (doors that were open)

From `TASK-ALPHAPLUS-ARCHITECTURE-SECURITY-BOUNDARY-AUDIT-001`:

| ID | Door | Priority then |
|----|------|----------------|
| **CSW-1** | One binary selected mainnet / testnet / signet / regtest / devtest / slowtest at runtime | P0 |
| **CSW-2** | `-assumevalid=<hex>` could skip script verification on mainnet | P0 |
| **CSW-3** | `HasValidProofOfWork` accepted compact `nBits` shape, not OpuHash | P0 |
| **CSW-8** | Installer / `start-node.cmd` shipped `rpcuser=user` / `rpcpassword=pass`; created `testnet3` / `regtest` | P0 |
| CSW-4 | Pool share-validator still required body membership (pre–A-9-C) | P1 |
| CSW-5 | Pool PPLNS / replay in RAM; trusted GBT `coinbasevalue` | P1 |
| CSW-6 | Wallet private export / import exist whenever unlocked | P1 |
| CSW-7 | Explorer partly a second ledger; defaults unsafe | P1 |
| CSW-9 | Auto-settle Convergence default ON | P2 |
| CSW-10 | Bitcoin-era leftover branches (BIP30, legacy retarget, variable-T APIs) | P2 |
| P0-6 | `-reindex-chainstate` can skip contextual header/block checks | P0 (not in hardening scope) |

Hardening P0 claimed to convert CSW-1, CSW-2, CSW-3, and CSW-8 into walls. This re-audit verifies those claims against the actual tree.

---

## Consensus integrity

### Required diffs (`HEAD~1` → `HEAD`)

| Path | Result | Classification |
|------|--------|----------------|
| `core/src/kernel/` | empty | unchanged |
| `core/src/consensus/` (tracked) | empty | unchanged |
| `core/src/pow.cpp` | empty | unchanged |
| `core/src/validation.cpp` | 12 lines | **validation boundary change only** |

`HasValidProofOfWork` now calls `CheckProofOfWork` for every header. Compact-target-only admission is gone. `ContextualCheckBlockHeader`, `ConnectBlock`, subsidy, RES, fees, and wallet accounting were not edited in the hardening commit.

### Freeze checklist

| Item | Status | Evidence |
|------|--------|----------|
| Genesis | unchanged | `kernel/chainparams.cpp` assert `0x1c2d643638c3db29…d758`; not in hardening diff |
| PoW algorithm | unchanged | `pow.cpp` empty vs `HEAD~1` |
| Difficulty calculation | unchanged | ASERT path not touched |
| RES rules | unchanged | `convergence_logic.cpp` / `rigor_consensus_lock.h` not in hardening diff |
| Convergence selection | unchanged | same |
| Winner selection | unchanged | same |
| Subsidy | unchanged | `GetBlockSubsidy` not in hardening diff |
| Fee model | unchanged | `consensus_rigor/fee` not in hardening diff |
| Wallet accounting | unchanged | no wallet spend-engine edits in hardening commit |

### Working-tree note (actual on-disk tree)

`git status` is **not clean**. Untracked `core/src/consensus/fingerprint.cpp` / `fingerprint.h` exist and are compiled as an observability library. The header states the API is **not** a consensus rule and must not be called from validation, PoW, RES, fee, or wallet-accounting paths. Tracked consensus sources still have an empty `git diff HEAD`.

Uncommitted edits exist in `core/src/node/miner.cpp`, `core/src/init.cpp`, `core/src/rpc/mining.cpp`, Qt, explorer, installer, and web. Those are **not** the hardening commit. They do not change genesis / PoW / RES / subsidy / fee identity. They **do** mean this tree is not a frozen release snapshot.

Live binary identity:

```
RIGOR version v0.1.1-alphaplus
AlphaPlus Consensus Freeze
BUILD_GIT_COMMIT=1db190b06007-dirty
NETWORK=mainnet
```

---

## Section 1 — CSW-1 Production Artifact Boundary

**Previous risk.** One binary exposed mainnet, testnet, regtest, and development networks.

**Wall built.** Compile-time profile `RIGOR_PRODUCTION_MAINNET` / `--enable-production-mainnet`:

- `ArgsManager::GetChainName` rejects `-testnet`, `-regtest`, `-devtest`, `-signet`, and `-chain=<non-main>`.
- `CreateChainParams` / `CreateBaseChainParams` refuse non-main.
- Development `./configure` (this tree’s current `bitcoin-config.h`: `RIGOR_PRODUCTION_MAINNET` undefined) keeps those networks.
- Release PE script always adds `CXXFLAGS=-DRIGOR_PRODUCTION_MAINNET`.
- NSIS no longer creates `testnet3` / `regtest`.
- `FormatReleaseIdentity` prints `NETWORK=`.

**Live test (development binary in this tree):**

```
$ ./src/rigord -version
RIGOR version v0.1.1-alphaplus
AlphaPlus Consensus Freeze
NETWORK=mainnet
```

`./src/rigord -testnet -version` still starts and prints `NETWORK=testnet`. That is expected for a development build. It is **not** the production artifact.

**Audit questions.**

| Question | Answer |
|----------|--------|
| Can a user accidentally start testnet on a **production** artifact? | No. Flags and `-chain` throw. |
| Can a user accidentally start testnet on **this tree’s current binary**? | Yes. This binary is a development build. |
| Can a production installer create test directories? | No. NSIS comment and code omit `testnet3` / `regtest`. |
| Are development networks separated? | Yes, by compile flag — not by a second `rigor-test` binary. |

**Residual (not a CSW-1 fail).** `-help` on the development binary still documents `-testnet` / `-regtest` / `-devtest`. Production artifacts must actually be built with the flag. `FormatReleaseIdentity` defaults to `NETWORK=mainnet` if `GetChainName()` throws (display only).

**Result: CSW-1 PASS**

---

## Section 2 — CSW-2 assumevalid Boundary

**Previous risk.** Bitcoin-derived `-assumevalid` could skip script checks.

**Search.** `grep -R "assumevalid" core/src` hits:

- `common/alphaplus_security.cpp` — fail-closed policy
- `common/init.cpp` — `InitConfig` abort
- `node/chainstatemanager_args.cpp` — second gate before applying the hash
- `init.cpp` — CLI help still lists the switch
- `validation.cpp` — remaining skip heuristic (only if an assumed-valid hash is configured)
- unit test

**Wall.** On mainnet, any explicit `-assumevalid` (CLI or conf) returns:

```
ERROR:
-assumevalid is disabled on AlphaPlus mainnet.
```

**Live test.**

```
$ ./src/rigord -assumevalid=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
Error: ERROR:
-assumevalid is disabled on AlphaPlus mainnet.
```

Exit code 1.

Compiled `defaultAssumeValid` remains genesis (`kernel/chainparams.cpp`). With assumevalid = genesis, the ConnectBlock skip can only apply to genesis itself. That is not an operator override.

**Other bypass parameters (not CSW-2, still doors).**

| Switch | Mainnet status |
|--------|----------------|
| `-minimumchainwork` | Still accepted (DEBUG_ONLY help; still applied) |
| `-checkpoints` | Still accepted |
| `-assumevalid` on regtest/testnet | Still allowed (required for `feature_assumevalid.py`) |

No hidden validation shortcut equivalent to a mainnet assumevalid override was found.

**Result: CSW-2 PASS**

---

## Section 3 — CSW-3 AlphaPlus PoW Admission Boundary

**Previous risk.** Header anti-DoS accepted compact `nBits` shape instead of OpuHash.

**Required flow (now implemented).**

```
Receive block/header
        ↓
Contextual / anti-DoS checks
        ↓
AlphaPlus CheckProofOfWork (OpuHash vs declared nBits)
        ↓
Accept / reject
```

**Path audit.**

| Path | Reaches `CheckProofOfWork`? |
|------|-----------------------------|
| `headers` → `CheckHeadersPoW` → `HasValidProofOfWork` | Yes — now OpuHash, not compact-only |
| `cmpctblock` | Yes — explicit check before `ProcessNewBlockHeaders(..., min_pow_checked=true)` |
| `BLOCK` | Yes — required before `min_pow_checked=true` |
| `AcceptBlockHeader` → `ContextualCheckBlockHeader` | Yes — unchanged consensus check |
| `ReadBlockFromDisk` (reindex / load) | Yes |
| `submitblock` RPC | Sets `min_pow_checked=true` (local/authenticated). `AcceptBlockHeader` still runs `ContextualCheckBlockHeader` → `CheckProofOfWork` |
| `generate` / `generatetoaddress` | Mines until `CheckProofOfWork`, then `ProcessNewBlock` |

`CheckBlockHeader` still only applies the genesis hash-identity branch; non-genesis PoW is enforced in `ContextualCheckBlockHeader` / `HasValidProofOfWork`. That is not a compact-nBits admission path.

Genesis remains exempt **inside** `CheckProofOfWork` when `header_hash == hashGenesisBlock` (hash identity; do not remine). That is the original CSW-3 conceptual residual, not a stale-nBits accept for peer headers.

**No path accepts:** stale nBits only, Bitcoin legacy compact-shape as “valid PoW,” or skipped OpuHash on header admission.

Unit test `header_admission_requires_opus_hash`: legal compact `nBits` without OpuHash fails `HasValidProofOfWork`. Genesis header still passes.

**Result: CSW-3 PASS**

---

## Section 4 — CSW-8 RPC Security Boundary

**Previous risk.** Default / test credentials shipped (`rpcuser=user`, `rpcpassword=pass`).

**Search.** `rpcuser` / `rpcpassword` in the tree:

- Generation and refuse logic: `core/src/common/alphaplus_security.cpp`
- Runtime refuse: `httprpc.cpp`
- Launchers: `load-rpc-auth.cmd`, `start-node.cmd`, `rigor-cli.cmd`, `rigor-miner.cmd` (use generated vars)
- Example conf: comments only; no shipped pair
- Tests / contrib / docs: expected
- **Leftover scripts (untracked):** `installer/windows/start-peer2.cmd`, `installer/windows/fix-miningresfeeaddr.cmd` still embed `user` / `pass`

**Wall (node).**

1. First mainnet start with no credentials writes `<datadir>/rpc-auth.conf` (`rpcuser=rigor_<16 hex>`, `rpcpassword=<64 hex>`), `includeconf=rpc-auth.conf`, owner-only `0600` on Unix.
2. Known defaults (`user`/`pass`, `rigor`/`rigor`, `password`, `change-me`, …) abort mainnet startup.
3. Cookie auth only if `-rpccookiefile` is set explicitly (or password remains empty after the generator skipped).

**Live tests.**

```
$ ./src/rigord -datadir=<tmp> -rpcuser=user -rpcpassword=pass
Error: ERROR:
Default or well-known RPC credentials are not allowed on AlphaPlus mainnet.
```

First start without credentials:

```
Generating RPC authentication credentials...
Generated configuration:
rpcuser=rigor_<random>
rpcpassword=<random>
Stored in <datadir>/rpc-auth.conf
```

`stat` on Linux: `rpc-auth.conf` mode `600`. `bitcoin.conf` contains `includeconf=rpc-auth.conf`.

**Installer.**

- AlphaPlus NSIS does not create `testnet3` / `regtest`.
- `rigor.conf.template` does not ship user/pass.
- `load-rpc-auth.cmd` generates or loads `rpc-auth.conf` and rejects `user`/`pass`.
- `start-node.cmd` uses those variables; it does not write `rpcuser=user`.

**Audit questions.**

| Question | Answer |
|----------|--------|
| Does the installer create secure configuration? | Node first-start does. Windows launchers do if `load-rpc-auth.cmd` is staged. |
| Are permissions correct? | Yes on Linux (`0600`). Windows C++ path does not set ACLs (`#ifndef WIN32`). |
| Are old credentials rejected? | Yes on mainnet. |
| Are test credentials unavailable? | Unavailable to a running mainnet node. Still present as strings in leftover untracked scripts. |

**Result: CSW-8 PASS** (node + primary launch path). Leftover Peer-2 / fix scripts are NEW-1 (P1), not a working bypass: a mainnet node refuses those credentials.

---

## Section 5 — New door discovery

### Wallet

| Check | Finding |
|-------|---------|
| Private key export | `dumporuprivkey`, `dumpresprivkey`, `listdescriptors true` still exist when unlocked |
| Unlock | `walletpassphrase` still accepts a multi-year timeout |
| Backup / restore | Descriptor export / `importrigorprivkey` / `importdescriptors` remain |
| RPC wallet access | No production `-walletexport` / `-walletimport` fail-closed profile |

Locked wallet does not expose secrets through those RPCs (unlock required). Unsafe commands still **exist** and are then checked. This is the original **CSW-6** door (P1). Hardening did not open a new export path.

Spend construction (immature coinbase, mixed ORU/RES, ORU-only fees) was not changed. Wallet **accounting** freeze holds.

### Pool

| Check | Finding |
|-------|---------|
| Share validation | `pool/share-validator/src/convergence.rs` still requires every `convergence_txid` in the mempool/body set (`ErrConvergenceMismatch`) |
| PPLNS | Still in-memory; `record_block_found_for_job(..., template.gbt.coinbasevalue)` |
| GBT trust | Node remains SSOT for block validity; pool still trusts GBT fields for off-chain splits |
| Coinbase | Pool Option A (no RES mint construction) unchanged |

Miner cannot force an invalid chain reward. Pool operator can still create **invalid off-chain liability**. Fake shares that fail node `submitblock` do not mint. This is original **CSW-4 / CSW-5** (P1).

### Explorer

| Check | Finding |
|-------|---------|
| Winner / C | Importer copies `getconvergenceresult` — does not `RunSelection` |
| Fingerprint | `consensusFingerprint.ts` hashes published RPC fields (or stores a node-published digest). Does not recompute the winner |
| Supply display | `status.ts` prefers `getconvergencestats.total_res_minted`; still also serves indexer aggregates |
| Defaults | `parseExplorerNetwork` now defaults to **mainnet** (improvement vs prior audit). `STOP_ON_ROOT_MISMATCH` still defaults `false`. `PG_URL` still `postgres:postgres`. `RIGOR_DATADIR` still defaults to `/var/lib/rigor/testnet` |

Explorer is still a window for winners and a cache for balances. Original **CSW-7** (P1), partially improved.

### Miner / GBT

GBT still publishes node-authored `nBits`, `target`, `mintime`, `curtime`, `prev_nBits`, coinbase, and `convergence_root`. `submitblock` still goes through `ProcessNewBlock` → contextual OpuHash. Uncommitted `miner.cpp` still refreshes ASERT `nBits` after `UpdateTime` on mainnet (`nPowASERTHalfLife > 0`). No new miner trust bypass.

### Hardening-introduced surfaces

| ID | Severity | Door |
|----|----------|------|
| NEW-1 | P1 | Untracked `start-peer2.cmd` writes `rpcuser=user` / `rpcpassword=pass` into `peer2/rigor.conf` and starts a second mainnet node. NSIS still has a “Start Peer-2” shortcut and `/nonfatal` copy. Node refuse makes this a **broken helper**, not a working default-secret. `fix-miningresfeeaddr.cmd` has the same pair. |
| NEW-2 | P1 | `EnsureProductionRpcCredentials` prints **and** `LogPrintf`s the generated password. `debug.log` becomes a secret file. |
| NEW-3 | P1 | Official tree is dirty (`-dirty` binary). Uncommitted explorer/Qt/web/miner/fingerprint work is mixed into the RC working tree. |
| NEW-4 | P1 | `build_alpha_windows_nsis.sh` does not stage `start-node.cmd` or `load-rpc-auth.cmd`. NSIS uses `/nonfatal`, so hardened launchers can be silently omitted. Wallet/node first-start still generates secrets. |
| NEW-5 | P2 | Default conf filename remains `bitcoin.conf`. Windows tools speak `rigor.conf`. Generator writes includeconf to both when `rigor.conf` already exists. |
| NEW-6 | P2 | Windows C++ path does not restrict `rpc-auth.conf` ACLs. |
| NEW-7 | P2 | `start-node.cmd` still appends `solomining=1` to an existing conf (original P1-9). |
| NEW-8 | P2 | Init log still reports “Default data directory …/.bitcoin”. |

None of these is a new P0 consensus or assumevalid-class bypass.

---

## Section 6 — Zero Trust / Consensus Fingerprint

Untracked (but present) library:

`ComputeConsensusFingerprint(ConvergenceRoot || winner_txid || Sigma || height || target_prev)` — 104-byte native preimage, Hash256. Explicitly **not** a block-acceptance predicate.

Explorer collect path prefers a node-published fingerprint; otherwise it hashes the same published fields. It does not independently select a winner.

The website Zero-Trust portal **does** recompute ConvergenceRoot, Sigma, winner, and fingerprint in TypeScript/WebCrypto from public inputs. That is a third implementation. Deviation is detectable (MATCH / MISMATCH). It is not consensus authority. Residual: implementation drift (original P2-8). Labeling says the portal does not trust explorer verdicts; it should remain labeled **reproduction, not authority**.

A different honest implementation that consumes the same native fields must produce the same fingerprint. A different winner or root is a detectable mismatch, not a silent override.

---

## Section 7 — Security classification

| ID | Status | Description |
|----|--------|-------------|
| CSW-1 | **PASS** | Production profile compiles out non-main networks. This tree’s live binary is a development build and can still start testnet. |
| CSW-2 | **PASS** | Explicit `-assumevalid` fail-closed on mainnet. Live test matches the required error. Default remains genesis. |
| CSW-3 | **PASS** | Header / compact / BLOCK admission runs AlphaPlus `CheckProofOfWork`. Compact-nBits-only door is closed. |
| CSW-8 | **PASS** | No shipped working default secret. First start generates `rpc-auth.conf` (0600). Known defaults refused. |
| CSW-4 | OPEN (P1) | Share-validator body-membership rule unchanged |
| CSW-5 | OPEN (P1) | In-memory PPLNS; trusts GBT `coinbasevalue` |
| CSW-6 | OPEN (P1) | Wallet export/import/sign still exist when unlocked |
| CSW-7 | OPEN (P1) | Explorer still a second ledger for some totals; `STOP_ON_ROOT_MISMATCH=false` |
| CSW-9 | OPEN (P2) | Auto-settle remains a product default (uncommitted Qt work present) |
| CSW-10 | OPEN (P2) | Bitcoin leftover branches untouched (correct — consensus freeze) |
| P0-6 | OPEN (P2 residual) | `-reindex-chainstate` still skips contextual checks. Disk load still runs `CheckProofOfWork`. Not introduced by hardening. |
| NEW-1 | OPEN (P1) | Leftover Peer-2 / fix scripts embed `user`/`pass`; NSIS still advertises Peer-2 |
| NEW-2 | OPEN (P1) | Generated RPC password written to stdout and `debug.log` |
| NEW-3 | OPEN (P1) | Dirty official tree / `-dirty` binary |
| NEW-4 | OPEN (P1) | NSIS staging can omit hardened launchers |
| NEW-5 | OPEN (P2) | `bitcoin.conf` vs `rigor.conf` split |
| NEW-6 | OPEN (P2) | No Windows ACL on `rpc-auth.conf` |
| NEW-7 | OPEN (P2) | `solomining=1` still appended to existing conf |
| NEW-8 | OPEN (P2) | Default-datadir log still says `.bitcoin` |

---

## Remaining risks

**Must treat as launch-blocking for the named product (P1).**

1. Do not ship a public pool until CSW-4 / CSW-5 are walls.
2. Do not advertise a networked hot wallet until CSW-6 is fail-closed.
3. Do not put a public explorer in front of users until CSW-7 halt-on-mismatch and RPC-only chain totals are walls.
4. Delete or rewrite `start-peer2.cmd` / `fix-miningresfeeaddr.cmd`; drop the Peer-2 Start Menu entry; stage `load-rpc-auth.cmd` + `start-node.cmd` explicitly.
5. Stop logging generated RPC passwords.
6. Ship only a clean, production-profile (`--enable-production-mainnet`) artifact. Do not ship this dirty working tree.

**P2 (after the P0 walls, not instead of them).** Genesis PoW hash exemption, `-minimumchainwork` / `-checkpoints` overrides, `-reindex-chainstate` contextual skip, conf filename, Windows ACLs, auto-settle consent, Bitcoin leftover deletion (no observable mainnet change).

---

## Launch recommendation

Ship the **P0 perimeter** (CSW-1/2/3/8) only as a **production-profile** binary built from a **clean** commit, with leftover Peer-2 helpers removed from the installer graph.

Do **not** call the full AlphaPlus product (node + pool + explorer + hot wallet) launch-ready. Wave 1 doors remain.

Do **not** change genesis, PoW, RES, subsidy, fees, or wallet accounting to close remaining doors.

---

## Confirmation

| Criterion | Status |
|-----------|--------|
| CSW-1 PASS | Yes |
| CSW-2 PASS | Yes (live) |
| CSW-3 PASS | Yes |
| CSW-8 PASS | Yes (live) |
| Consensus unchanged | Yes (hardening commit) |
| Genesis unchanged | Yes |
| PoW unchanged | Yes |
| RES unchanged | Yes |
| Fees unchanged | Yes |
| Wallet accounting unchanged | Yes |
| No new P0 doors | Yes |
| Official tree clean | **No** — dirty working tree |
| Additional P1 doors remain | Yes |

**git status (expected by this task):** dirty. Branch `mainnet-release-candidate-001` is 12 commits ahead of `origin/mainnet-release-candidate-001`. No commit, push, tag, or merge was performed.

---

## Final verdict

**PARTIAL**

Priority of remaining work: **P1**

P0 doors CSW-1, CSW-2, CSW-3, and CSW-8 are closed. Consensus is frozen. No new critical (P0) boundary was opened. Additional doors remain — original Wave-1 surfaces plus packaging / secret-logging leftovers in this official tree.

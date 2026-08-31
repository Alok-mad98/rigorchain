# RIGOR Identity Migration — Phase 1 Report

Task: `TASK-RIGOR-IDENTITY-MIGRATION-PHASE1-P0-026`  
Companion audit / plan: [`RIGOR_BITCOIN_LEGACY_NAMING_AUDIT_P0_024.md`](./RIGOR_BITCOIN_LEGACY_NAMING_AUDIT_P0_024.md), [`RIGOR_BITCOIN_DECOUPLING_MIGRATION_PLAN_P0_024.md`](./RIGOR_BITCOIN_DECOUPLING_MIGRATION_PLAN_P0_024.md)  
Date: 2026-08-26  
Type: **Identity-only rename.** No consensus, serialization, sighash, ScriptPubKey, AssetTypeFromScriptPubKey, Ed25519, or RPC *method* behavior changed.

**Forbidden this task (verified not edited as protocol):**

- `ScriptPubKey` / `AssetTypeFromScriptPubKey` logic
- Ed25519 verify path
- sighash tagged preimage
- wire serialization
- RPC method names (`getblock`, `getrawtransaction`, …)

**Forbidden this task (style):** mechanical `Bitcoin → RIGOR` global replace. MIT copyright banners stay.

Live protocol (unchanged):

```
UTXO
  → ScriptPubKey
  → AssetTypeFromScriptPubKey
  → Fixed Ownership Predicate
  → Ed25519 Verification
```

---

## Final gates

```
Test identity PASS
Help identity PASS
Build identity PASS
```

---

## 0. What Phase 1 owns

User-visible product strings and **install / test binary names** that no longer carry Bitcoin protocol meaning:

| Surface | From | To |
|---------|------|----|
| Unit-test binary | `test/test_bitcoin` | `test/test_rigor` |
| Boost module | `Bitcoin Core Test Suite` | **`RIGOR Protocol Test Suite`** |
| Util install name | `bitcoin-util` | `rigor-util` |
| CLI help / examples | `bitcoin-cli` | `rigor-cli` |
| Tx tool help | `bitcoin-tx` | `rigor-tx` |
| Wallet-tool help | `bitcoin-wallet` | `rigor-wallet` (install name was already `rigor-wallet`) |

Boost uses **Protocol**, not “RIGOR Core Test Suite”: this tree is not presented as a Bitcoin Core fork product.

`bitcoin-util` was listed as Phase 2 in P0-024. This task pulled the **install name + vectors + help** into Phase 1. Source filename `bitcoin-util.cpp` remains Phase 2.

---

## 1. CMake

**No project CMake for node binaries.** Autotools is the build. In-tree `CMakeLists.txt` hits are vendor (`leveldb`, `crc32c`) plus unrelated runtime curl sources. There was nothing to rename in a RIGOR CMake graph.

---

## 2. Test identity

### 2.1 Binary and Boost

| Item | Status |
|------|--------|
| `core/configure.ac` `BITCOIN_TEST_NAME=test_rigor` | Done |
| `core/src/Makefile.test.include` `bin_PROGRAMS += test/test_rigor` | Done |
| Generated `src/Makefile` `TEST_BINARY = test/test_rigor$(EXEEXT)` | Confirmed after `config.status` |
| `BOOST_TEST_MODULE RIGOR Protocol Test Suite` in `core/src/test/main.cpp` | Done |
| Linked `src/test/test_rigor` contains that string; no `Bitcoin Core Test Suite` | Confirmed via `strings` |
| Make aliases `rigor_test` / `rigor_test_check` / `rigor_test_clean` | Done |
| Qt GUI unit binary remains `qt/test/test_rigor-qt` | Already native; automake vars now `qt_test_test_rigor_qt_*` (dropped `qt_test_test_bitcoin_qt_*` aliases that produced automake warnings) |

### 2.2 Runners / CI / FV

| File | Change |
|------|--------|
| `core/ci/test/06_script_b.sh` | sequential unit binary `${BASE_OUTDIR}/bin/test_rigor` |
| `core/contrib/devtools/test_deterministic_coverage.sh` | `TEST_RIGOR_BINARY=src/test/test_rigor` (prior) |
| `formal_verification/scripts/run_differential_gate.sh` | `TEST_RIGOR` / `SKIP_TEST_RIGOR`; leftover `test_bitcoin` on disk still auto-detected with a rebuild warning |
| `formal_verification/scripts/run_fv_release_checklist.sh` | `FV_TEST_RIGOR` (`FV_TEST_BITCOIN` alias) |
| `core/test/functional/res_security/run_pre_mainnet.sh` | prefers `test_rigor` |
| `contrib/testing/local_mainnet_simulation/run_simulation.py` | operator follow-up commands use `test_rigor` |
| `core/test/README.md`, `core/src/test/README.md`, `core/doc/developer-notes.md` | live commands |

### 2.3 Functional Python class names

**Not renamed this phase:** `BitcoinTestFramework`, kwargs `bitcoind` / `bitcoincli`, tmpdir `bitcoin_func_test_`, `interface_bitcoin_cli.py`. Resolver already maps `bitcoin-cli` → `rigor-cli` and `bitcoin-util` → `rigor-util`. Skip messages now say `rigor-cli` / `rigor-util` / `rigor-wallet`.

`get_previous_releases.py` still looks for historical tarball names `bitcoind` / `bitcoin-cli` / `bitcoin-tx` — **KEEP** (those files existed in old release archives).

### 2.4 Verification run

```
./src/test/test_rigor --run_test=sanity_tests --catch_system_errors=no
→ *** No errors detected
```

Stale on-disk `src/test/test_bitcoin` from a previous link was removed after the rebuild so operators cannot pick the old name by accident.

---

## 3. Help identity

Live `--help` / `-help` on rebuilt binaries (2026-08-26):

| Binary | Observed |
|--------|----------|
| `src/rigor-cli` | `Usage:  rigor-cli [options] … Send command to Rigor Core` |
| `src/rigor-tx` | `Usage:  rigor-tx … Update hex-encoded RIGOR transaction` |
| `src/rigor-util` | `Usage:  rigor-util [options] [commands]` |
| `src/rigor-wallet` | `Usage: rigor-wallet [options] <command>` |

`strings` on those four plus `test_rigor`: no leftover `Usage:  bitcoin-*` or `Bitcoin Core Test Suite`.

RPC help examples already emit `> rigor-cli` (`HelpExampleCli` in `core/src/rpc/util.cpp`). RPC **method names** unchanged.

Also updated (user-facing, not copyright):

- Windows VERSIONINFO (`*-res.rc`) product names
- `core/contrib/devtools/utxo_snapshot.sh` usage
- `core/contrib/signet/getcoins.py` default `--cmd=rigor-cli`
- `core/contrib/signet/README.md` grind/cli examples
- Installed man `.TH` / synopsis leftovers (`BITCOIN-CLI` → `RIGOR-CLI`, `arda-tx` / `arda-wallet` residue → `rigor-*`)
- Fish/bash completions: `rigor-cli`, `rigor-tx`, `rigor-util` (superseded `bitcoin-cli` / `bitcoin-tx` / `bitcoin-util` completion files removed)

`PACKAGE_NAME` remains **Rigor Core** (`AC_INIT` / display). That is the existing product string, not “Bitcoin Core”.

---

## 4. Build identity

| Item | Status |
|------|--------|
| `BITCOIN_UTIL_NAME=rigor-util` | `configure.ac` / generated `configure` |
| `bin_PROGRAMS += rigor-util` | `src/Makefile.am` |
| Generated `am__EXEEXT_6 = rigor-util$(EXEEXT)` | Confirmed |
| Top-level `make rigor-util` forwards to `src/` | Done |
| `make bitcoin-util` is an **error stub** pointing at `rigor-util` | Done |
| `configure --help` / `AC_MSG_CHECKING` | `rigord` / `rigor-cli` / `rigor-tx` / `rigor-util` / `test_rigor` |
| Man install | `dist_man1_MANS += rigor-util.1` (already) |
| `gen-manpages.py` binary list | `rigord`, `rigor-cli`, `rigor-tx`, `rigor-wallet`, `rigor-util`, `qt/rigor-qt` |
| Util vectors | `test/util/data/rigor-util-test.json` with `./rigor-util` and `./rigor-tx`; runner loads that file |
| `ENABLE_BITCOIN_UTIL` in `test/config.ini` | **KEEP** (internal autotools key, not an install name) |

Source still named `bitcoin-util.cpp` / `bitcoin-cli.cpp` / `bitcoin-tx.cpp` / `bitcoind.cpp` — Phase 2.

Rebuilt: `src/rigor-util`, `src/rigor-cli`, `src/rigor-tx`, `src/rigor-wallet`, `src/test/test_rigor`. Leftover `src/bitcoin-util` binary from the previous name was deleted after the relink.

### 4.1 `rigor-util` vectors vs `rigor-tx` golden JSON

All **five** `./rigor-util` cases in `rigor-util-test.json` **PASS** (invalid args / grind errors).

`python3 test/util/test_runner.py` still **fails** a large set of **`./rigor-tx` JSON fixtures**. Those fixtures still expect Bitcoin Core decoder display (`[ALL]` sighash asm, P2PKH `addr(1…)` / `pubkeyhash`). Live RIGOR `rigor-tx -json` reports RIGOR script classification (`asset: INVALID` on P2PKH, no `[ALL]` token). That is **pre-existing protocol-display vs inherited goldens**, not a failed rename. Phase 1 does **not** rewrite those ScriptPubKey/sighash goldens.

Identity of the **exec names** in the vector file is PASS (`./bitcoin-util` / `./bitcoin-tx` are gone).

---

## 5. Explicit non-changes

| Item | Why |
|------|-----|
| RPC methods / JSON field names | RPC-SHAPE (P0-024) |
| `libbitcoinconsensus` SONAME / C ABI | Phase 3 |
| `LIBBITCOIN_*`, include guards `BITCOIN_*`, `config/bitcoin-config.h` | Phase 2 |
| `wallet.dat` filename / schema | P0-022 / Phase later |
| `COIN = 1e8` | CONSENSUS-SCALE |
| Copyright “The Bitcoin Core developers” | LEGAL |
| Historical `core/doc/release-notes/` and past security reports that *record* `test_bitcoin` runs | HISTORICAL |
| `bench/bench_bitcoin` | Not in this task’s binary list (optional later) |
| `BitcoinTestFramework` class | Python identity leftover; resolver already finds `rigord` / `rigor-cli` |
| MSVC `core/build_msvc/bitcoin-util/` project folder | Phase 2 / Windows build graph |
| Stale `core/doc/man/bitcoin-util.1` file in tree | Not installed (`Makefile.am` ships `rigor-util.1` only) |

---

## 6. Phase 2 / 3 leftovers (do not treat as Phase 1 blockers)

- Source filenames: `bitcoind.cpp`, `bitcoin-cli.cpp`, `bitcoin-tx.cpp`, `bitcoin-util.cpp`, `bitcoin-wallet.cpp`
- Autotools `LIBBITCOIN_*` archives
- `libbitcoinconsensus` ABI name
- Functional class `BitcoinTestFramework` / `interface_bitcoin_cli.py`
- Fish leftovers for `bitcoind` / `bitcoin-qt` / `bitcoin-wallet` filenames (daemon/GUI install names are already `rigord` / `rigor-qt` / `rigor-wallet`)
- Inherited `rigor-tx` util JSON content (Bitcoin Core asm / P2PKH addresses)
- Historical docs that mention `test_bitcoin` as a past command

---

## 7. How to re-verify

```bash
cd core
make -j$(nproc) rigor-util rigor-cli rigor-tx rigor-wallet
make -j$(nproc) -C src test/test_rigor

src/rigor-cli -help | head
src/rigor-tx -help | head
src/rigor-util -help | head
src/rigor-wallet -help | head

strings src/test/test_rigor | grep 'RIGOR Protocol Test Suite'
src/test/test_rigor --run_test=sanity_tests --catch_system_errors=no

# rigor-util identity cases (first five objects in rigor-util-test.json) PASS independently.
# Full test/util/test_runner.py still fails inherited rigor-tx JSON goldens (P2PKH/[ALL] display).
python3 test/util/test_runner.py  # expected FAIL on rigor-tx fixtures; not an identity regression
```

Confirm `src/Makefile` contains `TEST_BINARY = test/test_rigor` and `rigor-util$(EXEEXT)`, and that `src/bitcoin-util` / `src/test/test_bitcoin` are **not** produced.

---

## 8. Verdict

Phase 1 identity migration is complete for the requested surfaces.

```
Test identity PASS
Help identity PASS
Build identity PASS
```

Consensus spend path, RPC methods, and serialization are unchanged. Remaining Bitcoin strings are LEGAL, HISTORICAL, BUILD-INTERNAL, ABI, or deferred Python/source-filename work.

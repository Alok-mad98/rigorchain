# RIGOR Bitcoin Decoupling Migration Plan

Task: `TASK-RIGOR-BITCOIN-LEGACY-NAMING-AUDIT-P0-024`  
Companion audit: [`RIGOR_BITCOIN_LEGACY_NAMING_AUDIT_P0_024.md`](./RIGOR_BITCOIN_LEGACY_NAMING_AUDIT_P0_024.md)  
Date: 2026-08-26  
Type: **Plan only.** Phase 0 is this pair of documents. No consensus, serialization, RPC method names, or formal spend model changes in any phase below unless a later task explicitly owns that surface.

**Hard rule:** no `s/Bitcoin/RIGOR/g`. Each phase touches one **class** from the audit taxonomy.

---

## What is already done (do not redo)

Install names `rigord`, `rigor-cli`, `rigor-tx`, `rigor-wallet`, `rigor-qt`; datadir `~/.rigor`; config `rigor.conf`; package display “Rigor Core”; Qt class `RigorGUI`; amount display ORU/Tharn.

The remaining work is **leftover identity** around those products, plus one honest-ABI problem (`libbitcoinconsensus`).

---

## Phase 0 — Audit only

**Status: this task.**

- Full-tree classification (LEGAL / IDENTITY-SAFE / ABI / RPC-SHAPE / …).
- **No code change.**
- **No** ScriptPubKey / AssetTypeFromScriptPubKey / RES / ORU / CONVERGE / Ed25519 / Sighash / FV model edits.

Exit: the audit document exists and forbids mechanical replace.

---

## Phase 1 — Safe rename (user-visible strings + test binary)

**Goal:** operators and CI stop seeing “Bitcoin Core Test Suite” and `bitcoin-cli` in help, without moving hundreds of headers.

| Change | From | To | Why safe |
|--------|------|-----|----------|
| Boost module | `Bitcoin Core Test Suite` | `RIGOR Consensus Test Suite` | String only |
| Test binary | `test/test_bitcoin` | `test/test_rigor` | `configure.ac` `BITCOIN_TEST_NAME`, `Makefile.test.include`, FV scripts `TEST_BITCOIN` |
| Bench binary | `bench/bench_bitcoin` | `bench_rigor` | same class |
| CLI help | `Usage: bitcoin-cli` | `Usage: rigor-cli` | Install name already `rigor-cli` |
| Tx help | `bitcoin-tx` / “bitcoin transaction” | `rigor-tx` / “RIGOR transaction” | Install name already `rigor-tx` |
| Functional class / tmpdir | `BitcoinTestFramework`, `bitcoin_func_test_` | `RigorTestFramework`, `rigor_func_test_` | Python identity; keep RPC names |
| `interface_bitcoin_cli.py` | filename | `interface_rigor_cli.py` | test_runner list |
| Configure `--help` text | “build bitcoind / bitcoin-cli” | “build rigord / rigor-cli” | matches `bin_PROGRAMS` |
| `AC_INIT` URLs | bitcoin/bitcoin issues, bitcoincore.org | RIGOR tracker / site | identity |

**Do not in Phase 1**

- RPC methods (`getblock`, `getrawtransaction`, …).
- `libbitcoinconsensus` SONAME / C symbols.
- `wallet.dat`.
- Include guards / `LIBBITCOIN_*`.
- Copyright banners.

**Tests:** `make check` unit binary path; `test_runner.py` still finds daemon via existing `rigord` resolver; `run_differential_gate.sh` env vars.

**Note:** there is no `test_bitcoin-wallet` target. Do not create `rigor-wallet-test` unless a later task re-enables `wallet/test/*.cpp`. In-tree RIGOR wallet Boost cases already sit in `test_rigor`.

---

## Phase 2 — Directories and build system

**Goal:** source and install names match products. Still no consensus edits.

| Change | From | To |
|--------|------|-----|
| Util **install** name | `bitcoin-util` | `rigor-util` |
| Util test JSON | `bitcoin-util-test.json` exec `./bitcoin-util` | `rigor-util-test.json` exec `./rigor-util` (and `./rigor-tx`) |
| Daemon / CLI / tx / util / wallet sources | `bitcoind.cpp`, `bitcoin-cli.cpp`, `bitcoin-tx.cpp`, `bitcoin-util.cpp`, `bitcoin-wallet.cpp` | `rigord.cpp`, `rigor-cli.cpp`, … |
| Init multiprocess (if still built) | `bitcoin-node`, `bitcoin-gui`, `bitcoin-chainstate` | `rigor-node`, `rigor-gui`, `rigor-chainstate` or drop experimental targets |
| Qt files | `bitcoingui.cpp`, `bitcoinunits.cpp`, `bitcoin.cpp`, `bitcoin.qrc`, locales `bitcoin_*.ts` | `rigorgui.cpp`, `rigorunits.cpp`, `rigor.qrc`, … |
| Qt types still Bitcoin-named | `BitcoinUnits`, `BitcoinApplication`, `BitcoinAddressValidator` | `RigorUnits`, `RigorApplication`, `RigorAddressValidator` (`RigorGUI` already done) |
| Autotools libs | `libbitcoin_node.a`, `libbitcoin_consensus.a`, … | `librigor_node.a`, … |
| Config header primary | `config/bitcoin-config.h` | `config/rigor-config.h` as generated file; keep a one-line shim the other way if needed |
| Include guards | `BITCOIN_*` | `RIGOR_*` **only as a dedicated header-guard pass**, not a content replace |
| Stale man pages | `core/doc/man/bitcoin-*.1` (incl. `arda-util` title residue) | remove from tree once `rigor-*.1` is generated from current `--help` |
| `test_node.py` kwargs | `bitcoind=`, `bitcoin_cli=` | `rigord=`, `rigor_cli=` |

**Man page inconsistency to close:** `Makefile.am` already installs `rigor-util.1` while the binary is still `bitcoin-util`. Phase 2 must land **binary + JSON + help** together.

**Do not in Phase 2**

- Consensus function bodies.
- `bitcoinconsensus_*` C ABI (Phase 3).
- `COIN` / wire formats.
- `LICENSE` Bitcoin copyright lines.

---

## Phase 3 — Protocol-facing C API (`bitcoinconsensus`)

**Goal:** the shared library must not claim Bitcoin consensus.

### Facts

- Implementation already calls RIGOR `VerifyScript` (Ed25519 / asset-type programs).
- Header and symbols are Bitcoin Core libconsensus (`bitcoinconsensus_verify_script`, `BITCOINCONSENSUS_API_VER`, `-lbitcoinconsensus`).
- pkg-config **Description** still says “Bitcoin consensus protocol” while **Name** says Rigor Core — that Description is a defect, not a compatibility feature.

### ABI judgment

| Question | Answer |
|----------|--------|
| Is this still the Bitcoin Consensus API in **behavior**? | **No.** |
| Would renaming symbols break **this** chain’s consensus? | **No.** |
| Would renaming break **out-of-tree** linkers of `libbitcoinconsensus.so` from this tree? | **Yes, if any exist.** None are documented in-tree. |
| Should `bitcoinconsensus_*` remain as aliases that look like Bitcoin Script verify? | **No**, unless a named RIGOR consumer is found. Silent Bitcoin-named aliases are a footgun. |
| RPC `getblock` analogy? | **Does not apply.** RPC names are RIGOR’s client API. `bitcoinconsensus_*` pretends to be **Bitcoin’s** libconsensus. |

### Recommended move

1. Add `rigorconsensus.h` / `rigorconsensus_*` / `RIGORCONSENSUS_API_VER` / `librigorconsensus`.
2. Point pkg-config at the new name; Description: RIGOR script/ownership verify, not Bitcoin.
3. Do **not** change `VerifyScript` semantics, flag-bit numeric values (unless a dedicated ABI-break task proves no consumer), or serialization.
4. Remove or fail-closed the old symbols in the **same** major SONAME bump so no one keeps linking a Bitcoin-named lie.
5. Rename fuzz `script_bitcoin_consensus.cpp` to match.

If a later inventory finds a real external RIGOR `.so` user, ship **one** release with both symbols documented as “RIGOR verify, not Bitcoin Script,” then drop the old names.

---

## Phase 4 — Formal + docs sync

**Goal:** names in `formal_verification/` and `docs/` match the protocol and the Phase 1–3 binaries.

| Keep | Change |
|------|--------|
| “Derived from Bitcoin Core v25” in historical audits | Live runbooks: `bitcoind` → `rigord`, `test_bitcoin` → `test_rigor` |
| SHA-256d cited as Bitcoin Hash256 **composition** | Amount comments: satoshi → Tharn / base unit (same integers) |
| `rigor_v01` obsolete-Bitcoin constructors list | `BitcoinUnits::formatRes` comments after Qt rename |
| `LICENSE` / file copyright Satoshi / Bitcoin Core | User-facing docs that still teach `bitcoin-cli` |
| P0-022 wallet.dat / BDB residual report | New identity: RIGOR SQLite Wallet (filename policy is a **wallet** task, not a grep-replace) |

`formal_verification/scripts/run_differential_gate.sh` and golden-vector docs that invoke `./src/test/test_bitcoin` update with Phase 1.

**Still not in Phase 4:** Coq spend-model constructors, sighash tags, ScriptPubKey classification.

---

## Explicit non-goals (all phases)

```
RPC:     getblock, getrawtransaction, getblocktemplate, wallet RPCs  → KEEP
Scale:   COIN = 1e8, Tharn = 1, MAX_MONEY                            → KEEP
Legal:   Satoshi / Bitcoin Core copyright, LICENSE                   → KEEP
Compat:  bitcoin.conf fallback                                       → deprecate with logging, do not silent-delete
Wallet:  wallet.dat bytes / WalletBatch KV                           → separate wallet migration (P0-022)
Consensus path: ScriptPubKey → AssetType → Ed25519                   → KEEP
```

---

## Suggested order of work (after Phase 0)

```
Phase 1  test_bitcoin → test_rigor, help strings, functional class
    ↓
Phase 2  bitcoin-util → rigor-util, sources, Qt files, LIBBITCOIN_*, guards
    ↓
Phase 3  bitcoinconsensus → rigorconsensus (ABI bump, honest description)
    ↓
Phase 4  docs + FV scripts/comments (not Hash256-origin citations)
```

Each phase is a separate reviewable change set. Do not combine Phase 3 with Phase 1.

---

## BITCOIN LEGACY DECOUPLING STATUS

(Same as audit; repeated so this plan stands alone.)

```
Historical naming:     FAIL
Runtime dependency:    PASS
Consensus dependency:  PASS
Wallet dependency:     FAIL
Test framework:        FAIL
Formal terminology:    FAIL

RIGOR Native Identity Ready: NO
```

Phase 1 alone does not flip “Ready” to YES. Ready means Phases 1–3 landed and Phase 4 no longer documents Bitcoin as the **live** product name. LEGAL Bitcoin copyright and Hash256-origin citations remain forever and do **not** block Ready.

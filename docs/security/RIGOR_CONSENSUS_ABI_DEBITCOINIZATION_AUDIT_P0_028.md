# RIGOR Consensus ABI Debitcoinization Audit (P0-028)

Task: `TASK-RIGOR-CONSENSUS-ABI-DEBITCOINIZATION-AUDIT-P0-028`  
Type: **Audit only.** No code, no SONAME bump, no `VerifyScript` / ScriptPubKey / Ed25519 / serialization change.  
Date: 2026-08-26  
Tree: `rigor-official-tree`

This is the **highest-risk identity leftover** (P0-024 Phase 3). Do **not** rename in this task. Do **not** start `TASK-RIGOR-FORMAL-VERIFICATION-IDENTITY-SYNC-P0-029` until P0-026, P0-027, and this audit exist (they do). P0-029 itself still waits on an **ABI implementation** of the option chosen here — a docs/Coq rename must not outrun the library that Coq is supposed to describe.

Live spend path (unchanged):

```
UTXO → ScriptPubKey → AssetTypeFromScriptPubKey → Ownership predicate → Ed25519
```

`libbitcoinconsensus` is a **C wrapper around that path**. It is **not** Bitcoin Script / EvalScript / libconsensus-as-Bitcoin.

---

## Verdict

```
Behavior (VerifyScript):     PASS  (RIGOR)
ABI name / pkg-config:       FAIL  (advertises Bitcoin consensus)
In-tree C API consumers:     tests + fuzz + bench only
Out-of-tree RIGOR consumers: NONE found in this tree
Recommended implementation:  Option B  (honest rename, no silent Bitcoin aliases)
Do not implement in P0-028.
```

**Do not pick Option A** as the end state. Keeping `-lbitcoinconsensus` / `bitcoinconsensus.h` while the body is RIGOR `VerifyScript` is the misunderstanding this audit exists to stop.

---

## 1. What the library actually is

Install / link names today:

| Surface | Live value |
|---------|------------|
| Shared lib | `libbitcoinconsensus.la` → `libbitcoinconsensus.so` (libtool; **no** `-version-info` in `Makefile.am`) |
| Header installed | `script/bitcoinconsensus.h` via `include_HEADERS` → typically `$prefix/include/bitcoinconsensus.h` |
| pkg-config | `libbitcoinconsensus.pc` |
| pkg-config **Name** | `Rigor Core consensus library` |
| pkg-config **Description** | `Library for the Bitcoin consensus protocol.` ← **defect** |
| pkg-config **Libs** | `-lbitcoinconsensus` |
| C API version | `BITCOINCONSENSUS_API_VER 1` |
| Exported symbols | `bitcoinconsensus_verify_script`, `bitcoinconsensus_verify_script_with_amount`, `bitcoinconsensus_version` |
| Flag enum (header) | `NONE`, `SIGPUSHONLY` (`1<<5`), `CLEANSTACK` (`1<<8`), `WITNESS` (`1<<11`), `NULLFAIL` (`1<<14`) |

Implementation (`script/bitcoinconsensus.cpp`): deserialize `CTransaction`, then:

```cpp
VerifyScript(..., TransactionSignatureChecker(...), nullptr)
```

That is the RIGOR interpreter (asset programs + Ed25519), with `chain_id = uint256::ZERO` on this C path (already noted in P0-015 / P0-024). Flag bits are a **C ABI mask**, not a restored Bitcoin Script feature set. The header does **not** export P2SH / DERSIG / CLTV / CSV; `core/doc/shared-libraries.md` still lists those BIPs and says “Arda’s consensus” — **stale docs**, not the live header.

`rigord` / `test_rigor` do **not** `dlopen` this `.so`. They link static `libbitcoin_consensus.a`, which already compiles `script/bitcoinconsensus.cpp`. The shared library is an **extra export** for language bindings.

`formal_verification/` has **zero** hits for `bitcoinconsensus` / `libbitcoinconsensus`. Coq does not currently name this ABI.

---

## 2. Who depends (inventory)

### 2.1 Build / install

| Item | Role |
|------|------|
| `configure.ac` `--with-libs` (default **yes**) | `BUILD_BITCOIN_LIBS` → shared lib |
| `AC_CONFIG_FILES([libbitcoinconsensus.pc:libbitcoinconsensus.pc.in])` | pkg-config generate |
| `core/Makefile.am` `pkgconfig_DATA` | install `.pc` |
| `core/src/Makefile.am` `lib_LTLIBRARIES` + `include_HEADERS` | install `.so` + header |
| `configure.ac` “only libbitcoinconsensus can be built without Boost” | error copy |
| `core/build_msvc/libbitcoinconsensus/` | Windows project (name leftover) |
| `core/doc/design/libraries.md` | graph: *libbitcoinconsensus* = shared *libbitcoin_consensus* |

`--with-libs=no` skips the shared library; the node still has consensus in the static archive.

### 2.2 In-tree C/C++ callers of the C ABI

| File | Use |
|------|-----|
| `core/src/script/bitcoinconsensus.cpp` | definition |
| `core/src/test/script_tests.cpp` | `DoTest` cross-check vs JSON vectors + dedicated error-code cases |
| `core/src/test/fuzz/script_bitcoin_consensus.cpp` | fuzz target `script_bitcoin_consensus` |
| `core/src/bench/verify_script.cpp` | bench vs C API |
| `core/src/test/data/script_tests.json` | comment: chain_id matches libbitcoinconsensus |

No other `.cpp` includes `bitcoinconsensus.h`.

### 2.3 Explorer / pool / Python / website

**No** `bitcoinconsensus` / `libbitcoinconsensus` hits under `explorer/`, `pool/`, `web/`, `formal_verification/`.

`core/test/functional/feature_res_core_functional_longrun.py` only mentions a **link failure** of `libbitcoinconsensus` as an operator hint — not a Python binding.

### 2.4 Documented “external consumers”

`core/doc/shared-libraries.md` lists:

- node-libbitcoinconsensus (BitPay)
- java-libbitcoinconsensus
- bitcoinconsensus-php

Those are **Bitcoin Core / Arda-era bindings**. They are **not** RIGOR products. If they were pointed at this tree’s `.so`, they would believe they verified **Bitcoin Script**. They would not. That is why a silent Option A alias is a footgun.

No RIGOR-named out-of-tree linker (`-lbitcoinconsensus` from `rigorchain/rigorchain`) is documented in this repository.

### 2.5 What is **not** this ABI

| Item | Why |
|------|-----|
| `LIBBITCOIN_CONSENSUS` / `libbitcoin_consensus.a` | Internal static archive (Phase 2 name). Node consensus. Not the C SONAME. |
| RPC `getblock` / `getrawtransaction` | RPC-SHAPE. Keep. Unrelated to libconsensus symbols. |
| `libbitcoinkernel` | Experimental kernel lib. Separate. |
| GitHub `bitcoin/bitcoin` URLs in comments | HISTORICAL / identity for **P0-029**, not this `.so`. |

---

## 3. Option A vs Option B

### Option A — keep ABI, rename internals

Keep:

- `libbitcoinconsensus.so`
- `#include <bitcoinconsensus.h>`
- `bitcoinconsensus_verify_script*`
- `-lbitcoinconsensus`

Change only: file names inside the tree, comments, maybe pkg-config **Description**.

| | |
|--|--|
| Breaks existing `.so` linkers | **No** |
| Stops “Bitcoin Consensus Library” at the linker | **No** (`-lbitcoinconsensus` remains) |
| Stops pkg-config Description bug | **Only if Description is edited** (can be done without A or B) |
| Risk to `VerifyScript` | None if body untouched |
| Honesty | **FAIL** — name still claims Bitcoin |

**Reject as the product end state.** A Description-only fix is a **partial** identity patch and may ship **before** B, but it is not debitcoinization of the ABI.

### Option B — `rigorconsensus.h` / `librigorconsensus`

Ship:

| From | To |
|------|-----|
| `bitcoinconsensus.h` | `rigorconsensus.h` (install name) |
| `libbitcoinconsensus` | `librigorconsensus` |
| `libbitcoinconsensus.pc` | `librigorconsensus.pc` |
| `bitcoinconsensus_*` | `rigorconsensus_*` |
| `BITCOINCONSENSUS_API_VER` | `RIGORCONSENSUS_API_VER` (start at **1**, document not Bitcoin’s API) |
| Fuzz `script_bitcoin_consensus.cpp` | `script_rigor_consensus.cpp` |
| pkg-config Description | RIGOR script/ownership verify (Ed25519 / asset programs). **Not** “Bitcoin consensus protocol.” |
| `--with-libs` help | “build librigorconsensus” |

**Do not change:** `VerifyScript` body, serialization, flag **numeric** values (same `1<<5` etc. unless a later ABI task proves no consumer), RPC methods, wallet schema.

**Do not keep** `bitcoinconsensus_*` as a silent alias that still looks like Bitcoin libconsensus. P0-024: aliases only if a **named RIGOR** `.so` user exists. This inventory found **none**.

Same-release rule: drop old SONAME / header from `make install` in the **same** bump so Debian/homebrew leftovers cannot keep linking a Bitcoin-named lie.

| | |
|--|--|
| Breaks `-lbitcoinconsensus` | **Yes** (intended) |
| In-tree repair | tests, fuzz, bench, Makefile, pkg-config, msvc, `shared-libraries.md` |
| Consensus fork | **No**, if `VerifyScript` is byte-identical |
| Honesty | **PASS** |

### Hybrid (one release, both symbols)

Only if someone later produces a real RIGOR binary that already `dlopen`s this tree’s `libbitcoinconsensus.so`. Then: export `rigorconsensus_*` as the documented API; keep `bitcoinconsensus_*` for **one** SONAME with comments “RIGOR verify, not Bitcoin Script”; drop Bitcoin names in the next major. **Not justified by this audit.**

---

## 4. Recommended decision

**Implement Option B in a dedicated later patch (not P0-028).**

Order inside that patch:

1. Add `rigorconsensus.h` / `.cpp` wrapping the same `verify_script` helper (or rename files and symbols in one commit).
2. Switch `lib_LTLIBRARIES`, `include_HEADERS`, pkg-config, MSVC.
3. Point `script_tests.cpp`, fuzz, bench at the new names.
4. Delete install of `bitcoinconsensus.h` / `libbitcoinconsensus.pc`.
5. Rewrite `core/doc/shared-libraries.md` for RIGOR (drop Arda/BIP16 list; describe actual flags and Ed25519/asset verify). Canonical source links: **`https://github.com/rigorchain/rigorchain`**, not `github.com/bitcoin/bitcoin`.
6. Do **not** touch Coq in that patch — that is P0-029.

Risk controls:

- Diff of `VerifyScript` / interpreter / classify must be **empty**.
- `nm -D librigorconsensus.so` shows only `rigorconsensus_*`.
- `pkg-config --libs librigorconsensus` is `-lrigorconsensus`.
- `pkg-config --description` must not contain “Bitcoin”.

---

## 5. Hold for P0-029 (do not run yet)

`TASK-RIGOR-FORMAL-VERIFICATION-IDENTITY-SYNC-P0-029` updates `formal_verification/`, live docs, Coq namespace, and proof wording **after** identity work is classified:

| Prerequisite | Status now |
|--------------|------------|
| P0-026 test/help/build identity | **Done** (`test_rigor`, `rigor-cli`, `rigor-util`, …) |
| P0-027 wallet native identity | **Report done**; filename still `wallet.dat`, schema still KV, wallet is **SQLite not BDB** |
| P0-028 ABI audit | **This document**; library still `libbitcoinconsensus` until Option B lands |

P0-029 **must**:

1. Point operators and Coq comments at **`https://github.com/rigorchain/rigorchain`**. Replace leftover **product** URLs that still say `github.com/bitcoin/bitcoin` or `bitcoincore.org` in live runbooks / `AC_INIT` (`PACKAGE_BUGREPORT` / `PACKAGE_URL` still Bitcoin). **Keep** HISTORICAL citations in Core-era comments that document a past Bitcoin Core PR (those are not the project homepage).
2. State wallet storage honestly: **SQLite descriptor wallet**, **not Berkeley DB**. Default file is still named `wallet.dat` until a P0-027 *implementation* (target `rigor-wallet.sqlite`). Do not teach BDB restore. Do not claim a typed RIGOR SQL schema.
3. Align Coq / proof **terminology** with RIGOR spend (ScriptPubKey → asset type → Ed25519). Do **not** rename sighash tags or spend-model constructors as a drive-by. Do not claim `libbitcoinconsensus` is Bitcoin consensus — after Option B, name `librigorconsensus` in FV docs.

P0-029 must **not** pretend Option B already shipped if the `.so` is still `libbitcoinconsensus`.

---

## 6. Explicit non-changes (this audit)

| Item | Why |
|------|-----|
| `VerifyScript`, ScriptPubKey, AssetTypeFromScriptPubKey, Ed25519 | Consensus |
| Flag bit numeric values | ABI; keep across B unless a later task proves no consumer |
| RPC method names | RPC-SHAPE |
| `libbitcoin_consensus.a` identifier | Phase 2 static-lib rename, not this C ABI |
| Copyright banners | LEGAL |
| Implementation of Option B | Later patch |
| P0-029 Coq/docs sweep | After this audit + (ideally) after B, with GitHub + SQLite rules above |

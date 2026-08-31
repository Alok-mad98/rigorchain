# RIGOR Bitcoin Legacy Naming / Protocol Identity Decoupling Audit

Task: `TASK-RIGOR-BITCOIN-LEGACY-NAMING-AUDIT-P0-024`  
Type: **Audit only.** No code deleted. No consensus, ScriptPubKey, AssetTypeFromScriptPubKey, RES / ORU / CONVERGE / RES_MINT, Ed25519, Sighash, serialization, or formal model changed.  
Date: 2026-08-26  
Tree: `rigor-official-tree`

**Forbidden this task:** mechanical `Bitcoin → RIGOR` global replace.

---

## 0. Live protocol (identity baseline — not modified)

Authoritative spend path (unchanged by this audit):

```
UTXO
  → ScriptPubKey
  → AssetTypeFromScriptPubKey
  → Fixed Ownership Predicate
  → Ed25519 Verification
```

That path is **RIGOR consensus**. It is **not** Bitcoin Script VM, EvalScript, CHECKMULTISIG, or secp256k1 spend. Naming leftovers do not restore those surfaces.

This audit asks a different question: **does the tree still present itself as Bitcoin Core**, and if so, which names are (a) legal/historical, (b) leftover fork identity, or (c) still load-bearing protocol/API?

---

## 1. Method

Repo greps (case-insensitive where noted) over:

`core/src`, `core/test`, `docs`, `formal_verification`, `scripts`, plus `core/configure.ac`, `core/src/Makefile.am`, `core/Makefile.am`. CMake is vendor-only (`core/src/leveldb/`, `core/src/crc32c/`) — zero Bitcoin-product hits.

Counts below exclude generated object files (`.lo` / `.la` / `.o`), Qt `moc_` / `qrc_`, `Makefile.in`, `configure`, locale `.ts`, and `core/doc/release-notes/`. They still include copyright banners, so **raw counts overstate identity debt**.

| Pattern | `core/src` | `core/test` | `docs` | `formal_verification` | `scripts` |
|---------|------------|-------------|--------|------------------------|-----------|
| `bitcoin` / `Bitcoin` | 3454 | 1433 | 789 | 47 | 6 |
| `btc` / `BTC` | 45 | 158 | 8 | 0 | 0 |
| `satoshi` | 225 | 23 | 26 | 6 | 0 |
| `bitcoind` | 28 | 191 | 25 | 2 | 0 |
| `bitcoinconsensus` | 105 | 1 | 6 | 0 | 0 |
| `wallet.dat` | 75 | 46 | 53 | 0 | 0 |
| `legacy` | 711 | 196 | 213 | 77 | 0 |

`configure.ac`: 114 `bitcoin` hits. `core/src/Makefile.am`: 239. `core/Makefile.am`: 57.

**`core` as a search term is not used as a classifier.** The directory `core/` is the RIGOR node tree. Hits for “Bitcoin Core” (product) and autotools `BITCOIN_*` macros are classified separately. Blind `grep core` matches path names, `libbitcoin_consensus`, and English “core” and is not identity evidence.

---

## 2. Classification taxonomy (do not skip)

Every hit is one of:

| Class | Meaning | Modify? |
|-------|---------|---------|
| **LEGAL** | MIT copyright: Satoshi Nakamoto, The Bitcoin Core developers, `LICENSE` | **NO** (review only; SPDX/headers stay) |
| **HISTORICAL** | Comment / doc that this tree was forked from Bitcoin Core v25 | **NO** if it explains origin; **YES** if it implies live Bitcoin protocol |
| **IDENTITY-SAFE** | Test binary, help string, source filename, Boost module name | **YES** (Phase 1–2) |
| **BUILD-INTERNAL** | `LIBBITCOIN_*`, `BITCOIN_INCLUDES`, include guards `BITCOIN_*` | **YES** (Phase 2; high churn, no consensus) |
| **ABI** | `libbitcoinconsensus` C symbols / SONAME / pkg-config | **REVIEW** (Phase 3) |
| **RPC-SHAPE** | `getblock`, `getrawtransaction`, JSON field names | **NO** (RIGOR RPC compatibility, not Bitcoin-network compatibility) |
| **CONSENSUS-SCALE** | `COIN = 1e8`, SHA-256d, wire layout inherited as RIGOR consensus | **NO** (not a name; changing it is a fork) |
| **COMPAT-FALLBACK** | `bitcoin.conf` still read if `rigor.conf` absent | **REVIEW** (document or deprecate; not a rename-in-place) |
| **WALLET-ARTIFACT** | `wallet.dat` filename, “legacy wallet” RPC copy | **YES** name later; **NO** schema this task (see P0-022) |
| **UNIT-COMMENT** | “BTC” / “satoshi” in comments while amounts are ORU/Tharn | **YES** comments; **NO** `CAmount` scale |
| **NOISE** | `core/` path, vendor LevelDB, `hitbtc` in `pool/` (out of scope) | **NO** |

Mechanical replace is forbidden because LEGAL, RPC-SHAPE, and CONSENSUS-SCALE would be destroyed by a global substitute.

---

## 3. Already-native identity (do not “fix” again)

These are **already RIGOR** at the install / user-facing layer:

| Item | Status |
|------|--------|
| Daemon install name | `rigord` (`BITCOIN_DAEMON_NAME=rigord`) |
| CLI / tx / wallet-tool / GUI install names | `rigor-cli`, `rigor-tx`, `rigor-wallet`, `rigor-qt` |
| Datadir | `~/.rigor` (`GetDefaultDataDir`) |
| Primary config | `rigor.conf` (`RIGOR_CONF_FILENAME`) |
| Autotools package | `AC_INIT([Rigor Core], …, [rigor], …)` |
| Display version | `RIGOR_RELEASE_VERSION` / `"RIGOR version "` in `clientversion.cpp` |
| Runtime copyright line | `"The Rigor Core developers"` (`COPYRIGHT_HOLDERS_SUBSTITUTION`) |
| Qt main window class | `RigorGUI` (file still `bitcoingui.h`) |
| Display units | ORU / mORU / uORU / Tharn in `BitcoinUnits` |
| Installed man pages | `rigord.1`, `rigor-cli.1`, `rigor-tx.1`, `rigor-util.1`, `rigor-wallet.1`, `rigor-qt.1` |
| Functional test binary resolver | prefers `rigord` / `rigor-cli` over stale `bitcoind` |

Partial migration is why a global replace would double-rename live RIGOR names and miss the leftover source/help/ABI layer.

---

## 4. Classification table (load-bearing artifacts)

| Path | Current name | Type | Modify? | Suggestion |
|------|----------------|------|---------|------------|
| `core/src/test/main.cpp` | `BOOST_TEST_MODULE Bitcoin Core Test Suite` | IDENTITY-SAFE | **YES** | `RIGOR Consensus Test Suite` |
| `core/src/Makefile.test.include` | `test/test_bitcoin`, `BITCOIN_TESTS` | IDENTITY-SAFE | **YES** | `test/test_rigor` |
| `core/configure.ac` | `BITCOIN_TEST_NAME=test_bitcoin` | IDENTITY-SAFE | **YES** | `test_rigor` |
| `core/src/wallet/test/*` (Boost cases) | no `test_bitcoin-wallet` binary | — | **N/A** | Separate wallet Boost binary **does not exist**. Comment in `Makefile.test.include` says Core `wallet/test` cases are **disabled** in `test_bitcoin`. RIGOR wallet tests live as `test/wallet_*_tests.cpp` inside the same binary. Rename with `test_bitcoin` → `test_rigor`. Do **not** invent `test_bitcoin-wallet`. |
| `core/test/util/data/bitcoin-util-test.json` | `./bitcoin-util`, `./bitcoin-tx` exec names | IDENTITY-SAFE | **YES** | `rigor-util-test.json`; exec `./rigor-util` / `./rigor-tx` **after** install-name change |
| `core/src/bitcoin-cli.cpp` | compiled as `rigor-cli`; help still `Usage: bitcoin-cli` | IDENTITY-SAFE | **YES** | Rename source later; **help strings Phase 1** (`rigor-cli`) |
| `core/src/bitcoin-tx.cpp` | compiled as `rigor-tx`; help `bitcoin-tx` / “bitcoin transaction” | IDENTITY-SAFE | **YES** | Help → `rigor-tx` / “RIGOR transaction”. Source rename Phase 2 |
| `core/src/bitcoind.cpp` | compiled as `rigord`; comments still “bitcoind” | IDENTITY-SAFE | **YES** | Comments/help; source rename Phase 2 |
| `core/src/bitcoin-util.cpp` | **install name still `bitcoin-util`** | IDENTITY-SAFE | **YES** | Install `rigor-util` (man page already `rigor-util.1`; binary lagged) |
| `core/src/bitcoin-wallet.cpp` | compiled as `rigor-wallet`; `-legacy` flag | IDENTITY-SAFE / WALLET | **YES** help; **REVIEW** `-legacy` (create already rejects legacy) |
| `core/src/bitcoin-chainstate.cpp` | install `bitcoin-chainstate` (experimental, off by default) | IDENTITY-SAFE | **YES** | `rigor-chainstate` if the binary is kept |
| `core/src/init/bitcoin-node.cpp` | install `bitcoin-node` (multiprocess, experimental) | IDENTITY-SAFE | **YES** | `rigor-node` if kept |
| `core/src/init/bitcoin-gui.cpp` | `bitcoin-gui` multiprocess | IDENTITY-SAFE | **YES** | `rigor-gui` if kept |
| `core/src/bench/bench_bitcoin.cpp` | `bench/bench_bitcoin` | IDENTITY-SAFE | **YES** | `bench_rigor` |
| `core/src/script/bitcoinconsensus.h` | C ABI `bitcoinconsensus_*` | **ABI** | **REVIEW** | See §6.3. Semantically RIGOR; name is Bitcoin libconsensus |
| `core/src/script/bitcoinconsensus.cpp` | same; calls `VerifyScript` (RIGOR path) | **ABI** | **REVIEW** | Implementation is **not** Bitcoin Script consensus |
| `core/libbitcoinconsensus.pc` | `Name: Rigor Core consensus library` / `Description: Library for the Bitcoin consensus protocol.` / `-lbitcoinconsensus` | **ABI** + identity bug | **YES** (pkg-config text) / **REVIEW** (SONAME) | Description must not say Bitcoin consensus. Library rename is Phase 3 |
| `core/src/test/fuzz/script_bitcoin_consensus.cpp` | fuzzes `bitcoinconsensus_*` | IDENTITY-SAFE | **YES** with ABI rename | `script_rigor_consensus.cpp` |
| `core/src/config/bitcoin-config.h.in` | generated config header | BUILD-INTERNAL | **YES** Phase 2 | `rigor-config.h` is currently a **shim** that includes `bitcoin-config.h` |
| `core/src/config/rigor-config.h` | `#include <config/bitcoin-config.h>` | BUILD-INTERNAL | keep shim until header flip | Flip primary name, keep old include as one-release alias if needed |
| `core/src/qt/bitcoingui.{h,cpp}` | file Bitcoin; class `RigorGUI` | IDENTITY-SAFE | **YES** Phase 2 | `rigorgui.{h,cpp}` |
| `core/src/qt/bitcoinunits.{h,cpp}` | class `BitcoinUnits`; units are ORU/Tharn | IDENTITY-SAFE | **YES** Phase 2 | `RigorUnits` / `rigorunits.*` |
| `core/src/qt/bitcoin.cpp` | `BitcoinApplication`; files `bitcoin.qrc` | IDENTITY-SAFE | **YES** Phase 2 | `RigorApplication`; `rigor.qrc` |
| `core/src/qt/bitcoinaddressvalidator.*` | class name Bitcoin | IDENTITY-SAFE | **YES** Phase 2 | address validator for RIGOR encodings |
| `core/src/qt/locale/bitcoin_*.ts` | Qt locale basename | IDENTITY-SAFE | **YES** Phase 2 | high churn; pair with `.qrc` |
| `#ifndef BITCOIN_*` (hundreds of headers) | include guards | BUILD-INTERNAL | **YES** Phase 2 | `RIGOR_*` — mechanical **within this class only**, not a repo-wide Bitcoin string replace |
| `LIBBITCOIN_NODE` etc. `Makefile.am` | static lib names | BUILD-INTERNAL | **YES** Phase 2 | `librigor_node.a` … |
| `AC_INIT` bug URL | `https://github.com/bitcoin/bitcoin/issues` and `https://bitcoincore.org/` | IDENTITY-SAFE | **YES** | RIGOR issue tracker / site |
| File-header `Copyright … Satoshi Nakamoto` / `The Bitcoin Core developers` | LEGAL | **NO** | Keep. May **add** Rigor Core line (already done in some Qt files) |
| `LICENSE` | Bitcoin Core / Bitcoin Developers copyright | LEGAL | **NO** / **REVIEW** | MIT requires retaining. Adding Rigor Core is allowed; deleting Bitcoin is not |
| `bitcoin.conf` fallback (`BITCOIN_CONF_FILENAME`) | COMPAT-FALLBACK | **REVIEW** | Do not silently drop. Deprecate with a log, then remove in a later release |
| RPC `getblock`, `getrawtransaction`, `getblocktemplate`, … | RPC-SHAPE | **NO** | RIGOR clients (explorer, wallet, tests) depend on these names |
| `CAmount` / `COIN = 100000000` | CONSENSUS-SCALE | **NO** | 1 ORU = 1e8 base units (Tharn). Not a Bitcoin product name |
| Comments “N BTC” / “satoshis” in tests (`coinselector_tests.cpp`, `miner_tests.cpp`, …) | UNIT-COMMENT | **YES** | Say ORU / Tharn. Do not change numeric fixtures unless a test is already wrong |
| `FeeRate` enum `BTC_KVB` (`policy/feerate.h`) | UNIT-COMMENT / API | **YES** | `ORU_KVB` — wallet/RPC display; confirm no serialized fee-estimate key uses the enumerator name |
| `wallet.dat` default SQLite filename | WALLET-ARTIFACT | **YES** later | Target `wallet.sqlite` / RIGOR name. **User-data migration** — not this audit (P0-022) |
| RPC/help “legacy wallet”, `LegacyScriptPubKeyMan`, `-legacy` | WALLET-ARTIFACT | **YES** copy; **NO** delete types this phase | Create path already requires descriptors. Leftover **language** implies BDB wallets still exist |
| `core/test/functional/test_framework.py` | `BitcoinTestFramework`, `TMPDIR_PREFIX = "bitcoin_func_test_"` | IDENTITY-SAFE | **YES** | `RigorTestFramework`; tmp prefix `rigor_func_test_` |
| `core/test/functional/test_framework/test_node.py` | docs/params `bitcoind`, `bitcoin_cli` | IDENTITY-SAFE | **YES** | `rigord` / `rigor_cli` parameter names (map to same binaries) |
| `core/test/functional/interface_bitcoin_cli.py` | CLI interface test | IDENTITY-SAFE | **YES** | `interface_rigor_cli.py` |
| Functional tests `feature_segwit.py`, `feature_dersig.py`, `p2p_segwit.py`, … | mixed: hostility / inherited harness | HISTORICAL + test content | **REVIEW** | Keep as **reject / hostility** tests if they assert RIGOR refusal. Do not rename to imply SegWit is a RIGOR feature |
| `feature_rigor_*.py`, `feature_res_*.py`, `test/ed25519_*`, `script_asset_ownership_freeze_tests.cpp` | already RIGOR protocol tests | IDENTITY-SAFE (harness only) | **YES** harness names | Content is native; suite name should match |
| `formal_verification/coq/CanonicalHashKernel.v` | “Bitcoin Hash256” | HISTORICAL (algorithm) | **NO** | SHA-256d is the hash; citing Bitcoin as **composition origin** is accurate |
| `formal_verification/rigor_v01/*` | RIGOR v0.1 spend model | native | **NO** this task | Already decoupled from Script VM |
| `formal_verification/scripts/run_differential_gate.sh` | `TEST_BITCOIN`, `test_bitcoin` | IDENTITY-SAFE | **YES** with Phase 1 | `TEST_RIGOR` / `test_rigor` |
| `docs/wallet/BITCOIN_V25_WALLET_INHERITANCE_ANALYSIS.md` | historical analysis | HISTORICAL | **NO** | Keep as inheritance record |
| `docs/audit/BITCOIN_CRYPTO_RESIDUE_CLASSIFICATION.md` | historical audit | HISTORICAL | **NO** | Keep |
| `docs/security/*` audits saying “inside bitcoind” | docs using old binary name | IDENTITY-SAFE | **YES** Phase 4 | Say `rigord` when meaning the live node |
| Stale man `core/doc/man/bitcoin-*.1` | not in `dist_man1_MANS`; still in tree | IDENTITY-SAFE | **YES** | Delete or stop shipping once `rigor-*.1` is canonical (`bitcoin-util.1` still titled `arda-util` — leftover third name) |
| `core/src/wallet/bdb.cpp` | BDB source still present, not linked (`use_bdb=no`) | WALLET (P0-022) | not this task | Naming “Bitcoin wallet / BDB” is residual source, not a live backend |

---

## 5. Focus reviews

### 5.1 Test system

**Current advertised identity**

```
Bitcoin Core Test Suite   (BOOST_TEST_MODULE)
binary: test/test_bitcoin
functional: BitcoinTestFramework + bitcoind/bitcoin-cli parameter names
```

**What the tests actually are**

The Boost binary is a **mixed** suite:

| Kind | Examples | Role today |
|------|----------|------------|
| RIGOR protocol | `ed25519_*`, `script_asset_ownership_freeze_tests`, `rigor_witness_tx_tests`, `res_*`, `convergence_*`, `selection_golden_tests`, `wallet_tx_semantic_tests` | Live RIGOR consensus / wallet semantics |
| Inherited node plumbing | `addrman_tests`, `net_tests`, `mempool_tests`, `serialize_tests` | Still valid for this node |
| Hostility / reject | `script_segwit_tests`, Taproot reject functional tests, secp skips | Must keep **as refusal tests**, not as Bitcoin feature tests |

There is **no** `test_bitcoin-wallet` executable. `Makefile.test.include` explicitly does **not** register Core’s `wallet/test/*.cpp` Boost cases (“disabled until they are updated for Rigor”). Fuzz still compiles `wallet/test/fuzz/*`. Product wallet tests that *are* linked are the `test/wallet_*_tests.cpp` files inside `test_bitcoin`.

**Judgment:** the harness name is Bitcoin; a large and growing fraction of **content** is RIGOR. Renaming `test_bitcoin` → `test_rigor` and the Boost module string is **justified** and **safe** (Phase 1). It does not claim the inherited plumbing tests were rewritten.

Functional tests already launch `rigord`. Class `BitcoinTestFramework` and tmpdir `bitcoin_func_test_` are leftover identity.

`bitcoin-util-test.json` still execs `./bitcoin-util` and `./bitcoin-tx`. The **tx** tool install name is already `rigor-tx`; the JSON is stale relative to the binary. The **util** install name is still `bitcoin-util`, so JSON matches util and mismatches tx. That split is itself evidence against a global replace: the tree is mid-migration.

### 5.2 Tool names

| Source file | `bin_PROGRAMS` (Makefile.am) | User-visible help | Verdict |
|-------------|------------------------------|-------------------|---------|
| `bitcoind.cpp` | `rigord` | mostly invocation basename; comments “bitcoind” | Rename source Phase 2; comments Phase 1 |
| `bitcoin-cli.cpp` | `rigor-cli` | **still `bitcoin-cli`** | Help **YES** now; RPC **method** names **NO** |
| `bitcoin-tx.cpp` | `rigor-tx` | **still `bitcoin-tx` / “bitcoin transaction”** | Help YES |
| `bitcoin-util.cpp` | **`bitcoin-util`** | `bitcoin-util` | **Install name should become `rigor-util`** (man page already thinks so) |
| `bitcoin-wallet.cpp` | `rigor-wallet` | `-legacy` still advertised | Help/flag REVIEW |

**RPC:** `getblock`, `getrawtransaction`, `getblockcount`, GBT, wallet RPCs stay. Those are RIGOR’s JSON-RPC surface (explorer, Qt, functional tests). Renaming them is a **protocol-client break**, not identity cleanup.

Configure still prints “whether to build bitcoind / bitcoin-cli / bitcoin-tx” while the products are `rigord` / `rigor-cli` / `rigor-tx`. Operator-facing configure messages are IDENTITY-SAFE.

### 5.3 Consensus interface (`bitcoinconsensus.h`)

**Does the Bitcoin name still mean Bitcoin consensus?** No.

`bitcoinconsensus_verify_script*` deserializes a tx and calls `VerifyScript`. Live `VerifyScript` (`interpreter.cpp`) is the RIGOR dispatcher: RES mint, witness v0/v1 programs, Ed25519 — **not** EvalScript. Flag bits remain Bitcoin-shaped (`SCRIPT_VERIFY_WITNESS`, etc.) as a **C ABI mask**, not as a Bitcoin Script feature set. Prior audit (P0-015) already noted the compatibility flag mask and `chain_id = ZERO` on this path.

`libbitcoinconsensus.pc` currently **contradicts itself**:

- Name: “Rigor Core consensus library”
- Description: “Library for the Bitcoin consensus protocol.”

Keeping the **Bitcoin** symbol names advertises Bitcoin libconsensus to any `-lbitcoinconsensus` consumer. That is **more dangerous** than renaming: a Bitcoin-oriented caller would believe they verified Bitcoin Script. They would not.

**ABI risk (real, but not “Bitcoin network compatibility”):**

| Risk | Notes |
|------|--------|
| SONAME / `-lbitcoinconsensus` | Any out-of-tree linker using this fork’s library |
| C symbol names | `bitcoinconsensus_verify_script`, `BITCOINCONSENSUS_API_VER` |
| Header path | `<bitcoinconsensus.h>` vs `<rigorconsensus.h>` |
| Known in-tree consumer | fuzz target + optional `--enable-shared` install. No evidence of a Bitcoin-mainnet tool depending on **this** tree’s `.so` |

**Recommendation (Phase 3, not Phase 1):** introduce `rigorconsensus_*` as the honest API. Do **not** keep `bitcoinconsensus_*` as a silent alias that implies Bitcoin Script. A short deprecation alias is only justified if an actual RIGOR external is already linked; none is documented in-tree. Changing this is **ABI**, not consensus logic.

`bitcoinconsensus` is **not** a reason to keep Bitcoin Script.

### 5.4 Wallet

Live architecture (P0-022, unchanged): SQLite descriptor wallet, BDB not compiled. Default file name is still **`wallet.dat`**. RPC and comments still say **legacy wallet**. `bitcoin-wallet -legacy` is still parsed.

**Forbidden appearance vs reality**

| Phrase | In tree? | Live? |
|--------|----------|-------|
| “Bitcoin wallet” as product | comments / inheritance docs | **No** (Rigor Core wallet) |
| “legacy wallet” | RPC help, `LegacyScriptPubKeyMan`, tests | **Create: no.** Residual types/help: **yes** |
| BDB `wallet.dat` | `bdb.cpp` source; load fail-closed | **Not linked** |
| SQLite file named `wallet.dat` | `SQLiteDataFile` | **Yes** |

Target identity: **RIGOR SQLite Wallet**. Current identity: **Bitcoin Core v25 descriptor KV in SQLite, Bitcoin-era filename**.

This audit does **not** rename `wallet.dat` (user-data). It classifies the name as WALLET-ARTIFACT / YES-later.

### 5.5 Documentation

| Keep | Delete or rewrite in Phase 4 |
|------|------------------------------|
| Historical fork explanation (“derived from Bitcoin Core v25”) | Docs that call the live daemon `bitcoind` as if that were the product |
| LEGAL copyright | Active docs saying the test suite is “Bitcoin Core tests” |
| Algorithm origin (SHA-256d / Hash256) | “Bitcoin consensus library” pkg-config description |
| Inheritance audits (`BITCOIN_V25_WALLET_*`, crypto residue) | User-facing README/help that still trains `bitcoin-cli` |
| RPC names in API docs | Implied SegWit/Taproot as supported features |

`scripts/alpha/build_alpha_linux.sh` already documents `~/.rigor` and `bitcoin.conf` fallback — correct COMPAT-FALLBACK documentation, not a bug.

---

## 6. Forbidden modification zones (this task and any rename task)

Do **not** touch as part of identity work:

- `AssetTypeFromScriptPubKey` / `script/rigor_classify.cpp` semantics
- RES / ORU / CONVERGE / RES_MINT rules
- Ed25519 verify, sighash tags, `SignatureHash` layout
- Formal v0.1 model constructors
- Serialization of txs/blocks, `COIN`, `MAX_MONEY`
- RPC method **names**

Renames of files that **contain** those symbols are allowed only as **filename/identifier** moves that do not change bytes of consensus functions.

---

## 7. Formal verification terminology

| Artifact | Bitcoin/BTC/Core/Satoshi use | Class |
|----------|------------------------------|--------|
| `formal_verification/rigor_v01/` | “Obsolete Bitcoin / Alpha surfaces” as **non-constructors** | **KEEP** (correct contrast) |
| `CanonicalHashKernel.v`, `CanonicalDrawEncoder.v` | “Bitcoin Hash256” = SHA-256d | **KEEP** historical algorithm citation |
| `RigorSha256.v` | comparison to Bitcoin `CSHA256` / FIPS | **KEEP** |
| `rigor_consensus_spec.py` | “Bitcoin uint256::SetHex” memory layout | **KEEP** (endianness origin) |
| `ConnectBlockMint.v` / `OrderIndependence.v` / `legacy_account_model/Selection.v` | amounts in “satoshis” | **MODIFY** Phase 4 → Tharn / base units (same integers) |
| `fee-burn/Precision.v` | `BitcoinUnits::formatRes` | **MODIFY** when Qt class renames |
| `run_differential_gate.sh` | `test_bitcoin` | **MODIFY** with Phase 1 binary |
| `archive/OBSOLETE_MODEL_INVENTORY.md` | Bitcoin opcode leftover in C++ | **KEEP** historical |
| `legacy_account_model/` | directory “legacy” = old **account** model, not Bitcoin product | **KEEP** name as archive label |

Formal **specification of spend** is already RIGOR (`rigor_v01`). Formal **vocabulary** is not fully native (satoshi, test_bitcoin, Hash256-as-Bitcoin). That is terminology debt, not a Bitcoin VM dependency.

---

## 8. BITCOIN LEGACY DECOUPLING STATUS

| Gate | Result | Why |
|------|--------|-----|
| **Historical naming** | **FAIL** | Thousands of Bitcoin identity hits; mid-migration (`rigord` vs `bitcoin-util` vs help text `bitcoin-cli`) |
| **Runtime dependency** | **PASS** | No Bitcoin Core process, no Bitcoin P2P network, datadir `~/.rigor`. `bitcoin.conf` is a filename fallback only |
| **Consensus dependency** | **PASS** | Unique spend path is RIGOR ScriptPubKey → asset type → Ed25519. `libbitcoinconsensus` is a **misnamed RIGOR** verifier, not Bitcoin Script |
| **Wallet dependency** | **FAIL** | SQLite-only at link, but `wallet.dat` + “legacy wallet” language + v25 KV schema (P0-022). Not a Bitcoin-branded product, not a RIGOR-native wallet identity either |
| **Test framework** | **FAIL** | Content largely RIGOR + inherited plumbing; **name** is still Bitcoin Core Test Suite / `test_bitcoin` / `BitcoinTestFramework` |
| **Formal terminology** | **FAIL** | v0.1 model is native; older Coq/scripts still say satoshi / test_bitcoin / Bitcoin Hash256 (latter KEEP as origin) |

**RIGOR Native Identity Ready: NO**

Ready would require at least: Phase 1 test/help names, Phase 2 `bitcoin-util` install name + source/build identifiers, Phase 3 honest `rigorconsensus` ABI, Phase 4 docs/FV sync — without touching consensus.

Companion: [`RIGOR_BITCOIN_DECOUPLING_MIGRATION_PLAN_P0_024.md`](./RIGOR_BITCOIN_DECOUPLING_MIGRATION_PLAN_P0_024.md).

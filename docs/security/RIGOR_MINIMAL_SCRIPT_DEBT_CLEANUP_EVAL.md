# RIGOR Minimal Script Debt Cleanup — Evaluation

**Follows:** commit `6a0715d` (final hardening) and [RIGOR_MINIMAL_SCRIPT_FINAL_HARDENING_REPORT.md](./RIGOR_MINIMAL_SCRIPT_FINAL_HARDENING_REPORT.md)  
**Date:** 2026-08-19  
**Consensus impact:** **NONE**  
**`equivalence_claimed`:** `false`

This document is the Phase 1 impact analysis for four leftover items. Phase 2 implements only the items this report marks **FIX**. Items marked **KEEP / DOCUMENT** are not changed in consensus, validation, or descriptor spend paths.

Canonical statement:

> RIGOR minimizes script complexity rather than eliminating scripts. The protocol intentionally restricts execution semantics to preserve deterministic verification and reduce consensus attack surface.

---

## Verdicts

| # | Item | Verdict | Why |
|---|------|---------|-----|
| 1 | P2WSH attack surface | **REMOVED** | Consensus no longer accepts v0/32 outputs or inner-script execution. `wsh()` / `multi()` descriptors and `createmultisig` / `addmultisigaddress` are refused. |
| 2 | Missing `cvg2()` descriptor | **DO NOT ADD** | CVG2 is a protocol-controlled unspendable ORU-domain burn, built by `createconvergence`. A spendable-looking descriptor would confuse Bech32m/RES with Convergence. |
| 3 | `ParseScriptContext::P2TR` + Qt `Bech32m (Taproot)` | **FIX** | Dead parse context; live Qt already says RES. Cleaning does not change `res()` / `wpkh()` / `wsh()` or consensus. |
| 4 | Python `ORB2 = b"ORB2"` | **FIX** | Test-model vocabulary only. Align with consensus `RES2` and 32-byte `IsResScript`. Do not change C++ wire tags. |

**Not modified in this pass:** `core/src/consensus/`, `core/src/kernel/`, `pow.cpp`, `validation.cpp` rules, `IsAllowedScript`, `VerifyWitnessProgram`, RFC text.

---

## Item 1 — P2WSH as inherited opcode surface

### What it was

Witness v0 P2WSH (`WITNESS_V0_SCRIPTHASH`, 32-byte SHA256 of the witness script) was an inherited Bitcoin output type. Inner scripts ran at `SigVersion::WITNESS_V0` and were the last multi-key / multi-opcode spend path.

### Decision (superseded)

**P2WSH has been removed from consensus.** `IsAllowedScript` / `ClassifyRigorTxout` / `VerifyWitnessProgram` reject v0/32. `OP_CHECKMULTISIG` is `SCRIPT_ERR_BAD_OPCODE`. `wsh()` / `multi()` descriptors and `createmultisig` / `addmultisigaddress` are refused. Residual EvalScript remains in the binary for tests and `bitcoinconsensus` JSON vectors; it is not on any consensus spend path (P2WPKH, native RES, and OP_RES_MINT are structured signature checks).

---

## Item 2 — Missing `cvg2()` descriptor

### Current construction

Convergence outputs are built by wallet RPC `createconvergence` (`core/src/wallet/rpc/rpcspend.cpp`) using `Consensus::ConvergenceOutputScript()`. They are ORU-domain, not inserted as spendable UTXOs, and spending them fails with `bad-txns-spend-convergence`.

Wallet classification already distinguishes CVG2 vs RES by witness-v1 prefix (`IsConvergenceScript` vs `IsResScript`). RES receive/spend uses `res()`. `tr()` / `rawtr()` remain permanently rejected.

### What adding `cvg2()` would do

A new descriptor family would invent a **spendable-looking** import/export object for an **unspendable** protocol burn. Risks:

- users and external descriptor tools would treat it like `res()` / Bech32m receive;
- tests would need a new parse path adjacent to witness v1 RES, increasing the chance of mixing CVG2 with RES2;
- RC0 has no wallet need: construction is RPC-only.

### Wallet / test compatibility if added

Adding `cvg2()` would **not** be required for existing wallets, but it **would** expand the descriptor test matrix (`descriptor_tests.cpp`, import/export RPCs) and create a new public string that must stay unspendable forever. That is a feature, not a cleanup.

### Decision

**Do not implement `cvg2()`.** Document the omission as intentional. SSOT remains `createconvergence` + `ConvergenceOutputScript()`.

---

## Item 3 — `ParseScriptContext::P2TR` and Qt Taproot string

### Descriptor enum

`ParseScriptContext::P2TR` lived in `core/src/script/descriptor.cpp` only. `tr()` / `rawtr()` return errors **before** any nested parse context is entered. Grep showed **no caller** passing `ParseScriptContext::P2TR`. Branches that tested `ctx == ParseScriptContext::P2TR` were dead:

- 32-byte x-only hex key acceptance
- WIF key x-only normalization
- `ConstPubkeyProvider(..., xonly=true)` / `PKDescriptor(..., xonly=true)`

Live parse contexts remain `TOP`, `P2SH`, `P2WPKH`, `P2WSH`. `res()` uses `ParseEd25519ResKeyArg`, not the P2TR path.

### Wallet compatibility

| Surface | Effect of removing the enum |
|---------|-----------------------------|
| `wpkh()` / `wsh()` / `pk()` / miniscript-in-`wsh()` | Unchanged; never used P2TR context |
| `res()` | Unchanged |
| `tr()` / `rawtr()` rejection strings | Kept (still say use `res()`, not Taproot) |
| Imported Bitcoin Taproot descriptors | Still rejected |
| `OutputType::BECH32M` RPC name `"bech32m"` | Unchanged (encoding name, not Taproot) |

`ConstPubkeyProvider::m_xonly` / `PKDescriptor::m_xonly` remain as constructor flags defaulting to `false`. They are no longer set true from the parser.

### Qt

Live UI in `receivecoinsdialog.cpp` already labels the type **RES (Bech32m, witness v1)**. The string `Bech32m (Taproot)` survived only in English lupdate artifacts (`bitcoin_en.ts`, `bitcoin_en.xlf`). Other locale files were not mass-edited (they mostly still carry BIP-350 / SegWit wording, not this Taproot label).

### Decision

**Safe to clean.** No consensus, no address format change, no descriptor checksum change for valid RIGOR descriptors.

---

## Item 4 — Python `ORB2` → consensus `RES2`

### Mismatch

`core/test/rigor/rigor_model.py` defined `ORB2 = b"ORB2"` and treated RES as **witness v1 + 36-byte program starting with ORB2**. Consensus is:

- wire prefix **`RES2`** (`R E S 2`), not `ORB2`;
- `IsResScript`: witness v1, **32-byte** program, **not** CVG2;
- 36-byte v1 programs are **not** `IsAllowedScript` chain outputs.

Callers of the Python constant were `rigor_model.py` and `test_script_assets.py` only. Functional comment in `rigor_attack_blocks.py` mentions historical ORB2 and is left as a comment.

### Wallet / C++ compatibility

None. This is the constitution test model, not the wallet. Naive rename `ORB2` → `RES2` **without** fixing length 36 → 32 would still disagree with C++. The fix therefore:

- `RES2 = b"RES2"`; drop `ORB2`;
- classify RES as v1 / len 32 / not CVG2 (align `IsResScript`);
- test both RES2-prefixed and untagged 32-byte programs;
- reject the stale 36-byte shape.

C++ `RigorTags::Res2WirePrefix` and RFC-015 are **not** changed.

### Decision

**Safe to clean** in the Python model and its unit test. Run `python3 -m unittest` in `core/test/rigor`.

---

## Phase 2 implementation map

| Item | Code / docs |
|------|-------------|
| 1 | Docs only: this file + registry / hardening leftover tables |
| 2 | Docs only: explicit “no `cvg2()`” |
| 3 | `descriptor.cpp` (remove enum + dead branches); `bitcoin_en.ts` / `bitcoin_en.xlf` source strings |
| 4 | `rigor_model.py`, `test_script_assets.py` |

---

## Validation after Phase 2

| Check | Result |
|-------|--------|
| `python3 -m unittest discover -s . -p 'test_*.py' -v` in `core/test/rigor` | **PASS** (18 tests) |
| Descriptor public strings `wpkh` / `wsh` / `res` | Unchanged |
| `tr()` / `rawtr()` still rejected | Unchanged (rejection kept; P2TR parse context removed) |
| Consensus serialization / `IsAllowedScript` | Unchanged |
| C++ `test_bitcoin` descriptor suite | Not rebuilt (no CMake cache in this tree); live parse contexts unchanged |

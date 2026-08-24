# RIGOR Minimal Script Architecture — Final Hardening Report

**Task:** `TASK-RIGOR-MINIMAL-SCRIPT-ARCHITECTURE-FINAL-HARDENING-001`  
**Follows:** `TASK-RIGOR-MINIMAL-SCRIPT-CONSISTENCY-AUDIT-001`  
**Date:** 2026-08-19  
**Consensus impact:** **NONE**  
**`equivalence_claimed`:** `false`  
**Script behavior:** **unchanged**

This pass does not redesign scripts, add script types, or alter validation. It removes misleading names in comments/docs and publishes the official boundary.

Canonical statement:

> RIGOR minimizes script complexity rather than eliminating scripts. The protocol intentionally restricts execution semantics to preserve deterministic verification and reduce consensus attack surface.

---

## Supported scripts

| Family | Role |
|--------|------|
| Witness v0 P2WPKH | ORU spend layer; Ed25519 |
| Witness v0 P2WSH | ORU inner script; largest inherited surface; Ed25519 CHECKSIG |
| Witness v0 native 32-byte Ed25519 | ORU spend when program equals pubkey |
| Witness v1 CVG2 | Convergence commitment; ORU domain; unspendable |
| Witness v1 RES key-path | Native RES (`RES2` prefix or untagged 32-byte Ed25519 program) |
| `OP_RETURN` (value 0) | Data commitment only |

Witness v1 is **RIGOR-defined**. It is **not** Bitcoin Taproot / BIP341.

Registry: [RIGOR_SCRIPT_TYPE_REGISTRY.md](./RIGOR_SCRIPT_TYPE_REGISTRY.md)

---

## Rejected scripts

| Surface | Status |
|---------|--------|
| P2PKH / P2SH / bare multisig | Consensus-invalid as chain outputs |
| Bitcoin Taproot control blocks / annex / tapscript | Not on the consensus path |
| secp256k1 CHECKSIG | `SCRIPT_ERR_SECP256K1_DISABLED` |
| Arbitrary contract execution / VM / EVM | Not implemented |
| Spending CVG2 | `bad-txns-spend-convergence` |

---

## Comment / naming cleanup (no behavior)

Comments that called current v1 **ORB2** or **P2TR** were updated to **RES2** / native RES / not BIP341 in:

- `core/src/script/interpreter.cpp` (comments only)
- `core/src/script/interpreter.h` (comments only)
- `core/src/script/script.h`, `standard.h` (guard comments)
- `core/src/primitives/transaction.cpp` (comment only)
- `core/src/policy/policy.h`, `policy.cpp` (comments only; policy.cpp already enforced 2-item stacks)
- `core/src/script/signingprovider.cpp` (comment only)

**Not changed:** function names, wire bytes (`CVG2` / `RES2` / `RIGR`), `IsAllowedScript`, `VerifyWitnessProgram` control flow, wallet logic, miner, kernel, `core/src/consensus/`, `validation.cpp`.

---

## Wallet / descriptor review (gaps documented, not implemented)

| Path | Status |
|------|--------|
| ORU | `wpkh()` / `wsh()` descriptors |
| RES | `res()` descriptor; `tr()` / `rawtr()` permanently rejected |
| CVG2 | **No `cvg2()` descriptor** (intentional). Construction is `createconvergence` RPC / `ConvergenceOutputScript()`, not a descriptor family |
| Internal leftover | `ParseScriptContext::P2TR` **removed** in the debt-cleanup pass (dead parse context; `tr()`/`rawtr()` rejection kept) |
| Qt leftover | English lupdate source `Bech32m (Taproot)` replaced with live UI `RES (Bech32m, witness v1)` |
| Wallet comments | Some `wallet/` comments still say ORB2. Left untouched (no wallet logic edits) |

---

## Remaining risks

| Risk | Notes |
|------|--------|
| P2WSH inner scripts | Real opcode surface; Ed25519-only CHECKSIG. **Kept** for RC0 (AutoResMint + `wsh()`). See debt-cleanup eval |
| `WitnessV1Res` extends `XOnlyPubKey` | Storage type from Bitcoin era; not BIP341 tweak semantics |
| Python `rigor_model.py` | **Aligned:** `RES2` + 32-byte `IsResScript` shape (was stale 36-byte `ORB2`) |
| 36-byte claim program constant | Not an `IsAllowedScript` output; Python model now rejects it as RES |
| `equivalence_claimed = false` | Unchanged |
| Wallet ORB2 comments | Still present in a few `wallet/` comments; not consensus |

---

## Verification boundary

Formal verification remains a property of the Coq **model** plus finite differential gates. Minimal scripts make that obligation finite. They do not prove C++ refinement.

```
equivalence_claimed = false
```

---

## Website / whitepaper

Updated to **Minimal Script Architecture** (not “No Script”), with the required two-sentence statement, is/is-not identity, and links to the registry and security boundary.

---

## Validation

| Check | Result |
|-------|--------|
| `npm run build` (`web/rigorchain-site`) | **PASS** (`tsc --noEmit && vite build`) |
| Consensus serialization / script bytes | Unchanged |
| `core/src/kernel/`, `core/src/consensus/`, `pow.cpp`, `validation.cpp` logic, miner | Unchanged |

C++ unit tests were not re-run against a rebuilt `test_bitcoin` because no consensus object code changed. Comment-only edits do not alter `VerifyWitnessProgram`.

Follow-up leftover eval and safe cleanups: [RIGOR_MINIMAL_SCRIPT_DEBT_CLEANUP_EVAL.md](./RIGOR_MINIMAL_SCRIPT_DEBT_CLEANUP_EVAL.md).

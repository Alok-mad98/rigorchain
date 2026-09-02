# RIGOR Script / Opcode Security Audit (P0-078)

**Task:** `TASK-RIGOR-SCRIPT-OPCODE-SECURITY-AUDIT-P0-078`  
**Date:** 2026-08-31  
**Nature:** Transaction Script / opcode boundary security audit. Isolation of
spend authority + fail-closed malformed input. This is not a cryptographic
proof of Ed25519, and it does not enlarge the script surface.

This TASK does not modify Consensus Kernel, Transaction Kernel,
`VerifyRigorPredicate` semantics, Sighash rules, Ed25519 rules, Asset
Classification, or Ledger Transition. It does not add an opcode, a Script VM,
a compatibility layer, or a Bitcoin Script interpreter.

```
scriptPubKey / scriptSig / witness / signature / pubkey / sighash
        ↓
TransactionKernel
        ↓
VerifyRigorPredicate          (unique spend authority)
        ↓
Ed25519  or  REJECT
```

Forbidden entries: `EvalScript`, `VerifyScript`, CScript VM, opcode
interpreter, stack machine.

```
SCRIPT_OPCODE_SECURITY_STATUS = CLOSED
SCRIPT_VM_PRESENT             = NO
OPCODE_SURFACE_COUNT          = 0
DUPLICATE_VALIDATION_PATH     = NONE
FUZZ_STATUS                   = PASS
CONSENSUS_MODIFIED            = NO

SCRIPT_AUTHORITY_COUNT        = 1
SCRIPT_VM_RESIDUE             = NONE
LEGACY_SCRIPT_RESIDUE_FOUND   (named reserved-inert bytes; not executable)

FINAL_VERDICT:
RIGOR_SCRIPT_OPCODE_SECURITY_AUDIT_CLOSED
```

Reproduce: `bash tools/run-script-opcode-security-audit.sh`

---

## Proof scope

### PASS

| Claim | Meaning |
|-------|---------|
| single predicate authority | Production spend is `VerifyRigorPredicate` only (`predicate_verify.cpp`), implemented as `MakeKernelInput` + `VerifyRigorPredicateBoundary`. `validation.cpp` does not call the boundary. `VerifyRigorRegistryPredicate` forwards. Lint: `tools/lint-script-authority.py`. |
| no script VM execution | `EvalScript`, `VerifyScript`, `ExecuteOpcode`, `ConditionStack`, `popstack`, and opcode dispatch are absent from the production spend path. `lint-no-script-vm.py` fails the build if they reappear. |
| no opcode bypass | Named Bitcoin opcodes (`OP_CHECKSIG`, `OP_CHECKMULTISIG`, `OP_CHECKLOCKTIMEVERIFY`, `OP_CHECKSEQUENCEVERIFY`, `OP_DUP`, `OP_HASH160`, `OP_EQUALVERIFY`) are not identifiers on the production path. Live executable opcode count is 0. |
| malformed input rejection | Empty script, random bytes, unknown opcode, truncated opcode, oversized witness, invalid stack, invalid signature, wrong pubkey, and wrong sighash are `REJECT`. Tests: `core/src/test/test_script_opcode_security.cpp`. |
| deterministic failure | Reject does not crash, throw, or mutate the input. Fuzz smoke: `fuzz/script_opcode_boundary/`. |

### NOT CLAIM

| Out of scope | Why |
|--------------|-----|
| cryptographic primitive proof | Ed25519 correctness is assumed from the provider. This audit does not prove the signature scheme. |
| compiler correctness | C++ compilation and inlining are outside the model. |
| hardware security | Side channels, fault injection, and host compromise are out of scope. |

This note is a boundary + rejection audit, not a Coq refinement of
`predicate_verify.cpp`.

---

## 1. Script authority inventory

Manifest: `formal_verification/script_opcode_audit_manifest.json`

| Role | Symbol |
|------|--------|
| Unique spend authority | `VerifyRigorPredicate` |
| Allowed entries | `scriptPubKey`, `scriptSig`, `witness`, `signature`, `pubkey`, `sighash` |
| Forbidden entries | `EvalScript`, `VerifyScript`, CScript VM, opcode interpreter, stack machine |

`SCRIPT_AUTHORITY_COUNT = 1`

Production call: `CScriptCheck::operator()` in `validation.cpp` calls
`VerifyRigorPredicate`. Wallet `ProduceSignature` / feebumper size checks
request that same function; they do not define a second body.

---

## 2. Opcode surface

Live VM-executed opcodes: **0**.

Live template markers (classified, not executed):

| Byte | Name | Spendable |
|------|------|-----------|
| `0x6a` | `RIGOR_MARKER_RETURN` | no |
| `0xbb` | `RIGOR_MARKER_RES_MINT` | yes (mint predicate) |
| `0xbd` | `RIGOR_MARKER_CONVERGE` | no |

`0xbc` is burned. `0xbe`–`0xff` are invalid (fail closed).

**LEGACY_SCRIPT_RESIDUE_FOUND** (inert; `SCRIPT_VM_RESIDUE = NONE`):

- `RIGOR_RESERVED_LEGACY_*` names in `rigor_bytes.h` (parser / `GetOpName` / RPC asm)
- `IsOpSuccess` BIP342 name table
- unused `CastToBool`
- leftover constants `MAX_OPS_PER_SCRIPT`, `MAX_STACK_SIZE`, `MAX_PUBKEYS_PER_MULTISIG`
- `CountScriptOpsLegacyTestOnly` (tests/fuzz only)
- dead `TxoutType` arms (`PUBKEY`, `PUBKEYHASH`, `SCRIPTHASH`, `MULTISIG`, …)
- leftover `SCRIPT_ERR_*` names

None of these is an execution path. P2PKH / P2WSH / P2SH / lone CHECKSIG
bytes are `REJECT`.

---

## 3. No second validation path

`validation.cpp`, `primitives/transaction.cpp`, `wallet/`, and `rpc/` must
not implement signature validation, asset classification, or script parsing
as a spend decision. Spend judgment is:

```
TransactionKernel
        ↓
VerifyRigorPredicate
```

Lint detects a duplicate predicate definition, a duplicate `EvalScript` /
`VerifyScript` body, and opcode dispatch of `RIGOR_RESERVED_LEGACY_CHECKSIG`.

---

## 4. Malformed script attack

`core/src/test/test_script_opcode_security.cpp` covers:

| Input | Result |
|-------|--------|
| empty script | REJECT |
| random bytes | REJECT |
| unknown opcode | REJECT |
| truncated opcode | REJECT |
| oversized witness | REJECT |
| invalid stack data | REJECT |
| invalid signature | REJECT |
| wrong pubkey | REJECT |
| wrong sighash | REJECT |

Forbidden: crash, undefined behavior, partial state mutation.

---

## 5. Opcode fuzz

`fuzz/script_opcode_boundary/` feeds random `scriptPubKey`, `scriptSig`,
witness, and opcode bytes into `VerifyRigorPredicateBoundary`.

```
NO CRASH
NO ACCEPT_INVALID
NO CONSENSUS_BYPASS
SCRIPT_FUZZ_STATUS = PASS
```

An accept with `crypto_accept = false` is `CONSENSUS_BYPASS`. An accept that
is not a live mint or witness v0/20 / v1/32 template is `ACCEPT_INVALID`.

---

## 6. Sighash boundary

The script layer cannot change the canonical sighash layout. Mutating
transaction version, input index, output hash, amount, spent script (asset
id), chain id, or witness-bound fields changes the preimage. With
`crypto_accept = false` the spend is `INVALID`. There is no hashtype byte.

---

## Gates

```
bash tools/run-script-opcode-security-audit.sh
```

Runs: authority lint, script opcode lint (`lint-no-script-vm.py`),
transaction kernel refinement lint, malformed-input test, fuzz smoke,
differential gate.

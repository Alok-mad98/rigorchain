# RIGOR Wallet Security Baseline (P0-077)

**Task:** `TASK-RIGOR-WALLET-SECURITY-BASELINE-P0-077`  
**Date:** 2026-08-31  
**Nature:** Mainnet wallet security baseline. Isolation model + fail-closed
request firewall. This is not a formal proof of the whole wallet, and wallet
is not a consensus authority.

This TASK does not modify consensus rules, Consensus Kernel, Transaction
Kernel, Ledger Kernel, Genesis parameters, issuance, RES schedule, draw, or
supply formula. It does not add a second classifier, a second signature
verifier, or a wallet consensus authority.

```
Wallet RPC / UI request
        ↓
WalletSecurityBoundary::Evaluate     (request firewall)
        ↓
WalletIntent
        ↓
TransactionBuilder                   (canonical wallet encoding)
        ↓
Canonical Transaction
        ↓
WalletSigningBoundary                (bind digest; no sighash rewrite)
        ↓
Transaction Kernel                   (VerifyRigorPredicate)
```

```
WALLET_SECURITY_STATUS       = CLOSED
KEY_SECURITY_STATUS          = CLOSED
TX_BUILD_STATUS              = CLOSED
SIGNING_BOUNDARY_STATUS      = CLOSED
FUZZ_STATUS                  = PASS

PRIVATE_KEY_EXPOSURE         = NONE
INVALID_TX_GENERATION        = NONE
CONSENSUS_BYPASS             = NONE
CRASH_FOUND                  = NO
CONSENSUS_MODIFIED           = NO

FINAL_VERDICT:
WALLET_SECURITY_BASELINE_CLOSED
```

Reproduce: `bash tools/run_wallet_security_gate.sh`

---

## Proof scope

### PASS

| Claim | Meaning |
|-------|---------|
| wallet boundary isolation | `core/src/wallet/security/` cannot call issuance, RES reward, draw, supply, `LedgerApply`, `ClassifyRigorTxout`, or `VerifyRigorPredicate`. It cannot mutate UTXO / ledger. Lint: `tools/lint-wallet-security.py`. |
| signing boundary | Wallet cannot change sighash layout, witness format, or signature flags. Wrong key / wrong digest / mutated tx / replay are rejected. Production Ed25519 remains `ed25519::SignDetached` / `CKey::Sign`. |
| transaction construction safety | Negative amount, overflow amount, unknown asset, invalid destination, duplicate input, immature coin, and insufficient balance are `REJECT`. An accepted intent still sets `must_enter_transaction_kernel`. |
| key exposure prevention | Secrets are wiped after `Sign` / import. Results are `secret_isolated`. Production wallet logs must not `printf` / `LogPrint` private-key material. Generic RPC results do not echo secrets. |
| invalid request rejection | RPC malformed / missing parameter / oversized input / invalid address / invalid amount evaluate to `REJECT`. RPC generates a request only; it does not call consensus authority. |

### NOT CLAIM

| Out of scope | Why |
|--------------|-----|
| hardware wallet security | External signers / secure elements are not in this baseline. |
| OS compromise resistance | A compromised host can still read process memory. |
| user password strength | Passphrase quality is an operator choice. |
| physical device security | Theft, evil maid, and cold-boot are out of scope. |

This note is an isolation + rejection baseline, not a Coq refinement of `wallet/spend.cpp`.

---

## 1. Wallet security boundary

Directory: `core/src/wallet/security/`

| Type | Role |
|------|------|
| `WalletTransactionRequest` / `WalletIntent` | Request to construct a transfer. Not `ConsensusKernelInput`. |
| `WalletSecurityBoundary` | Fail-closed evaluator. Never throws. Never calls Kernel formulas. Never logs secrets. |
| `TransactionBuilder` | Wallet-layer canonical encoding. Not consensus `CTransaction` serialization. |
| `WalletSigningBoundary` | Bind a provided canonical digest to a signer. Does not implement Ed25519 or sighash. |
| `WalletSecurityResult` | Exclusive output: `ACCEPT_TO_BUILDER` / `REJECT`. |

Forbidden at this layer:

- Wallet directly modifying UTXO
- Wallet directly modifying ledger state
- Wallet directly deciding spend validity
- Wallet directly deciding asset classification
- A third decision (wallet-accept-as-consensus)

Production hook: `CreateTransaction` in `wallet/spend.cpp` runs
`WalletSecurityBoundary::Evaluate` on the recipient vector before coin
selection. `REJECT` aborts construction. `ACCEPT_TO_BUILDER` continues into
the existing `CreateTransactionInternal` path. Final spend validity remains
`VerifyRigorPredicate`.

`libbitcoinkernel` does not contain `wallet/security`. Wallet is not a
consensus library.

---

## 2. Private key security audit

| Surface | Rule |
|---------|------|
| memory lifetime | `WalletSigningBoundary::Sign` wipes `request.secret` before return. Import wipes caller material after `EvaluateImport`. |
| serialization | Boundary results never copy secret bytes into `canonical_tx` or `reason`. |
| backup / export | Explicit export RPCs (`dumporuprivkey`, `dumpresprivkey`, `listdescriptors true`) remain user-initiated export. They are not generic RPC results. This baseline does not add a new dump path. |
| import | `EvaluateImport` accepts a request only. It does not log material. |

Prohibited:

- private key plaintext logs
- debug output leak
- generic RPC returning private keys
- crash-dump of secrets from this boundary (secrets are wiped; `reason` is a static token)

Secret-handling lint: `tools/lint-wallet-security.py` scans `wallet/` (excluding tests) for `printf` / `LogPrint` of `EncodeSecret` / `private_key` / `log(secret)`.

---

## 3. Transaction construction boundary

Wallet construction must pass:

```
WalletIntent → TransactionBuilder → Canonical Transaction → VerifyRigorPredicate
```

Covered:

| Case | Decision |
|------|----------|
| ordinary ORU transfer | `ACCEPT_TO_BUILDER` (`must_enter_transaction_kernel`) |
| RES transfer (mature) | `ACCEPT_TO_BUILDER` (`must_enter_transaction_kernel`) |
| maturity not reached | `REJECT` `immature` |
| maturity reached | `ACCEPT_TO_BUILDER` (Kernel still decides spend) |
| invalid / unknown asset | `REJECT` `unknown-asset` |
| insufficient balance | `REJECT` `insufficient-balance` |
| overflow amount | `REJECT` `overflow-amount` |
| negative amount | `REJECT` `negative-amount` |
| invalid destination | `REJECT` `invalid-destination` |
| duplicate input | `REJECT` `duplicate-input` |

`TransactionBuilder::Build` refuses to emit a canonical encoding when Evaluate
rejects. An accept encoding is a wallet-layer byte layout, not a second
consensus serializer.

---

## 4. Signing boundary

```
Transaction → Canonical sighash (existing production API) → Ed25519
```

This module does **not** modify:

- sighash layout
- witness format
- signature flags
- Ed25519 verification rules

`WalletSigningBoundary` requires `digest_layout_locked`, `witness_format_locked`,
and `signature_flags_locked`. Unlocking any of them is `REJECT`.

Covered:

| Case | Decision |
|------|----------|
| wrong key | `REJECT` `wrong-key` |
| wrong digest | `REJECT` `wrong-digest` |
| modified tx after signing | `REJECT` (`wrong-digest` / `modified-tx-after-signing`) |
| signature replay on another tx | `REJECT` |

The isolation `signature_tag` is a binding of `key_id` + digest used by the
standalone audit. Production witness signatures remain `CKey::Sign` /
`ed25519::SignDetached`. This is not a second Ed25519.

---

## 5. Asset safety

Wallet-visible tags:

| Tag | Sendable |
|-----|----------|
| ORU | yes (registered) |
| RES | yes (registered) |
| UNKNOWN | no |

Wallet cannot:

- send a custom unknown asset
- bypass `ClassifyRigorTxout` (the unique matcher stays in Transaction Kernel)
- forge local asset metadata that consensus would accept

All final classification and spend judgments enter the Transaction Kernel.

---

## 6. RPC / API security

`WalletSecurityBoundary::EvaluateRpc` covers:

- malformed request
- missing parameter
- oversized input
- invalid address
- invalid amount

RPC output is a request (`ACCEPT_TO_BUILDER` / `SIGNING_API` / `IMPORT` /
`VALIDATION_API`). It cannot call `GetBlockSubsidy`, `GetResMintReward`,
`ResolveConvergenceDraw`, `RigorSupplyDelta`, or `LedgerApply`.

---

## 7. Wallet fuzz

`fuzz/wallet_security/`

| Entry | Target |
|-------|--------|
| transaction builder | `Evaluate` + `TransactionBuilder::Build` |
| address parser | `ParseAddress` |
| amount parser | `ParseAmount` |
| signing request | `WalletSigningBoundary::Sign` / `VerifyBinding` |
| restore / import | `EvaluateImport` |

Random bytes: no crash, no key leak (`reason` is never secret material), no
invalid transaction output (reject paths emit empty `canonical_tx`; mutation
after sign is not `ACCEPT`).

Smoke: `bash fuzz/wallet_security/run_smoke.sh`

---

## 8. Tests and gates

| Artifact | Role |
|----------|------|
| `core/src/test/test_wallet_security_boundary.cpp` | key isolation, construction, signing mutation, invalid amount/asset, RPC malformed |
| `tools/lint-wallet-security.py` | isolation + secret-handling + exclusive decision set |
| `tools/run_wallet_security_gate.sh` | lint + compile + tests + fuzz smoke + differential gate |

Existing gates that must still pass (unchanged consensus):

- `formal_verification/scripts/run_differential_gate.sh`
- authority lint (`tools/lint-consensus-authority.py`, `lint-consensus-authority-security.py`)
- consensus kernel freeze (`tools/lint-consensus-kernel-freeze.py`)

The P077 gate re-runs the authority and freeze lints so `CONSENSUS_MODIFIED = NO`
is checked on every wallet gate run.

---

## Unchanged (forbidden)

Consensus Kernel, Transaction Kernel, Ledger Kernel, Genesis parameters,
issuance / RES / draw / supply / classify / spend formula bodies, sighash
rules, and Ed25519 rules were **not** modified by this TASK. Wallet did not
gain consensus logic.

# RIGOR Soft-Fork / Native Upgrade Model (P0-026)

**Task:** P0-026  
**Status:** Design freeze recommendation. No consensus change in this task.  
**Does not inherit:** Bitcoin NOP replacement, BIP342 `OP_SUCCESS`, unknown-witness anyone-can-spend.

Related: [namespace audit](./RIGOR_NATIVE_OPCODE_NAMESPACE_AUDIT_P0_026.md), [predicate registry](./RIGOR_PREDICATE_REGISTRY_DESIGN_P0_026.md).

---

## 0. Decision

| Upgrade | Mechanism | Fork class |
|---------|-----------|------------|
| New signature algorithm (Ed25519 → Dilithium, etc.) | New **predicate version** (new ScriptPubKey template + verifier) | Coordinated **hard fork** (fail-closed today) |
| New asset type | New `AssetType` + dedicated predicate | Coordinated **hard fork** |
| New covenant / privacy primitive | New template + dedicated checker | Coordinated **hard fork** |
| Tightening an *existing* predicate (reject something currently accepted) | Same template, stricter checker | **Soft fork** (new nodes reject more) |
| NOP / `OP_NEW_SIGCHECK` / `OP_NEW_ASSET` | — | **Forbidden** |
| Unknown witness anyone-can-spend | — | **Forbidden** |

Soft-fork *capability* is preserved only as **restrictive changes to already-accepted templates**. New spending programs are **version activation / hard fork**, not opcode activation.

---

## 1. Current unknown-version behavior (measured)

Witness programs are BIP141-shaped: first byte `OP_0` or `OP_1`–`OP_16`, then a 2–40 byte push.

| Case | Create (`CheckTransaction`) | Spend (`VerifyScript`) |
|------|-----------------------------|------------------------|
| v0 / 20 | ORU, accepted | Ed25519 P2WPKH |
| v1 / 32 native RES | RES, accepted | pk == program, Ed25519 |
| v0 / 32 (P2WSH) | `ASSET_INVALID` | would be `WRONG_LENGTH` if it existed |
| v1 CVG2/RES2 | `ASSET_INVALID` | fail closed |
| v2–v16 well-formed | `ASSET_INVALID` / `bad-rigor-script-type` | `SCRIPT_ERR_RIGOR_SCRIPT_TYPE` if version ∉ {0,1} |
| Missing `SCRIPT_VERIFY_WITNESS` | n/a | `RIGOR_SCRIPT_TYPE`, **never** anyone-can-spend (F-03) |

Bitcoin: unknown version = anyone-can-spend at consensus, discouraged at policy → later soft fork.

RIGOR: unknown version = **fail closed at create and spend**. The leftover error name `SCRIPT_ERR_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM` is wording only (P0-025).

**Therefore: adding witness v2 as a live asset requires a release whose classifier accepts it. Old nodes reject those outputs. That is a hard fork, not a Bitcoin-style soft fork.**

---

## 2. Forbidden upgrade patterns

### 2.1 `OP_NEW_SIGCHECK` / `OP_RIGOR_X`

Would require an interpreter that treats a new byte as a computation. RIGOR has no interpreter. Restoring one is a consensus-surface explosion and is out of scope forever.

### 2.2 NOP reservation

`OP_NOP1`–`OP_NOP10` exist only as names. A scriptPubKey containing them is not a live template → INVALID. There is no “NOP succeeds, later becomes CLTV” path.

Rename them `RIGOR_RESERVED_LEGACY_NOP*` and leave them inert.

### 2.3 Anyone-can-spend vacant witness versions

Would restore Bitcoin’s unsafe pre-activation window. Forbidden even though it would make later signature requirements a true soft fork.

---

## 3. Required pattern: predicate version

```
ScriptPubKey bytes
      |
Classifier          (closed match)
      |
Predicate ID        (registry)
      |
Dedicated verifier  (Ed25519 today; Dilithium / other tomorrow)
      |
State transition
```

### 3.1 New signature algorithm (Ed25519 → Dilithium)

**Do not** add `OP_DILITHIUM`.

Do:

1. Specify `PredResV2` (example): `version_nibble ‖ 32-or-N-byte program` with a **new** first-byte or tagged program that v0.1 does not accept.  
2. Add `AssetType` or keep RES with a new lock kind (registry, not a second classifier).  
3. `VerifyScript` branch: structural checks + Dilithium verify. New `SigVersion` / domain tag.  
4. Coq: new `SpendPath` constructor; `PredFuture` becomes a v1.x registry entry.  
5. Wallet: construct only via registry. No CHECKSIG-shaped scripts.

Old nodes: `ASSET_INVALID` on create. **Hard fork** (coordinated release / version bit that gates *when miners/nodes require the new template*, not when a NOP starts executing).

Optional pre-mainnet (only if a scheduled slot is wanted **before** genesis):

- Accept create of a tagged `v2` program as **unspendable** (`ASSET_RESERVED` / burn).  
- Later allow spends under the new verifier.

Making an unspendable output spendable is still a **hard fork** (old nodes reject the spend). It is *scheduled*, not stealth. Prefer not to pre-create unspendable asset slots unless a concrete v2 spec exists before genesis.

### 3.2 New asset type

**Do not** add `OP_NEW_ASSET`.

Do:

```
AssetType = RIGOR_ASSET_V2   // or ASSET_FOO
scriptPubKey matches Template_V2
Classifier → AssetType
Predicate_V2 → verifier
Conservation rules per pool (CheckTxInputs)
```

Same fork class as 3.1.

### 3.3 Covenant / privacy

Same as CONVERGE: a **dedicated template** and a dedicated checker (possibly unspendable). Not `OP_CHECKSIGADD`, not tapscript.

---

## 4. What *is* a RIGOR soft fork

New nodes enforce a **stricter** subset of currently valid behavior on **existing** templates. Old nodes still accept the new blocks.

Examples that would be soft forks (not proposed here, only classified):

| Change | Why soft |
|--------|----------|
| Reject off-curve v1 programs at **create** (today consensus allows the bytes; spend already fails crypto) | New nodes reject more creates |
| Tighten mint canonical encoding further | If something extra is currently accepted |
| Narrow `IsPushOnly` so `OP_RETURN` + `0x50` is no longer `NULL_DATA` | New nodes reject more datacarrier shapes |

These do **not** add Dilithium or a new asset. They only shrink the accepted set of current templates.

Transaction `nLockTime` / `nSequence` remain independent of opcode CLTV/CSV.

---

## 5. Version activation (recommended language)

Do not say “soft fork” for shipping `PredResV2`.

Say:

```
VERSION ACTIVATION = coordinated hard fork
  - new classifier branch
  - new predicate id
  - new verifier
  - old nodes fail closed on the new template
```

Deployment signaling (BIP9-like bit, or a simple height) may coordinate *when* the network switches. Signaling does not make unknown bytes succeed on old nodes.

`GetBlockScriptFlags` today is constant (no delayed script-flag activation). Keep it that way unless a future tightening soft fork needs a flag — and even then the flag must only **restrict**, never open a VM.

---

## 6. Vacant bytes 0xbe–0xfe

They are **not** NOP. They are not in the live enum. Using one as a new marker terminator is allowed **only** together with a new predicate spec (hard fork). Using one as an executable opcode is forbidden.

Witness v2–v16 first bytes (`RIGOR_PUSH_2`…`PUSH_16`) already exist as **push/version nibbles**. They must not be “opened” as anyone-can-spend. A future v2 **program** is a new classifier arm, not “version 2 is now SUCCESS.”

---

## 7. Formal

`RigorUpgradeNamespace.v`:

- `PredFuture n` ∉ v0.1 registry  
- `activate_future` does not set `opcode_vm_in_consensus`  
- `OP_CHECKSIG` remains `AssetInvalid` as a scriptPubKey  

`equivalence_claimed` remains false versus C++ until a rename lands.

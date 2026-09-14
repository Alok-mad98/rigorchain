# Chapter 3 — Native UTXO: How the Two Identities Exist on the Ledger

Chapter 2 wrote the three vertices as frozen rules. This chapter states
how those two identities exist as ledger objects. The claim is still
architectural: the protocol fixes **objects and classification**, not an
off-chain registry and not a colour painted on a single UTXO.

If ORU and RES were only annotations on one output, Chapter 1’s split
would not have happened. Annotations can be ignored, rewritten by an
indexer, or re-merged by another contract. The split must be the same
classification every full node reads from the same bytes.

```
TxOut = (nValue, scriptPubKey)
shape of scriptPubKey  →  asset identity
consensus predicates   →  whether that output was born legally, and who may spend it
```

---

## 3.1 Why the identities must be native

Overlays are cheaper. Colouring an existing UTXO, keeping balances in a
side table, or issuing a “reserve” from a contract does not require a
consensus change. That is also why dual-asset designs are rare: one
ticker, one listing, one UTXO type is easier to explain and to operate.

The cost of that ease is Chapter 1. If the two identities are not in
consensus, the miner budget and the scarce stock can be tied back to the
same object off the ledger. A registry can credit RES without anyone
destroying ORU. An inscription can stick a store-of-value story back
onto the medium of exchange. A contract can make conversion two-way.

The protocol therefore refuses three shortcuts:

| Shortcut | What it looks like | What it breaks |
|----------|--------------------|----------------|
| Registry | A global `(asset, balance)` or issuance list | Nodes must trust a table instead of replaying bytes |
| Overlay | Colour, inscription, rune, wrapped asset | The UTXO is still one unit; the split lives in an indexer |
| General VM | Arbitrary contracts that redefine “ORU / RES” | Identity becomes writable; the verification surface is no longer closed |

RIGOR accepts a harder public story — two native identities, one one-way
transform — so medium of exchange and store of value are not the same
consensus object.

---

## 3.2 The ledger object is still a UTXO

The node heritage is Bitcoin. An unspent output remains:

```
TxOut = (nValue, scriptPubKey)
```

There is no account model as consensus truth. There is no parallel
`asset` field on `CTxOut`. There is no `(asset, value, script)` triple.
Ownership changes only by spending previous outputs and creating new
ones.

```
Previous UTXO
        ↓
Transaction validation (classify, sign, isolate, mint predicate)
        ↓
New UTXO owner
```

No hidden balances. No global mutable account state. A full node does
not consult an external registry to know what an output is or who may
spend it.

The same accepted block bytes must produce the same validity decision.

---

## 3.3 Script classifies identity; consensus decides legitimacy

Asset identity is not external metadata. `scriptPubKey` is parsed as a
witness program. Witness version and program shape classify the output
as ORU, RES, or (for zero-value `OP_RETURN`) none. The function is
`AssetTypeFromScriptPubKey`. Classification is consensus-visible.

```
script shape
        ↓
AssetTypeFromScriptPubKey
        ↓
ORU  |  RES  |  NONE  |  INVALID
```

The earlier shorthand “the script is the asset” correctly forbade a
parallel asset field. The precise rule is: **script defines identity;
consensus defines legitimacy.** Parsing Witness v1 is not the same as
a legal RES mint. Birth of RES still requires an `OP_CONVERGE` destroy
in the same transaction and

```
R > 0  →  R · (2²⁵ + h)² ≤ B << 40
```

Script is the compliance / classification layer. It states the
conditions under which an output may exist and later be spent.
Ownership and transfer are enforced by UTXO consumption, predicate
verification, and signatures. Script alone does not authorize asset
creation.

This avoids hidden asset metadata, dependence on an external indexer,
and two machines answering “what is this?” differently.

---

## 3.4 A closed template list

Production has **no Script VM**. There is no `EvalScript` and no
`interpreter.cpp`. Bytes are classified. Spends go through
`VerifyRigorPredicate`. Live templates are a closed list:

| Script shape | Classification | Asset | Spendable | What it is |
|--------------|----------------|-------|-----------|------------|
| Witness v0, 20-byte program | `WITNESS_V0_KEYHASH` | ORU | Yes | Everyday hold and transfer |
| Witness v1, 32-byte program | `WITNESS_V1_RES` | RES | Yes | The scarce stock |
| `<20-byte HASH160> 0xbd` | `CONVERGE` | ORU | **No** (not in the UTXO set) | Destroy container; mint input |
| Zero-value `0x6a` (push-only) | `NULL_DATA` | NONE | No | Data carrier |
| Anything else | `NONSTANDARD` | INVALID | — | Consensus-invalid |

Coinbase `<maturity> <HASH160> 0xbb` is **not** the RFC-020 mint path.
RES is not born in coinbase.

Consensus rejects as new monetary outputs: P2PKH, P2SH, P2WSH, bare
multisig, unknown witness versions, Taproot control-block spends.
Spend signatures
are Ed25519 only. secp256k1 / ECDSA CHECKSIG hard-fails.

**Witness v1 is not Bitcoin Taproot.** It reuses the BIP141 version byte
as a native RES container. No control blocks, no annex, no tapscript.
Sharing a version number is not sharing a VM.

The list is closed so every honest node can carry the verification
burden. Every additional execution capability creates additional
consensus responsibility.

---

## 3.5 Isolation of the two pools

Conservation is enforced **per asset pool**. That is not a wallet
convention.

```
ORU inputs  cannot  fund RES outputs
RES inputs  cannot  fund ORU outputs
RES → ORU conversion    illegal
```

The only path that is not “ORU cannot become RES” is not a transfer. It
is Chapter 2’s one-way transform: the same body transaction destroys ORU
and, when the predicate holds, mints new Witness v1 RES. Destroyed ORU
leaves circulation. It is not renamed 1:1. The new RES is a new UTXO,
not a colour on the old output.

The `OP_CONVERGE` output itself is classified ORU-domain: unspendable,
and not inserted into the UTXO set after in-block validation. It
declares destroy amount `B` for the mint arithmetic. At most one
positive-value Convergence output per transaction, and only from ORU
inputs.

---

## 3.6 Not a registry, not an overlay

| | Registry / contract account | Colour / inscription / rune | RIGOR |
|--|----------------------------|-----------------------------|-------|
| Where identity lives | A global table or contract store | An indexer’s note on one UTXO | The shape of `scriptPubKey` |
| How a full node knows | Read the table or run the contract | Depend on external parsing | Classify the same bytes |
| Can RES “appear” with no destroy? | Yes (the table is written) | Yes (only the note changes) | No |
| Are the two identities one consensus object? | Often | Yes (the base unit is still one) | No: v0 and v1 are two pools |
| Verification surface | Grows with contracts | Grows with indexer rules | Closed templates + an integer predicate |

ORU and RES are not colored coins, Bitcoin inscriptions, Ordinals,
Runes, smart-contract units, wrapped assets, or an external accounting
layer. They are two native identities inside the RIGOR UTXO system.
Neither is a secondary representation of the other. There is no
parent–child asset relation. They sit at the same protocol layer,
separated by classification and isolation, coupled one-way by
Convergence.

> ORU and RES are two native asset identities. The split happens in
> consensus, not in the explorer.

---

## 3.7 Spends: predicates after classification

A spend does not interpret arbitrary bytecode. The node confirms which
pool the input belongs to, then the predicate for that pool:

| Identity | Spend model |
|----------|-------------|
| ORU (Witness v0, 20-byte) | Two-item witness; program is `HASH160(pubkey)`; CHECKSIG is Ed25519 |
| RES (Witness v1, 32-byte) | Two-item witness `<sig, 32-byte pk>`; the pubkey must equal the program |
| Convergence | Unspendable; not inserted into the UTXO set |
| `OP_RETURN` | Unspendable; value must be 0 |

The signing domain is separated from Bitcoin’s default path. There is
no `SigVersion::BASE`. There is no P2PKH template as a consensus spend
path.

The kernel model proves classifier range, partition, and spendability
by class (abstract `sig_ok`). That is not a VM and not an Ed25519
unforgeability proof.

---

## 3.8 Narrow capability is a prerequisite for verification

This chapter does not compete with programmable platforms. Monetary
integrity does not require unlimited programmability. More paths create
more failures. A smaller execution surface makes stronger verification
possible.

```
Less capability
        ↓
Less complexity
        ↓
Smaller attack surface
        ↓
More feasible mathematical verification
```

The goal is not maximum programmability. The goal is maximum
verifiability. Script exists so ownership and asset identity can be
checked. Script does not exist so the ledger can become a general-purpose
computer.

The finer template list and security boundary are the standalone
engineering chapter
[`RIGOR_MINIMAL_SCRIPT_ARCHITECTURE.md`](./RIGOR_MINIMAL_SCRIPT_ARCHITECTURE.md).
That file is an implementation audit, not a second economic claim.

---

## 3.9 What this chapter does not claim

- It does not claim C++ equivalence to the kernel model.
- It does not claim Ed25519 unforgeability inside the kernel.
- It does not write “minimal script” as “no script.” Bytes are still
  classified; spends still go through predicates.
- It does not write Witness v1 as Bitcoin Taproot.
- It does not write Convergence as a smart contract. It is a destroy
  container; mint arithmetic is Chapter 4.

**Identity lives in the script. Legitimacy lives in consensus.**
Chapter 4 states how Convergence is verified as that one-way bridge —
the destroy is visible, the mint is an integer predicate in the same
transaction, not a draw and not a 1:1 counter.

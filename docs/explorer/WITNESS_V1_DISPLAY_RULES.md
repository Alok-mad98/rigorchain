# Witness V1 Display Rules (Explorer)

**Task:** `TASK-EXPLORER-BOOTSTRAP-001`  
**Baseline:** `mainnet-freeze-rfc014-2026-05-25`  
**Sources:** RFC-008, RFC008_ERRATUM_RESERVED_ZERO.md, TASK-WITNESS-EXT-001 freeze, `interpreter.cpp`.

---

## 1. Terminology lock

| Use in UI/docs | Do NOT use |
|----------------|------------|
| Witness **v1** | Taproot, BIP341, x-only pubkey path |
| **OP_CONVERGE** | Convergence burn / draw-entry (not a witness v1 program) |
| **RES** program | “P2TR”, “Taproot address” |
| **PCORB** mint proof | “Tapscript”, “control block”, “script path” (Bitcoin sense) |
| **ORU** witness v0 | — |

Historical RFC-014 / retired design: Witness v1 prefix ASCII `CVG2` was an earlier convergence encoding. It is **not** a live consensus script. Do not label live Convergence as “CVG2” or “Witness v1 convergence.” `OP_CONVERGE` is the only live Convergence classification.

Development databases created before script alignment may contain retired
classifications. Public network launch will rebuild the explorer view and
indexed observations from genesis using current consensus classification
rules. No production
CVG2 migration is required. Historical CVG2 conversion is not a consensus
concern.

Policy: [`../archive/migration/MIGRATION_NOTES.md`](../archive/migration/MIGRATION_NOTES.md).

“Script path” is retired vocabulary for RES: SECURITY-001-A / A-7 left a witness v1 RES output with
exactly one spending path, the Ed25519 key path. The PCORB proof is not a RES spend path at all — it
rides on the **ORU** funding input of a body mint transaction.

RIGOR witness v1 is **not** Bitcoin Taproot. Display MUST NOT imply BIP341 semantics.

---

## 2. Program classes

| Class | Prefix / rule | Asset domain | Display label |
|-------|---------------|--------------|---------------|
| **OP_CONVERGE** | 22-byte lock: `OP_PUSHBYTES_20 \|\| HASH160 \|\| OP_CONVERGE (0xbd)` | ORU | Script: `OP_CONVERGE`. Asset: ORU. State: Unspendable burn output / draw participation |
| **RES2** | Witness v1, wire prefix `RES2` | RES | “RES (RES2)” |
| **Native RES** | Witness v1, 32-byte Ed25519 pubkey | RES | “RES (native)” |
| **Retired CVG2 v1** | Historical RFC-014 / retired design: first 4 bytes `CVG2` | NONE (not a third asset) | “Historical RFC-014 / retired Witness v1 CVG2 (not OP_CONVERGE)” |
| **Unknown v1** | Not matching above | Invalid at consensus | “Invalid v1 program” |

Addresses: testnet Bech32 HRP **`trg`**; v1 programs use **`trg1p`** style per descriptor policy — not `bc1p`.

---

## 3. PCORB witness display (6-item stack)

Display order: **index 0 at top** (human-readable), matching RPC decode bottom-to-top convention — document which convention the UI uses and keep it consistent.

| Index | Size | Field name | Display |
|-------|------|------------|---------|
| 0 | 32 | `convergence_txid` | Hex + link to convergence tx page |
| 1 | 8 | `convergence_amount_sat` | LE64 → ORU amount |
| 2 | 32 | `bind_hash` | Hex |
| 3 | 8 | `height` | LE64 integer |
| 4 | 4 | `target_prev` | LE32 compact — label **“replay bind (parent nBits)”** |
| 5 | 32 | `convergence_root` | Hex |

**Layout change (SECURITY-001-A / A-7).** The stack was 11 items. The UI MUST NOT render or expect a
σ signature (former item 6), a pubkey (7), a leaf script (8), or reserved-zero slots (9, 10) — they
no longer exist on the wire. Any push beyond index 5 is **not** part of the proof; for a
devtest P2WSH-funded mint the trailing push is the redeem script (`OP_DROP×6 + OP_TRUE`, hex
`75757575757551`), giving 7 witness items in total. Render such a trailing push as
“funding redeem script”, not as a proof field.

σ (`ConvergeProofDigest`) is unchanged and is still the draw digest. It is simply never signed, so
there is no signature to display or verify.

**Sidebar disclaimer (required on mint pages):**

> `target_prev` binds the proof to parent proof-of-work compact bits for replay (Sigma-CR-01). RES draw inequality uses the constant `res_probability_target` (RFC-017 shipping `E = 246`, windowed; Historical RFC-014: `2^240`), not `target_prev`.

---

## 4. txid / wtxid display rules

| Context | Show txid | Show wtxid |
|---------|-----------|------------|
| Transaction page header | **Primary** | Secondary, labeled “Witness txid” |
| Convergence candidate tables | **Only txid** | Hide or footnote |
| Convergence lineage graph | **Only txid** | Forbidden as edge key |
| RES mint proof `convergence_txid` | **txid** | Never substitute wtxid |
| Mempool (optional) | txid primary | wtxid for package debug |

**Forbidden:** URL routes like `/tx/{wtxid}` as canonical.

---

## 5. Spending a native RES output — one path only

| Path | Stack depth | Display |
|------|-------------|---------|
| Key-path transfer | 2 items | “RES key-path spend” |
| Anything else | any | Red: “invalid RES spend” — consensus rejects it |

**SECURITY-001-A / A-7.** A witness v1 RES output has exactly one spending path: the 2-item Ed25519
key path, whose signature commits to the spending transaction through the sighash. The 11-item
“PCORB script path” was removed. Its only authorization was `VerifyConvergeProof` over σ, and σ says
nothing about the spending transaction — so a claim broadcast on that path was unauthenticated with
respect to its own outputs, and anyone could copy the eleven witness items into their own
transaction spending the same RES output to themselves and outbid the owner on fee. The explorer
MUST NOT offer, decode, or label any RES spend as a script path.

The PCORB witness in §3 is a separate thing: it appears on the **ORU** funding input of a body mint
transaction, never on a RES input.

Do not show a Bitcoin-style control block diagram. Optional: collapsible hex per item.

---

## 6. Reserved witness slots — removed (A-7)

The PCORB stack no longer has reserved slots. Former items 9 (32 zero bytes) and 10 (64 zero bytes)
were removed along with the RES script path that was their only reader, so there is no reserved-zero
state to render or police on this witness.

| State | Display |
|-------|---------|
| Any push beyond index 5 on a mint proof | Not a reserved slot. Label per §3 (funding redeem script) or, if unexpected, flag as non-canonical |

**Policy:** Do not resurrect a reserved-zero display for PCORB. Do not interpret trailing pushes as flags or version.

**Future extensions:** Require new RFC — UI MUST NOT guess semantics.

---

## 7. Canonicality notes

| Topic | Rule |
|-------|------|
| Witness serialization | Multiple witness serializations may exist pre F-WX-08; post enforcement, placeholders MUST be zero |
| Chainstate identity | Consensus uses semantic proof fields; explorer lists **txid** for set membership |
| σ layout | σ is a digest, not a signature. Nothing signs it (A-7) — do not display a signature field on the PCORB proof |
| Leaf script | **Removed** (A-7) — the PCORB stack has no leaf script push; Leaf-Freeze-01 no longer has a wire surface |
| Program length | Consensus v1 program MUST be 32 bytes |

---

## 8. Coinbase witness (display)

Coinbase may carry convergence root / CVGE pushes — link to mining docs. Do not label coinbase as RES mint unless `rigor_res_minted` on block refers to **body** mint.

---

## 9. Document metadata

| Field | Value |
|-------|--------|
| Risk classification | **MEDIUM** (terminology); **HIGH** if Taproot implied |
| Explorer classification | **DISPLAY-RULES** |
| Consensus impact | **NONE** |
| Replay impact | **INDIRECT** — mislabeled bind fields mislead auditors |
| Determinism impact | **NONE** |
| Indexing risk | **MEDIUM** |
| Minimal safe recommendation | 6-item PCORB table; RES key path only; txid primary; forbidden Taproot vocabulary |

# SECURITY-001-A5 — Design Analysis: binding the RES reward to the convergence winner

> **DEFERRED 2026-08-02.** A-5 is removed from SECURITY-001-A scope by governance decision and
> is now tracked as
> [`TASK-SECURITY-001-A5-ECONOMIC-GOVERNANCE`](./TASK-SECURITY-001-A5-ECONOMIC-GOVERNANCE.md).
> Reason: A-5 concerns RES economic attribution and the CVG2 witness format, which is an
> independent protocol-governance question rather than a security determination.
> SECURITY-001-A closes on A-1…A-4 and A-6 only. **No A-5 code may be written.**
>
> This document remains the technical reference for that task; its analysis is unchanged.

**Status:** DESIGN — no implementation. Deferred to a separate governance task.
**Subtask:** SECURITY-001-A finding A-5 (removed from scope; see banner)
**Repository:** `.` @ `2b3354be3e` (working tree per `SECURITY-001-A-FROZEN-BASELINE.md`)
**Date:** 2026-08-02

---

## 0. The gating question — read before evaluating any option

A-5 was written as: *"RES reward is paid to a miner-selected script, not to the convergence
owner."* That sentence contains an unstated premise — that the protocol **intends** the
convergence owner to receive the RES. I went looking for that intent and **could not find it.**

| Source | What it says about who receives RES |
|--------|--------------------------------------|
| RFC-014 | Nothing. Fixes the probability law and issuance rate only. `miner.cpp` listed as `UNCHANGED`. |
| RFC-015 | Nothing. Changes `RES_REWARD_AMOUNT` 1 → 100 RES. Mentions "miner and convergence participant feedback" without adjudicating between them. |
| `docs/mining/**` (14 documents) | Uniformly assume the **pool** performs the body mint and receives the RES. `TASK-DOCS-003` §1: "The **pool stack** … performs **RES body mint** when authorized." |
| `pool/**` | Entire payout stack built on the pool receiving RES and distributing it to hashrate contributors. |
| Code | `mining_res_receive_script`, `-miningresfeeaddr` — the naming itself encodes miner receipt. |

**So A-5 is one of two very different things, and which one determines whether it is a
vulnerability at all:**

- **Reading 1 — authorization defect.** Protocol intent is that the party who burned ORU
  into the convergence output earns the RES. Then the current code is a genuine
  misappropriation: the miner harvests a reward the protocol owes someone else, and A-5 is
  a High-severity finding requiring a consensus fix.
- **Reading 2 — documentation / threat-model defect.** Protocol intent is that the miner
  earns RES as compensation for performing the draw and the body mint. Then nothing is
  stolen — the miner is the intended recipient — and A-5 is not a consensus bug. The
  residual issue is economic: a convergence creator burns ≥1 ORU for a lottery ticket whose
  prize goes to a third party, which is a strange incentive but a *designed* one.

**I cannot resolve this from the repository. It is a governance decision, not an engineering
one.** Under Reading 2 the correct disposition of A-5 is "assessed, no code change; amend the
threat model and RFC to state the intent explicitly" — the same disposition A-7 received.

Everything below assumes Reading 1, because that is the only reading under which an
implementation question exists. **Do not approve an option without first ruling on the
reading.**

---

## 1. Verified current behaviour

All statements in this section were confirmed by reading the code at the frozen baseline.

### 1.1 The reward destination is unconstrained

```280:282:core/src/node/auto_res_mint.cpp
    mtx.vout[0].nValue = Consensus::RES_REWARD_AMOUNT;
    mtx.vout[0].scriptPubKey = mining_res_receive_script;
    mtx.vout[1].nValue = oru_change;
```

`mining_res_receive_script` comes from the miner's `-miningresfeeaddr`. Nothing downstream
checks it.

### 1.2 The consensus evaluator never looks at the RES output script

`rigor::ConnectEvaluateResMintTx` (`core/src/consensus/rigor_res.cpp:65-172`) validates, in
order: presence of a RES output, input asset domain, proof parse, height / `target_prev` /
`bind_hash` chain binding (added by A-1…A-3), winner existence, amount binding (A-4),
ConvergenceRoot membership, and the σ inequality plus signature. It reads
`o.scriptPubKey` exactly once — to test `IsResScript(o.scriptPubKey)` — and never compares
it to anything.

### 1.3 The proof signing key is bound to nothing

```144:146:core/src/consensus/res_proof.cpp
    return ed25519::VerifyDetached(Span<const unsigned char>{proof.sig.data(), ed25519::SIGNATURE_SIZE},
        Span<const unsigned char>{h.begin(), 32},
        Span<const unsigned char>{proof.pubkey.data(), ed25519::PUBKEY_SIZE});
```

`proof.pubkey` is supplied by whoever builds the mint transaction and is verified only
against a signature they also supplied. The proof is **self-certifying**: it proves the
builder can sign, not that they are entitled. This is the mechanism by which the miner can
mint against someone else's convergence.

### 1.4 CVG2 has 28 unconstrained bytes — already, today

```26:33:core/src/script/standard.cpp
bool IsConvergenceScript(const CScript& scriptPubKey)
{
    int witnessversion;
    std::vector<unsigned char> program;
    if (!scriptPubKey.IsWitnessProgram(witnessversion, program)) return false;
    if (witnessversion != 1 || program.size() != WITNESS_V1_RES_SIZE) return false;
    return std::equal(RigorTags::CVG2.begin(), RigorTags::CVG2.end(), program.begin());
}
```

Only bytes 0–3 (`"CVG2"`) are checked. **Bytes 4–31 are 28 bytes of free, consensus-accepted
payload space.** The wallet currently writes zeros there
(`convergenceres.cpp:44-52`). `convergence_consensus_tests.cpp` already exercises a CVG2
program with a non-zero tail and consensus accepts it as a convergence output.

This single fact is what makes Option A cheap and Option C redundant.

### 1.5 There is no "convergence UTXO"

CVG2 outputs never enter the UTXO set:

- `UpdateCoins` asserts `!inputs.HaveCoin(...)` for every convergence output (`validation.cpp:1872-1875`).
- `DisconnectBlock` skips them (`validation.cpp:2105`, `2171`).
- Spending a CVG2 input is rejected outright: `bad-txns-spend-convergence` (`tx_verify.cpp:194-196`).
- `getreplaysnapshot` documents the invariant `converge_count == 0`.

A convergence output is a **burn**. This is fatal to Option B as literally worded; see §4.

### 1.6 The witness layout is frozen

`ConvergeProof` is six fixed pushes plus sig, pubkey, leaf, and control block
(`res_proof.h:81-97`). The header comment is explicit: *"Field order is consensus-critical …
Do not reorder, insert, or remove fields without a hard-forking protocol upgrade."*

**Any option that adds a proof field is a hard fork and is out of scope.** All three options
below are designed to avoid this.

---

## 2. Threat model (under Reading 1)

| # | Actor | Capability | Current outcome |
|---|-------|-----------|-----------------|
| T1 | Block producer | Chooses `mining_res_receive_script` | Takes 100 RES per winning draw regardless of who funded the convergence |
| T2 | Block producer | Chooses `proof.pubkey` freely | Can construct a valid proof for **any** convergence in the block without the creator's consent or key |
| T3 | Convergence creator | Burns ≥1 ORU | Receives nothing; has no way to assert entitlement |
| T4 | Block producer | Selects which convergences to include | Combined with T2, controls both the candidate set and the payout — the draw is advisory |

The severity of T1/T2 under Reading 1 is High: it is unauthorized appropriation of the
protocol's only RES issuance channel. Under Reading 2 T1/T2 are not defects, and only T3
remains as an economic-design observation.

---

## 3. Option A — bind an owner commitment into the CVG2 witness program

Use the 28 free bytes at `program[4..32]` to commit to the entitled party. Two sub-variants
differ in *what* is committed, and they are not equivalent.

### A1 — commit to the Ed25519 proof public key

`program[4..32] := SHA256(ed25519_pubkey)[0..28]`; consensus additionally requires
`SHA256(proof.pubkey)[0..28] == winner_cvg2_program[4..32]`.

This closes T2: only the holder of the committed key can produce a valid proof. But it does
**not** close T1 — the payout script stays free, so if the creator hands the miner a
pre-signed proof (which they must, since the miner builds the block), the miner still
chooses the destination. Worse, it **breaks automatic minting**: `TryCreateAutomaticResMintTx`
cannot sign without the creator's secret key, so every draw would require an interactive
round trip with the winner inside the block-assembly window. **Reject A1.**

### A2 — commit to the RES payout script (recommended)

`program[4..32] := SHA256(res_payout_scriptPubKey)[0..28]`; consensus additionally requires
that the RES output of the mint transaction satisfy
`SHA256(res_output.scriptPubKey)[0..28] == winner_cvg2_program[4..32]`.

Closes T1 directly and defuses T2 (a forged proof is still possible but pays the rightful
owner, so it is no longer profitable). Preserves non-interactive automatic minting: the
miner builds and signs the mint transaction as today, but the destination is dictated by the
winner's own commitment, which is already on chain.

**Truncation analysis.** 28 bytes = 224 bits. The property required is second-preimage
resistance (224 bits — ample). Collision resistance is 112 bits, but a collision is an
attack the *owner* would have to mount against *their own* payout, which gains them nothing.
Acceptable, and worth stating in the RFC rather than leaving implicit.

#### Eight-dimension impact — A2

| Dimension | Assessment |
|-----------|-----------|
| **Consensus** | One new rule in `ConnectEvaluateResMintTx`. Needs the winner's CVG2 `scriptPubKey`, which is reachable: `FindConvergenceAmountInBlock` already locates that output and would be extended to return the script. The undo path reads `block.vtx` directly, so it is equally available — **no new coins-view dependency**, which is the main reason this is safer than Option B. New reject reason: `bad-rigor-res-payout-mismatch`. |
| **Backward compatibility** | Every existing convergence output has a zero tail, so a strict rule invalidates all of them. Two policies: (a) treat zero tail as "legacy, unbound" and keep current behaviour — safe but leaves T1 open forever; (b) reject zero-tail convergences from the draw. **(b) is affordable only because A-6 already forces a full chain reset** (all pre-A-6 chain data is discarded — though note that genesis blocks themselves are *not* regenerated; see RFC-014 v1.1 §6). A-5 and A-6 should therefore ship in the *same* reset; sequencing them separately costs a second one. |
| **Witness format** | **UNCHANGED.** No new proof field, no push-order change, no hard fork. This is the decisive advantage over any proof-carried alternative. |
| **Transaction serialization** | **UNCHANGED.** `CTxOut` untouched; the commitment lives inside the existing 32-byte witness program. Consistent with the script-as-asset rule — the asset and its entitlement are both parsed from `scriptPubKey`. |
| **Wallet** | `ConvergenceOutputScript()` gains a payout-script parameter (single construction site: `wallet/spend.cpp`). `createconvergence` gains an optional payout address, defaulting to a fresh RES (witness-v1) address from the same wallet. The wallet must persist the mapping so it can recognise an incoming reward. Qt `transactiondesc.cpp` / `transactionrecord.cpp` need the new display. |
| **Miner** | `TryCreateAutomaticResMintTx` reads the committed hash from the winner's CVG2 program instead of using `mining_res_receive_script`. `-miningresfeeaddr` stops governing the RES reward. **Miners lose RES income** — this is the economic consequence that makes §0 a governance question rather than a bug fix. |
| **Pool** | **Largest impact.** `pool/stratum-server/src/payout.rs` and `mint-engine/src/txbuild.rs` are built on the pool receiving RES and distributing it by share. Under A2 the RES bypasses the pool entirely. Either the pool's RES payout module is retired, or the pool must itself become a convergence creator (committing to its own payout script) to keep earning — a business-model change, not a refactor. Must be settled with SECURITY-001-E before implementation. |
| **Explorer** | Must decode `program[4..32]` and display the committed payout target; CVG2 outputs stop being opaque burns. Address indexing gains a "convergence beneficiary" relation. Non-blocking. |
| **Formal verification** | σ and ConvergenceRoot are unaffected as *functions* — they hash txids, and the txid changes but the mapping does not. One new lemma: "accepted mint ⇒ RES output script matches the winner's CVG2 commitment." Golden vectors must be regenerated because convergence txids change; that regeneration is already required by A-6, so the cost is shared. Coordinate with SECURITY-001-F, which has just re-armed the Coq harness. |

---

## 4. Option B — bind the convergence UTXO owner script hash

**Not implementable as worded, for three independent reasons.**

1. **There is no convergence UTXO** (§1.5). The only coherent reinterpretation is "the ORU
   prevout(s) funding the convergence transaction."
2. **Asset-domain violation — fatal.** Those prevouts are ORU, i.e. witness v0.
   `WitnessRFCBindingOk` rejects RES sent to any non-v1 script
   (`wallet/asset_domain.cpp:25-27`), and `IsResScript` requires witness v1. So the
   RES reward *cannot* be paid to the funding script. Bridging would require a v0→v1
   derivation that does not exist and cannot be derived from a hash.
3. **Ambiguity and grindability.** A convergence transaction may spend N inputs with N
   distinct owners. Any canonical pick ("first input") is chosen by the creator and
   therefore grindable, and multi-party funding has no defensible answer at all.

Secondary problem even if 1–3 were solved: resolving the convergence transaction's inputs at
mint-validation time requires the pre-block coins view or undo data, because by then those
prevouts are already spent within the same block. That adds a state dependency to a rule that
Option A satisfies from block data alone.

**Recommendation: reject Option B.**

---

## 5. Option C — bind a winning-transaction output commitment

Add an explicit commitment output (`OP_RETURN` + tag + payout script hash) to the convergence
transaction. Consensus locates the winner in the block, parses the commitment, and requires
the RES output to match.

Functionally equivalent to A2. Differences, all unfavourable:

| Dimension | Assessment vs A2 |
|-----------|------------------|
| **Consensus** | Comparable, but adds an `OP_RETURN` parsing surface: shape validation, tag collision, and a duplicate-commitment tie-break rule. More code, more edge cases. |
| **Backward compatibility** | Identical — chain-invalidating for the same reason. No advantage. |
| **Witness format** | Unchanged (equal to A2). |
| **Transaction serialization** | Unchanged in *format*, but every convergence transaction grows by a third output (~40–43 bytes) permanently. |
| **Wallet** | Comparable, plus fee/dust handling for the extra output. |
| **Miner / Pool / Explorer** | Same economic consequences as A2. |
| **Formal verification** | Strictly more work: a new output-shape predicate must be modelled, whereas A2 reuses the existing CVG2 program predicate. |
| **Standardness** | Consumes the transaction's single permitted `OP_RETURN`, foreclosing other uses. |

Its only genuine advantage is commitment width: an `OP_RETURN` can carry a full 32-byte hash
(or the literal script) rather than A2's 28 truncated bytes. Given the truncation analysis in
§3, that advantage does not justify the permanent size cost and the extra parsing surface.

**Recommendation: reject Option C**, unless the 112-bit collision bound in A2 is deemed
unacceptable by governance — in which case C becomes the fallback.

---

## 6. Comparison

| | A1 | **A2** | B | C |
|---|---|---|---|---|
| Closes T1 (payout redirection) | ✗ | **✓** | n/a | ✓ |
| Closes T2 (forged proof profitable) | ✓ | **✓** | n/a | ✓ |
| Witness format unchanged | ✓ | **✓** | ✓ | ✓ |
| Transaction size unchanged | ✓ | **✓** | ✓ | ✗ (+~40 B) |
| Non-interactive auto-mint preserved | ✗ | **✓** | ✓ | ✓ |
| Implementable at all | ✓ | **✓** | **✗** | ✓ |
| New consensus state dependency | no | **no** | yes | no |
| Commitment width | 224 b | **224 b** | — | 256 b |
| Verdict | reject | **recommend** | reject | fallback only |

---

## 7. Recommendation

1. **First, rule on §0.** If Reading 2 holds, close A-5 as "assessed, no change required" and
   amend RFC-014/RFC-015 plus the threat model to state that the block producer is the
   intended RES recipient. No code changes.
2. **If Reading 1 holds, implement Option A2**, subject to:
   - a governance decision accepting that miners and pools lose RES income;
   - resolution with SECURITY-001-E of what the pool's RES payout stack becomes;
   - shipping in the **same** chain reset as A-6, so the chain is reset once, not twice;
   - RFC amendment specifying the commitment construction, the 224-bit truncation rationale,
     and the zero-tail rejection rule.
3. **Do not implement anything under this finding until items in (1) or (2) are signed off.**

---

## 8. Explicitly out of scope

- The residual grinding-by-candidate-set finding (miner varies which convergences to include
  to shop for a favourable ConvergenceRoot). Independent of A-5; needs σ to depend on PoW.
- Any change to `rigor_consensus_lock.h` or other PROOF-034 SSOT priority-1 files.
- Any change to block header format, transaction serialization, or the PoW hash flow.

# SECURITY-001-A / A-7 — RES spending paths and the ConvergeProof signature

**Status:** implemented, awaiting review
**Scope:** consensus (script verification, proof parsing, mint witness), relay policy, wallet RPC surface, mining pool, formal spec, off-chain tooling
**Chain impact:** invalidating. Merges with the A-5/A-6 chain reset; no separate reset.

---

## 1. What prompted this

A-5 closed with an observation rather than a finding: the ConvergeProof's Ed25519 signature did not
bind any identity, because the verifying key was supplied by the witness itself. The question put to
review was how to give that signature a genuine authorization role.

The answer turned out to be that it cannot have one, and that pursuing it would have missed a live
vulnerability sitting next to it. Both are addressed here.

---

## 2. Finding A-7.1 (critical): RES claim transactions were hijackable in flight

### 2.1 The defect

A witness v1 RES output had two spending paths in `VerifyWitnessProgram`
(`core/src/script/interpreter.cpp`):

| path | witness | authorization | commits to the spending tx? |
|---|---|---|---|
| key path | 2 items: sig+hashtype, pubkey | `CheckEd25519Signature` over the BIP143-style sighash | **yes** |
| PCORB script path | 11 items | `VerifyConvergeProof` over the σ digest | **no** |

σ is `Hash256(convergence_txid ‖ convergence_root ‖ bind_commit ‖ LE32(height))`. Every input is a
property of the convergence being proved and of the block context. Not one byte of the spending
transaction — not its outputs, not its other inputs, not its locktime — enters the digest. The
interpreter documented the omission deliberately, on the grounds that the 64-byte push could not
also carry a 65-byte sighash signature.

The eleven checks the branch performed were: two reserved pushes all-zero, leaf script equal to
`OP_1`, `wit[7]` equal to the witness program, the proof parses, the witness amount matches the
proof, `CheckConvergeProofBinding` against the block, and `VerifyConvergeProof`. All eleven are
functions of the witness, the prevout and the block. None is a function of where the money goes.

### 2.2 Exploitation

The owner calls `createresclaim`, which builds a transaction spending their RES output on the script
path and broadcasts it. The witness now contains a valid σ signature under the owner's own program.

An observer copies `vin[0].scriptWitness` verbatim into a transaction that spends the same RES
output to an address of their choosing. Every check above passes identically, because the witness is
unchanged and none of the checks look at the outputs. The attacker attaches a higher fee and wins
the race. The owner's RES is gone.

Preconditions, all of which held:

- The 11-item shape was **relay-standard** (`IsNativeResMintWitnessPolicyStandard`,
  `core/src/policy/policy.cpp`), so the victim's transaction propagated and was visible in mempools.
- `createresclaim` was a **registered wallet RPC with no devtest gate**
  (`core/src/wallet/rpc/rpcwallet.cpp`).
- No forgery was required. The attacker never needs the owner's key; the owner publishes the only
  material needed, and publishing it is the whole point of broadcasting.

The exposure was bounded only by whether anyone used the RPC. An unused path is not a safe path.

### 2.3 Remedy: delete the path, do not repair it

The 11-item branch is removed. A witness v1 RES output is now spendable only over the key path,
whose signature commits to the transaction.

Repairing the path instead — requiring an additional sighash-committed signature — was rejected. It
would have left a RES output with two spending paths, one of them strictly redundant, and would have
kept the σ signature that A-7.2 shows is meaningless. Removal is smaller and strictly safer.

Two further oddities disappear with it. Spending your own RES no longer requires referencing a
*winning convergence draw* (`VerifyConvergeProof` enforces the σ < T·f eligibility inequality), a
coupling that never had any meaning for a transfer. And the reserved pushes at `wit[9]`/`wit[10]`,
described in the source as placeholders for a future Taproot-era rule, are gone — that rule is
forbidden by the architecture constitution anyway.

### 2.4 Nothing is stranded

Wallet signing (`core/src/script/sign.cpp`) takes the script path only when
`sigdata.res_script_path->active` is set, which only `createresclaim` did. Ordinary RES transfers
have always used the key-path branch, which produces a detached Ed25519 signature plus `SIGHASH_ALL`
and is untouched. `res_single_spend_path_tests.cpp` asserts the key path still relays.

---

## 3. Finding A-7.2 (informational, by design): the ConvergeProof signature authorized nothing

### 3.1 What it proved

`VerifyConvergeProof` verified `wit[6]` over σ under the key in `wit[7]`. The message is a pure
function of the six proof fields, and A-1..A-4 already verify each of those fields against the block
independently. The key is chosen by whoever writes the witness. A valid signature therefore
demonstrated one thing: that somebody possessed *some* Ed25519 private key. Any keypair generated on
the spot satisfied it, in microseconds.

### 3.2 Why no identity could be bound to it

Authorization is only meaningful over discretion, and after A-1..A-5 the mint has none. Given a
block's convergence set: F is determined, the winner is determined by σ ordering, the amount is fixed
at 100 RES, and the payout script is fixed by the winner's RESO declaration. `validation.cpp:2874`
rejects any block where a winner is not accompanied by exactly one RES mint. There is no free choice
for anyone to authorize.

Each candidate signer fails for its own reason:

**The RES owner** — the only signer that would have meant anything. σ contains `convergence_root`,
the root over the block's whole candidate set, which the owner cannot know when creating their
convergence transaction. They cannot pre-sign. Requiring their signature would make minting
interactive: the block producer would have to reach the winner and obtain a signature inside the
block window, turning an offline user into a liveness dependency and a denial-of-service target, in
exchange for authorizing a payment they are receiving anyway.

**The block producer or ORU funder** — the mint transaction is already authenticated by its ORU
funding input, whose signature commits to the whole transaction via sighash. A second signature by
the same party adds nothing; under a different key it would be exactly as unbound as before.

**A designated mint authority** — would give the signature real force, and violates the architecture
constitution's prohibition on administrative keys in the consensus path (Iron Rule #7). Not available.

### 3.3 Remedy

`wit[6]` and `wit[7]` are removed, along with `ConvergeProof::sig`, `::pubkey`, `::has_sig` and the
`ed25519::VerifyDetached` call. σ itself is unchanged and still decides the draw; it is simply no
longer signed.

The reasoning is recorded in `core/src/consensus/res_proof.h` so the next reader does not
reintroduce it expecting it to gate anything.

### 3.4 Why removal rather than leaving it in place

A check that cannot fail for an adversary is not a check. Leaving it had three ongoing costs:

- **It misdirected operational security.** `PRODUCTION_TREASURY_ARCHITECTURE.md` instructed
  operators to hold this "mint key" in an HSM. That is effort spent protecting a key whose
  disclosure grants nothing, and worse, it invited the belief that the mint path was key-gated.
- **It cost consensus surface.** 96 witness bytes per mint and one Ed25519 verification on the
  block-connect path, plus a field every future reader had to reason about.
- **It cost review attention permanently.** This is the second audit cycle in which the signature had
  to be analysed before it could be dismissed.

The pool now holds **no key at all** on the RES mint path.

---

## 4. Changes

### 4.1 Consensus

| File | Change |
|---|---|
| `script/interpreter.cpp` | Removed the 11-item PCORB branch from the witness v1 RES path. Key path only. |
| `consensus/res_proof.h` | `ConvergeProof` reduced to the six proof fields. Removed `ConvergeProofLeafScript`/`ConvergeProofLeafHash` declarations and the dead `ConvergeProofRFC005` struct. RFC quotes annotated as superseded. |
| `consensus/res_proof.cpp` | `ParseConvergeProof` no longer parses a signature. `VerifyConvergeProof` returns the draw inequality directly. Leaf helpers and their hash removed. |
| `consensus/res_mint_witness.{h,cpp}` | Builder emits six pushes; takes no key. New `PCORB_BODY_RES_MINT_WITNESS_ITEMS = 6`. |
| `consensus/rigor_res.cpp`, `consensus/res_proof.cpp` | The "PCORB dual-semantics" header comments described mint mode and claim mode sharing one witness layout. Claim mode no longer exists; the comments now state the single meaning while keeping the normative rule that the domain comes only from `GetTransactionInputAsset`. |
| `node/auto_res_mint.cpp` | Bootstrap redeem script drops 6 pushes, not 11. Devtest signing key (`RIGOR_DEVTEST_AUTO_RES_MINT_V1`) removed. |
| `policy/policy.cpp` | Witness v1 RES is standard only at 2 items. `IsNativeResMintWitnessPolicyStandard` deleted. |

### 4.2 Wallet

| File | Change |
|---|---|
| `wallet/rpc/rpcspend.cpp`, `rpcwallet.cpp` | `createresclaim` removed and unregistered, along with two helpers (`LookupResSpendSecretFromWallet`, `ComputeConvergenceRootForNextBlockTemplate`) and the includes that only it needed. |
| `rpc/client.cpp` | Eight `createresclaim` rows removed from the argument-conversion table, which still described a method that no longer exists. |
| `script/sign.{h,cpp}` | `ResScriptPathSpendRequest` and `SignatureData::res_script_path` removed, with the script-path witness builder and its merge logic. |
| `wallet/coincontrol.h`, `wallet/wallet.cpp` | `m_res_script_path_fee_dummy` and its fee-estimation witness removed. |
| `script/descriptor.h` | `res(KEY)` documented as having one spending path. |

### 4.3 Build: a latent archive cycle, surfaced and removed

Deleting the RPC made `rigor-wallet` fail to link with undefined references to
`Consensus::GetTransactionInputAsset`, `ParseConvergeProof` and `CheckTxOutAssetWitnessBinding`.

The cause predates A-7. `libbitcoin_node.a` and `libbitcoin_consensus.a` were mutually recursive, so
a single linker pass over them resolved only when some earlier object happened to have pulled the
right members in already. Nothing guaranteed that, and removing unrelated code disturbed the
accident. No number of repeated archives is a sound fix for a true cycle, so the cycle itself was
measured with `nm` rather than guessed at:

- **node → consensus:** ~90 objects. Expected, and correct layering.
- **consensus → node:** exactly one edge — `consensus_rigor/fee.o` calling
  `Consensus::GetTransactionInputAsset`, defined in `consensus/tx_verify.cpp`.

That single back-edge created the whole cycle. `consensus_rigor/fee.cpp` was moved from
`libbitcoin_consensus_a_SOURCES` to `libbitcoin_node_a_SOURCES`, which is where it belongs: it
resolves an input's asset through a `CCoinsViewCache`, i.e. it needs chainstate, and upstream keeps
coins-view-dependent validation in the node library for the same reason. `libbitcoin_consensus.a`
now has **zero** dependencies on `libbitcoin_node.a`, verified by re-running the `nm` scan, and
`rigor_wallet_LDADD` is back to a single ordered pass with the duplicate `$(LIBBITCOIN_NODE)`
removed.

No source file changed and `libbitcoinconsensus.la` is unaffected, since its source list is separate
and already contained both files. `rigor-tx` and `bitcoin-util` link `libbitcoin_consensus` without
`libbitcoin_node`; they were already proof that nothing they use pulls `fee.o`, since if it were
pulled its unresolved node symbol would have failed those links long ago.

One incremental-build hazard was found while verifying this: `ar` does not remove a member when its
source leaves the list, so a stale `libbitcoin_consensus_a-fee.o` remained inside the archive and
would have kept satisfying the link from the wrong library. It was purged with `ar d` and the
archive re-indexed. **A clean rebuild is recommended for the release build** rather than relying on
the incremental tree.

### 4.4 Mining pool

| File | Change |
|---|---|
| `mint-engine/src/witness.rs` | Six-push builder. Removed `devtest_ed25519_keypair`, `signing_key_from_seed`, `verify_pcorb_ed25519`, `CONVERGE_LEAF_SCRIPT`. New `PCORB_WITNESS_ITEMS = 6`. |
| `mint-engine/src/txbuild.rs` | Redeem script is `OP_DROP × 6 ‖ OP_TRUE` (`75757575757551`). No signing key. |
| `mint-engine/src/{engine,types,block,lib}.rs` | `ed25519_seed` removed from config and build params; the C-6 production gates on the seed removed with it. |
| `pilot/src/security_config.rs` | `RIGOR_POOL_MINT_ED25519_SEED_HEX` removed. |

### 4.5 Documentation and governance

Fifty-two documents referenced the removed constructs. Live specifications were rewritten to describe
present reality; dated audits, verdicts and release records were annotated and left legible, on the
principle that a security remediation must not edit the history that justifies it.

The governance conflicts are the part worth reviewing, because three frozen rules were invalidated:

| Document | Disposition |
|---|---|
| `rfcs/RFC008_ERRATUM_RESERVED_ZERO.md` | **Moot.** It fixed the item count at 11 and froze indices 9–10 to zero. Neither the count nor the placeholders exist. |
| `freeze/H_WITNESS_RESERVED_ZERO_FREEZE_REVIEW.md` | **Moot.** Its finding — one semantic spend having many valid `wtxid`s — is resolved by deleting the path rather than canonicalising it. The RULE-1 outcome it demanded is achieved. |
| `freeze/WITNESS_RESERVED_ZERO_ACTIVATION_REPORT.md` | **Reverted.** F-WX-08 / F-WX-09 enforcement is gone with its host; `SCRIPT_ERR_WITNESS_RESERVED_NONZERO` is unreachable on that path. |
| `freeze/TASK-WITNESS-EXT-001-…-freeze.md` | **Partially superseded.** F-WX-08, F-WX-09, F-WX-10 and F-WX-11 are void; F-WX-14 and F-WX-15 amended; the other fourteen rules stand. |
| `formal/RIGOR_CONSENSUS_INVARIANTS.md` INV-16 | **Retired.** Leaf-Freeze-01 constrained a push that no longer exists. |
| `formal/RIGOR_FORMAL_STATE_MODEL.md` §10 | **Amended.** The `ValidWitness` predicate dropped the leaf conjunct, and its signature clause was split: the mint proof carries no signature, and the only Ed25519 signature in a RES spend is the sighash-committed key-path one. |

`F-WX-14` deserves explicit note: it froze the witness item count absent a hard fork, and A-7 changed
it. The rule was honoured rather than bypassed — A-7 is chain-invalidating and ships before mainnet
with the A-5/A-6 reset, which is the process F-WX-14 exists to compel.

One consequence is worth stating plainly for future extensibility work. TASK-WITNESS-EXT-001 had
nominated `wit[9][0]` as the host for a future `WitnessV1Flags` soft fork. That host is gone and **no
byte anywhere in the witness is now reserved**. This is an improvement, not a gap: a future extension
can no longer quietly repurpose an ignored byte and must define a new witness mode under a new RFC,
which is what F-WX-07 and F-WX-20 always required.

Operationally, `mining/M04_CUSTODY_RUNBOOK.md` strikes the mint-key HSM enrollment, ceremony,
attestation and rotation procedures outright, and deletes the Custody Officer role, which existed
only to run that ceremony. `release/evidence/release006/M-04/M04_HSM_ENROLLMENT.json` is retained as
the historical record of the superseded key and is referenced as such from the runbook.

Left deliberately unchanged: `rfcs/RFC-003` lists `sigma_signature` among **forbidden** names, which
is now more apt rather than less; `freeze/H_RFC_014_REVIEW_VERDICT.md` states that RFC-014 does not
touch the leaf layer, which remains a true statement about RFC-014's scope; and
`security/SECURITY_001_A_FINAL_COMMIT_SCOPE.json` records what an earlier commit did.

### 4.6 Formal verification

`spec/rigor_consensus_spec.py`: `validate_orb_proof` no longer takes `signature_valid`, and
`bad-ORB-signature` is gone. `test_runner.py` and the three affected golden vectors updated. All
three vectors only ever set `signature_valid: true`, so no adversarial coverage was lost.

---

## 5. Verification

| Suite | Command | Result |
|---|---|---|
| C++ build | `make -j$(nproc)` | clean, no warnings |
| C++ unit | `./src/test/test_bitcoin` | 752 cases, 0 failures |
| Golden vectors | `python3 formal_verification/test_runner.py` | 16/16 |
| Freeze integrity | `python3 sim/freeze/freeze_integrity_checker.py` | PASS |
| Explorer indexer | `npm test` | 44/44 |
| Pool crates | `cargo test` per crate | mint-engine 33, pilot 10, stratum-server 57, winner-engine 9, block-assembler 23, rigor-gbt 23, share-validator 17, rigor-convergence-root 3, rigor-pool-pg 3 — all pass |

`rigor-miner-agent` could not be built in this environment because a crate download was blocked by
the network sandbox. It does not reference the witness layout or the mint key.

A repository-wide scan for `createresclaim`, `res_script_path`, `ConvergeProofLeaf`,
`m_res_script_path_fee_dummy`, `ResScriptPathSpendRequest`, `verify_pcorb_ed25519`,
`devtest_ed25519_keypair`, the old `757575757575757575757551` redeem script, the mint seed
environment variable and `signature_valid` returns no live code references. Remaining hits are in
dated audit and release records, which are left intact as history.

### 5.1 New regression coverage

`core/src/test/res_single_spend_path_tests.cpp`:

- `relay_rejects_removed_pcorb_script_path` — an 11-item stack on a native RES input is non-standard.
- `script_rejects_removed_pcorb_script_path_even_with_matching_program` — script verification rejects
  it with `SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH` even when `wit[7]` equals the program exactly, which
  is the condition the removed branch checked.
- `relay_accepts_key_path_two_item_witness` — the surviving path is untouched.

`mint-engine/src/witness.rs`: `pcorb_stack_carries_no_signature_or_key` fails if any 64-byte push
reappears, which would desynchronise the pool from `ParseConvergeProof`.

### 5.2 Tests removed

`res_createresclaim_tests.cpp` and `res_witness_reserved_zero_tests.cpp` covered only the deleted
path and are gone. `res_script_path_policy_tests.cpp` was replaced by
`res_single_spend_path_tests.cpp`, which asserts the inverse property. The tapleaf freeze test in
`res_proof_math_tests.cpp` went with the leaf helpers; the leaf hash appeared in no freeze snapshot
or golden vector.

Three σ-domain tests had premises that referred to the signature and were rewritten to assert what
is actually enforced now. Two of them previously demonstrated cross-difficulty replay resistance
*cryptographically*, because a signature over σ(target_prev_main) could not verify against
σ(target_prev_fork). That mechanism is gone. Replay resistance is now **structural**: A-3 requires
`proof.target_prev == pindex->pprev->nBits`, leaving no field for the attacker to move, and A-1/A-2/
A-4 do the same for height, bind_hash and the convergence amount. The tests now assert σ domain
separation and point at the A-1..A-4 binding cases for the rejection itself. This is a change in
which layer carries the property, not a loss of the property — and it is the same redundancy that
made the signature removable.

---

## 6. Residual risk

**Convergence-transaction censorship.** A block producer can exclude convergence transactions
entirely, leaving F empty and no winner to pay. This is the ordinary censorship any miner has over
any transaction and is not fixable at consensus. It is not mint withholding: `validation.cpp:2874`
forbids including a winner without exactly one RES mint, so a producer that carries the draw cannot
decline to pay it, redirect it, or pocket it.

**No proof of possession on the RESO declaration.** A convergence may declare a `program32` whose
private key nobody holds, making the minted RES unspendable. This is self-harm, indistinguishable
from any user burning their own funds, and consensus does not police it — no more than Bitcoin
checks that you can spend your own outputs. Declaring a *third party's* key is a valid gift, so
replay-resistance is not wanted here either.

**A-5 economic governance** remains open under `TASK-SECURITY-001-A5-ECONOMIC-GOVERNANCE`.

**`consensus/selection.cpp` is compiled into two archives.** Found while verifying the link fix, and
reported rather than changed because it is outside A-7's remit. The file is listed in both
`libbitcoin_node_a_SOURCES` and `libbitcoin_consensus_a_SOURCES`, so `SelectWinner`, `IsEligible`,
`RunSelection`, `GetWeight`, `ComputeSigmaHash` and `ComputeConvergenceRoot` each have two strong
definitions and the linker silently takes whichever archive it reaches first. The two copies agree
today because they are built from one source, but they are built under different per-archive
`CPPFLAGS`, so nothing structurally guarantees that the convergence winner is selected by the same
code in every binary. This deserves its own task: pick one owning archive, and add a build-time
check that no strong symbol is defined in both.

---

## 7. Chain impact

The witness layout and the set of valid spending paths both change, so previously valid transactions
and blocks no longer validate. This is invalidating and merges with the reset already scheduled for
A-5/A-6. No additional reset is required.

Governance note: RFC-009 §5 and RFC-004 §10 both specify a 64-byte signature field in the proof
serialization. Those clauses are superseded for the mint witness. The header records this inline;
formal RFC amendment should follow the same route as the RFC-014 v1.1 amendment.

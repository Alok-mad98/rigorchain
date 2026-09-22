# Chapter 7 — Local Verification: Independent Validation of Public Data

Chapter 6 supplied a shared height and said displayed mining status is
not a source of validity. This chapter states the substitute: public
bytes recomputed locally, admitted if equal, rejected if not.

The claim is architectural. The protocol fixes that **commitment and
recomputation must match**. It does not claim that cheating cannot
happen. It claims that unequal bytes fail the published checks.

```
public data
        ↓
independent calculation
        ↓
compare to the commitment
        ↓
accept / reject
```

Explorers, pools, wallets, and websites may **display** data.
Consensus validity is decided by full-node evaluation of the same
public bytes.

Roles remain separate:

| Actor | Role |
|-------|------|
| Explorer | displays |
| Node | decides |
| Wallet | constructs |
| Verifier | recomputes |

A displayed result is an observation. Consensus validity is determined
by independent verification.

---

## 7.1 Display is not consensus decision

Mining software, explorers, and wallets exist so people can inspect
shares, balances, and height. Inspection is not admission.

If a displayed verdict is treated as consensus, the display becomes a
counterparty. Chapter 1’s security and decentralisation assumptions
require that validity remain a local evaluation of public data, not an
outsourced screen.

The intended habit is not “a more authoritative website.” It is:

```
take the public bytes, run the same function, compare
```

The full node `rigord` does this for every block it accepts. The Zero
Trust page in the browser does this for selected fields. Anyone who
evaluates `(R, B, h)` from Chapter 5 on paper is doing the same thing.
The scale differs. The standard does not: equal passes, unequal fails.

---

## 7.2 Which bytes are public

Convergence and block finding have no “internal transaction” path. The
inputs the decision needs are in data that has already been broadcast:

| Object | Where it is public | What is recomputed locally |
|--------|--------------------|----------------------------|
| Destroy `B` | Value of the `OP_CONVERGE` output | Exists, unique, from ORU |
| Mint `R` | Sum of Witness v1 outputs in the same tx | `R · (2²⁵ + h)² ≤ B << 40` |
| Height `h` | Height of the including block | Matches chain order |
| Coinbase | First transaction in the block | States this block’s height; pays ORU only |
| Merkle root | Block header | Equals the recomputed root of the tx list |
| Work | The 80-byte header | `OpuHash(header) ≤ target` |
| Block identity | The same 80-byte header | SHA256d, not the same hash as OpuHash |

Change any step without changing the later commitments, and
recomputation fails: the block is invalid. Validity is equality of
commitment and recomputation, not a flag on an explorer.

---

## 7.3 A commitment chain that can be walked

A block a third party can regenerate walks one chain of bindings:

```
body transaction
        ↓
OP_CONVERGE destroys B
        ↓
Witness v1 RES outputs sum R (may be 0)
        ↓
R > 0 → R · (2²⁵ + h)² ≤ B << 40
        ↓
coinbase (this block’s height; ORU only)
        ↓
coinbase txid
        ↓
merkle root
        ↓
block header
        ↓
OpuHash = SHA3-256("RGRPOW01" || header_80)
```

If any step fails to match, the block is invalid. An explorer may
display VALID; the block is still valid only if recomputation matches.
Coinbase states this block’s height and pays ORU only. An underpaid
mint fails the predicate. A pool credit display does not make it valid.

That is self-proving: the block is not valid because it was displayed.
It is valid because it was recomputed.

---

## 7.4 What each surface is for

| Surface | What it is | What it is not |
|---------|------------|----------------|
| Full node `rigord` | Applies consensus rules; admits or rejects blocks | A website’s backend opinion |
| Wallet | Constructs and spends; may display balances | A source of consensus validity |
| Explorer / indexer | Views and retrieves public data | Consensus |
| Pool dashboard | An operator view of shares and hashpower | A source of block legality |
| Website Zero Trust | Recomputes selected fields in the browser | A substitute for a full node |

A wallet may show “RES minted.” That is a spending-tool screen.
Admitting that RES still requires the same-tx destroy and the
predicate. A pool may show “block found.” Admitting that block still
requires OpuHash, Merkle, and coinbase. An explorer may show a
fingerprint. Checking the fingerprint still requires computing it
locally from the same bytes.

---

## 7.5 A fingerprint is observational, not a rule

The **consensus fingerprint** publishes a deterministic identity over
already-public fields. It is observational. It does not accept blocks
and it does not reject blocks.

A mismatched fingerprint cannot make an invalid block valid. A matching
fingerprint cannot replace full-node validation. An explorer may
display it. A verifier recomputes it. Equality means “the published
identity matches the local bytes.” It does not mean the publisher was
an authority.

Zero Trust pages and an explorer’s independent-verification exit exist
to send a display back to recomputation. The live entry is
`/zero-trust-verification`. They are tools, not an extra consensus
layer.

---

## 7.6 What this chapter does not claim

- It does not claim that cheating is impossible. It claims that
  unequal bytes fail.
- It does not claim that browser recomputation replaces a full node.
  Selected-field recomputation is not `ConnectBlock`.
- It does not claim that a fingerprint is a consensus rule.
- It does not claim C++ equivalence to the kernel model. Model proofs
  are Chapter 8. Even if the model holds, the bytes the node is
  checking still have to be recomputed.
- It does not write independent validation as “do not run a verifier.”
  The decision must execute somewhere: a node, a local program, or
  paper. The executor does not have to be the displayer.

**A conclusion may be displayed. Validity must be recomputed.**
Chapter 8 draws the boundary of formal proof: invariants that hold in
the model are not a proof that the running binary is equivalent.

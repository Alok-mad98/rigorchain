# Chapter 6 — Consensus Verification

Trust in RIGOR is not “trust the team”. Trust is **recompute the function**.

---

## Self-Proving Block Commitment

A valid block carries public commitments that a third party can regenerate
from the same bytes.

```
Public block
        ↓
eligible convergence transactions
        ↓
Convergence Root / Draw Candidate Root
        ↓
σ and winner
        ↓
compare to commitment
        ↓
accept only if equal
```

In engineering language, the check walks a single chain of bindings:

```
Convergence tx
    → script / CVG2 commitment
    → Convergence Root
    → coinbase commitment
    → coinbase txid
    → merkle root
    → block header
    → OpuHash
```

If any step fails to match, the block is invalid. That is **Self-Proving
Block Commitment**: the block is not valid because a website said so. It is
valid only if the committed values equal the recomputed values.

Nobody is asked to trust an explorer `VALID` flag.

---

## Fingerprint Library

The **Consensus Fingerprint Library** publishes a deterministic identity
over already-public fields. It is **observational**. It does not accept or
reject blocks.

```
ConsensusFingerprint =
  Hash256(ConvergenceRoot || winner_txid || Sigma || height || target_prev)
```

An explorer may *display* a fingerprint. A verifier *recomputes* it.
Equality means the published identity matches the local bytes. It does not
mean the publisher was an authority.

---

## Zero Trust

> You do not need to trust a node. You can independently verify the result yourself.

| Surface | Role |
|---------|------|
| Full node (`rigord`) | Applies consensus rules |
| Website Zero Trust portal | Recomputes TX / block fields in the browser |
| Explorer | Viewer / indexer |
| Wallet | Spending tool, not an oracle |
| Pool dashboard | Operator observer |

All critical inputs are public: candidate txids, sort order, Convergence
Root, σ, height, and the resulting winner. There is no private “internal
transaction” path. A manipulation that breaks the self-proving chain makes
the block invalid.

Formal verification of the **model** exists as a research layer. It does
**not** claim that the shipping binary is proven identical to that model.

```
equivalence_claimed = false
```

Independent verification means: download the data, run the function, compare
the bytes.

Live site (keep; do not replace):

- `/zero-trust-verification`
- `/zero-trust-verification/what-is-zero-trust`

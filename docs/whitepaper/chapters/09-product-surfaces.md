# Chapter 9 — Product Surfaces: A Display Remains a Display

Chapter 8 drew the line between the model and the running binary.
This chapter states how people see and participate in those rules:
wallet, explorer, pool, documentation, website. The surfaces must live
with the rules. They must not rewrite them.

The claim is still architectural. The protocol fixes that **a surface
may display a conclusion; it may not be that conclusion**. It does not
read a green Chapter 8 as “the chain has been proved.” It does not read
product completeness as consensus completeness.

```
rules accepted or rejected in the node
        ↓
a surface displays public facts
        ≠
the surface becomes a source of validity
        ≠
a formal-verification page becomes an equivalence proof
```

Chapter 7 already said that a pool, explorer, wallet, or website may
display a conclusion and none of them may be that conclusion. This
chapter does not recopy that table. It blocks the collapse that
follows Chapter 8: printing “verified” on a product surface and
reading it as a licence for the running binary.

---

## 9.1 Why coexistence must be stated

Products exist because people need to see balances, shares, height,
and explanation. Without sight, the rules exist only among node
operators, and the exchange and participation Chapter 1 requires
cannot happen. Sight is a condition of use.

Sight is not decision. Chapter 7 already refused to outsource validity
to a screen. If products are forbidden, the protocol cannot be used.
If products are also allowed to become authority, the counterparty
Chapter 1 refused moves from the pool dashboard to the homepage copy.
Coexistence means: the surface must be usable; authority must remain
in the node.

Formal documentation is the most dangerous surface. What it says is
true: the model contains machine-checked invariants. Once that true
sentence sits on a product surface, readers supply the half-sentence
that was not written — “so the chain has been proved.” Chapter 8 does
not issue that half-sentence. Chapter 9 forbids a product surface from
issuing it on Chapter 8’s behalf.

Shortening the allowed sentence rewrites the honesty flags. Product
copy does not hold that licence.

---

## 9.2 What each surface may do, and what it may not

| Surface | It may | It may not |
|---------|--------|------------|
| Full node `rigord` | Apply consensus rules; accept or reject blocks | Be overridden by a website, wallet, or explorer opinion |
| Wallet | Construct, spend, show balances and maturity | Be a validity oracle; write “minted” as “the predicate passed” |
| Pool / mining page | Show shares, height, ORU pay | Allocate RES; show an underpaid mint as a legal credit |
| Explorer | Retrieve blocks, transactions, fingerprints | Accept or reject blocks; let a `VALID` flag replace recomputation |
| Zero Trust | Recompute selected fields in the browser | Replace `ConnectBlock` or a full node |
| Whitepaper / docs | Bind the public argument; point to living correspondence | Rewrite `core/src/`; write a day’s vector count as a parameter |
| Formal verification page | Publish what is proved and what is not | Write `equivalence_claimed` as true |
| Lab / experiment | Observe and research | Act as a refinement proof or “mainnet is complete” |
| Website | Entry and explanation | Consensus authority; “bug free” or “zero risk” marketing |

How to click through a visible mining path is
[`07-mining-windows.md`](./07-mining-windows.md). That is a use path,
not another set of consensus rules.

A wallet may show “RES minted.” Admitting that RES still requires the
same-transaction destroy and the predicate. A pool may show “block
found.” Admitting that block still requires OpuHash, Merkle, and
coinbase. An explorer may show a fingerprint. Checking the fingerprint
still requires computing it from the same bytes. A formal page may
show a green build. Admitting a block in transit still requires
recomputation in the node — green is not a licence to skip Chapter 7.

---

## 9.3 Three ways Chapter 8 is misread on a product surface

A product surface compresses a boundary into a badge, a title, and a
marketing line. Each of the following compressions reads Chapter 8 as
the sentence it refused:

```
“selected invariants are proved in the model”
        compressed into
“the chain has been proved”
```

1. **Drop the word “model.”** A Coq-green badge on the home page or
   the download page is read as the binary, not the kernel model.
2. **Write finite vectors as universal equivalence.** Agreement at
   locked differential cases must not print as “C++ is proved equal to
   Coq.”
3. **Write “do not trust the pool” as “do not run a node.”** Chapter 7
   addresses the displayer, not verification itself. The decision
   still has to execute somewhere. The homepage is not that place.

Product copy may repeat the one public sentence in Chapter 8. It may
not shorten it into “the chain has been proved.” Shortening
rewrites the honesty flags as true. This chapter does not issue
that rewrite.

Living entry points remain tools, not an extra consensus layer:
`/zero-trust-verification` sends display back to recomputation;
`/documentation/formal-verification` publishes the boundary; wallet,
mining, and explorer are for participation and observation.

---

## 9.4 Documentation work is not a consensus change

Completing mining guides, checksums, placeholder pages, and chapter
HTML is product and documentation work. It does not change the
predicate, the 128 ORU per block, the classifier, or the derived
ceiling.

If a product page shows a “next step,” it can only mean “how these
surfaces are written clearly.” It cannot mean “these rules will be
rewritten on a date.” The rules are already frozen in the running
node. Surfaces follow the rules. They do not walk in front of them.

No date is given for filling the derived ceiling. No investment yield
is promised. Whether demand arrives is still not a rule — Chapter 2
already said so.

---

## 9.5 What this chapter does not claim

- It does not claim that product surfaces are complete, or that a
  placeholder is a protocol hole.
- It does not claim that browser recomputation replaces a full node.
- It does not claim that the website or this paper is the source of
  truth. The source of truth is `core/src/`.
- It does not write the formal page as equivalence already holding,
  or an experiment page as a refinement proof.
- It does not write visible mining as a source of validity.
- It does not write this chapter’s surface list as another set of
  consensus rules.

**A display remains a display. The rules remain in the node.** The
appendix lists frozen parameters and the engineering index — lookup,
not another claim:
[`10-appendix.md`](./10-appendix.md)
(Chinese: [`10-appendix.zh.md`](./10-appendix.zh.md)).

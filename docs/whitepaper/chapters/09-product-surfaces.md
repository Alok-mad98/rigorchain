# Chapter 9 — Product Surfaces: A Display Remains a Display

Chapter 8 drew the line between the model and the running binary.
This chapter states how people see and participate in those rules:
wallet, explorer, pool, documentation, website. Surfaces present the
rules. They do not rewrite them.

The claim is architectural. The protocol fixes that **a surface may
display a conclusion; it may not be that conclusion**. It does not
read a green Chapter 8 as “the chain has been proved.” It does not read
product completeness as consensus completeness.

The website is not authority. The explorer is not consensus. The pool
is not an issuer. Product completeness and consensus completeness are
different engineering objectives.

```
rules accepted or rejected in the node
        ↓
a surface displays public facts
        ≠
the surface becomes a source of validity
        ≠
a formal-verification page becomes an equivalence proof
```

Chapter 7 already stated that explorers, pools, wallets, and websites
may display data, while consensus validity remains a full-node
decision. This chapter does not recopy that table. It records the
corresponding limit after Chapter 8: printing “verified” on a product
surface is not a licence for the running binary.

---

## 9.1 Why surfaces and consensus coexist

Products exist because people need to inspect balances, shares, height,
and explanation. Without those views, the rules exist only among node
operators, and the exchange and participation described in Chapter 1
cannot be used. Display is a condition of use.

Display is not decision. Chapter 7 already placed validity in local
evaluation of public bytes. If products are absent, the protocol is
hard to use. If products are treated as authority, validity moves from
the node to a screen. Coexistence means: the surface should be usable;
authority remains in the node.

Formal documentation is a sensitive surface. What it says can be true:
the model contains machine-checked invariants. Once that sentence sits
on a product page, readers may supply a half-sentence that was not
written — “so the chain has been proved.” Chapter 8 does not issue that
half-sentence. Product copy should not issue it on Chapter 8’s behalf.

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

## 9.3 Three ways Chapter 8 is compressed on a product surface

A product surface compresses a boundary into a badge, a title, or a
short line. Each of the following compressions reads Chapter 8 as the
sentence it refused:

```
“selected invariants are proved in the model”
        compressed into
“the chain has been proved”
```

1. **Drop the word “model.”** A Coq-green badge on the home page or
   the download page is read as the binary, not the kernel model.
2. **Write finite vectors as universal equivalence.** Agreement at
   locked differential cases should not print as “C++ is proved equal
   to Coq.”
3. **Write independent validation as “do not run a node.”** Chapter 7
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
- It does not claim that the website or this paper is the protocol.
  The protocol defines the rules. `core/src/` realizes them.
- It does not write the formal page as equivalence already holding,
  or an experiment page as a refinement proof.
- It does not write visible mining as a source of validity.
- It does not write this chapter’s surface list as another set of
  consensus rules.

**A display remains a display. The protocol defines the rules. The node realizes them.** Security
model and threat boundaries follow in Chapter 10.

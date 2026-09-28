export const DOCUMENTS: Record<string, string> = {
  readme: `RIGOR.TXT


RIGOR

A permissionless proof-of-work blockchain.


It runs on computers.
It communicates through a network.
It follows deterministic rules.


SYSTEM

Consensus:
Proof of Work

Ledger:
UTXO

Hash:
OpuHash

Block Header:
80 bytes


NETWORK

Network:
RIGOR MAINNET

Explorer: explorer.rigorchain.com
Source: github.com/rigorchain/rigorchain


Open My Computer for node, wallet, miner, protocol, and source.
Open RIGOR BROWSER for live chain data.
`,

  consensus: `CONSENSUS.TXT

RIGOR consensus is Proof of Work on a UTXO ledger.

The active chain is the valid chain with the greatest cumulative
chainwork. Ties break deterministically.

Validity is the conjunction of header, OpuHash, transaction, mint,
supply, and witness canonicality predicates.

Protocol documents:
  RFC-020  Deterministic economics (RES destroy cost, ORU subsidy)
  RFC-018  RES-transfer ORU fee burn

This file is a description. It is not consensus authority.


ASSETS

ORU
  Fee and proof-of-work subsidy asset.
  Subsidy R(h) = 128 ORU for h >= 1.
  Scale: 1 ORU = 1e8 atoms.

RES
  Created only when a body transaction destroys ORU under
  consensus rules. RES is not a mining reward.
  Cap: 2^22 RES.


EQUIVALENCE

equivalence_claimed = false
documentation_consensus_authority = false

The production node is the operational consensus implementation.
`,

  opuhash: `OPUHASH.TXT

OpuHash is the live proof-of-work function.

  OpuHash(header) = SHA3-256("RGRPOW01" || header_80)
  valid           <=>  OpuHash(header) <= ExpandCompact(nBits)

SHA3-256 is FIPS PUB 202 (domain suffix 0x06).

Block identity is not OpuHash.

  BlockHash = SHA256d(header_80)

Merkle root, txid, and related commitments remain SHA256d.

Difficulty adjustment is ASERT (aserti3-2d).
  Spacing: 600 s
  Half-life: 43200 s
  Per-block clamp: +/- 12.5%

Source:
  rigor_kernel::GetRigorPowHash
  CheckProofOfWork
`,

  block: `BLOCK.TXT

A block is an 80-byte header plus an ordered transaction list.

Header (80 bytes) commits to:
  version
  previous block hash
  merkle root
  time
  nBits
  nonce

Proof-of-work validity uses OpuHash on the same 80-byte header.
Block identity uses SHA256d of the same header.

Coinbase scriptSig begins with the BIP34 height.

The first transaction is the coinbase. It may create the ORU subsidy.
It does not create RES.

See CONSENSUS.TXT and OPUHASH.TXT.
`,

  transaction: `TRANSACTION.TXT

RIGOR uses a UTXO ledger.

A transaction consumes previous outputs and creates new outputs.
Ordinary transfers cannot create ORU or RES.

Asset class is inferred from the output script.
There is no (asset, value, script) field on CTxOut.

ORU spend path:
  Witness v0 / 20-byte program

RES spend path:
  Witness v1 / 32-byte Ed25519 program

Convergence destroy:
  <20-byte HASH160> 0xbd
  Classified ORU. Not a spendable UTXO.

RES mint:
  Body transaction that destroys ORU at the confirming height
  and creates RES outputs under the RFC-020 predicate.

Conservation on non-coinbase transactions:
  inputs = outputs + claimable fee
`,

  network: `NETWORK.TXT

Network:
RIGOR MAINNET

  Chain id:     main
  P2P port:     8760
  RPC port:     8761
  Address HRP:  rg
  Address form: rg1q

Genesis:
  e2ce92cfef740f92e8cc84a8541d8e760c776c668afd9b61740b3a4315241468

Explorer:
  explorer.rigorchain.com
  Read-only indexer. Not a validation authority.

Do not expose JSON-RPC to the public Internet.
`,

  security: `SECURITY.TXT

UTXO conservation
  Ordinary transfers cannot create ORU or RES.

Script validation
  Restricted monetary script families.
  Classification is not issuance.
  There is no general-purpose script machine.

Proof-of-Work
  OpuHash = SHA3-256("RGRPOW01" || header_80)
  ASERT difficulty.

Authentication
  Ed25519 ownership of spendable outputs.

Formal verification
  Selected consensus properties are machine-checked in a Coq model.
  equivalence_claimed = false
  refinement_proved = false
  The model is not a proof that C++ matches Coq.

Reporting
  Do not open a public issue for consensus, funds, or node
  availability findings.
  Use GitHub private vulnerability reporting on rigorchain/rigorchain.

See SECURITY.md in the repository.
`,

  faq: `FAQ.TXT

Q: What is RIGOR?
A: A permissionless proof-of-work blockchain. UTXO ledger.
   Dual native assets: ORU and RES.

Q: What network is this desktop on?
A: RIGOR MAINNET.

Q: What is OpuHash?
A: SHA3-256("RGRPOW01" || 80-byte header). Used for PoW validity.
   Block identity remains SHA256d.

Q: What do miners receive?
A: ORU subsidy. RES is not a proof-of-work reward.

Q: How is RES created?
A: A body transaction destroys ORU under consensus rules
   (RFC-020). Pool policy cannot mint RES.

Q: Where is the explorer?
A: explorer.rigorchain.com  (read-only indexer)

Q: Where is the source?
A: github.com/rigorchain/rigorchain

Q: Does this desktop validate the chain?
A: No. It displays documents and read-only network data.
   Validate with a local node.
`,

  checksums: `CHECKSUMS.TXT

Official checksums are published with each GitHub Release.

  https://github.com/rigorchain/rigorchain/releases

Verify:

  sha256sum -c SHA256SUMS

Do not run a binary that does not match the published checksums.
`,

  release_notes: `RELEASE_NOTES.TXT

Release notes are published with each tagged release.

  https://github.com/rigorchain/rigorchain/releases

Network:
RIGOR MAINNET
`,
};

export const WHITEPAPER_PAGES: { title: string; body: string }[] = [
  {
    title: "RIGOR Protocol Whitepaper",
    body: `A Dual-Asset Proof-of-Work Protocol
with Convergence and Independent Verification

Document status: public protocol narrative (not consensus SSOT)

This paper describes purpose, consensus design, security
assumptions, and implementation limits. Live admit-or-reject
is defined by consensus code together with RFC-020 and RFC-018.


RIGOR =
    2 Native Assets (ORU, RES)
  + Closed monetary script templates
  + UTXO local validation
  + Ed25519 authentication
  + OpuHash (SHA3-256 / RGRPOW01) + ASERT
  + Consensus-defined RES issuance
  + Fee destroy (RFC-018)
  + Coq protocol-model verification
  + No global asset state


Without:
  token registry
  account ledger
  contract storage
  general-purpose VM
`,
  },
  {
    title: "Assets and work",
    body: `ORU
  Operational asset.
  Enters circulation through coinbase subsidy.
  Leaves circulation through Convergence destroys
  and the RFC-018 fee-destroy share.
  Subsidy: 128 ORU for height >= 1.

RES
  Scarce reserve.
  Minted in a body transaction that destroys ORU
  at the confirming height (RFC-020).
  Cap: 2^22 RES.
  Not a mining reward.

Work
  OpuHash(header) = SHA3-256("RGRPOW01" || header_80)
  valid <=> OpuHash(header) <= target
  Block identity: SHA256d(header_80)

Header
  80 bytes. Unchanged wire layout.
`,
  },
  {
    title: "Verification boundary",
    body: `equivalence_claimed = false
refinement_proved   = false

Formal verification proves properties of the protocol model.
It does not prove that the C++ implementation is a refinement
of that model.

Explorer is a read-only indexer.
This desktop is not a validator.

Full narrative:
  docs/whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md
  in github.com/rigorchain/rigorchain

Normative economics:
  docs/protocol/RFC-020.md
  docs/protocol/RFC-018.md
`,
  },
];

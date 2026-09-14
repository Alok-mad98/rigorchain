# RIGOR

Production dual-asset UTXO blockchain.

Display version is `RIGOR_RELEASE_VERSION` in `core/src/clientversion.h`.

```
equivalence_claimed = false
```

Formal verification covers properties of the protocol **model**. This
repository does not claim C++ equivalence.

Protocol specifications are normative SSOT.
Implementations realize the frozen protocol.

---

## 1. What is RIGOR

RIGOR is a dual-asset UTXO protocol. ORU is the utility asset. RES is
minted only by destroying ORU at `destroy_cost(h)`. Asset class is
inferred from `scriptPubKey`. There is no account model and no
administrative key on the consensus path.

Formal verification covers the protocol **model**, not C++ equivalence.

**Whitepaper:** [`docs/whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md`](docs/whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md)

Short companions (not a substitute for the paper):

| Paper | Question |
|-------|----------|
| [`docs/whitepaper/RIGOR_PROTOCOL_IDENTITY.md`](docs/whitepaper/RIGOR_PROTOCOL_IDENTITY.md) | What is RIGOR? |
| [`docs/whitepaper/RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md`](docs/whitepaper/RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md) | How does the mathematics run? |
| [`docs/whitepaper/RIGOR_FORMAL_VERIFICATION_DESIGN.md`](docs/whitepaper/RIGOR_FORMAL_VERIFICATION_DESIGN.md) | What is the model, and where does proof stop? |

Protocol SSOT: [`docs/protocol/RFC-020.md`](docs/protocol/RFC-020.md),
[`docs/protocol/RFC-018.md`](docs/protocol/RFC-018.md).

---

## 2. Protocol identity

| Field | Production value |
|-------|------------------|
| Chain name | `RIGOR` |
| Mainnet genesis | `f8aceb50d974f3157c7bc2dfffab28dee0c97d99e4e3ec931b84e06862fdd717` |
| Magic bytes | `ce f1 db fa` |
| P2P port | `8760` |
| RPC port | `8761` |
| Bech32 HRP | `rg` |
| Consensus lock | `RIGOR_CONSENSUS_LOCK_VERSION = 2` |

Lock files: `core/src/consensus/params.h`,
`core/src/consensus/rigor_consensus_lock.h`.

AlphaPlus / Alpha1 names are historical release channels, not the current
product. See [`CHANGELOG.md`](CHANGELOG.md).

Extracted facts:
[`docs/developer/CODE_SSOT_PROTOCOL_FACTS.md`](docs/developer/CODE_SSOT_PROTOCOL_FACTS.md).

---

## 3. Consensus overview

```
RIGOR =
    Dual Native Assets
  + UTXO Monetary Model
  + Deterministic Issuance
  + Deflationary Fee Burn
  + Probabilistic Convergence Selection
  + Adaptive Proof-of-Work (ASERT)
  + Cryptographic Ownership (Ed25519)
```

Consensus state is the UTXO set plus validation rules. Rebuildable indexes
are not monetary truth.

- Ordinary transfers cannot mint.
- ORU coinbase subsidy and coinbase RES mint are explicit exceptions.
- `ConnectBlock` is the admission path. Explorer display is observation.

There is **no Bitcoin Script VM**. Production spends are structured
predicates (`VerifyRigorPredicate`). Live markers: RETURN `0x6a`, RES mint
`0xbb`, CONVERGE `0xbd`.

Mathematics: [`docs/whitepaper/RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md`](docs/whitepaper/RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md).  
Opcode table: [`docs/specification/RIGOR_OPCODE_SPECIFICATION.md`](docs/specification/RIGOR_OPCODE_SPECIFICATION.md).

---

## 4. Native assets

| Asset | Role |
|-------|------|
| **ORU** | Transaction asset, fee asset, mining subsidy, Convergence burn |
| **RES** | Reserve asset. Deterministic issuance after Convergence. **Not a PoW reward.** |

PoW does not create RES.

ORU: 8 decimals (`COIN = 1e8`). Genesis subsidy is **0**. Height ≥ 1
pays **128 ORU** forever. No ORU cap.

RES: 8 decimals (`RES_COIN = 1e8`). Cap `RES_MAX_SUPPLY =
419430400000000` atoms (`2²²` RES). Minted only in a body transaction
that burns ORU (`OP_CONVERGE`) and creates Witness v1 RES in the same
transaction when `R · (2²⁵+h)² ≤ B << 40`.

RFC-018: half of the ORU fee on a RES-transfer is unclaimable. That burn
is not a UTXO and not a wallet balance.

---

## 5. Mining

**Node consensus PoW** is SHA3-256 over a domain-tagged header:

```
digest = SHA3-256("RGRPOW01" || header_80)
```

Difficulty is ASERT (aserti3-2d), mainnet half-life **12 hours** (43200 s),
anchor height 1, ±12.5% per-block clamp. Target spacing 10 minutes.

SOLO path: `getblocktemplate` → copy canonical coinbase bytes → nonce
search → `submitblock`. The node builds ORU subsidy and scriptSig.
RES mint is a body transaction, not a coinbase prize. Stratum is not a
SOLO URL.

The shipped `minerd` binary searches SHA3-256 `"RGRPOW01" || header_80`,
matching node `CheckProofOfWork`.

[`miner/README_MINER.md`](miner/README_MINER.md).

---

## 6. Wallet

`rigor-qt` / `rigor-wallet` / `rigor-cli`. Dual-asset
descriptor wallet (ORU + RES). Backup/restore with `backupwallet` /
`restorewallet`.

The wallet constructs transactions. Consensus decides Convergence
outcomes and RES mints. The wallet does not mine.

Guide: [`docs/user/Wallet.md`](docs/user/Wallet.md).

---

## 7. Explorer

Independent observation product. Not consensus. Not a ledger of record.

Display identity matches the node: genesis
`f8aceb50…d717`, HRP `rg`, RPC `8761`.

[`docs/explorer/EXPLORER_ARCHITECTURE.md`](docs/explorer/EXPLORER_ARCHITECTURE.md).

---

## 8. Pool

ORU-only mining coordination. GBT from `rigord` on RPC **8761**. The pool
product pays **ORU only**. It does not mint RES. Yaamp/yiimp paths under
`pool/` are quarantined leftovers.

[`pool/README.md`](pool/README.md).  
[`docs/pool/PRODUCTION_MINING_FLOW.md`](docs/pool/PRODUCTION_MINING_FLOW.md).

---

## 9. Security model

| Layer | Role |
|-------|------|
| UTXO conservation | Ordinary transfers cannot create ORU or RES |
| Script validation | Closed monetary script families; classification ≠ issuance |
| Proof-of-Work | OpuHash = SHA3-256 (`RGRPOW01`) with ASERT difficulty |
| Ed25519 | Cryptographic ownership of spendable outputs |
| Formal protocol properties | Model invariants — not C++ refinement |

Vulnerability reporting: [`SECURITY.md`](SECURITY.md). Do not expose
JSON-RPC to the public Internet.

Security audits: [`docs/security/`](docs/security/).

---

## 10. Verification model

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

Formal verification proves properties of the protocol model. It does not
prove that production C++ is a refinement of that model.

Operator map: [`docs/formal/VerificationStatus.md`](docs/formal/VerificationStatus.md).  
Reproduce guide: [`docs/security/HOW_TO_VERIFY_RIGOR.md`](docs/security/HOW_TO_VERIFY_RIGOR.md).

---

## 11. Build instructions

See [`docs/developer/Build.md`](docs/developer/Build.md) and `core/doc/build-*.md`.

```bash
cd core
./autogen.sh
./configure
make -j$(nproc)
```

Binaries: `rigord`, `rigor-cli`, `rigor-qt` (if Qt enabled), `rigor-wallet`.

Miner: `cd miner && ./autogen.sh && ./configure && make`.  
Explorer: `explorer/` (Node). Pool: `pool/pilot` (`cargo build --release`).

---

## 12. Release information

Current binaries: [GitHub Releases](https://github.com/rigorchain/rigorchain/releases).
Version history: [`CHANGELOG.md`](CHANGELOG.md).
A past tag’s notes (example): [`docs/release/RIGOR_V0.1.3_RELEASE_NOTES.md`](docs/release/RIGOR_V0.1.3_RELEASE_NOTES.md).

---

## Repository layout

| Path | Purpose |
|------|---------|
| `core/` | Full node, wallet, consensus |
| `docs/` | Active documentation (`docs/README.md`) |
| `archive/` | Historical RFC / design / formal records (not normative) |
| `explorer/`, `miner/`, `pool/`, `web/` | Product components |
| `verification/` | Public verification map |
| `formal_verification/` | Coq model, golden vectors, manifests |

Documentation classification: [`docs/README.md`](docs/README.md).

## License

MIT — see [LICENSE](LICENSE).

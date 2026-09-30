# Consensus Freeze Check Before Security Patches

**Recorded:** 2026-09-30, before any file in this hardening series was edited.
**Scope:** the six approved patches from `docs/security/BITCOIN_V25_TO_CURRENT_SECURITY_AUDIT.md`.
**Rule:** a patch that edits the files or the constants below is out of this series.

```
consensus_files_touched = false
```

## Frozen files

SHA-256 recorded before the series. A later check must reproduce these digests.

| Path | SHA-256 |
|------|---------|
| `core/src/validation.cpp` | `00bef539c1cfd9ea386e0475abbfe4be4bdedb83ac0fed03d7949dfeb9e5dca2` |
| `core/src/script/rigor_classify.cpp` | `a9c921a04c50a8fc029bde5d34abbc098f79df1a7c131dbdff8f33297953a02e` |
| `core/src/script/predicate_verify.cpp` | `5186a1d68f94075dd1753e465051c2b0b5509a2daa6037758a1cebbdcc9ba36b` |
| `core/src/consensus/tx_verify.cpp` | `d1c0536edbe870ca83f7dc51cf93bf1fa36ef79426680338d0fb98e4922cf930` |
| `core/src/pow.cpp` | `66f89914fd2b477f20e5bf6ce3ea6033169400ef8d5a8206d3ad1ce385585d8b` |
| `core/src/rigor_kernel/pow_digest.cpp` | `ad7244c8901184e42785feb740108d8243cc5dcd926df12a7cf2639be55193eb` |
| `core/src/rigor_kernel/pow_digest.h` | `d906bbc5cdd8ebd727c9beb5072d31863994ab1be488d94b09db60dc30e895e9` |
| `core/src/rigor_kernel/keccak.cpp` | `73826ad3773267854b8e593875a18f6f2f06cb734305e9192848bed81c5f2e92` |
| `core/src/rigor_kernel/keccak.h` | `f217e664a1822bb00f0d0a18ed2dedcf007865c34ee0e6aae7929cc828b5c6d6` |
| `core/src/consensus/rigor_consensus_lock.h` | `efd0777ab7ddaec70bd6553c57a6ba749942fe90dda9bb7e66d97ac676473c7f` |

`core/src/consensus/*` and `core/src/rigor_kernel/*` are in the freeze. The table lists the files that carry the constants below. The series must leave the rest of those directories byte-identical as well.

## Frozen rules confirmed in tree

| Rule | Where it lives | Value confirmed |
|------|----------------|-----------------|
| ORU subsidy | `ORU_FLAT_BLOCK_SUBSIDY` in `rigor_consensus_lock.h` | `128 * COIN` (12,800,000,000 tharns). No halving term. |
| RES max supply | `RES_MAX_SUPPLY` | `419430400000000` = `2^22 * RES_COIN` |
| RES creation | convergence burn path in `tx_verify.cpp` / issuance | RES is minted by destroying ORU. Coinbase does not create RES. |
| `OP_CONVERGE` | `RIGOR_MARKER_CONVERGE` | `0xbd` |
| Asset templates | classifier | Witness v0 is ORU. Witness v1 is RES. |
| Signature | `predicate_verify.cpp` | Ed25519 only. Domains `RGRORU01` and `RGRRES01`. |
| Proof of work | `rigor_kernel/pow_digest.h` | `SHA3-256("RGRPOW01" \|\| header_80)` |

## What this series is allowed to edit

| Patch | Files | Consensus file |
|-------|-------|----------------|
| Assumeutxo `hash_serialized_2` | `core/src/kernel/coinstats.cpp`, regtest pins, tests | none |
| Inbound onion whitelist | `core/src/net.cpp`, `core/src/net.h`, tests | none |
| Compact-block witness classification | `core/src/blockencodings.cpp`, tests | none. Nonce rule stays in `validation.cpp` and is not edited. |
| Attacker-controlled logs | logging implementation only. `validation.cpp` call sites stay. | none |
| Settings write | `core/src/util/settings.cpp`, tests | none |
| 32-bit resource caps | mempool and dbcache argument checks, tests | none. `CheckBlock` stays in the frozen `validation.cpp`. |

Invalid-chain log lines sit in `Chainstate::InvalidChainFound` inside `validation.cpp`. They are rate-limited by the logger, which already receives `__func__`, so that file is not edited.

## Result

The freeze set matches the rules above. Implementation may proceed only inside the allowed files.

## Post-patch recheck

**Rechecked:** 2026-09-30, after the six patches and before commit.

Every digest in the table above was recomputed and still matches. `git diff` on `core/src/validation.cpp`, `core/src/consensus/`, `core/src/script/rigor_classify.cpp`, `core/src/script/predicate_verify.cpp`, `core/src/pow.cpp`, and `core/src/rigor_kernel/` is empty.

```
consensus_files_touched = false
```

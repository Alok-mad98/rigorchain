# Kernel Cone

The **kernel cone** is the only object this package verifies.

**Public claim (only):** Formally verified critical consensus invariants
(Coq consensus kernel only).

The cone is frozen as of P0-111. Expanding it to the node, network,
wallet, or storage engine requires a new task and new claims discipline.

## In the cone

| Domain | What is in scope |
|---|---|
| UTXO rules | Structural `valid_tx`, apply/consume/create, no double-spend in the model |
| ORU conservation | `ORU_in = ORU_out + fee`; `new = old + subsidy + fees - burn` |
| RES issuance schedule | Coinbase mint bounded by `W(h)`; draw eligibility is a hypothesis |
| Script recognizers | Total, deterministic, authority-free classification; disjoint branches |
| PoW target validation | Digest-as-integer ≤ target; compact decode is a lower bound |
| ASERT calculation | Deterministic integer model; `Z.quot` vs `Z.div` split |

Sources: `Supply/`, `Script/`, `Pow/`, `proofs/`, `Kernel/`.

## Not in the cone

| Area | Reason |
|---|---|
| P2P networking | No network model |
| Mempool | Policy / relay are out of scope |
| Block propagation | No peer or inventory protocol |
| Wallet signing | Wallet format and keys are out of scope |
| Storage engine | Database format is unchanged and unverified here |
| RPC | No RPC surface in this cone |
| C++ `ConnectBlock` | No implementation refinement |
| Crypto primitive implementations | Abstract `H`; no sha3/keccak/ed25519 import |

## Freeze statement

`KERNEL_CONE_FROZEN=TRUE` means: the verification object is this cone
and only this cone. It does not grow by implication to the full node.

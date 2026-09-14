# Lottery Coq cone — archived under RFC-019

**Do not delete these files.** Phase 5 of the RES issuance refactor
(RES issuance mechanism refactor execution plan) classifies them as **class C — lottery-internal properties**.
They are moved here with `git mv` so history is intact and so they cannot
be cited as live theorems. They are no longer in `_CoqProject` /
`_CoqProject.protocol-core`.

RFC-019 replaced the CONVERGENCE draw (σ, eligibility, 32-block window,
one winner per block, coinbase RES mint) with a deterministic unit price
RATE(k). RFC-020 then replaced RATE(k) with the continuous pole predicate
`R · (2²⁵+h)² ≤ B << 40`. A mint is an ordinary body transaction. There is
no draw to specify, so proofs about the draw have no referent and cannot
be rewritten to the shipping rule. They stay here as the retired cone.

```
equivalence_claimed = false
```

These proofs are not wrong about the old mechanism. They are about a
mechanism that is no longer consensus.

## What is here

| Tree | Why C-class |
|------|-------------|
| `coq/Phase5Equivalence/` | Catalog / freeze paperwork of draw ↔ runtime equivalence. Not the citable cone (`_CoqProject.protocol-core`). |
| `coq/Phase4FConsensus/` | Frozen-C++ oracle and draw-trace observation. |
| `coq/Phase4GRefinement/` | Draw-hash / eligibility refinement catalogs. |
| `coq/ReplayReorgCore/` | Entire directory (8,285 lines): mint-from-draw, windowed reorg, σ-adjacent replay. |
| `coq/DrawCandidateRoot.v` | Merkle root over the draw candidate set. |
| `coq/WindowExclusion.v` | 32-block window exclusion. |
| `coq/Kernel/KernelSigma.v` | σ as a kernel authority. |
| `coq/Kernel/KernelSelection.v` | Winner selection. |
| `coq/Kernel/KernelEligibility.v` | `H_i < T · f_i`. |
| `rigor_v01/WindowGeneric.v` | Parameterised draw window. |
| `rigor_v01/WindowTarget.v` | Windowed probability target. |
| `rigor_v01/WindowSpecialization.v` | W=32 specialisation. |
| `rigor_v01/W32ReorgStability.v` | Reorg stability of the 32-block window. |

## What is not here (on purpose)

- Category A of `rigor_kernel` (UTXO, ORU, PoW/ASERT, script, codec). That
  cone is the refactor_guard L3 / L8 baseline and must stay proved.
- B-class modules (`RESIssuance.v`, `RESValidator.v`, `SupplySchedule.v`,
  `ResSchedule.v`, `CanonicalIssuance.v`, `ReorgIssuanceBound.v`). Rewritten
  to RFC-020 pole predicate / flat 128 ORU / closed-form `2²²` cap. Live
  under `_CoqProject` / `_CoqProject.protocol-core`. They are not this cone.

## Dependents parked until B rewrite

These live files `Require` something in this archive. They are **not**
deleted. They are dropped from `_CoqProject` until Phase 5 step 3 rewrites
them or they lose the Require. Leaving them in the project would make
"the remaining engineering still builds" false for a mechanical reason.

- `coq/ScriptMintWitness.v`
- `coq/DrawBridgeProgress.v`
- `coq/Kernel/GoldenHarness.v`
- `coq/Kernel/KernelClosure.v`
- `coq/Kernel/KernelRoot.v`
- `coq/Kernel/NegativeCoreHarness.v`
- `coq/ConnectBlock/ConnectBlockSupply.v`
- `coq/Proofs/Correctness.v` (Requires `KernelEligibility`)
- `coq/ConsensusKernelBridge.v`
- `coq/ConsensusKernelAlignment.v`
- `coq/ConsensusKernelRefinement.v`
- `coq/ExtractConsensusKernel.v`
- `coq/ExtractConsensusKernelC.v`
- `coq/SupplyDeltaBridge.v`
- `coq/ConnectBlockInputPreparation.v`
- `coq/KernelResultApplication.v`
- `coq/ConnectBlockKernelBoundary.v`
- `coq/ConsensusSurfaceClosure.v`
- `coq/UTXOStateTransitionRefinement.v`
- `rigor_v01/GenesisZeroIssuance.v`
- `rigor_v01/ConnectBlockSupply.v`

`CanonicalIssuance.v`, `ReorgIssuanceBound.v`, and
`MintSupplyConservation.v` no longer Require window / draw modules.
`MintSupplyConservation.v` is the RFC-020 conservation slice
(`apply_delta`); it is not a pole-predicate proof.

ABI files (`consensus_kernel_c_abi_vectors.json`, C++
`issuance_bridge.cpp` / `kernel_types.h`) are **not** changed in this
commit. The plan forbids touching only one side of the ABI. They move
together with the B-class rewrite (Phase 5 steps 3 and 7).

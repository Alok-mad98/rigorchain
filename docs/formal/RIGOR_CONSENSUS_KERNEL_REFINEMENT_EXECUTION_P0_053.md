# Consensus Kernel Execution Refinement Boundary (P0-053)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-REFINEMENT-EXECUTION-P0-053`  
**Date:** 2026-08-30  
**Nature:** Scoped execution comparison on a finite vector domain. Not a ∀-proof.

```
CONSENSUS_KERNEL_REFINEMENT_EXECUTION_STATUS
VECTOR_DOMAIN              = PASS
EXTRACTION_EXECUTION       = PASS
CPP_EXECUTION              = PASS
BRIDGE_COMPARE             = PASS
DUPLICATE_KERNEL           = PASS
COQ_ADMITTED               = 0
COQ_AXIOMS                 = 0
COQ_PARAMETERS             = 0
REFINEMENT_PROVED          = false
EQUIVALENCE_PROVED         = false
PRODUCTION_CONSENSUS_MODIFIED = NO
PROTOCOL_PARAMETERS_MODIFIED  = NO
ISSUANCE_CHANGED           = NO
RES_CHANGED                = NO
POW_CHANGED                = NO
```

ALIGNMENT ≠ EXTRACTION ≠ EXECUTION AGREEMENT ON V ≠ REFINEMENT ≠ EQUIVALENCE.

---

## REFINEMENT_SCOPE

Finite canonical vector domain `V` recorded in
`formal_verification/consensus_kernel_refinement_vectors.json`.

Compared surfaces:

| Surface | Coq extracted | C++ production Kernel |
|---|---|---|
| issuance | `issuance_epoch` / `oru_subsidy` / `get_res_mint_reward` / `res_issued_at_height` | `ComputeRigorIssuance` |
| maturity | `coinbase_mature` | `nCoinbaseMaturity` / `COINBASE_MATURITY` |
| classify | `AssetClassification.classify` | `ClassifyRigorTxout` → `ClassifyRigorAsset` |
| draw | `resolve_convergence_draw` / `select_winner` | `ResolveConvergenceDraw(F, T)` |
| supply ledger | `ledger_oru_delta` | `RigorSupplyDelta.oru_circulating_delta` |
| supply apply | `apply_delta` | `RigorSupplyDelta` (layer-aligned, not field-identical) |

`GetConvergenceDrawSetForBlock` is state assembly. It is not Kernel and is
not in this comparison.

---

## NOT_PROVEN

- `∀ height`
- `∀ byte`
- `∀ state`

This TASK does not prove mathematical refinement of C++ by Coq, and does
not prove observational equivalence.

Do not set `REFINEMENT_PROVED=true`.  
Do not set `EQUIVALENCE_PROVED=true`.

---

## CURRENT_AUTHORITY

The C++ production Consensus Kernel (`ConnectBlock` → handwritten C++).

Production consensus, issuance, RES schedule, ORU halving, maturity,
draw formula, classification semantics, supply formula, genesis, PoW,
ASERT, miner, pool, wallet, and P2P are unchanged.

---

## EXTRACTED_KERNEL

Verification oracle only. The extracted OCaml CLI is not linked into
`ConnectBlock`. There is no second consensus implementation.

Adapter files convert I/O and call the production or extracted Kernel.
They do not recode formulas.

Forbidden duplicate-kernel names (absent):

- `coq_formula_copy.cpp`
- `extracted_formula_copy.cpp`
- `adapter_formula.cpp`

---

## Domain V (pins)

### Issuance

Heights: `0, 1, 99, 100, 101, 209999, 210000, 210001, 419999, 420000`.

`epoch(h) = floor(h / 210000)`.

Height 0:

- schedule RES = `40000000`
- actual RES issuance = `0`

`schedule != issuance` is mandatory.

### Maturity

Source heights `0, 1, 99, 100, 101`. ORU maturity = RES maturity = 100.
Genesis produces no maturity source.

### Classification

Canonical `scriptPubKey` bytes: ORU, RES, invalid, malformed.
Byte-for-byte asset-tag equality.

### Draw

Finite candidate vectors: empty `F`, single candidate, equal weight
(order swap / tie-break identity), different σ, `T = 0`.

No constructed SHA256 collision is claimed. Equal-σ different-txid is
therefore not a fabricated hash collision vector; order independence
checks that both kernels name the same winner when `F` is reordered.

### Supply

ORU only, RES only, fee, burn, converge, reject.

Ledger layer (`subsidy − converge`) is not the Coq circulating layer
(`subsidy − converge − burn`). The layers stay distinct.

---

## Executable bridge

```
vector.json
    → extracted Coq CLI   (formal_verification/extracted/consensus_kernel_cli)
    → C++ refinement CLI  (calls production Kernel only)
    → JSON canonical compare
    → PASS / FAIL
```

Gate: `formal_verification/run_consensus_kernel_refinement_gate.sh`  
(wrapper: `run_consensus_kernel_refinement_gate.sh`)

On FAIL: STOP. Vectors are not auto-corrected.

---

## FINAL_VERDICT

`CONSENSUS_KERNEL_REFINEMENT_EXECUTION_CLOSED`

Meaning: extracted execution and C++ Kernel execution agree on the
finite domain `V`.

Not meaning: Coq refines C++. Not meaning: the kernels are equivalent.
Not meaning: production now runs OCaml.

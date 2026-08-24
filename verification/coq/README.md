# Coq — public map

Canonical corpus: [`formal_verification/coq/`](../../formal_verification/coq/) (`_CoqProject`: 212 modules).

| Public folder | Focus | Primary sources |
|---------------|-------|-----------------|
| [protocol-core/](./protocol-core/) | Dual-pool, UTXO replay, script shape, opaque boundaries | `ReplayReorgCore/*`, `ScriptMintWitness.v`, `ConnectBlock*` |
| [monetary-policy/](./monetary-policy/) | RES schedule / cap / H-1; ORU subsidy bound | `ResSchedule.v`, `ResMintObligation.v`, `ConnectBlockMint.v` |
| [convergence/](./convergence/) | σ, Convergence_Root, DCR, selection, window | `Selection.v`, `OrderIndependence.v`, `ConvergeRoot.v`, `DrawCandidateRoot.v`, `WindowExclusion.v`, `Sigma*.v` |

Load-bearing theorem index: [`docs/security/FORMAL_COQ_LOAD_BEARING_THEOREMS.md`](../../docs/security/FORMAL_COQ_LOAD_BEARING_THEOREMS.md).  
Full name inventory: [`docs/security/FORMAL_THEOREM_INVENTORY.json`](../../docs/security/FORMAL_THEOREM_INVENTORY.json).

```
equivalence_claimed = false
```

Theorems are about the **model**. They do not prove shipping `core/src/**` equals the model.

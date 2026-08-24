# Python executable specification

**Canonical SSOT:** [`formal_verification/spec/rigor_consensus_spec.py`](../../formal_verification/spec/rigor_consensus_spec.py)

Supporting modules under `formal_verification/spec/` (e.g. `rigor_uint512.py`).

| Role | Notes |
|------|-------|
| Draw / σ / roots / schedule | Normative byte semantics for the differential gate |
| Dual-pool / mint-shape / cap predicates | Shared with NegativeCore / EdgeCross vector oracles |
| Authority | When docs conflict with this file for *byte* behavior, **the code wins** |

```
equivalence_claimed = false
```

This is Layer 1 of the three-layer stack — not a proof that C++ matches it universally.

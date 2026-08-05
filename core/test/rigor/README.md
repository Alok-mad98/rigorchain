# RIGOR constitution test suite

This suite validates RIGOR consensus invariants directly.

Design rules:

- Tests validate invariants, not implementation internals.
- Each file maps to one invariant layer.
- Expectations are RIGOR-specific (not blindly inherited from legacy Bitcoin behavior).

Files:

- `rigor_model.py`
- `test_script_assets.py`
- `test_res_mint_binding.py`
- `test_commitment.py`
- `test_chain_consistency.py`
- `test_invalid_blocks_rigor.py`
- `test_forge_winner.py`
- `test_required_scenarios.py`

Run:

```bash
cd /path/to/ring/core/test/rigor
python3 -m unittest discover -v
```

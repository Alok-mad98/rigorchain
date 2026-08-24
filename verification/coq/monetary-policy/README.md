# Coq — monetary policy

**Canonical paths:**

| Topic | File |
|-------|------|
| RES schedule `W(k)`, finite sum, pins, cost floor | `formal_verification/coq/ResSchedule.v` |
| Winner-must-mint / H-1 / cap dual-insurance | `formal_verification/coq/ReplayReorgCore/ResMintObligation.v` |
| ORU subsidy schedule + connect mint amounts | `formal_verification/coq/ReplayReorgCore/ConnectBlockMint.v` |
| Mint cost `E=246` ⇒ `C=1024` | `formal_verification/coq/Selection.v` |

**Example theorems:** `max_issuance_within_cap`, `schedule_sum_value`, `reward_nonincreasing`, `cost_floor_nondecreasing_along_schedule`, `winner_must_mint`, `oru_supply_bounded_by_block_count`.

**Parity (not Coq):** `formal_verification/tests/test_rfc017_schedule_parity.py` (`k = 0..251`).

**Boundary:** `cm_draw_won` is abstract — not Selection winner / owner / PCORB.

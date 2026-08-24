# RFC-018 golden vectors

Finite pins for RES-transfer ORU fee burn. Independent of the 422/215 draw-kernel lock.

| File | What it pins |
|------|----------------|
| [`rfc018_fee_burn.json`](rfc018_fee_burn.json) | Share function, tx class, mixed-block circulating delta, replay/reorg, RES isolation, six-decimal display |

Regenerate by importing `golden_vectors()` from `formal_verification/python-spec/rfc018_fee_burn.py`. The parity test fails if this file drifts.

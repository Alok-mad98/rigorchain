"""Supply-layer semantic golden / model checks.

Does not call coqc. Does not claim C++ equivalence.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
PYSPEC = REPO / "formal_verification" / "python-spec"
GOLDEN = REPO / "formal_verification" / "golden-vectors" / "supply_layer_semantics.json"
sys.path.insert(0, str(PYSPEC))

from rfc018_fee_burn import (  # noqa: E402
    calculate_fee_burn,
    format_res_amount,
    TX_FEE_RES_TRANSFER,
    RES_COIN,
)
from supply_layer import (  # noqa: E402
    circulating_from_indexed_rows,
    circulating_spendable_oru,
    cvg2_contributes_to_circulating,
    fee_burn_contributes_to_circulating,
    format_res_amount as sl_format_res,
    golden_vectors,
    rfc002_ledger_oru,
    wallet_balance,
)


def fail(msg: str) -> None:
    print(f"FAIL: {msg}", file=sys.stderr)
    sys.exit(1)


def main() -> None:
    vec = golden_vectors()
    if vec["equivalence_claimed"] is not False:
        fail("equivalence_claimed must be false")
    if RES_COIN != 1_000_000 or RES_COIN == 100:
        fail("RES_COIN lock")

    fb = vec["rfc018_fee_burn"][0]
    if calculate_fee_burn(fb["res_transfer_fee"], TX_FEE_RES_TRANSFER) != fb["burn"]:
        fail("RFC-018 burn share")
    if fee_burn_contributes_to_circulating(fb["burn"]) != 0:
        fail("fee burn entered circulating")

    cvg = vec["cvg2_locked_oru"][0]
    if cvg2_contributes_to_circulating(cvg["locked_atoms"]) != 0:
        fail("CVG2 entered circulating")

    for row in vec["res_six_decimal_display"]:
        got = sl_format_res(row["atoms"])
        if got != row["display"] or format_res_amount(row["atoms"]) != row["display"]:
            fail(f"RES display {row['id']}: {got} != {row['display']}")

    circ = vec["circulating_supply"][0]
    ledger = rfc002_ledger_oru(circ["issued"], circ["converge_destroyed"])
    spendable = circulating_spendable_oru(
        circ["issued"], circ["converge_destroyed"], circ["fee_burn"], circ["underclaim"]
    )
    if ledger != circ["ledger_rfc002"]:
        fail("RFC-002 ledger")
    if spendable != circ["circulating_spendable"]:
        fail("circulating spendable")
    if spendable + cvg["locked_atoms"] != circ["wrong_if_cvg2_counted_as_circulating"]:
        fail("CVG2 overcount identity")
    if circ["indexed_rows_circulating"] != 100_000_000:
        fail("indexed circulating rows")

    wal = vec["wallet_spendable"][0]
    if wallet_balance(wal["coins"]) != wal["balance"]:
        fail("wallet spendable UTXO model")

    if not GOLDEN.is_file():
        fail(f"missing golden {GOLDEN}")
    disk = json.loads(GOLDEN.read_text())
    if disk != vec:
        fail("golden file mismatch vs supply_layer.golden_vectors()")

    print("PASS supply-layer semantics (model; equivalence_claimed=false)")


if __name__ == "__main__":
    main()

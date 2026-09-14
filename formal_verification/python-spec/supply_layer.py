"""Supply-layer semantic model (observational).

Canonical quantities (must not be collapsed):

- total issued ORU
- convergence destroyed ORU
- RFC-018 fee-burn destroyed ORU
- circulating spendable ORU

Does not retarget consensus. ``equivalence_claimed = False``.
"""

from __future__ import annotations

from typing import Any, Dict, List, Mapping, Sequence

ORU_COIN = 100_000_000
RES_COIN = 100_000_000
RES_DISPLAY_DECIMALS = 8
RES_ATOMIC_UNIT = 1

CVG2_V1_SCRIPT_PREFIX_HEX = "512043564732"


def rfc002_ledger_oru(issued: int, converge_destroyed: int) -> int:
    return issued - converge_destroyed


def circulating_spendable_oru(
    issued: int,
    converge_destroyed: int,
    fee_burn: int,
    underclaim: int = 0,
) -> int:
    return issued - converge_destroyed - fee_burn - underclaim


def cvg2_contributes_to_circulating(_locked_atoms: int) -> int:
    return 0


def fee_burn_contributes_to_circulating(_burn_atoms: int) -> int:
    return 0


def is_cvg2_script_hex(script_pubkey_hex: str) -> bool:
    return script_pubkey_hex.strip().lower().startswith(CVG2_V1_SCRIPT_PREFIX_HEX)


def circulating_from_indexed_rows(rows: Sequence[Mapping[str, Any]]) -> int:
    total = 0
    for row in rows:
        if row.get("spent"):
            continue
        if is_cvg2_script_hex(str(row.get("script_pubkey", ""))):
            continue
        total += int(row["amount_atoms"])
    return total


def format_res_amount(atoms: int) -> str:
    neg = atoms < 0
    m = -atoms if neg else atoms
    q, r = divmod(m, RES_COIN)
    sign = "-" if neg else ""
    return f"{sign}{q}.{r:0{RES_DISPLAY_DECIMALS}d}"


def wallet_coin_spendable(coin: Mapping[str, Any]) -> bool:
    return bool(
        coin.get("owned")
        and coin.get("mature")
        and coin.get("trusted")
        and not coin.get("unspendable")
    )


def wallet_balance(coins: Sequence[Mapping[str, Any]]) -> int:
    return sum(int(c["value"]) for c in coins if wallet_coin_spendable(c))


def golden_vectors() -> Dict[str, Any]:
    issued = 10 * ORU_COIN
    converge = 1 * ORU_COIN
    fee_burn = ORU_COIN // 2
    underclaim = 0
    ledger = rfc002_ledger_oru(issued, converge)
    circulating = circulating_spendable_oru(issued, converge, fee_burn, underclaim)
    cvg2_locked = 3 * ORU_COIN

    spendable_script = "001409ba7bdc9a1c2f6114431e483f26c1de0271893c"
    cvg2_script = CVG2_V1_SCRIPT_PREFIX_HEX + "00" * 28
    rows = [
        {"spent": False, "script_pubkey": spendable_script, "amount_atoms": 100_000_000},
        {"spent": False, "script_pubkey": cvg2_script, "amount_atoms": cvg2_locked},
        {"spent": True, "script_pubkey": spendable_script, "amount_atoms": 200_000_000},
    ]

    return {
        "task": "TASK-RIGOR-SUPPLY-LAYER-SEMANTIC-CONSISTENCY-P1-001",
        "equivalence_claimed": False,
        "refinement_proved": False,
        "hash_kernel_shared_binary_with_cpp": False,
        "rfc018_fee_burn": [
            {
                "id": "FB-SEM-000",
                "res_transfer_fee": 6000,
                "burn": 3000,
                "contributes_to_circulating": fee_burn_contributes_to_circulating(3000),
            }
        ],
        "cvg2_locked_oru": [
            {
                "id": "CVG2-000",
                "locked_atoms": cvg2_locked,
                "contributes_to_circulating": cvg2_contributes_to_circulating(cvg2_locked),
            }
        ],
        "res_eight_decimal_display": [
            {"id": "RES-DISP-0", "atoms": 0, "display": "0.00000000"},
            {"id": "RES-DISP-1", "atoms": RES_COIN, "display": "1.00000000"},
            {"id": "RES-DISP-TENTH", "atoms": RES_COIN // 10, "display": "0.10000000"},
            {"id": "RES-DISP-ATOM", "atoms": RES_ATOMIC_UNIT, "display": "0.00000001"},
            {"id": "RES-DISP-W0", "atoms": 40 * RES_COIN, "display": "40.00000000"},
        ],
        "circulating_supply": [
            {
                "id": "CIRC-000",
                "issued": issued,
                "converge_destroyed": converge,
                "fee_burn": fee_burn,
                "underclaim": underclaim,
                "ledger_rfc002": ledger,
                "circulating_spendable": circulating,
                "wrong_if_cvg2_counted_as_circulating": circulating + cvg2_locked,
                "indexed_rows_circulating": circulating_from_indexed_rows(rows),
            }
        ],
        "wallet_spendable": [
            {
                "id": "WAL-000",
                "coins": [
                    {
                        "owned": True,
                        "mature": True,
                        "trusted": True,
                        "unspendable": False,
                        "value": 5 * ORU_COIN,
                    },
                    {
                        "owned": True,
                        "mature": True,
                        "trusted": True,
                        "unspendable": True,
                        "value": cvg2_locked,
                    },
                ],
                "balance": 5 * ORU_COIN,
            }
        ],
    }


assert RES_COIN == 100_000_000
assert RES_COIN != 100
assert RES_COIN != 1_000_000
assert format_res_amount(RES_COIN) == "1.00000000"
assert cvg2_contributes_to_circulating(ORU_COIN) == 0
assert fee_burn_contributes_to_circulating(500) == 0
assert circulating_spendable_oru(1000, 100, 50, 0) == 850
assert rfc002_ledger_oru(1000, 100) == 900

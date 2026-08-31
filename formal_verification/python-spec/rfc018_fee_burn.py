"""RFC-018 RES-transfer ORU fee burn — Layer-1 executable spec.

Mirrors ``Consensus::OruResTransferFeeBurnShare`` in
``core/src/consensus/rigor_consensus_lock.h`` and
``formal_verification/coq/fee-burn/``.

Fees are ORU-denominated. The burn share never mints, burns, or recaps RES.
Circulating ORU change for a transfer-only block is:

    delta_oru = coinbase_subsidy - destroyed_fee

The C++ ledger scalar ``m_chain_oru_total`` remains RFC-002
(``S' = S + subsidy - converge``) and does not subtract fee burns.

``equivalence_claimed = False``.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable, List, Optional, Sequence, Tuple

ORU_RES_TRANSFER_FEE_BURN_NUM = 1
ORU_RES_TRANSFER_FEE_BURN_DEN = 2

ORU_COIN = 100_000_000
MAX_MONEY = 10_000_000_000 * ORU_COIN  # matches core/src/consensus/amount.h; not an ORU supply cap

RES_ATOMIC_UNIT = 1
RES_COIN = 1_000_000
RES_DISPLAY_DECIMALS = 6
RES_MAX_SUPPLY = 21_000_000 * RES_COIN

TX_FEE_ORU_ONLY = "oru_only"
TX_FEE_RES_TRANSFER = "res_transfer"
TX_FEE_MINT = "mint"
TX_FEE_CONVERGE = "converge"

RES_TRANSFER_CLASSES = frozenset({TX_FEE_RES_TRANSFER})


def oru_res_transfer_fee_burn_share(fee: int) -> int:
    """Floor share of an ORU fee that coinbase must not claim."""
    if fee <= 0:
        return 0
    return (fee * ORU_RES_TRANSFER_FEE_BURN_NUM) // ORU_RES_TRANSFER_FEE_BURN_DEN


def is_res_transfer(tx_class: str) -> bool:
    return tx_class == TX_FEE_RES_TRANSFER


def calculate_fee_burn(fee: int, tx_class: str) -> int:
    if not is_res_transfer(tx_class):
        return 0
    return oru_res_transfer_fee_burn_share(fee)


def calculate_miner_fee(fee: int, tx_class: str) -> int:
    return fee - calculate_fee_burn(fee, tx_class)


def coinbase_oru_cap(fees_oru: int, burn_acc: int, subsidy: int) -> int:
    """ConnectBlock / CreateNewBlock claimable ORU cap."""
    return (fees_oru - burn_acc) + subsidy


def circulating_oru_delta(subsidy: int, burn: int, converge: int = 0) -> int:
    """UTXO-level ORU change. Ledger scalar omits ``burn`` (RFC-018 §5)."""
    return subsidy - converge - burn


def ledger_oru_delta(subsidy: int, converge: int = 0) -> int:
    """RFC-002 ``m_chain_oru_total`` step: subsidy − converge."""
    return subsidy - converge


def format_res_amount(atoms: int) -> str:
    """Six-decimal RES display matching C++ ``FormatResAmount``.

    1 RES → ``1.000000``; 0.1 RES → ``0.100000``.
    """
    neg = atoms < 0
    if atoms == -(1 << 63):
        m = 1 << 63
    else:
        m = -atoms if neg else atoms
    q, r = divmod(m, RES_COIN)
    sign = "-" if neg else ""
    return f"{sign}{q}.{r:0{RES_DISPLAY_DECIMALS}d}"


@dataclass(frozen=True)
class FeeTx:
    tx_class: str
    fee: int


@dataclass(frozen=True)
class FeeBurnState:
    oru: int
    res: int
    burn: int = 0
    miner: int = 0


def apply_one(state: FeeBurnState, tx: FeeTx) -> Optional[FeeBurnState]:
    if tx.fee < 0:
        return None
    b = calculate_fee_burn(tx.fee, tx.tx_class)
    m = calculate_miner_fee(tx.fee, tx.tx_class)
    return FeeBurnState(
        oru=state.oru - b,
        res=state.res,
        burn=state.burn + b,
        miner=state.miner + m,
    )


def apply_txs(state: FeeBurnState, txs: Sequence[FeeTx]) -> Optional[FeeBurnState]:
    cur = state
    for tx in txs:
        nxt = apply_one(cur, tx)
        if nxt is None:
            return None
        cur = nxt
    return cur


def txs_burn(txs: Iterable[FeeTx]) -> int:
    return sum(calculate_fee_burn(tx.fee, tx.tx_class) for tx in txs)


def apply_block_circulating(
    state: FeeBurnState,
    txs: Sequence[FeeTx],
    subsidy: int,
    converge: int = 0,
) -> Optional[FeeBurnState]:
    after = apply_txs(state, txs)
    if after is None:
        return None
    return FeeBurnState(
        oru=after.oru + subsidy - converge,
        res=after.res,
        burn=after.burn,
        miner=after.miner,
    )


def rollback(state: FeeBurnState, snap: FeeBurnState) -> FeeBurnState:
    """Restore the pre-block snapshot (DisconnectBlock analogue)."""
    del state
    return snap


# ---------------------------------------------------------------------------
# Golden pins (must match Coq fee-burn/Accounting.v + Precision.v)
# ---------------------------------------------------------------------------

COQ_BURN_PINS: List[Tuple[int, int]] = [
    (0, 0),
    (1, 0),
    (2, 1),
    (3, 1),
    (7, 3),
    (1000, 500),
    (ORU_COIN, ORU_COIN // 2),
    (125 * ORU_COIN, (125 * ORU_COIN) // 2),
    (MAX_MONEY, MAX_MONEY // 2),
    (MAX_MONEY - 1, (MAX_MONEY - 1) // 2),
]

COQ_CLASS_PINS: List[Tuple[int, str, int, int]] = [
    (1000, TX_FEE_ORU_ONLY, 0, 1000),
    (1000, TX_FEE_RES_TRANSFER, 500, 500),
    (1000, TX_FEE_MINT, 0, 1000),
    (1000, TX_FEE_CONVERGE, 0, 1000),
]

COQ_DISPLAY_PINS: List[Tuple[int, str]] = [
    (0, "0.000000"),
    (RES_COIN, "1.000000"),
    (RES_COIN // 10, "0.100000"),
    (RES_ATOMIC_UNIT, "0.000001"),
]


def golden_vectors() -> dict:
    """Finite RFC-018 vectors for Coq / Python / C++ parity."""
    burn_rows = []
    for fee, burn in COQ_BURN_PINS:
        burn_rows.append(
            {
                "id": f"FB-SHARE-{fee}",
                "fee": fee,
                "class": TX_FEE_RES_TRANSFER,
                "burn": burn,
                "miner": fee - burn,
            }
        )
    class_rows = []
    for fee, cls, burn, miner in COQ_CLASS_PINS:
        class_rows.append(
            {
                "id": f"FB-CLASS-{cls}-{fee}",
                "fee": fee,
                "class": cls,
                "burn": burn,
                "miner": miner,
            }
        )
    display_rows = []
    for atoms, text in COQ_DISPLAY_PINS:
        display_rows.append(
            {
                "id": f"FB-DISP-{atoms}",
                "atoms": atoms,
                "display": text,
            }
        )

    oru_only = 4000
    res_xfer = 6000
    mixed_burn = calculate_fee_burn(res_xfer, TX_FEE_RES_TRANSFER)
    mixed_fees = oru_only + res_xfer
    subsidy = 125 * ORU_COIN
    pre = FeeBurnState(oru=10 * ORU_COIN, res=40 * RES_COIN)
    txs = [
        FeeTx(TX_FEE_ORU_ONLY, oru_only),
        FeeTx(TX_FEE_RES_TRANSFER, res_xfer),
    ]
    post = apply_block_circulating(pre, txs, subsidy, 0)
    assert post is not None
    replay = apply_block_circulating(pre, txs, subsidy, 0)
    restored = rollback(post, pre)

    return {
        "rfc": "RFC-018",
        "equivalence_claimed": False,
        "refinement_proved": False,
        "hash_kernel_shared_binary_with_cpp": False,
        "burn_share": {
            "num": ORU_RES_TRANSFER_FEE_BURN_NUM,
            "den": ORU_RES_TRANSFER_FEE_BURN_DEN,
        },
        "res_coin": RES_COIN,
        "res_atomic_unit": RES_ATOMIC_UNIT,
        "res_display_decimals": RES_DISPLAY_DECIMALS,
        "share": burn_rows,
        "class": class_rows,
        "display": display_rows,
        "mixed_block": {
            "id": "FB-MIXED-000",
            "oru_only_fee": oru_only,
            "res_transfer_fee": res_xfer,
            "burn": mixed_burn,
            "fees": mixed_fees,
            "claimable_without_subsidy": mixed_fees - mixed_burn,
            "subsidy": subsidy,
            "circulating_delta": circulating_oru_delta(subsidy, mixed_burn, 0),
            "res_unchanged": True,
        },
        "replay": {
            "id": "FB-REPLAY-000",
            "same_burn": post.burn == replay.burn if replay is not None else False,
            "same_oru": post.oru == replay.oru if replay is not None else False,
            "same_res": post.res == replay.res if replay is not None else False,
        },
        "reorg": {
            "id": "FB-REORG-000",
            "rollback_restores_oru": restored.oru == pre.oru,
            "rollback_restores_res": restored.res == pre.res,
        },
        "isolation": {
            "id": "FB-RES-000",
            "pre_res": pre.res,
            "post_res": post.res,
            "minted_res": post.res - pre.res,
            "res_cap": RES_MAX_SUPPLY,
            "res_cap_unchanged": True,
        },
    }


assert RES_COIN == 1_000_000
assert RES_COIN != 100
assert format_res_amount(RES_COIN) == "1.000000"
assert format_res_amount(RES_COIN // 10) == "0.100000"
assert oru_res_transfer_fee_burn_share(0) == 0
assert oru_res_transfer_fee_burn_share(1) == 0
assert oru_res_transfer_fee_burn_share(2) == 1
assert calculate_fee_burn(1000, TX_FEE_ORU_ONLY) == 0
assert calculate_fee_burn(1000, TX_FEE_RES_TRANSFER) == 500

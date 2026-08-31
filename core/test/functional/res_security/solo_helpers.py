#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Helpers for the solo-mining security suite: fake body RES mint, submitblock."""

from __future__ import annotations

from decimal import Decimal
from typing import Optional

from test_framework.messages import (
    COIN,
    COutPoint,
    CTransaction,
    CTxIn,
    CTxOut,
)
from test_framework.script import CScript, RIGOR_PUSH_1

from res_security.harness import RES_REWARD_ATOMS


def res2_script(program32: Optional[bytes] = None) -> CScript:
    """Native witness v1 RES (32-byte program). Historical name kept for call sites."""
    if program32 is None:
        prog = b"\x22" * 32
    else:
        if len(program32) != 32:
            raise ValueError("RES program must be 32 bytes")
        prog = program32
    return CScript([RIGOR_PUSH_1, prog])


def fake_oru_funded_res_mint(
    node,
    *,
    bind_hash: bytes,
    proof_height: int,
    target_prev: int,
    convergence_root: bytes,
    convergence_txid: bytes,
    owner_program32: Optional[bytes] = None,
    amount_sats: int = 0,
) -> CTransaction:
    """Build a body ORU→RES tx. Consensus rejects this at CheckTxInputs (`mixed-asset-output`).

    The PCORB carrier and RIGOR_MARKER_RES_MINT payout now live on the coinbase; leftover witness
    fields are ignored. The unused kwargs keep the historical call site compiling.
    """
    _ = (bind_hash, proof_height, target_prev, convergence_root, convergence_txid, amount_sats)
    utxos = node.listunspent(1)
    selected = None
    for u in utxos:
        if str(u.get("asset", "ORU")).upper() not in ("ORU", "ORUM", "BTC"):
            continue
        if not u.get("spendable", False):
            continue
        sats = int((Decimal(str(u["amount"])) * Decimal(COIN)).to_integral_value())
        if sats > 2000:
            selected = (u, sats)
            break
    if selected is None:
        raise AssertionError("no spendable ORU UTXO for fake body mint")
    u, sats = selected
    fee = 1000
    tx = CTransaction()
    tx.nVersion = 2
    tx.vin = [CTxIn(COutPoint(int(u["txid"], 16), int(u["vout"])))]
    tx.vout = [
        CTxOut(RES_REWARD_ATOMS, res2_script(owner_program32)),
        CTxOut(sats - fee, CScript(bytes.fromhex(u["scriptPubKey"]))),
    ]
    tx.rehash()
    return tx


def submit_block(node, block) -> Optional[str]:
    """None if accepted, else reject reason."""
    return node.submitblock(hexdata=block.serialize(with_witness=True).hex())

#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Model → wire: build CBlock objects for P2P / consensus experiments (test_framework).

``build_block`` assembles a block header + vtx list compatible with
``test_framework.messages`` / ``msg_block``, with coinbase carrying segwit
commitment, optional CVGE v2 (ConvergenceDrawCandidates root + cumulative convergence) ``OP_RETURN``,
and optional RIGOR ``OP_RETURN`` (draw-target compact only; see ``RigorTags::RIGOR``).

Callers supply already-signed ``convergence_txs`` / ``res_claim_tx`` (RES body tx) when exercising
real acceptance; this module only handles layout and OP_RETURN payloads.
"""

from __future__ import annotations

import struct
import time
from dataclasses import dataclass
from decimal import Decimal, ROUND_DOWN
from typing import Collection, List, Optional, Protocol, Sequence, Tuple, Union

from .blocktools import (
    VERSIONBITS_LAST_OLD_BLOCK_VERSION,
    add_witness_commitment,
    apply_coinbase_convergence_root,
    create_coinbase,
)
from .messages import CBlock, CTransaction, CTxOut, tx_from_hex
from .script import CScript, OP_0, OP_1, OP_RETURN

# --- Witness v1 programs: CVG2 (32 bytes) / RES2 draw-claim (36 bytes) ---------

# Default 32-byte convergence program (tag + padding). CVG2 padding matches contrib devtest helper.
CVG2_TAG = b"CVG2" + bytes.fromhex("11" * 28)

# SECURITY-001-A / A-5: owner declaration carried in an OP_RETURN of the convergence tx.
RESO_MAGIC = b"RESO"
RESO_VERSION = b"\x01"
DEFAULT_RES_OWNER_PROGRAM = bytes.fromhex("22" * 32)
# Consensus-locked wire spelling (``RigorTags::Res2WirePrefix``).
RES2_WIRE_PREFIX = b"RES2"

_COIN = 100_000_000


def _listunspent_utxo_is_native_p2wpkh_oru(u: dict) -> bool:
    """True if ``listunspent`` row is native P2WPKH (``0014`` + 20-byte hash), ORU-domain.

    Aligns with ``contrib/devtools/devtest-create-convergence-tx.is_p2wpkh_oru_utxo``:
    BIP143 witness signing requires exact prevout amounts; non-v0-keyhash inputs are skipped.
    """
    spk = u.get("scriptPubKey")
    if isinstance(spk, dict):
        spk = spk.get("hex")
    if not isinstance(spk, str) or len(spk) < 4:
        return False
    h = spk.lower()
    return h.startswith("0014") and len(h) == 44


CONVERGENCE_COMMITMENT_VERSION = 2
CVGE_MAGIC = b"CVGE"
# 4 magic + 1 version + 8 cumulative + 32 draw_candidate_root (matches convergence_commitment.cpp v2)
CVGE_PAYLOAD_SIZE = 45


def make_convergence_program_32(amount_sats: int) -> bytes:
    """32-byte witness v1 program: ``CVG2`` + 8-byte **big-endian** amount + zero padding (matches ``ExtractconvergenceAmount``)."""
    if amount_sats < 0 or amount_sats > 0xFFFFFFFFFFFFFFFF:
        raise ValueError("amount_sats must fit in uint64")
    prog = bytearray(32)
    prog[0:4] = b"CVG2"
    prog[4:12] = int(amount_sats).to_bytes(8, "big", signed=False)
    return bytes(prog)


def make_convergence_script(amount_sats: int) -> CScript:
    """
    Witness v1 convergence output script (``OP_1`` + 32-byte program).

    Consensus ``GetconvergenceAmount`` sums **output ``nValue``** on every ``IsconvergenceScript`` vout.
    The embedded 8-byte field (BE) is decoded by ``ExtractconvergenceAmount`` / ``GetconvergenceAmountFromScript`` for tooling.
    """
    return make_cvg2_scriptpubkey(make_convergence_program_32(amount_sats))


def wallet_prevtx_entry_for_signrawtransactionwithwallet(node, txid: str, vout: int) -> dict:
    """
    One ``prevtxs`` element for ``signrawtransactionwithwallet`` from the wallet's copy of ``txid``.

    Without ``-txindex``, ``getrawtransaction`` cannot load arbitrary confirmed transactions; Bitcoin
    Core documents using ``gettransaction`` for in-wallet txs. With ``verbose=true``, ``decoded`` matches
    ``decoderawtransaction`` (``src/wallet/rpc/transactions.cpp``).
    """
    wtx = node.gettransaction(txid, False, True)
    decoded = wtx["decoded"]
    outs = decoded["vout"]
    if vout < 0 or vout >= len(outs):
        raise AssertionError(f"gettransaction decoded vout out of range: txid={txid} vout={vout}")
    vout_entry = outs[vout]
    spk_obj = vout_entry["scriptPubKey"]
    spk_hex = spk_obj["hex"] if isinstance(spk_obj, dict) else spk_obj
    if not isinstance(spk_hex, str) or not spk_hex:
        raise AssertionError("gettransaction decoded vout missing scriptPubKey hex")
    val = vout_entry["value"]
    prev_amount_str = format(Decimal(str(val)), ".8f")
    return {
        "txid": txid,
        "vout": int(vout),
        "scriptPubKey": spk_hex,
        "amount": prev_amount_str,
    }


def prepare_unsigned_convergence_tx(
    node,
    convergence_orum: Decimal,
    fee_sats: int = 1000,
    *,
    spend_addresses: Optional[Collection[str]] = None,
    change_address: Optional[str] = None,
    minconf: int = 1,
    res_owner_program: Optional[bytes] = None,
) -> tuple[str, list[dict], dict]:
    """
    Build a consensus-shaped unsigned convergence ORUM-domain raw tx + ``prevtxs`` for
    ``signrawtransactionwithwallet`` (same shape as ``create_convergence_tx``).

    Returns ``(raw_hex, prev_txs, meta)`` where *meta* includes ``utxo`` and ``convergence_sats`` for tests.
    ``prev_txs`` use ``gettransaction(txid, false, true)`` → ``decoded`` (wallet RPC path when
    ``getrawtransaction`` is unavailable without ``-txindex``), same prevout view as ``decoderawtransaction``.

    Use ``minconf=0`` when spending an unconfirmed change output from a prior convergence tx in the same
    mempool (still requires ``spend_addresses`` / ``change_address`` pinned to that pool).
    """
    convergence_sats = int((convergence_orum * Decimal(_COIN)).to_integral_value(rounding=ROUND_DOWN))
    if convergence_sats <= 0:
        raise ValueError("convergence_orum must be > 0")
    fee_use = max(0, int(fee_sats))
    target = convergence_sats + fee_use

    lu_kw: dict = {"minconf": minconf, "query_options": {"minimumAmount": Decimal("2.0")}}
    if spend_addresses is not None:
        lu_kw["addresses"] = list(spend_addresses)
    utxos = node.listunspent(**lu_kw)
    selected = None
    for u in utxos:
        asset = str(u.get("asset", "ORU")).upper()
        if asset not in ("ORU", "ORUM", "BTC"):
            continue
        if not u.get("spendable", False):
            continue
        if not _listunspent_utxo_is_native_p2wpkh_oru(u):
            continue
        sats = int((Decimal(str(u["amount"])) * Decimal(_COIN)).to_integral_value(rounding=ROUND_DOWN))
        if sats >= target:
            selected = (u, sats)
            break
    if selected is None:
        raise AssertionError("no suitable ORUM-domain (ORU) utxo for convergence+fee; mine more coinbase first")

    utxo, input_sats = selected
    change_sats = input_sats - target
    if change_sats <= 0:
        raise AssertionError("UTXO too small for convergence output + fee")

    prog = make_convergence_program_32(convergence_sats)
    conv_spk_hex = "5120" + prog.hex()
    dec = node.decodescript(conv_spk_hex)
    if "address" not in dec:
        raise AssertionError(f"decodescript did not yield address: {dec}")
    convergence_addr = dec["address"]

    change_addr = change_address if change_address is not None else node.getrawchangeaddress("bech32")
    convergence_amt_str = format(Decimal(convergence_sats) / Decimal(_COIN), "f")
    change_amt_str = format(Decimal(change_sats) / Decimal(_COIN), "f")

    raw = node.createrawtransaction(
        [{"txid": utxo["txid"], "vout": utxo["vout"]}],
        {
            convergence_addr: {"amount": convergence_amt_str, "asset": "ORU"},
            change_addr: {"amount": change_amt_str, "asset": "ORU"},
            # SECURITY-001-A / A-5 R1: without a RESO owner declaration the convergence is a
            # valid ORU burn but never enters F, so no draw would ever have a candidate.
            "data": make_res_owner_declaration_payload(res_owner_program).hex(),
        },
    )
    spk_raw = utxo.get("scriptPubKey")
    if isinstance(spk_raw, dict):
        list_spk_hex = spk_raw.get("hex")
    else:
        list_spk_hex = spk_raw
    if not isinstance(list_spk_hex, str) or not list_spk_hex:
        raise AssertionError("listunspent entry missing scriptPubKey; cannot sign convergence input")

    vout_idx = int(utxo["vout"])
    prev_entry = wallet_prevtx_entry_for_signrawtransactionwithwallet(node, utxo["txid"], vout_idx)
    if prev_entry["scriptPubKey"].lower() != list_spk_hex.lower():
        raise AssertionError(
            f"listunspent scriptPubKey disagrees with gettransaction decoded: list={list_spk_hex!r} wallet={prev_entry['scriptPubKey']!r}"
        )
    prev_txs = [prev_entry]
    meta = {
        "utxo": utxo,
        "convergence_sats": convergence_sats,
        "convergence_addr": convergence_addr,
        "change_addr": change_addr,
    }
    return raw, prev_txs, meta


def create_convergence_tx(
    node,
    convergence_orum: Decimal,
    fee_sats: int = 1000,
    *,
    spend_addresses: Optional[Collection[str]] = None,
    change_address: Optional[str] = None,
    minconf: int = 1,
    res_owner_program: Optional[bytes] = None,
) -> CTransaction:
    """
    Build and sign a consensus-shaped single-convergence ORUM-domain transaction using the wallet.

    Picks a spendable ORUM-domain (ORU on wire) UTXO, pays ``convergence_orum`` to a witness-v1 CVG2 output
    (amount in script matches ``nValue``), and returns change in ORU. Fee is relay policy only.
    Does not broadcast; caller may ``sendrawtransaction`` / ``testmempoolaccept``.

    If ``spend_addresses`` is set, only UTXOs paid to those addresses are considered (recommended
    when the wallet also has ``importprivkey`` coinbase keys: otherwise ``listunspent`` may pick a
    legacy-shaped key first and ``signrawtransactionwithwallet`` fails with witness hash mismatch).

    If ``change_address`` is set, ORU change is sent there instead of ``getrawchangeaddress``; pin it
    to the same address as ``spend_addresses`` when looping multiple convergence txs from one ORUM pool.

    Signing uses ``signrawtransactionwithwallet`` with **prevtxs** from ``wallet_prevtx_entry_for_signrawtransactionwithwallet``
    (``gettransaction`` ``decoded``), matching the wallet RPC path used when ``getrawtransaction`` requires ``-txindex``.
    """
    raw, prev_txs, _meta = prepare_unsigned_convergence_tx(
        node,
        convergence_orum,
        fee_sats,
        spend_addresses=spend_addresses,
        change_address=change_address,
        minconf=minconf,
        res_owner_program=res_owner_program,
    )
    signed = node.signrawtransactionwithwallet(raw, prev_txs)
    if not signed.get("complete"):
        raise AssertionError(f"signrawtransactionwithwallet incomplete: {signed}")
    return tx_from_hex(signed["hex"])


def make_res_owner_declaration_payload(program_32: Optional[bytes] = None) -> bytes:
    """``RESO`` || version || 32-byte witness v1 RES program (``Consensus::RES_OWNER_PAYLOAD_SIZE``)."""
    p = DEFAULT_RES_OWNER_PROGRAM if program_32 is None else program_32
    if len(p) != 32:
        raise ValueError("RES owner program must be 32 bytes")
    return RESO_MAGIC + RESO_VERSION + p


def make_cvg2_scriptpubkey(program_32: Optional[bytes] = None) -> CScript:
    """P2TR-style witness v1 output script: OP_1 <32-byte program> (CVG2 convergence)."""
    p = CVG2_TAG if program_32 is None else program_32
    if len(p) != 32:
        raise ValueError("convergence program must be 32 bytes")
    return CScript([OP_1, p])


def make_res2_claim_program_bytes(convergence_txid: Optional[bytes] = None) -> bytes:
    """36-byte witness v1 program: ASCII ``RES2`` + 32-byte convergence winner txid (``WITNESS_V1_RES2_CLAIM_PROGRAM_SIZE``)."""
    fid = convergence_txid if convergence_txid is not None else bytes(32)
    return RES2_WIRE_PREFIX + fid


def make_res2_claim_scriptpubkey(program_36: Optional[bytes] = None) -> CScript:
    """Witness v1 output script for RES draw-claim (``OP_1`` + 36-byte program)."""
    p = make_res2_claim_program_bytes() if program_36 is None else program_36
    if len(p) != 36:
        raise ValueError("RES2 claim program must be 36 bytes")
    return CScript([OP_1, p])


def make_rigor_commitment_script(res_draw_target_nbits: int) -> CScript:
    """Coinbase OP_RETURN: ``RIGR`` magic + uint32 LE ``res_draw_target_nbits`` (8 bytes; ``IsRigorCommitment``)."""
    nb = int(res_draw_target_nbits) & 0xFFFFFFFF
    payload = bytearray(8)
    payload[0:4] = b"RIGR"  # RigorTags::RIGOR — 4 ASCII bytes, not the 5-byte word "RIGOR"
    struct.pack_into("<I", payload, 4, nb)
    return CScript([OP_RETURN, bytes(payload)])


def make_cvge_commitment_script(
    cumulative_convergence: int,
    draw_candidate_root: bytes,
    *,
    version: int = CONVERGENCE_COMMITMENT_VERSION,
) -> CScript:
    """Coinbase OP_RETURN CVGE v2 payload (must match ``ParseconvergenceCommitmentPayload`` / ConnectBlock)."""
    if len(draw_candidate_root) != 32:
        raise ValueError("draw_candidate_root must be 32 bytes")
    if version != CONVERGENCE_COMMITMENT_VERSION:
        raise ValueError("unsupported CVGE version")
    payload = bytearray(CVGE_PAYLOAD_SIZE)
    payload[0:4] = CVGE_MAGIC
    payload[4] = version & 0xFF
    struct.pack_into("<Q", payload, 5, int(cumulative_convergence) & 0xFFFFFFFFFFFFFFFF)
    payload[13:45] = draw_candidate_root
    return CScript([OP_RETURN, bytes(payload)])


@dataclass
class CvgeCommitmentSpec:
    """Serialized CVGE v2 coinbase commitment fields."""

    cumulative_convergence: int
    draw_candidate_root: bytes


class _RigorCommitmentLike(Protocol):
    res_draw_target_nbits: int


RigorCommitmentArg = Union[int, Tuple[object, int], _RigorCommitmentLike, None]


def _normalize_rigor_commitment(commitment: RigorCommitmentArg) -> Optional[int]:
    """Return compact draw target bits, or ``None`` to omit RIGOR coinbase output."""
    if commitment is None:
        return None
    if isinstance(commitment, int):
        return int(commitment) & 0xFFFFFFFF
    if isinstance(commitment, tuple):
        _w, n = commitment
        return int(n) & 0xFFFFFFFF
    return int(commitment.res_draw_target_nbits) & 0xFFFFFFFF


def _init_block_from_coinbase(
    hash_prev_block: int,
    coinbase: CTransaction,
    txlist: Sequence[CTransaction],
    *,
    bip34_height: int,
    ntime: Optional[int],
    target_bits: int,
    version: Optional[int],
    rpc_node: Optional[object] = None,
    convergence_root: Optional[bytes] = None,
) -> CBlock:
    """Like ``create_block`` but allows ``hash_prev_block == 0`` (genesis-style)."""
    block = CBlock()
    block.nVersion = version if version is not None else VERSIONBITS_LAST_OLD_BLOCK_VERSION
    block.nTime = int(time.time() + 600) if ntime is None else int(ntime)
    block.hashPrevBlock = int(hash_prev_block)
    block.nBits = int(target_bits) & 0xFFFFFFFF
    block.vtx.append(coinbase)
    for tx in txlist:
        if not hasattr(tx, "calc_sha256"):
            tx = tx_from_hex(tx)
        block.vtx.append(tx)
    apply_coinbase_convergence_root(block, bip34_height, rpc_node=rpc_node, convergence_root=convergence_root)
    block.hashMerkleRoot = block.calc_merkle_root()
    block.calc_sha256()
    return block


def build_block(
    convergence_txs: Sequence[CTransaction],
    res_claim_tx: Optional[CTransaction],
    commitment: RigorCommitmentArg,
    target_bits: int,
    *,
    hash_prev_block: int,
    block_height: int,
    cvge: Optional[CvgeCommitmentSpec] = None,
    cvge_script_pubkey: Optional[bytes] = None,
    fees: int = 0,
    coinbase_pubkey: Optional[bytes] = None,
    coinbase_script_pubkey: Optional[CScript] = None,
    coinbase_value_sats: Optional[int] = None,
    ntime: Optional[int] = None,
    block_version: Optional[int] = None,
    rpc_node: Optional[object] = None,
    convergence_root: Optional[bytes] = None,
) -> CBlock:
    """Assemble a ``CBlock`` ready to wrap in ``msg_block`` for P2P submission.

    :param convergence_txs: Signed transactions (each typically spends ORUM-domain (ORU) to a CVG2 output).
    :param res_claim_tx: Optional RES body / draw-claim tx (or ``None``). For consensus-valid winner
        blocks past genesis, rules require a RES claim when there is a convergence winner.
    :param commitment: compact ``res_draw_target_nbits`` (``int``), legacy ``(ignored, nbits)`` tuple,
        object with ``res_draw_target_nbits``, or ``None`` to omit RIGOR output (invalid on non-genesis chains).
    :param target_bits: Block header ``nBits`` (PoW target compact form).
    :param hash_prev_block: Previous block hash as ``uint256`` int (same convention as RPC-derived ints;
        zero is allowed, unlike ``blocktools.create_block``).
    :param block_height: BIP34 height embedded in coinbase.
    :param cvge: Optional CVGE commitment; omit only when building intentionally invalid blocks.
    :param cvge_script_pubkey: Raw ``scriptPubKey`` bytes (e.g. from ``getblocktemplate``'s
        ``default_convergence_commitment``) — mutually exclusive with ``cvge`` (this wins if set).
    :param fees: ORUM-domain (ORU) fees included in coinbase vout[0] (regtest subsidy path).
    :param coinbase_value_sats: If set, overrides ``vout[0].nValue`` after ``create_coinbase``.
    :param block_version: Optional block ``nVersion`` (default: regtest legacy version 4).
    :param convergence_root: 32 raw bytes to commit in the coinbase scriptSig instead of a root
        derived from this block's body. Required whenever the draw window holds candidates from
        earlier blocks (SECURITY-001-A9-C); ``getblocktemplate``'s ``convergence_root``, reversed
        from RPC hex, is the value the node will check against.
    """
    txlist: List[CTransaction] = list(convergence_txs)
    if res_claim_tx is not None:
        txlist.append(res_claim_tx)

    # Rigor ``CheckTxOutAssetWitnessBinding`` rejects bare OP_TRUE / P2PKH on any vout; match the
    # node's GBT dummy (``WitnessV0KeyHash{}`` → ``OP_0`` + 20-byte keyhash).
    cb_spk = coinbase_script_pubkey
    if coinbase_pubkey is None and cb_spk is None:
        cb_spk = CScript([OP_0, bytes(20)])

    cb = create_coinbase(
        height=block_height,
        pubkey=coinbase_pubkey,
        script_pubkey=cb_spk,
        fees=fees,
        nValue=50,
    )
    if coinbase_value_sats is not None:
        cb.vout[0].nValue = int(coinbase_value_sats)

    block = _init_block_from_coinbase(
        hash_prev_block,
        cb,
        txlist,
        bip34_height=block_height,
        ntime=ntime,
        target_bits=target_bits,
        version=block_version,
        rpc_node=rpc_node,
        convergence_root=convergence_root,
    )

    # convergence commitment / RIGOR must be added before the segwit commitment: BIP141 requires the commitment
    # to be the last coinbase output, and CTransaction.sha256 is cached — mutating vout after
    # add_witness_commitment leaves a stale txid and yields bad-txnmrklroot on submitblock.
    if cvge_script_pubkey is not None:
        block.vtx[0].vout.append(CTxOut(0, CScript(cvge_script_pubkey)))
    elif cvge is not None:
        block.vtx[0].vout.append(
            CTxOut(0, make_cvge_commitment_script(cvge.cumulative_convergence, cvge.draw_candidate_root))
        )

    rigor_nb = _normalize_rigor_commitment(commitment)
    if rigor_nb is not None:
        block.vtx[0].vout.append(CTxOut(0, make_rigor_commitment_script(rigor_nb)))

    add_witness_commitment(block)

    for tx in block.vtx:
        tx.calc_sha256()
    block.hashMerkleRoot = block.calc_merkle_root()
    block.rehash()
    return block


def hash_prev_from_rpc_hex(block_hash_hex: str) -> int:
    """Convert ``getblockhash`` / ``getbestblockhash`` hex string to ``CBlock.hashPrevBlock`` int."""
    return int(block_hash_hex, 16)

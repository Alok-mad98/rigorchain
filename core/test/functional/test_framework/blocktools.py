#!/usr/bin/env python3
# Copyright (c) 2015-2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Utilities for manipulating blocks and transactions.

CTxOut serialization is Bitcoin-native (nValue + scriptPubKey). Asset domain
(ORU vs RES) is implied by scriptPubKey via AssetTypeFromScriptPubKey (v0/20 = ORU; native v1/32 = RES; RIGOR_MARKER_CONVERGE = ORU burn).
"""

import struct
import time
import unittest
from decimal import Decimal

from .address import (
    address_to_scriptpubkey,
    key_to_p2sh_p2wpkh,
    key_to_p2wpkh,
    script_to_p2sh_p2wsh,
    script_to_p2wsh,
    test_coinbase_script_pubkey,
)
from .messages import (
    CBlock,
    COIN,
    COutPoint,
    CTransaction,
    CTxIn,
    CTxInWitness,
    CTxOut,
    SEQUENCE_FINAL,
    hash256,
    ser_uint256,
    tx_from_hex,
    uint256_from_str,
)
from .script import (
    CScript,
    CScriptNum,
    CScriptOp,
    RIGOR_PUSH_0,
    RIGOR_PUSH_1,
    RIGOR_PUSH_16,
    RIGOR_MARKER_CONVERGE,
    RIGOR_MARKER_RETURN,
)
from .script_util import (
    key_to_p2wpkh_script,
    keys_to_multisig_script,
    script_to_p2wsh_script,
)
from .util import assert_equal

WITNESS_SCALE_FACTOR = 4
MAX_BLOCK_SIGOPS = 20000
MAX_BLOCK_SIGOPS_WEIGHT = MAX_BLOCK_SIGOPS * WITNESS_SCALE_FACTOR

# Genesis block time (regtest)
TIME_GENESIS_BLOCK = 1296688602

MAX_FUTURE_BLOCK_TIME = 2 * 60 * 60

# Coinbase transaction outputs can only be spent after this number of new blocks (network rule)
COINBASE_MATURITY = 100

# Must match CRegTestParams::consensus.nSubsidyHalvingInterval, nBlockSubsidyBase,
# nBlockSubsidyMaxHalvingShifts, and GetBlockSubsidy (validation.cpp).
REGTEST_SUBSIDY_HALVING_INTERVAL = 150
REGTEST_SUBSIDY_BASE_SATS = 1000 * COIN
REGTEST_SUBSIDY_MAX_HALVING_SHIFTS = 3


def get_regtest_block_subsidy_sats(height):
    """Regtest block subsidy in satoshis (no fees), matching consensus GetBlockSubsidy."""
    if height == 0:
        return 0
    halvings = height // REGTEST_SUBSIDY_HALVING_INTERVAL
    halvings = min(halvings, REGTEST_SUBSIDY_MAX_HALVING_SHIFTS)
    return REGTEST_SUBSIDY_BASE_SATS >> halvings


def make_ctxout(n_value, script_pub_key):
    """Build a CTxOut (Bitcoin-native: nValue + scriptPubKey only)."""
    return CTxOut(n_value, script_pub_key)

# From BIP141
WITNESS_COMMITMENT_HEADER = b"\xaa\x21\xa9\xed"

NORMAL_GBT_REQUEST_PARAMS = {"rules": ["segwit"]}
VERSIONBITS_LAST_OLD_BLOCK_VERSION = 4
# validation.h: MAX_SUPPORTED_REORG_DEPTH (288) + CONVERGENCE_DRAW_WINDOW (32).
MAX_SUPPORTED_REORG_DEPTH = 288
CONVERGENCE_DRAW_WINDOW = 32
MIN_BLOCKS_TO_KEEP = MAX_SUPPORTED_REORG_DEPTH + CONVERGENCE_DRAW_WINDOW

# Default destination for homemade coinbases. Anyone-can-spend is ASSET_INVALID on RIGOR,
# so there is no implicit fallback: every coinbase has to name a real output script.
DEFAULT_COINBASE_SCRIPT_PUBKEY = test_coinbase_script_pubkey()


def create_block(hashprev=None, coinbase=None, ntime=None, *, version=None, tmpl=None, txlist=None, rpc_for_convergence_root=None, coinbase_script_pubkey=None):
    """Create a block (with regtest difficulty).

    Either pass a ready ``coinbase``, or pass ``coinbase_script_pubkey`` to have one built.
    """
    block = CBlock()
    if tmpl is None:
        tmpl = {}
    block.nVersion = version or tmpl.get('version') or VERSIONBITS_LAST_OLD_BLOCK_VERSION
    block.nTime = ntime or tmpl.get('curtime') or int(time.time() + 600)
    block.hashPrevBlock = hashprev or int(tmpl['previousblockhash'], 0x10)
    if tmpl and not tmpl.get('bits') is None:
        block.nBits = struct.unpack('>I', bytes.fromhex(tmpl['bits']))[0]
    else:
        block.nBits = 0x207fffff  # difficulty retargeting is disabled in REGTEST chainparams
    if coinbase is None:
        assert coinbase_script_pubkey is not None, (
            "create_block needs either coinbase= or coinbase_script_pubkey="
        )
        coinbase = create_coinbase(height=tmpl['height'], script_pubkey=coinbase_script_pubkey)
    block.vtx.append(coinbase)
    if txlist:
        for tx in txlist:
            if not hasattr(tx, 'calc_sha256'):
                tx = tx_from_hex(tx)
            block.vtx.append(tx)
    bip34_height = tmpl.get('height') if tmpl else None
    if bip34_height is None:
        bip34_height = infer_bip34_height_from_coinbase(block.vtx[0])
    apply_coinbase_convergence_root(block, bip34_height, rpc_node=rpc_for_convergence_root)
    block.hashMerkleRoot = block.calc_merkle_root()
    block.calc_sha256()
    return block

def get_witness_script(witness_root, witness_nonce):
    witness_commitment = uint256_from_str(hash256(ser_uint256(witness_root) + ser_uint256(witness_nonce)))
    output_data = WITNESS_COMMITMENT_HEADER + ser_uint256(witness_commitment)
    return CScript([RIGOR_MARKER_RETURN, output_data])

def add_witness_commitment(block, nonce=0):
    """Add a witness commitment to the block's coinbase transaction.

    According to BIP141, blocks with witness rules active must commit to the
    hash of all in-block transactions including witness."""
    # First calculate the merkle root of the block's
    # transactions, with witnesses.
    witness_nonce = nonce
    witness_root = block.calc_witness_merkle_root()
    # witness_nonce should go to coinbase witness.
    block.vtx[0].wit.vtxinwit = [CTxInWitness()]
    block.vtx[0].wit.vtxinwit[0].scriptWitness.stack = [ser_uint256(witness_nonce)]

    # witness commitment is the last RIGOR_MARKER_RETURN output in coinbase
    block.vtx[0].vout.append(CTxOut(0, get_witness_script(witness_root, witness_nonce)))
    block.vtx[0].rehash()
    block.hashMerkleRoot = block.calc_merkle_root()
    block.rehash()


def _is_converge_scriptpubkey(spk) -> bool:
    """Canonical RIGOR_MARKER_CONVERGE: OP_PUSHBYTES_20 || 20-byte hash || RIGOR_MARKER_CONVERGE."""
    raw = bytes(spk)
    return len(raw) == 22 and raw[0] == 0x14 and raw[21] == RIGOR_MARKER_CONVERGE


def _count_cvg2_positive_convergence_outputs(tx: CTransaction) -> int:
    """Count positive-value RIGOR_MARKER_CONVERGE outputs."""
    count = 0
    for o in tx.vout:
        if o.nValue > 0 and _is_converge_scriptpubkey(o.scriptPubKey):
            count += 1
    return count


def _get_cvg2_convergence_amount_sats(tx: CTransaction) -> int:
    """Sum positive-value RIGOR_MARKER_CONVERGE output amounts."""
    total = 0
    for o in tx.vout:
        if not _is_converge_scriptpubkey(o.scriptPubKey):
            continue
        if o.nValue > 0:
            total += o.nValue
    return total


def _outpoint_to_rpc_txid(prevout: COutPoint) -> str:
    return ser_uint256(prevout.hash)[::-1].hex()


def _tx_fee_sats_via_rpc(rpc, tx: CTransaction):
    """Return fee in satoshis, or None if an input UTXO could not be resolved at chain tip (+mempool)."""
    in_sum = 0
    for tin in tx.vin:
        if tin.prevout.hash == 0:
            continue
        txid = _outpoint_to_rpc_txid(tin.prevout)
        u = rpc.gettxout(txid, tin.prevout.n, True)
        if u is None:
            return None
        in_sum += int((Decimal(str(u["value"])) * Decimal(COIN)).to_integral_value())
    out_sum = sum(o.nValue for o in tx.vout)
    return in_sum - out_sum


def calc_convergence_root_bytes(vtx, *, rpc_node=None):
    """Match ``Consensus::ComputeconvergenceRootFromconvergenceSet`` (convergence subset + fee > 0, sorted txids, Hash256)."""
    if len(vtx) <= 1:
        return b"\x00" * 32

    structural = []
    for tx in vtx[1:]:
        tx.calc_sha256()
        n_cvg = _count_cvg2_positive_convergence_outputs(tx)
        if n_cvg == 0:
            continue
        if n_cvg != 1:
            raise ValueError("Block must have exactly one positive convergence output per convergence tx")
        if _get_cvg2_convergence_amount_sats(tx) < COIN:
            continue
        structural.append(tx)

    if len(structural) < 2:
        return b"\x00" * 32

    if rpc_node is None:
        raise ValueError(
            "calc_convergence_root_bytes: block has ≥2 structural convergence transactions (RIGOR_MARKER_CONVERGE); pass rpc_node=... "
            "so fees can be checked against UTXO (matches consensus)."
        )

    convergence_ids = []
    for tx in structural:
        fee = _tx_fee_sats_via_rpc(rpc_node, tx)
        if fee is None or fee <= 0:
            continue
        convergence_ids.append(tx.sha256)

    if len(convergence_ids) < 2:
        return b"\x00" * 32

    convergence_ids.sort(key=lambda h: ser_uint256(h))
    payload = b"".join(ser_uint256(h) for h in convergence_ids)
    return hash256(payload)


def coinbase_height_convergence_script(height, convergence_root_bytes, paid_winner_txid=None):
    """Canonical coinbase scriptSig: BIP34 height ‖ 32-byte C ‖ 32-byte paid-winner txid (zeros if none)."""
    assert len(convergence_root_bytes) == 32
    paid = b"\x00" * 32 if paid_winner_txid is None else paid_winner_txid
    assert len(paid) == 32
    if height <= 16:
        return CScript([CScriptOp.encode_op_n(height), convergence_root_bytes, paid])
    return CScript([CScriptNum(height), convergence_root_bytes, paid])


def _scriptnum_pushdata_to_int(vch: bytes) -> int:
    """Decode minimal-encoded script number from a single push's payload (BIP34 height bytes)."""
    if len(vch) == 0:
        return 0
    result = 0
    for i, byte in enumerate(vch):
        result |= int(byte) << (8 * i)
    if vch[-1] & 0x80:
        num_mask = (2 ** (len(vch) * 8) - 1) >> 1
        result &= num_mask
        result *= -1
    return result


def infer_bip34_height_from_coinbase(cb):
    """Recover BIP34 height from the first push of the coinbase scriptSig (Rigor: C then paid-winner txid follow)."""
    sig = CScript(cb.vin[0].scriptSig)
    opcode, data, _ = next(sig.raw_iter())
    if opcode == RIGOR_PUSH_0:
        return 0
    if RIGOR_PUSH_1 <= opcode <= RIGOR_PUSH_16:
        return CScriptOp(opcode).decode_op_n()
    if data is not None:
        # `CScript([CScriptNum(h), ...])` joins CScriptNum.encode(h) without an extra push wrapper, so
        # the stream often begins with OP_PUSHBYTES_n (n<0x4c) and `data` is the raw vch (see tests).
        return _scriptnum_pushdata_to_int(data)
    raise ValueError("cannot infer BIP34 height from coinbase scriptSig")


def apply_coinbase_convergence_root(block, bip34_height, *, rpc_node=None, convergence_root=None, paid_winner_txid=None):
    """Set coinbase scriptSig to canonical height + C + paid-winner txid (zeros if not supplied).

    Pass ``convergence_root`` (32 raw bytes, consensus memory order) to commit a root the node
    computed. Since SECURITY-001-A9-C, C spans the whole draw window, so a block whose window
    holds candidates buried in earlier blocks cannot have its root derived from ``block.vtx``:
    take ``getblocktemplate``'s ``convergence_root`` instead, reversed from its RPC hex.
    ``paid_winner_txid`` is 32 raw bytes (memory order); GBT ``paid_winner_txid`` RPC hex must
    be reversed the same way. Prefer copying ``default_coinbase_scriptsig`` when present.
    """
    fr = calc_convergence_root_bytes(block.vtx, rpc_node=rpc_node) if convergence_root is None else convergence_root
    assert len(fr) == 32
    block.vtx[0].vin[0].scriptSig = coinbase_height_convergence_script(bip34_height, fr, paid_winner_txid)
    block.vtx[0].rehash()


def script_BIP34_coinbase_height(height):
    return coinbase_height_convergence_script(height, b"\x00" * 32)


def create_coinbase(height, *, script_pubkey, extra_output_script=None, fees=0, nValue=50):
    """Create a coinbase transaction.

    ``script_pubkey`` is mandatory: RIGOR has no anyone-can-spend output (that shape is
    ASSET_INVALID) and no secp P2PK, so a default would only ever build a consensus-invalid
    coinbase and surface far from the caller as ``bad-rigor-script-type``. Pass
    ``DEFAULT_COINBASE_SCRIPT_PUBKEY`` when the destination does not matter.

    If extra_output_script is given, make a 0-value output to that
    script. This is useful to pad block weight/sigops as needed. """
    coinbase = CTransaction()
    coinbase.vin.append(CTxIn(COutPoint(0, 0xffffffff), script_BIP34_coinbase_height(height), SEQUENCE_FINAL))
    coinbaseoutput = CTxOut()
    coinbaseoutput.nValue = nValue * COIN
    if nValue == 50:
        coinbaseoutput.nValue = get_regtest_block_subsidy_sats(height)
        coinbaseoutput.nValue += fees
    coinbaseoutput.scriptPubKey = script_pubkey
    coinbase.vout = [coinbaseoutput]
    if extra_output_script is not None:
        coinbaseoutput2 = CTxOut()
        coinbaseoutput2.nValue = 0
        coinbaseoutput2.scriptPubKey = extra_output_script
        coinbase.vout.append(coinbaseoutput2)
    coinbase.calc_sha256()
    return coinbase

def create_tx_with_script(prevtx, n, script_sig=b"", *, amount, script_pub_key=CScript()):
    """Return one-input, one-output transaction object
       spending the prevtx's n-th output with the given amount.

       Can optionally pass scriptPubKey and scriptSig, default is anyone-can-spend output.
    """
    tx = CTransaction()
    assert n < len(prevtx.vout)
    tx.vin.append(CTxIn(COutPoint(prevtx.sha256, n), script_sig, SEQUENCE_FINAL))
    tx.vout.append(CTxOut(amount, script_pub_key))
    tx.calc_sha256()
    return tx

def get_legacy_sigopcount_block(block, accurate=True):
    count = 0
    for tx in block.vtx:
        count += get_legacy_sigopcount_tx(tx, accurate)
    return count

def get_legacy_sigopcount_tx(tx, accurate=True):
    count = 0
    for i in tx.vout:
        count += i.scriptPubKey.GetSigOpCount(accurate)
    for j in tx.vin:
        # scriptSig might be of type bytes, so convert to CScript for the moment
        count += CScript(j.scriptSig).GetSigOpCount(accurate)
    return count

def witness_script(use_p2wsh, pubkey):
    """Create a scriptPubKey for a pay-to-witness TxOut.

    This is either a P2WPKH output for the given pubkey, or a P2WSH output of a
    1-of-1 multisig for the given pubkey. Returns the hex encoding of the
    scriptPubKey."""
    if not use_p2wsh:
        # P2WPKH instead
        pkscript = key_to_p2wpkh_script(pubkey)
    else:
        # 1-of-1 multisig
        witness_script = keys_to_multisig_script([pubkey])
        pkscript = script_to_p2wsh_script(witness_script)
    return pkscript.hex()

def create_witness_tx(node, use_p2wsh, utxo, pubkey, encode_p2sh, amount):
    """Return a transaction (in hex) that spends the given utxo to a segwit output.

    Optionally wrap the segwit output using P2SH."""
    if use_p2wsh:
        program = keys_to_multisig_script([pubkey])
        addr = script_to_p2sh_p2wsh(program) if encode_p2sh else script_to_p2wsh(program)
    else:
        addr = key_to_p2sh_p2wpkh(pubkey) if encode_p2sh else key_to_p2wpkh(pubkey)
    if not encode_p2sh:
        assert_equal(address_to_scriptpubkey(addr).hex(), witness_script(use_p2wsh, pubkey))
    return node.createrawtransaction([utxo], {addr: amount})

def send_to_witness(use_p2wsh, node, utxo, pubkey, encode_p2sh, amount, sign=True, insert_redeem_script=""):
    """Create a transaction spending a given utxo to a segwit output.

    The output corresponds to the given pubkey: use_p2wsh determines whether to
    use P2WPKH or P2WSH; encode_p2sh determines whether to wrap in P2SH.
    sign=True will have the given node sign the transaction.
    insert_redeem_script will be added to the scriptSig, if given."""
    tx_to_witness = create_witness_tx(node, use_p2wsh, utxo, pubkey, encode_p2sh, amount)
    if (sign):
        signed = node.signrawtransactionwithwallet(tx_to_witness)
        assert "errors" not in signed or len(["errors"]) == 0
        return node.sendrawtransaction(signed["hex"])
    else:
        if (insert_redeem_script):
            tx = tx_from_hex(tx_to_witness)
            tx.vin[0].scriptSig += CScript([bytes.fromhex(insert_redeem_script)])
            tx_to_witness = tx.serialize().hex()

    return node.sendrawtransaction(tx_to_witness)

class TestFrameworkBlockTools(unittest.TestCase):
    def test_create_coinbase(self):
        height = 20
        coinbase_tx = create_coinbase(height=height, script_pubkey=DEFAULT_COINBASE_SCRIPT_PUBKEY)
        # Rigor coinbase scriptSig is height + C + paid-winner txid, not BIP34 height alone.
        assert_equal(infer_bip34_height_from_coinbase(coinbase_tx), height)

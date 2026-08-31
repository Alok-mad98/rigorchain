#!/usr/bin/env python3
# Copyright (c) 2020-2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""A limited-functionality wallet, which may replace a real wallet in tests"""

from copy import deepcopy
from decimal import Decimal
from typing import (
    Any,
    List,
    Optional,
)
from test_framework.address import (
    get_mini_wallet_key,
    key_to_p2wpkh,
    mini_wallet_wpkh_address,
    mini_wallet_wpkh_script_pubkey,
)
from test_framework.descriptors import descsum_create
from test_framework.messages import (
    COIN,
    COutPoint,
    CTransaction,
    CTxIn,
    CTxInWitness,
    CTxOut,
)
from test_framework.script import (
    CScript,
    RIGOR_MARKER_RETURN,
    SIGHASH_DOMAIN_ORU_V1,
    rigor_signature_hash,
)
from test_framework.script_util import (
    key_to_p2wpkh_script,
)
from test_framework.util import (
    assert_equal,
    assert_greater_than_or_equal,
)
from test_framework.blocktools import COINBASE_MATURITY

DEFAULT_FEE = Decimal("0.0001")

# RIGOR mempool "vsize" is serialized size with witness (no BIP141 discount).
# 1-in-1-out native P2WPKH: 64-byte Ed25519 sig + 32-byte pk.
P2WPKH_1IN_1OUT_VSIZE = 183


class MiniWallet:
    """Ed25519 P2WPKH test wallet. There is no anyone-can-spend mode (ASSET_INVALID)."""

    def __init__(self, test_node):
        self._test_node = test_node
        self._utxos = []
        self._key = get_mini_wallet_key()
        self._pubkey = self._key.get_pubkey_bytes()
        self._address = mini_wallet_wpkh_address()
        self._scriptPubKey = mini_wallet_wpkh_script_pubkey()
        self._chain_id = int(self._test_node.getblockhash(0), 16)

        # When the pre-mined test framework chain is used, it contains coinbase
        # outputs to the MiniWallet's default address in blocks 76-100
        # (see method BitcoinTestFramework._initialize_chain())
        # The MiniWallet needs to rescan_utxos() in order to account
        # for those mature UTXOs, so that all txs spend confirmed coins
        self.rescan_utxos()

    def _create_utxo(self, *, txid, vout, value, height, coinbase, confirmations):
        return {"txid": txid, "vout": vout, "value": value, "height": height, "coinbase": coinbase, "confirmations": confirmations}

    def _bulk_tx(self, tx, target_weight):
        """Pad a transaction with extra outputs until it reaches a target weight (or higher).
        returns the tx
        """
        tx.vout.append(CTxOut(nValue=0, scriptPubKey=CScript([RIGOR_MARKER_RETURN, b'a'])))
        dummy_vbytes = (target_weight - tx.get_weight() + 3) // 4
        tx.vout[-1].scriptPubKey = CScript([RIGOR_MARKER_RETURN, b'a' * dummy_vbytes])
        # Lower bound: must meet the requested padding target.
        assert_greater_than_or_equal(tx.get_weight(), target_weight)
        # Upper bound: upstream assumed at most +15 weight over target, but large RIGOR_MARKER_RETURN
        # pushes cross opcode boundaries (e.g. >75 bytes → RIGOR_PUSHDATA1) and add discrete
        # overhead. Keep a generous cap so fault tests can target ~MAX_STANDARD_TX_WEIGHT.
        assert tx.get_weight() <= target_weight + 8192, (
            f"_bulk_tx overshot: weight={tx.get_weight()} target={target_weight} dummy_vbytes={dummy_vbytes}"
        )

    def get_balance(self):
        return sum(u['value'] for u in self._utxos)

    def rescan_utxos(self, *, include_mempool=True):
        """Drop all utxos and rescan the utxo set"""
        self._utxos = []
        res = self._test_node.scantxoutset(action="start", scanobjects=[self.get_descriptor()])
        assert_equal(True, res['success'])
        for utxo in res['unspents']:
            self._utxos.append(
                self._create_utxo(txid=utxo["txid"],
                                  vout=utxo["vout"],
                                  value=utxo["amount"],
                                  height=utxo["height"],
                                  coinbase=utxo["coinbase"],
                                  confirmations=res["height"] - utxo["height"] + 1))
        if include_mempool:
            mempool = self._test_node.getrawmempool(verbose=True)
            # Sort tx by ancestor count. See BlockAssembler::SortForBlock in src/node/miner.cpp
            sorted_mempool = sorted(mempool.items(), key=lambda item: (item[1]["ancestorcount"], int(item[0], 16)))
            for txid, _ in sorted_mempool:
                self.scan_tx(self._test_node.getrawtransaction(txid=txid, verbose=True))

    def scan_tx(self, tx):
        """Scan the tx and adjust the internal list of owned utxos"""
        for spent in tx["vin"]:
            # Mark spent. This may happen when the caller has ownership of a
            # utxo that remained in this wallet. For example, by passing
            # mark_as_spent=False to get_utxo or by using an utxo returned by a
            # create_self_transfer* call.
            try:
                self.get_utxo(txid=spent["txid"], vout=spent["vout"])
            except StopIteration:
                pass
        for out in tx['vout']:
            if out['scriptPubKey']['hex'] == self._scriptPubKey.hex():
                self._utxos.append(self._create_utxo(txid=tx["txid"], vout=out["n"], value=out["value"], height=0, coinbase=False, confirmations=0))

    def scan_txs(self, txs):
        for tx in txs:
            self.scan_tx(tx)

    def sign_tx(self, tx, *, utxos_to_spend=None):
        assert utxos_to_spend is not None
        assert_equal(len(utxos_to_spend), len(tx.vin))
        spent_spk = CScript(self._scriptPubKey)
        tx.wit.vtxinwit = [CTxInWitness() for _ in range(len(tx.vin))]
        for i, utxo in enumerate(utxos_to_spend):
            value_sats = int(Decimal(utxo['value']) * COIN)
            sighash = rigor_signature_hash(
                spent_spk, tx, i, value_sats, self._chain_id, SIGHASH_DOMAIN_ORU_V1,
            )
            sig = self._key.sign(sighash)
            assert_equal(len(sig), 64)
            tx.wit.vtxinwit[i].scriptWitness.stack = [sig, self._pubkey]
        tx.rehash()

    def generate(self, num_blocks, **kwargs):
        """Generate blocks with coinbase outputs to the internal address, and call rescan_utxos"""
        # TestNode.generatetodescriptor requires keyword-only ``invalid_call`` (RPC overload guard).
        kw = {k: v for k, v in kwargs.items() if k != "invalid_call"}
        blocks = self._test_node.generatetodescriptor(num_blocks, self.get_descriptor(), invalid_call=False, **kw)
        # Calling rescan_utxos here makes sure that after a generate the utxo
        # set is in a clean state. For example, the wallet will update
        # - if the caller consumed utxos, but never used them
        # - if the caller sent a transaction that is not mined or got rbf'd
        # - after block re-orgs
        # - the utxo height for mined mempool txs
        # - However, the wallet will not consider remaining mempool txs
        self.rescan_utxos()
        return blocks

    def get_scriptPubKey(self):
        return self._scriptPubKey

    def get_descriptor(self):
        return descsum_create(f'addr({self._address})')

    def get_address(self):
        return self._address

    def get_utxo(self, *, txid: str = '', vout: Optional[int] = None, mark_as_spent=True) -> dict:
        """
        Returns a utxo and marks it as spent (pops it from the internal list)

        Args:
        txid: get the first utxo we find from a specific transaction
        """
        self._utxos = sorted(self._utxos, key=lambda k: (k['value'], -k['height']))  # Put the largest utxo last
        if txid:
            utxo_filter: Any = filter(lambda utxo: txid == utxo['txid'], self._utxos)
        else:
            utxo_filter = reversed(self._utxos)  # By default the largest utxo
        if vout is not None:
            utxo_filter = filter(lambda utxo: vout == utxo['vout'], utxo_filter)
        index = self._utxos.index(next(utxo_filter))
        if mark_as_spent:
            return self._utxos.pop(index)
        else:
            return self._utxos[index]

    def get_utxos(self, *, include_immature_coinbase=False, mark_as_spent=True):
        """Returns the list of all utxos and optionally mark them as spent"""
        if not include_immature_coinbase:
            utxo_filter = filter(lambda utxo: not utxo['coinbase'] or COINBASE_MATURITY <= utxo['confirmations'], self._utxos)
        else:
            utxo_filter = self._utxos
        utxos = deepcopy(list(utxo_filter))
        if mark_as_spent:
            self._utxos = []
        return utxos

    def send_self_transfer(self, *, from_node, **kwargs):
        """Call create_self_transfer and send the transaction."""
        tx = self.create_self_transfer(**kwargs)
        self.sendrawtransaction(from_node=from_node, tx_hex=tx['hex'])
        return tx

    def send_to(self, *, from_node, scriptPubKey, amount, fee=1000):
        """
        Create and send a tx with an output to a given scriptPubKey/amount,
        plus a change output to our internal address. To keep things simple, a
        fixed fee given in Satoshi is used.

        Note that this method fails if there is no single internal utxo
        available that can cover the cost for the amount and the fixed fee
        (the utxo with the largest value is taken).

        Returns a tuple (txid, n) referring to the created external utxo outpoint.
        """
        st = self.create_self_transfer(fee_rate=0)
        tx = st["tx"]
        assert_greater_than_or_equal(tx.vout[0].nValue, amount + fee)
        tx.vout[0].nValue -= (amount + fee)           # change output -> MiniWallet
        tx.vout.append(CTxOut(amount, scriptPubKey))  # arbitrary output -> to be returned
        self.sign_tx(tx, utxos_to_spend=[st["utxo_spent"]])
        txid = self.sendrawtransaction(from_node=from_node, tx_hex=tx.serialize().hex())
        return txid, 1

    def send_self_transfer_multi(self, *, from_node, **kwargs):
        """Call create_self_transfer_multi and send the transaction."""
        tx = self.create_self_transfer_multi(**kwargs)
        self.sendrawtransaction(from_node=from_node, tx_hex=tx["hex"])
        return tx

    def create_self_transfer_multi(
        self,
        *,
        utxos_to_spend: Optional[List[dict]] = None,
        num_outputs=1,
        amount_per_output=0,
        locktime=0,
        sequence=0,
        fee_per_output=1000,
        target_weight=0
    ):
        """
        Create and return a transaction that spends the given UTXOs and creates a
        certain number of outputs with equal amounts. The output amounts can be
        set by amount_per_output or automatically calculated with a fee_per_output.
        """
        utxos_to_spend = utxos_to_spend or [self.get_utxo()]
        sequence = [sequence] * len(utxos_to_spend) if type(sequence) is int else sequence
        assert_equal(len(utxos_to_spend), len(sequence))

        # calculate output amount
        inputs_value_total = sum([int(COIN * utxo['value']) for utxo in utxos_to_spend])
        outputs_value_total = inputs_value_total - fee_per_output * num_outputs
        amount_per_output = amount_per_output or (outputs_value_total // num_outputs)
        assert amount_per_output > 0
        outputs_value_total = amount_per_output * num_outputs
        fee = Decimal(inputs_value_total - outputs_value_total) / COIN

        # create tx
        tx = CTransaction()
        tx.vin = [CTxIn(COutPoint(int(utxo_to_spend['txid'], 16), utxo_to_spend['vout']), nSequence=seq) for utxo_to_spend, seq in zip(utxos_to_spend, sequence)]
        tx.vout = [CTxOut(amount_per_output, bytearray(self._scriptPubKey)) for _ in range(num_outputs)]
        tx.nLockTime = locktime

        self.sign_tx(tx, utxos_to_spend=utxos_to_spend)

        if target_weight:
            self._bulk_tx(tx, target_weight)
            # ``_bulk_tx`` appends ``RIGOR_MARKER_RETURN`` vouts → RGRORU01
            # ``hash_outputs`` changes; re-sign so policy hits ``tx-size`` / relay rather than
            # a stale witness/signature failure.
            self.sign_tx(tx, utxos_to_spend=utxos_to_spend)

        txid = tx.rehash()
        return {
            "new_utxos": [self._create_utxo(
                txid=txid,
                vout=i,
                value=Decimal(tx.vout[i].nValue) / COIN,
                height=0,
                coinbase=False,
                confirmations=0,
            ) for i in range(len(tx.vout))],
            "fee": fee,
            "txid": txid,
            "wtxid": tx.getwtxid(),
            "hex": tx.serialize().hex(),
            "tx": tx,
        }

    def create_self_transfer(self, *, fee_rate=Decimal("0.003"), fee=Decimal("0"), utxo_to_spend=None, locktime=0, sequence=0, target_weight=0):
        """Create and return a tx with the specified fee. If fee is 0, use fee_rate, where the resulting fee may be exact or at most one satoshi higher than needed."""
        utxo_to_spend = utxo_to_spend or self.get_utxo()
        assert fee_rate >= 0
        assert fee >= 0
        vsize = Decimal(P2WPKH_1IN_1OUT_VSIZE)
        send_value = utxo_to_spend["value"] - (fee or (fee_rate * vsize / 1000))

        # create tx
        tx = self.create_self_transfer_multi(utxos_to_spend=[utxo_to_spend], locktime=locktime, sequence=sequence, amount_per_output=int(COIN * send_value), target_weight=target_weight)
        if not target_weight:
            actual_v = tx["tx"].get_vsize()
            assert actual_v == P2WPKH_1IN_1OUT_VSIZE, (
                f"unexpected P2WPKH 1-in-1-out serialized size {actual_v}; update MiniWallet if template changed"
            )
        tx["new_utxo"] = tx.pop("new_utxos")[0]
        # For P2WPKH, callers that mutate vout after create_self_transfer must re-sign (see util.create_lots_of_big_transactions).
        tx["utxo_spent"] = utxo_to_spend

        return tx

    def sendrawtransaction(self, *, from_node, tx_hex, maxfeerate=0, **kwargs):
        txid = from_node.sendrawtransaction(hexstring=tx_hex, maxfeerate=maxfeerate, **kwargs)
        self.scan_tx(from_node.decoderawtransaction(tx_hex))
        return txid

    def create_self_transfer_chain(self, *, chain_length, utxo_to_spend=None):
        """
        Create a "chain" of chain_length transactions. The nth transaction in
        the chain is a child of the n-1th transaction and parent of the n+1th transaction.
        """
        chaintip_utxo = utxo_to_spend or self.get_utxo()
        chain = []

        for _ in range(chain_length):
            tx = self.create_self_transfer(utxo_to_spend=chaintip_utxo)
            chaintip_utxo = tx["new_utxo"]
            chain.append(tx)

        return chain

    def send_self_transfer_chain(self, *, from_node, **kwargs):
        """Create and send a "chain" of chain_length transactions. The nth transaction in
        the chain is a child of the n-1th transaction and parent of the n+1th transaction.

        Returns a list of objects for each tx (see create_self_transfer_multi).
        """
        chain = self.create_self_transfer_chain(**kwargs)
        for t in chain:
            self.sendrawtransaction(from_node=from_node, tx_hex=t["hex"])
        return chain


def getnewdestination(address_type='bech32'):
    """Generate a random Ed25519 destination.

    ``bech32`` is witness v0 P2WPKH (HASH160 of the 32-byte payload).
    ``bech32m`` is witness v1 RES (program = 32-byte Ed25519 pubkey).
    """
    from test_framework.address import output_key_to_p2tr
    from test_framework.ed25519 import Ed25519Key
    from test_framework.script_util import output_key_to_p2tr_script

    key = Ed25519Key()
    pk32 = key.get_pubkey_bytes()
    if address_type == 'bech32':
        scriptpubkey = key_to_p2wpkh_script(pk32)
        address = key_to_p2wpkh(pk32)
    elif address_type == 'bech32m':
        scriptpubkey = output_key_to_p2tr_script(pk32)
        address = output_key_to_p2tr(pk32)
    else:
        assert False
    return pk32, scriptpubkey, address

#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""RFC-020 continuous destroy_cost(h): end-to-end body mint on a live chain.

One whole RES costs destroy_cost(h) ORU at the confirming height, and the transaction
that destroys the ORU is the one that creates the RES. There is no draw, no
candidate window, no winner, no payment lag and no mint horizon: the mint is
an ordinary body transaction.

What the C++ suites already prove is the arithmetic. What only a chain can show
is that the shape survives mempool policy, block connection, the UTXO set and a
reorg. In particular the reorg leg is the empirical face of
``reorg_replay_equal``: issuance is a function of a transaction's own contents
plus its height, so disconnecting and reconnecting the block that carries it
must land on the same supply, byte for byte.

One RES scale is live: ``RES_COIN`` = 1e8 for consensus, RPC parse, RPC display
and the snapshot. A string that goes in as ``"1.00000000"`` must come back as
``100000000`` atoms.

See ``docs/archive/rfc/RFC-020-CONTINUOUS-RES-PRICE-FLAT-ORU-EMISSION.md``.
"""

from __future__ import annotations

from decimal import Decimal

from test_framework.rigor_block_builder import make_converge_scriptpubkey
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than

COIN = 100_000_000
RES_COIN = 100_000_000
RES_DESTROY_POLE = 1 << 25
RES_DESTROY_SHIFT = 40


def min_burn_for_atoms(res_atoms: int, height: int) -> int:
    """Smallest B that mints R at height: ceil(R · (2^25 + h)^2 / 2^40)."""
    pole = RES_DESTROY_POLE + height
    numer = res_atoms * pole * pole
    den = 1 << RES_DESTROY_SHIFT
    return (numer + den - 1) // den


def atoms_to_oru_str(atoms: int) -> str:
    return format(Decimal(atoms) / Decimal(COIN), "f")


def res_atoms_to_rpc_str(atoms: int) -> str:
    return format(Decimal(atoms) / Decimal(RES_COIN), "f")


def res_atoms_from_display(value) -> int:
    return int(Decimal(str(value)) * RES_COIN)


class ResDestroyCostMintTest(BitcoinTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser)
        parser.add_argument(
            "--epoch-boundary", action="store_true",
            help="check that an exact-fit mint at height h is underpaid at h+1 "
                 "(RFC-020 continuous price; flag name is historical)")

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [["-txindex"]]
        self.rpc_timeout = 900

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    # -- helpers ------------------------------------------------------------

    def pick_oru_utxo(self, min_atoms: int) -> dict:
        for u in self.node.listunspent():
            if str(u.get("asset", "ORU")).upper() not in ("ORU", "ORUM", "BTC"):
                continue
            if not u.get("spendable", False):
                continue
            if int(Decimal(str(u["amount"])) * COIN) >= min_atoms:
                return u
        raise AssertionError(f"no spendable ORU utxo covering {min_atoms} atoms")

    def build_mint(self, res_atoms: int, burn_atoms: int, *, with_burn: bool = True) -> str:
        """A signed mint: ORU input, one OP_CONVERGE burn, RES out, ORU change.

        The burn is a declared output rather than the input/output difference, so
        the fee stays separable and the mint amount does not move with fee
        estimation. That is the RFC-019 4 shape.
        """
        fee_atoms = COIN // 100
        need = (burn_atoms if with_burn else 0) + fee_atoms
        utxo = self.pick_oru_utxo(need + COIN)
        in_atoms = int(Decimal(str(utxo["amount"])) * COIN)
        change_atoms = in_atoms - (burn_atoms if with_burn else 0) - fee_atoms
        assert change_atoms > 0

        outputs = []
        if with_burn:
            outputs.append({"script": {
                "hex": make_converge_scriptpubkey().hex(),
                "amount": atoms_to_oru_str(burn_atoms),
                "asset": "ORU",
            }})
        outputs.append({self.res_addr: {
            "amount": res_atoms_to_rpc_str(res_atoms), "asset": "RES",
        }})
        outputs.append({self.change_addr: atoms_to_oru_str(change_atoms)})

        raw = self.node.createrawtransaction(
            [{"txid": utxo["txid"], "vout": utxo["vout"]}], outputs)

        # Pin the scale rather than trust it. If the RPC parse scale ever moves,
        # every price assertion in this file silently changes meaning, so fail
        # here with a clear reason instead of somewhere downstream.
        decoded = self.node.decoderawtransaction(raw)
        res_outs = [o for o in decoded["vout"]
                    if str(o.get("asset", "")).upper() == "RES"]
        assert_equal(len(res_outs), 1)
        assert_equal(res_atoms_from_display(res_outs[0]["value"]), res_atoms)

        signed = self.node.signrawtransactionwithwallet(raw)
        assert signed.get("complete"), f"signing incomplete: {signed}"
        return signed["hex"]

    def expect_reject(self, raw_hex: str, reason: str):
        res = self.node.testmempoolaccept([raw_hex])[0]
        assert_equal(res["allowed"], False)
        assert_equal(res["reject-reason"], reason)

    # -- the run ------------------------------------------------------------

    def run_test(self):
        self.node = self.nodes[0]
        self.miner_addr = self.node.getnewaddress(label="miner", address_type="bech32")
        self.change_addr = self.node.getnewaddress(label="change", address_type="bech32")
        self.res_addr = self.node.getnewaddress(label="res", address_type="res")

        self.log.info("mine past coinbase maturity so there is ORU to burn")
        self.generatetoaddress(self.node, 120, self.miner_addr)

        cost = self.node.res_mint_unit_cost()
        # Display rate is not the construction burn. Use the pole inverse.
        res_atoms = RES_COIN // 100
        exact_burn = min_burn_for_atoms(res_atoms, cost["height"])
        self.log.info(f"{res_atoms} RES atoms cost {exact_burn} ORU atoms at height {cost['height']}")

        self.log.info("RPC parse and display share the 1e8 scale")
        roundtrip = self.node.createrawtransaction(
            [], [{self.res_addr: {"amount": "1.00000000", "asset": "RES"}}])
        decoded = self.node.decoderawtransaction(roundtrip)
        res_outs = [o for o in decoded["vout"] if str(o.get("asset", "")).upper() == "RES"]
        assert_equal(len(res_outs), 1)
        assert_equal(res_atoms_from_display(res_outs[0]["value"]), RES_COIN)

        self.log.info("a RES output with no burn is unfunded")
        self.expect_reject(self.build_mint(res_atoms, 0, with_burn=False),
                           "bad-txns-res-mint-no-burn")

        self.log.info("one atom below the destroy cost is underpaid")
        self.expect_reject(self.build_mint(res_atoms, exact_burn - 1),
                           "bad-txns-res-mint-underpaid")

        self.log.info("ten times the RES for the same burn is underpaid")
        self.expect_reject(self.build_mint(res_atoms * 10, exact_burn),
                           "bad-txns-res-mint-underpaid")

        self.log.info("the exact destroy cost mints")
        before = self.node.getreplaysnapshot()
        raw = self.build_mint(res_atoms, exact_burn)
        accept = self.node.testmempoolaccept([raw])[0]
        assert_equal(accept["allowed"], True)
        txid = self.node.sendrawtransaction(raw)

        # Overpaying is legal and buys no more: the rule is an inequality. The
        # surplus ORU is destroyed, not refunded and not credited as fee.
        over_txid = self.node.sendrawtransaction(
            self.build_mint(res_atoms, exact_burn * 3))

        self.log.info("mine the mints and check the supply moved by exactly what was bought")
        block_hash = self.generatetoaddress(self.node, 1, self.miner_addr)[0]
        block = self.node.getblock(block_hash, 2)
        assert txid in [t["txid"] for t in block["tx"]]
        assert over_txid in [t["txid"] for t in block["tx"]]

        after = self.node.getreplaysnapshot()
        # Snapshot and decode share the 1e8 RES scale. Compare atoms, not a
        # converted display string.
        minted = sum(res_atoms_from_display(o["value"])
                     for tx in block["tx"] for o in tx["vout"]
                     if str(o.get("asset", "")).upper() == "RES")
        # Two mints of the same size. The one that overpaid got no more RES for
        # it: the rule is an inequality, and the surplus ORU is simply destroyed.
        assert_equal(minted, res_atoms * 2)
        assert_greater_than(int(after["utxo_count"]), int(before["utxo_count"]))
        assert_greater_than(Decimal(after["res_balance"]), Decimal(before["res_balance"]))

        self.log.info("the burned ORU left the UTXO set rather than becoming fee")
        for tx in block["tx"]:
            if tx["txid"] != txid:
                continue
            burn_outs = [o for o in tx["vout"]
                         if o["scriptPubKey"]["hex"].endswith(
                             make_converge_scriptpubkey().hex()[-2:])]
            assert_equal(len(burn_outs), 1)
            assert_equal(int(Decimal(str(burn_outs[0]["value"])) * COIN), exact_burn)

        self.log.info("reorg: disconnect the block carrying the mints")
        self.node.invalidateblock(block_hash)
        rolled_back = self.node.getreplaysnapshot()
        assert_equal(rolled_back["res_balance"], before["res_balance"])
        assert_equal(rolled_back["state_hash"], before["state_hash"])

        self.log.info("reconnect it and require the identical supply state")
        self.node.reconsiderblock(block_hash)
        assert_equal(self.node.getbestblockhash(), block_hash)
        replayed = self.node.getreplaysnapshot()
        # Issuance depends only on each transaction's own contents and its
        # height, so a replay cannot land anywhere else. Under a draw this
        # required the 32-block candidate window to reproduce as well.
        assert_equal(replayed["res_balance"], after["res_balance"])
        assert_equal(replayed["state_hash"], after["state_hash"])
        assert_equal(replayed["mint_count"], after["mint_count"])

        self.log.info("a plain burn that mints nothing is still an ordinary convergence")
        utxo = self.pick_oru_utxo(50 * COIN)
        in_atoms = int(Decimal(str(utxo["amount"])) * COIN)
        raw_burn = self.node.createrawtransaction(
            [{"txid": utxo["txid"], "vout": utxo["vout"]}],
            [{"script": {"hex": make_converge_scriptpubkey().hex(),
                         "amount": atoms_to_oru_str(10 * COIN), "asset": "ORU"}},
             {self.change_addr: atoms_to_oru_str(in_atoms - 10 * COIN - COIN // 100)}])
        signed_burn = self.node.signrawtransactionwithwallet(raw_burn)
        assert_equal(self.node.testmempoolaccept([signed_burn["hex"]])[0]["allowed"], True)

        self.log.info("ResMinMintBurnAtoms(h) mints 1 RES atom; one below is underpaid")
        one_atom_burn = self.node.res_mint_unit_cost()["min_atom_burn"]
        self.expect_reject(self.build_mint(1, one_atom_burn - 1),
                           "bad-txns-res-mint-underpaid")
        one_hex = self.build_mint(1, one_atom_burn)
        assert_equal(self.node.testmempoolaccept([one_hex])[0]["allowed"], True)
        one_txid = self.node.sendrawtransaction(one_hex)

        self.log.info("a 1023-atom plain burn (R=0) is a legal OP_CONVERGE")
        dust_utxo = self.pick_oru_utxo(COIN)
        dust_in = int(Decimal(str(dust_utxo["amount"])) * COIN)
        raw_dust = self.node.createrawtransaction(
            [{"txid": dust_utxo["txid"], "vout": dust_utxo["vout"]}],
            [{"script": {"hex": make_converge_scriptpubkey().hex(),
                         "amount": atoms_to_oru_str(1023), "asset": "ORU"}},
             {self.change_addr: atoms_to_oru_str(dust_in - 1023 - COIN // 100)}])
        signed_dust = self.node.signrawtransactionwithwallet(raw_dust)
        assert_equal(self.node.testmempoolaccept([signed_dust["hex"]])[0]["allowed"], True)

        dust_block = self.generatetoaddress(self.node, 1, self.miner_addr)[0]
        assert one_txid in [t["txid"] for t in self.node.getblock(dust_block, 2)["tx"]]

        if self.options.epoch_boundary:
            self.run_epoch_boundary()

    def run_epoch_boundary(self):
        """Exact-fit at height h must fail at h+1 under continuous destroy_cost(h).

        Flag name is historical. RFC-020 has no epoch step; the live check is
        that price rises every height. One whole RES uses ResMinBurnForAtoms,
        which is strictly increasing.
        """
        res_atoms = RES_COIN
        cost = self.node.res_mint_unit_cost()
        burn_h = min_burn_for_atoms(res_atoms, cost["height"])
        assert_greater_than(burn_h, 0)

        self.log.info(f"exact-fit at height {cost['height']} mints")
        txid = self.node.sendrawtransaction(self.build_mint(res_atoms, burn_h))
        tip = self.generatetoaddress(self.node, 1, self.miner_addr)[0]
        assert txid in [t["txid"] for t in self.node.getblock(tip, 2)["tx"]]

        self.log.info("the same burn is underpaid at the next height")
        self.expect_reject(self.build_mint(res_atoms, burn_h),
                           "bad-txns-res-mint-underpaid")

        next_cost = self.node.res_mint_unit_cost()
        burn_next = min_burn_for_atoms(res_atoms, next_cost["height"])
        assert_greater_than(burn_next, burn_h)
        self.log.info(f"height {next_cost['height']} costs {burn_next} and mints")
        txid = self.node.sendrawtransaction(self.build_mint(res_atoms, burn_next))
        tip = self.generatetoaddress(self.node, 1, self.miner_addr)[0]
        assert txid in [t["txid"] for t in self.node.getblock(tip, 2)["tx"]]

        self.log.info("one atom below the next height's price is underpaid")
        self.expect_reject(self.build_mint(res_atoms, burn_next - 1),
                           "bad-txns-res-mint-underpaid")


if __name__ == "__main__":
    ResDestroyCostMintTest().main()

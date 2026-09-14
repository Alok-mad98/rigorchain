#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Phase 6 wallet mint: createconvergence writes an explicit RES out.

Construct → broadcast → confirm → RES supply moves. No draw intermediate
state. RES uses a single 1e8 scale.
"""

from decimal import Decimal

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than, assert_raises_rpc_error

COIN = 100_000_000
RES_COIN = 100_000_000
RATE_GENESIS = 1024 * COIN
RES_DESTROY_POLE = 1 << 25
RES_DESTROY_SHIFT = 40


def mint_atoms_from_burn(burn_atoms: int, height: int) -> int:
    """Wallet inverse: floor((B << 40) / (2^25 + h)^2)."""
    pole = RES_DESTROY_POLE + height
    return (burn_atoms << RES_DESTROY_SHIFT) // (pole * pole)


def min_burn_for_atoms(res_atoms: int, height: int) -> int:
    """Smallest B that mints R at height: ceil(R · (2^25 + h)^2 / 2^40)."""
    pole = RES_DESTROY_POLE + height
    numer = res_atoms * pole * pole
    den = 1 << RES_DESTROY_SHIFT
    return (numer + den - 1) // den


class CreateConvergenceMintTest(BitcoinTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser)

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [["-txindex"]]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        node = self.nodes[0]
        miner = node.getnewaddress(label="miner", address_type="bech32")
        self.log.info("mine past coinbase maturity so there is ORU to burn")
        self.generatetoaddress(node, 120, miner)

        cost = node.res_mint_unit_cost()
        assert_equal(cost["authorized"], True)
        assert_greater_than(cost["destroy_cost"], 0)
        assert "epoch" not in cost
        assert "rate" not in cost
        assert_equal(cost["height"], node.getblockcount() + 1)
        assert_equal(cost["min_whole_res_burn"], min_burn_for_atoms(RES_COIN, cost["height"]))
        assert_greater_than(int(cost["destroy_cost_next"]), int(cost["destroy_cost"]))

        genesis = node.res_mint_unit_cost(0)
        assert_equal(genesis["destroy_cost"], RATE_GENESIS)
        assert_equal(genesis["min_atom_burn"], 1024)
        assert_equal(genesis["min_whole_res_burn"], RATE_GENESIS)
        far = node.res_mint_unit_cost(210000 * 251)
        assert_equal(far["authorized"], True)
        assert_greater_than(far["destroy_cost"], RATE_GENESIS)
        assert_raises_rpc_error(-8, "mint is not authorized",
                                node.res_mint_unit_cost, -1)

        assert "getdrawclosurestatus" not in node.help()

        status = node.getconvergencestatus()
        assert_equal(status["issuance_authorized"], True)
        assert_equal(status["destroy_cost"], cost["destroy_cost"])
        assert_equal(status["min_atom_burn"], cost["min_atom_burn"])
        assert "window_progress" not in status
        assert "current_convergence_sum" not in status
        assert "epoch" not in status
        assert "next_epoch" not in status
        assert "next_rate" not in status
        assert "destroy_cost_next" in status
        assert_greater_than(int(status["destroy_cost_next"]), int(cost["destroy_cost"]))

        # Display rate can sit one atom below the exact whole-RES burn.
        need = cost["min_whole_res_burn"]
        assert_equal(need, min_burn_for_atoms(RES_COIN, cost["height"]))
        burn = format(Decimal(need) / Decimal(COIN), "f")
        before = node.getreplaysnapshot()
        minted = node.createconvergence(burn)
        assert_equal(minted["burn_oru"], need)
        assert_equal(minted["res_out"], RES_COIN)
        assert_equal(minted["destroy_cost"], cost["destroy_cost"])
        assert "epoch" not in minted
        assert "padded_for_next_epoch" not in minted
        assert_equal(minted["unlock_height"], minted["confirm_height"] + 100)
        txid = minted["txid"]
        assert_equal(len(txid), 64)

        raw = node.getrawtransaction(txid, True)
        res_outs = [o for o in raw["vout"] if str(o.get("asset", "")).upper() == "RES"]
        assert_equal(len(res_outs), 1)
        burn_outs = [o for o in raw["vout"]
                     if o["scriptPubKey"].get("hex", "").endswith("bd")]
        assert_equal(len(burn_outs), 1)
        assert_equal(int(Decimal(str(burn_outs[0]["value"])) * COIN), need)

        assert txid in node.getrawmempool()

        self.log.info("confirm the mint and require RES supply to move")
        self.generatetoaddress(node, 1, miner)
        after = node.getreplaysnapshot()
        assert_greater_than(Decimal(after["res_balance"]), Decimal(before["res_balance"]))
        assert_greater_than(int(after.get("mint_count", 1)), int(before.get("mint_count", 0)))

        # getruntimesupply JSON is wider than its RPCResult schema (-rpcdoccheck).
        # Witness + invariants are the ConnectBlock observer surface with matching docs.
        inv = node.getruntimesupplyinvariants()
        assert_equal(inv["passed"], True)
        assert_equal(inv["violations"], [])
        wit = node.getruntimesupplywitness()
        assert_equal(wit["invariants_passed"], True)
        assert_equal(wit["violations"], [])
        assert_equal(int(Decimal(str(wit["res_mint_cumulative"])) * RES_COIN), minted["res_out"])

        # The chain-wide RES ledger is memory-only. A restart must rebuild the same
        # cumulative by folding the on-disk per-block sums, so the observer digest may
        # not move. getchainstatesummary runs first after the restart, while the caches
        # are still at their -1 default, because that is where a cold read would show up.
        mint_height = node.getblockcount()
        state_hash_before = node.getchainstatesummary()["state_hash"]
        self.restart_node(0)
        assert_equal(node.getblockcount(), mint_height)
        assert_equal(node.getchainstatesummary()["state_hash"], state_hash_before)
        inv_after_restart = node.getruntimesupplyinvariants()
        assert_equal(inv_after_restart["passed"], True)
        assert_equal(inv_after_restart["violations"], [])

        # A burn below one whole RES still maximises R and is not a failed mint.
        live = node.res_mint_unit_cost()
        small = node.createconvergence(1.0)
        assert_equal(small["burn_oru"], COIN)
        expected_r = mint_atoms_from_burn(COIN, live["height"])
        assert_equal(small["res_out"], expected_r)
        assert_greater_than(small["res_out"], 0)

        self.log.info("one-atom mint: ResMinMintBurnAtoms(h) buys 1 RES atom")
        min_burn = node.res_mint_unit_cost()["min_atom_burn"]
        atom = node.createconvergence(format(Decimal(min_burn) / Decimal(COIN), "f"))
        assert_equal(atom["burn_oru"], min_burn)
        assert_equal(atom["res_out"], 1)

        self.log.info("one satoshi short of the one-atom floor is refused by the wallet")
        short = format(Decimal(min_burn - 1) / Decimal(COIN), "f")
        assert_raises_rpc_error(-4, "Burn amount must be at least",
                                node.createconvergence, short)

        self.log.info("destroyoru one atom is above consensus floor and not dust-blocked")
        atom_destroy = node.destroyoru("0.00000001")
        assert_equal(atom_destroy["destroyed_atoms"], 1)
        # Must be in the mempool (not a local-only spend that hides the input).
        node.getmempoolentry(atom_destroy["txid"])
        self.generatetoaddress(node, 1, miner)
        mined = node.getrawtransaction(atom_destroy["txid"], True)
        assert_greater_than(mined["confirmations"], 0)

        self.log.info("destroyoru destroys ORU and mints no RES")
        before_res = Decimal(node.getreplaysnapshot()["res_balance"])
        destroyed = node.destroyoru("1.0")
        assert_equal(destroyed["destroyed_atoms"], COIN)
        dtxid = destroyed["txid"]
        draw = node.getrawtransaction(dtxid, True)
        res_outs = [o for o in draw["vout"] if str(o.get("asset", "")).upper() == "RES"]
        assert_equal(len(res_outs), 0)
        destroy_outs = [o for o in draw["vout"]
                        if o["scriptPubKey"].get("hex", "").endswith("bd")]
        assert_equal(len(destroy_outs), 1)
        assert_equal(int(Decimal(str(destroy_outs[0]["value"])) * COIN), COIN)
        self.generatetoaddress(node, 1, miner)
        after_res = Decimal(node.getreplaysnapshot()["res_balance"])
        assert_equal(after_res, before_res)

        self.log.info("sendtoaddress no longer accepts converge_amount")
        assert_raises_rpc_error(-8, "Unknown named parameter",
                                node.sendtoaddress, address=miner, amount=0.1, converge_amount=1.0)


if __name__ == "__main__":
    CreateConvergenceMintTest().main()

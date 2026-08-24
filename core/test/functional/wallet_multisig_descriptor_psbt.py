#!/usr/bin/env python3
# Copyright (c) 2021-2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test a basic M-of-N multisig setup between multiple people using descriptor wallets and PSBTs, as well as a signing flow.

Ed25519 BIP32 is hardened-only (SLIP-0010), so watch-only wallets cannot expand
ranged xpubs. Participants exchange concrete pubkeys for a single receive and
change script instead of a ranged xpub descriptor.
"""

from test_framework.descriptors import descsum_create
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_approx,
    assert_equal,
)


class WalletMultisigDescriptorPSBTTest(BitcoinTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser, legacy=False)

    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True
        self.wallet_names = []
        self.extra_args = [["-keypool=100"]] * self.num_nodes

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()
        self.skip_if_no_sqlite()

    @staticmethod
    def _check_psbt(psbt, to, value, multisig):
        """Helper function for any of the N participants to check the psbt with decodepsbt and verify it is OK before signing."""
        tx = multisig.decodepsbt(psbt)["tx"]
        amount = 0
        for vout in tx["vout"]:
            address = vout["scriptPubKey"]["address"]
            assert_equal(multisig.getaddressinfo(address)["ischange"], address != to)
            if address == to:
                amount += vout["value"]
        assert_approx(amount, float(value), vspan=0.001)

    def participants_create_multisigs(self, recv_pks, chg_pks):
        """Import the same non-ranged sortedmulti scripts into a watch-only wallet on each node."""
        recv_desc = descsum_create(f"wsh(sortedmulti({self.M},{','.join(recv_pks)}))")
        chg_desc = descsum_create(f"wsh(sortedmulti({self.M},{','.join(chg_pks)}))")
        for i, node in enumerate(self.nodes):
            node.createwallet(wallet_name=f"{self.name}_{i}", blank=True, descriptors=True, disable_private_keys=True)
            multisig = node.get_wallet_rpc(f"{self.name}_{i}")
            result = multisig.importdescriptors([
                {
                    "desc": recv_desc,
                    "active": True,
                    "internal": False,
                    "timestamp": "now",
                },
                {
                    "desc": chg_desc,
                    "active": True,
                    "internal": True,
                    "timestamp": "now",
                },
            ])
            assert all(r["success"] for r in result)
            yield multisig

    def run_test(self):
        self.M = 2
        self.N = self.num_nodes
        self.name = f"{self.M}_of_{self.N}_multisig"
        self.log.info(f"Testing {self.name}...")

        participants = {
            "signers": [node.get_wallet_rpc(node.createwallet(wallet_name=f"participant_{self.nodes.index(node)}", descriptors=True)["name"]) for node in self.nodes],
            "multisigs": []
        }

        self.log.info("Each participant contributes one receive pubkey and one change pubkey...")
        recv_pks = []
        chg_pks = []
        for signer in participants["signers"]:
            recv = signer.getnewaddress("", "bech32")
            chg = signer.getrawchangeaddress("bech32")
            recv_pks.append(signer.getaddressinfo(recv)["pubkey"])
            chg_pks.append(signer.getaddressinfo(chg)["pubkey"])

        self.log.info("Every participant imports the same watch-only sortedmulti scripts...")
        participants["multisigs"] = list(self.participants_create_multisigs(recv_pks, chg_pks))

        self.log.info("Check that every participant's multisig generates the same addresses...")
        receive_addresses = [multisig.getnewaddress(address_type='bech32m') for multisig in participants["multisigs"]]
        assert all(address == receive_addresses[0] for address in receive_addresses)
        change_addresses = [multisig.getrawchangeaddress() for multisig in participants["multisigs"]]
        assert all(address == change_addresses[0] for address in change_addresses)

        self.log.info("Get a mature utxo to send to the multisig...")
        coordinator_wallet = participants["signers"][0]
        self.generatetoaddress(self.nodes[0], 101, coordinator_wallet.getnewaddress(address_type='bech32m'))

        deposit_amount = 6.15
        multisig_receiving_address = participants["multisigs"][0].getnewaddress(address_type='bech32m')
        self.log.info("Send funds to the resulting multisig receiving address...")
        coordinator_wallet.sendtoaddress(multisig_receiving_address, deposit_amount)
        self.generate(self.nodes[0], 1)
        for participant in participants["multisigs"]:
            assert_approx(participant.getbalance(), deposit_amount, vspan=0.001)

        self.log.info("Send a transaction from the multisig!")
        to = participants["signers"][self.N - 1].getnewaddress(address_type='bech32m')
        value = 1
        self.log.info("First, make a sending transaction, created using `walletcreatefundedpsbt` (anyone can initiate this)...")
        psbt = participants["multisigs"][0].walletcreatefundedpsbt(inputs=[], outputs={to: value}, options={"feeRate": 0.00010})

        psbts = []
        self.log.info("Now at least M users check the psbt with decodepsbt and (if OK) signs it with walletprocesspsbt...")
        for m in range(self.M):
            signers_multisig = participants["multisigs"][m]
            self._check_psbt(psbt["psbt"], to, value, signers_multisig)
            signing_wallet = participants["signers"][m]
            partially_signed_psbt = signing_wallet.walletprocesspsbt(psbt["psbt"])
            psbts.append(partially_signed_psbt["psbt"])

        self.log.info("Finally, collect the signed PSBTs with combinepsbt, finalizepsbt, then broadcast the resulting transaction...")
        combined = coordinator_wallet.combinepsbt(psbts)
        finalized = coordinator_wallet.finalizepsbt(combined)
        coordinator_wallet.sendrawtransaction(finalized["hex"])

        self.log.info("Check that balances are correct after the transaction has been included in a block.")
        self.generate(self.nodes[0], 1)
        assert_approx(participants["multisigs"][0].getbalance(), deposit_amount - value, vspan=0.001)
        assert_equal(participants["signers"][self.N - 1].getbalance(), value)

        self.log.info("Send another transaction from the multisig, this time with a daisy chained signing flow (one after another in series)!")
        psbt = participants["multisigs"][0].walletcreatefundedpsbt(inputs=[], outputs={to: value}, options={"feeRate": 0.00010})
        for m in range(self.M):
            signers_multisig = participants["multisigs"][m]
            self._check_psbt(psbt["psbt"], to, value, signers_multisig)
            signing_wallet = participants["signers"][m]
            psbt = signing_wallet.walletprocesspsbt(psbt["psbt"])
            assert_equal(psbt["complete"], m == self.M - 1)
        finalized = coordinator_wallet.finalizepsbt(psbt["psbt"])
        coordinator_wallet.sendrawtransaction(finalized["hex"])

        self.log.info("Check that balances are correct after the transaction has been included in a block.")
        self.generate(self.nodes[0], 1)
        assert_approx(participants["multisigs"][0].getbalance(), deposit_amount - (value * 2), vspan=0.001)
        assert_equal(participants["signers"][self.N - 1].getbalance(), value * 2)


if __name__ == "__main__":
    WalletMultisigDescriptorPSBTTest().main()

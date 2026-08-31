#!/usr/bin/env python3
# Copyright (c) 2026 The RIGOR developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Spendable outputs are one Ed25519 public key.

RIGOR locks are ORU (`wpkh`, witness v0 keyhash) or RES (`res`, witness v1). Each
binds one public key and is spent with one signature. Anything else is not a
lock: CheckTransaction rejects it as `bad-rigor-script-type` before policy or
wallet code runs.

Bitcoin leftover constructors (P2PKH, P2WSH, P2SH, CHECKMULTISIG) appear here
only as illegal payloads. They are not RIGOR address types.
"""

from test_framework.script import (
    CScript,
    RIGOR_PUSH_1,
    RIGOR_RESERVED_LEGACY_CHECKMULTISIG,
)
from test_framework.script_util import (
    key_to_p2pkh_script,
    keys_to_multisig_script,
    script_to_p2sh_script,
    script_to_p2wsh_script,
)
from test_framework.descriptors import descsum_create
from test_framework.ed25519 import Ed25519Key
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_raises_rpc_error,
    assert_equal,
)
from test_framework.wallet import MiniWallet

RPC_METHOD_NOT_FOUND = -32601
BAD_SCRIPT = "bad-rigor-script-type"


class SingleKeyOutputsTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1

    def test_bitcoin_rpcs_absent(self):
        node = self.nodes[0]
        self.log.info("Bitcoin leftover RPCs are not registered")
        for method in ("createmultisig", "addmultisigaddress"):
            assert_raises_rpc_error(RPC_METHOD_NOT_FOUND, "Method not found",
                                    node.__getattr__(method), 2, [])
            assert_equal(node.help(method), f"help: unknown command: {method}")

    def test_bitcoin_descriptors_rejected(self, keys):
        node = self.nodes[0]
        a, b = keys[0].hex(), keys[1].hex()
        self.log.info("Descriptors other than wpkh() and res() do not parse")
        for expr, needle in (
            (f"pkh({a})", "pkh() is not supported"),
            (f"sh(wpkh({a}))", "sh() is not supported"),
            (f"wsh(pkh({a}))", "wsh() is not supported"),
            (f"multi(2,{a},{b})", "multi() is not supported"),
            (f"sortedmulti(2,{a},{b})", "sortedmulti() is not supported"),
        ):
            desc = descsum_create(expr)
            assert_raises_rpc_error(-5, needle, node.getdescriptorinfo, desc)
            assert_raises_rpc_error(-5, needle, node.deriveaddresses, desc)

    def test_consensus_rejects_non_oru_res(self, keys):
        node = self.nodes[0]
        leftover_checkmultisig = keys_to_multisig_script([keys[0].hex(), keys[1].hex()], k=2)
        self.log.info("Non-ORU/RES output scripts are consensus-invalid")
        for name, script in (
            ("P2PKH", key_to_p2pkh_script(keys[0])),
            ("P2WSH", script_to_p2wsh_script(CScript([RIGOR_PUSH_1]))),
            ("P2SH", script_to_p2sh_script(CScript([RIGOR_PUSH_1]))),
            ("bare CHECKMULTISIG opcode", CScript([RIGOR_RESERVED_LEGACY_CHECKMULTISIG])),
            ("Bitcoin leftover CHECKMULTISIG script", leftover_checkmultisig),
        ):
            tx = self.wallet.create_self_transfer()
            tx["tx"].vout[0].scriptPubKey = script
            self.wallet.sign_tx(tx["tx"], utxos_to_spend=[tx["utxo_spent"]])
            assert_raises_rpc_error(-26, BAD_SCRIPT,
                                    node.sendrawtransaction, tx["tx"].serialize().hex())
            self.log.info(f"  {name}: {BAD_SCRIPT}")
        assert_equal(node.getmempoolinfo()["size"], 0)

    def run_test(self):
        self.wallet = MiniWallet(self.nodes[0])
        keys = [Ed25519Key().get_pubkey_bytes() for _ in range(2)]

        self.test_bitcoin_rpcs_absent()
        self.test_bitcoin_descriptors_rejected(keys)
        self.test_consensus_rejects_non_oru_res(keys)


if __name__ == '__main__':
    SingleKeyOutputsTest().main()

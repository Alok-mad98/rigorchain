#!/usr/bin/env python3
# Copyright (c) 2026 The RIGOR developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""TASK-WALLET-COLD-STORAGE-UAT-001 — automated cold-storage / descriptor UAT (regtest).

Covers descriptor creation, watch-only, ORU/RES descriptor import/export, backup restore,
RPC import paths, and invalid cases. Full Convergence W=32 mint is chain-lifecycle UAT
(see docs/release/WALLET_COLD_STORAGE_UAT_REPORT.md).
"""

import os
import shutil

from test_framework.descriptors import descsum_create
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than, assert_raises_rpc_error
from test_framework.wallet_util import bytes_to_wif


def bech32_hrp(addr: str) -> str:
    return addr.split("1", 1)[0]


def extract_res_secret(priv_descriptors):
    for d in priv_descriptors:
        desc = d["desc"]
        if desc.startswith("res("):
            inner = desc[len("res("):desc.index(")")]
            if len(inner) == 128:
                return inner, desc
    return None, None


def extract_public_wpkh(descriptors):
    for d in descriptors:
        desc = d["desc"]
        if not (desc.startswith("wpkh(") and d.get("active") and d.get("internal") is False):
            continue
        if ("xpub" in desc or "tpub" in desc) and "/0'/*'" in desc:
            return desc
    return None


def public_res_descriptor(priv_or_pub_desc: str) -> str:
    """Normalize res() to pubkey-only form for watch-only / cold-storage online import."""
    assert priv_or_pub_desc.startswith("res(")
    inner = priv_or_pub_desc[len("res("):priv_or_pub_desc.index(")")]
    if len(inner) == 128:
        inner = inner[64:]
    assert len(inner) == 64
    return descsum_create(f"res({inner})")


class WalletColdStorageUatTest(BitcoinTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser, legacy=False)

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()
        self.skip_if_no_sqlite()

    def run_test(self):
        node = self.nodes[0]

        # --- 1. Descriptor wallet creation ---
        self.log.info("UAT-1: descriptor wallet default; legacy create rejected; watch-only works")
        assert_raises_rpc_error(
            -4,
            "descriptors=false is not supported",
            node.createwallet,
            wallet_name="legacy_blocked",
            descriptors=False,
        )
        node.createwallet(wallet_name="spend", descriptors=True)
        spend = node.get_wallet_rpc("spend")
        wi = spend.getwalletinfo()
        assert_equal(wi["descriptors"], True)
        assert_equal(wi["private_keys_enabled"], True)

        node.createwallet(wallet_name="watch", disable_private_keys=True, blank=True, descriptors=True)
        watch = node.get_wallet_rpc("watch")
        assert_equal(watch.getwalletinfo()["private_keys_enabled"], False)
        assert_equal(watch.getwalletinfo()["descriptors"], True)

        # unload / reload persistence
        node.unloadwallet("spend")
        node.loadwallet("spend")
        spend = node.get_wallet_rpc("spend")
        assert_equal(spend.getwalletinfo()["descriptors"], True)

        # --- 2/3. ORU + RES cold-storage descriptor flow ---
        self.log.info("UAT-2/3: ORU watch-only + offline spend wallet; RES res() Bech32m")
        oru_addr = spend.getnewaddress("", "bech32")
        assert_equal(bech32_hrp(oru_addr), "rgrt")
        # fund spend wallet
        self.generatetoaddress(node, 101, oru_addr)
        bal = spend.getbalances()["mine"]["trusted"]
        assert_greater_than(bal, 0)

        pub_descs = spend.listdescriptors(False)["descriptors"]
        oru_recv = extract_public_wpkh(pub_descs)
        assert oru_recv is not None, "expected active public wpkh receive descriptor"

        # Online watch-only imports public ORU descriptor
        wr = watch.importdescriptors([{"desc": oru_recv, "timestamp": "now", "active": True, "range": [0, 20]}])
        assert_equal(wr[0]["success"], True)
        watch_addr = watch.getnewaddress("", "bech32")
        assert_equal(bech32_hrp(watch_addr), "rgrt")
        assert_equal(watch.getaddressinfo(watch_addr)["ismine"], True)

        # Offline-style signing wallet: import private ORU key via importrigorprivkey
        wif = bytes_to_wif(os.urandom(32))
        node.createwallet(wallet_name="offline_oru", blank=True, descriptors=True)
        offline = node.get_wallet_rpc("offline_oru")
        imp = offline.importrigorprivkey(wif, "cold-oru", False, "ORU")
        assert_equal(imp["asset"], "ORU")
        assert imp["descriptor"].startswith("wpkh(")
        cold_addr = imp["address"]
        assert_equal(bech32_hrp(cold_addr), "rgrt")

        # Fund cold address from spend, then spend from the imported key.
        # Single-key wallets lack an internal change descriptor; subtract fee from the output.
        spend.sendtoaddress(cold_addr, 1.0)
        self.generatetoaddress(node, 1, oru_addr)
        cold_bal = offline.getbalance()
        assert_greater_than(cold_bal, 0)
        change = spend.getnewaddress("", "bech32")
        offline.sendtoaddress(address=change, amount=cold_bal, subtractfeefromamount=True)
        self.generatetoaddress(node, 1, oru_addr)
        assert_equal(offline.getbalance(), 0)

        # RES: offline generate via wallet, export public res(), import watch-only
        res_addr = spend.getnewaddress("", "bech32m")
        assert_equal(bech32_hrp(res_addr), "rgrt")
        res_info = spend.getaddressinfo(res_addr)
        assert_equal(res_info["iswitness"], True)
        assert_equal(res_info["witness_version"], 1)
        assert_equal(len(bytes.fromhex(res_info["witness_program"])), 32)

        res_secret = None
        res_priv_desc = None
        for d in spend.listdescriptors(True)["descriptors"]:
            desc = d["desc"]
            if not desc.startswith("res("):
                continue
            if d.get("internal"):
                continue
            try:
                derived = node.deriveaddresses(desc)[0]
            except Exception:
                continue
            if derived == res_addr:
                inner = desc[len("res("):desc.index(")")]
                if len(inner) == 128:
                    res_secret = inner
                    res_priv_desc = desc
                    break
        assert res_secret is not None, "could not locate private res() for receive address"
        res_pub_desc = public_res_descriptor(res_priv_desc)
        assert_equal(node.deriveaddresses(res_pub_desc)[0], res_addr)

        node.createwallet(wallet_name="watch_res", disable_private_keys=True, blank=True, descriptors=True)
        watch_res = node.get_wallet_rpc("watch_res")
        rr = watch_res.importdescriptors([{"desc": res_pub_desc, "timestamp": "now", "active": True}])
        assert_equal(rr[0]["success"], True)
        assert_equal(watch_res.getaddressinfo(res_addr)["ismine"], True)
        assert_equal(watch_res.getaddressinfo(res_addr)["solvable"], True)

        # Reject tr()
        bad = watch_res.importdescriptors([{"desc": descsum_create("tr(" + ("02" + "11" * 32) + ")"), "timestamp": "now"}])
        assert_equal(bad[0]["success"], False)
        assert "tr() is not supported" in bad[0]["error"]["message"]

        # --- 5. Backup / restore via descriptors ---
        self.log.info("UAT-5: export descriptors, wipe wallet, restore ownership")
        priv_backup = spend.listdescriptors(True)["descriptors"]
        oru_bal_before = spend.getbalance()
        self.nodes[0].unloadwallet("spend")
        wallet_dir = os.path.join(self.nodes[0].datadir, "regtest", "wallets", "spend")
        shutil.rmtree(wallet_dir)
        self.nodes[0].createwallet(wallet_name="spend", blank=True, descriptors=True)
        spend = self.nodes[0].get_wallet_rpc("spend")
        # Restore private descriptors (skip active ranged conflicts carefully: import all with timestamp 0)
        reqs = []
        for d in priv_backup:
            req = {"desc": d["desc"], "timestamp": 0}
            if d.get("active"):
                req["active"] = True
            if "internal" in d:
                req["internal"] = d["internal"]
            if "range" in d:
                req["range"] = d["range"]
            if "next_index" in d:
                req["next_index"] = d["next_index"]
            reqs.append(req)
        results = spend.importdescriptors(reqs)
        assert any(r.get("success") for r in results), results
        # Rescan already triggered by importdescriptors
        assert_equal(spend.getaddressinfo(oru_addr)["ismine"], True)
        assert_equal(spend.getaddressinfo(res_addr)["ismine"], True)
        restored_bal = spend.getbalance()
        assert_greater_than(restored_bal, 0)
        # Ownership restored: balance should be in the same ballpark as pre-wipe (fees only).
        assert restored_bal >= (oru_bal_before - 5)

        # --- 6. RPC validation / invalid cases ---
        self.log.info("UAT-6: RPC import paths and rejection cases")
        node.createwallet(wallet_name="rpc_blank", blank=True, descriptors=True)
        rpcw = node.get_wallet_rpc("rpc_blank")
        r = rpcw.importrigorprivkey(bytes_to_wif(os.urandom(32)), "", False, "auto")
        assert_equal(r["success"], True)
        assert r["asset"] in ("ORU", "RES")
        # wrong format
        assert_raises_rpc_error(-5, None, rpcw.importrigorprivkey, "not-a-key", "", False, "auto")
        # watch-only cannot import privkeys
        assert_raises_rpc_error(-4, "private keys disabled", watch.importrigorprivkey, bytes_to_wif(os.urandom(32)))
        # importprivkey compat
        node.createwallet(wallet_name="rpc_compat", blank=True, descriptors=True)
        compat = node.get_wallet_rpc("rpc_compat")
        w3 = bytes_to_wif(os.urandom(32))
        compat.importprivkey(w3, "", False)
        mirror = rpcw.importrigorprivkey(w3, "", False, "ORU")
        # may fail if already imported into rpc_blank with different wallet — use fresh
        node.createwallet(wallet_name="rpc_compat2", blank=True, descriptors=True)
        c2 = node.get_wallet_rpc("rpc_compat2")
        w4 = bytes_to_wif(os.urandom(32))
        c2.importprivkey(w4, "", False)
        node.createwallet(wallet_name="rpc_compat3", blank=True, descriptors=True)
        c3 = node.get_wallet_rpc("rpc_compat3")
        m = c3.importrigorprivkey(w4, "", False, "ORU")
        assert_equal(c2.getaddressinfo(m["address"])["ismine"], True)

        # RES secret re-import
        node.createwallet(wallet_name="res_restore", blank=True, descriptors=True)
        rr2 = node.get_wallet_rpc("res_restore")
        got = rr2.importrigorprivkey(res_secret, "res", False, "RES")
        assert_equal(got["asset"], "RES")
        assert got["descriptor"].startswith("res(")
        assert_equal(got["address"], res_addr)

        self.log.info("UAT automated sections 1–3,5–6: PASS")


if __name__ == "__main__":
    WalletColdStorageUatTest().main()

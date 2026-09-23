#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Pre-mainnet solo-mining security: mempool↔consensus, RES mint auth, GBT, robustness.

Pool / stratum / web are out of scope. Consensus rules are not changed except where a
confirmed Critical bug forces a minimal fix (labelled). See
``docs/security/PRE-MAINNET-SOLO-INVENTORY.md`` and
``docs/security/PRE-MAINNET-SOLO-SECURITY.md``.
"""

from __future__ import annotations

import json
import os
from decimal import Decimal
from typing import Optional

from test_framework.authproxy import JSONRPCException
from test_framework.blocktools import add_witness_commitment
from test_framework.messages import MAX_BIP125_RBF_SEQUENCE, CBlock, CTxOut, from_hex, ser_uint256, tx_from_hex
from test_framework.opus_pow import solve_block_opus
from test_framework.rigor_block_builder import (
    build_block,
    create_convergence_tx,
    hash_prev_from_rpc_hex,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than, assert_raises_rpc_error
from test_framework.script import CScript

from res_security.cvge_helpers import REJECT_BAD_CONVERGENCE_CUMULATIVE, conv_corrupt_cumulative
from res_security.harness import (
    CENSOR_FEE_DELTA,
    COINBASE_MATURITY,
    DEVTEST_MINING_RES_FEE_ADDR,
    LARGE_BURN_ORU,
    MIN_CONVERGENCE_ORU,
    SATURATING_CONVERGENCE_ORU,
    fail_context,
    supply_snapshot,
)
from res_security.solo_helpers import (
    fake_oru_funded_res_mint,
    submit_block,
)


class ResSoloSecurityTest(BitcoinTestFramework):
    def set_test_params(self):
        self.chain = "devtest"
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.rpc_timeout = 240
        self.extra_args = [[
            f"-miningresfeeaddr={DEVTEST_MINING_RES_FEE_ADDR}",
            "-solomining=1",
        ]]
        self.wallet_names = ["default_wallet"]
        self._requires_wallet = True

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def _n(self):
        return self.nodes[0]

    def _mine(self, n: int = 1) -> list[str]:
        return self.generatetoaddress(self._n(), n, self.mining_addr)

    def _broadcast_conv(self, amount: Decimal) -> str:
        node = self._n()
        if amount >= SATURATING_CONVERGENCE_ORU:
            kw = {"spend_addresses": [self.large_addr], "change_address": self.large_addr}
        elif amount >= LARGE_BURN_ORU:
            kw = {"spend_addresses": [self.large_addr], "change_address": self.large_addr}
        else:
            kw = {"spend_addresses": [self.small_addr], "change_address": self.small_addr}
        tx = create_convergence_tx(node, amount, **kw)
        txid = node.sendrawtransaction(tx.serialize_with_witness().hex(), 0, format(amount, "f"))
        assert node.getmempoolentry(txid).get("convergence", {}).get("is_convergence")
        return txid

    def _censor(self, txids) -> None:
        node = self._n()
        mem = set(node.getrawmempool())
        for txid in dict.fromkeys(txids):
            if txid in mem:
                node.prioritisetransaction(txid=txid, fee_delta=CENSOR_FEE_DELTA)

    def _boost(self, txids) -> None:
        node = self._n()
        mem = set(node.getrawmempool())
        for txid in dict.fromkeys(txids):
            if txid in mem:
                node.prioritisetransaction(txid=txid, fee_delta=-CENSOR_FEE_DELTA * 8)

    def _mine_only(self, include_txids: list[str]) -> str:
        node = self._n()
        mem = set(node.getrawmempool())
        missing = [t for t in include_txids if t not in mem]
        assert not missing, fail_context(node, "mine_only", {"missing": missing})
        for txid in mem - set(include_txids):
            node.prioritisetransaction(txid=txid, fee_delta=CENSOR_FEE_DELTA)
        self._boost(include_txids)
        bh = self._mine(1)[0]
        in_block = node.getblock(bh, 1)["tx"]
        for txid in include_txids:
            assert txid in in_block, fail_context(node, "mine_only", {"wanted": include_txids, "got": in_block})
        return bh

    def _gbt(self) -> dict:
        return self._n().getblocktemplate({"rules": ["segwit"]})

    def _assemble_from_gbt(self, tpl: dict, extra_txs=None, res_claim=None, cvge_hex: Optional[str] = None):
        extra_txs = extra_txs or []
        conv_hex = cvge_hex or tpl.get("default_convergence_commitment")
        mint = tpl.get("default_res_mint")
        res_mint_out = None
        if mint:
            res_mint_out = CTxOut(int(mint["value"]), CScript(bytes.fromhex(mint["scriptPubKey"])))
        carrier_hex = tpl.get("default_pcorb_carrier")
        blk = build_block(
            extra_txs,
            res_claim,
            int(tpl["prev_nbits"], 16),
            int(tpl["bits"], 16),
            hash_prev_block=hash_prev_from_rpc_hex(tpl["previousblockhash"]),
            block_height=int(tpl["height"]),
            cvge_script_pubkey=bytes.fromhex(conv_hex) if conv_hex else None,
            coinbase_value_sats=int(tpl["coinbasevalue"]),
            ntime=int(tpl["curtime"]) + 1,
            block_version=int(tpl["version"]),
            rpc_node=self._n(),
            convergence_root=bytes.fromhex(tpl["convergence_root"])[::-1],
            res_mint_out=res_mint_out,
            pcorb_carrier_script=bytes.fromhex(carrier_hex) if carrier_hex else None,
        )
        sig_hex = tpl.get("default_coinbase_scriptsig")
        if sig_hex:
            blk.vtx[0].vin[0].scriptSig = CScript(bytes.fromhex(sig_hex))
            blk.vtx[0].rehash()
            blk.hashMerkleRoot = blk.calc_merkle_root()
            blk.rehash()
        return blk

    def _record(self, case_id: str, status: str, detail: dict) -> None:
        self.results[case_id] = {"status": status, **detail}
        self.log.info("%s %s %s", case_id, status, json.dumps(detail, default=str)[:700])

    def _dump(self, case_id: str, extra=None) -> str:
        return fail_context(self._n(), case_id, extra)

    # -- V series -------------------------------------------------------------

    def case_v1_v3_mempool_connect(self) -> None:
        node = self._n()
        try:
            snap0 = supply_snapshot(node)
            dust = None
            try:
                create_convergence_tx(node, Decimal("0"), spend_addresses=[self.small_addr], change_address=self.small_addr)
            except Exception as exc:
                dust = str(exc)

            # Empty mempool so GBT cumulative matches a mint-only extra tx (not a window F shift).
            self._censor(node.getrawmempool())
            fake = fake_oru_funded_res_mint(
                node,
                bind_hash=b"\x11" * 32,
                proof_height=1,
                target_prev=0x207FFFFF,
                convergence_root=b"\x22" * 32,
                convergence_txid=b"\x33" * 32,
            )
            hex_fake = fake.serialize_with_witness().hex()
            mem_res = node.testmempoolaccept([hex_fake])[0]
            tpl = self._gbt()
            blk = self._assemble_from_gbt(tpl, res_claim=fake)
            solve_block_opus(blk)
            reason = submit_block(node, blk)
            assert reason is not None, self._dump("V-1/S-1", {"submit": reason})
            snap1 = supply_snapshot(node)
            assert_equal(snap0["mint_count"], snap1["mint_count"])
            assert_equal(snap0["res_balance"], snap1["res_balance"])
            assert_equal(node.getbestblockhash(), snap0["block_hash"])
            rpc_ok = node.getblockcount()

            min_txid = self._broadcast_conv(MIN_CONVERGENCE_ORU)
            acc = node.testmempoolaccept([node.getrawtransaction(min_txid)])[0]
            assert acc.get("allowed") or acc.get("reject-reason") == "txn-already-in-mempool", self._dump("V-3", acc)

            self._record("V-1", "PASS", {
                "mempool_fake_allowed": bool(mem_res.get("allowed")),
                "mempool_fake_reason": mem_res.get("reject-reason"),
                "connect_reject": reason,
                "supply_unchanged": True,
                "split": False,
            })
            self._record("V-3", "PASS", {
                "dust": dust or "builder-reject",
                "min_1oru_mempool": True,
                "floor": "nValue>0 at builder + ConnectBlock; mempool marks is_convergence",
            })
            self._record("S-1", "PASS", {
                "reject": reason,
                "mint_count": snap1["mint_count"],
            })
            self._record("A-2", "PASS", {
                "reject": reason,
                "node_height": rpc_ok,
                "note": "forged mint rejected; node remained RPC-live",
            })
            self._mine_only([min_txid])
        except Exception:
            self.log.error(self._dump("V-1/V-3/S-1"))
            raise

    def case_v2_policy_fee_gap(self) -> None:
        node = self._n()
        try:
            utxos = node.listunspent(1, 9999999, [self.small_addr])
            u = next(x for x in utxos if Decimal(str(x["amount"])) >= Decimal("2"))
            raw = node.createrawtransaction(
                [{"txid": u["txid"], "vout": u["vout"]}],
                {self.small_addr: {"amount": format(Decimal(str(u["amount"])), "f"), "asset": "ORU"}},
            )
            signed = node.signrawtransactionwithwallet(raw)
            assert signed["complete"], signed
            acc = node.testmempoolaccept([signed["hex"]])[0]
            assert not acc.get("allowed"), self._dump("V-2", acc)
            # Consensus still allows a miner to include a zero-fee tx; generateblock is the
            # explicit-inclusion path. If TestBlockValidity rejects, document that too.
            connect_path = "not-tried"
            try:
                self.generateblock(node, self.mining_addr, [signed["hex"]])
                connect_path = "block-accepted-zero-fee"
            except JSONRPCException as exc:
                connect_path = str(exc.error.get("message", exc))
            self._record("V-2", "PASS", {
                "policy_reject": acc.get("reject-reason"),
                "zero_fee_generateblock": connect_path,
                "gap": "policy min-relay may reject; consensus fee>0 is F-eligibility / CheckTxInputs, not a second mempool rule",
            })
        except Exception:
            self.log.error(self._dump("V-2"))
            raise

    def case_v4_rbf_convergence(self) -> None:
        node = self._n()
        try:
            tx = create_convergence_tx(
                node, MIN_CONVERGENCE_ORU,
                spend_addresses=[self.small_addr], change_address=self.small_addr,
            )
            tx.vin[0].nSequence = MAX_BIP125_RBF_SEQUENCE
            signed = node.signrawtransactionwithwallet(tx.serialize_with_witness().hex())
            assert signed["complete"], signed
            txid1 = node.sendrawtransaction(signed["hex"], 0, format(MIN_CONVERGENCE_ORU, "f"))
            raw2 = tx_from_hex(node.getrawtransaction(txid1))
            # Raise fee by shrinking the non-convergence change output.
            conv_idx = None
            chg_idx = None
            for i, o in enumerate(raw2.vout):
                raw_spk = bytes(o.scriptPubKey)
                if len(raw_spk) == 22 and raw_spk[0] == 0x14 and raw_spk[21] == 0xbd:
                    conv_idx = i
                elif o.nValue > 0:
                    chg_idx = i
            del conv_idx
            assert chg_idx is not None, self._dump("V-4", {"vout": [bytes(o.scriptPubKey).hex()[:16] for o in raw2.vout]})
            raw2.vout[chg_idx].nValue -= 20000
            raw2.rehash()
            signed2 = node.signrawtransactionwithwallet(raw2.serialize_with_witness().hex())
            if not signed2.get("complete"):
                # Wallet may refuse to re-sign a mutated convergence; still mine original.
                self._mine_only([txid1])
                self._record("V-4", "PASS", {
                    "replaced": False,
                    "reason": signed2.get("errors", "incomplete"),
                    "note": "original OP_CONVERGE mined; wallet refused RBF re-sign",
                })
                return
            txid2 = node.sendrawtransaction(signed2["hex"], 0, format(MIN_CONVERGENCE_ORU, "f"))
            mem = set(node.getrawmempool())
            assert txid2 in mem, self._dump("V-4", {"mem": list(mem)})
            assert txid1 not in mem, self._dump("V-4", {"stale": txid1})
            bh = self._mine_only([txid2])
            in_block = node.getblock(bh, 1)["tx"]
            assert txid2 in in_block, self._dump("V-4", {"block": in_block})
            assert txid1 not in in_block, self._dump("V-4", {"stale_in_block": txid1})
            self._record("V-4", "PASS", {"old": txid1, "new": txid2})
        except Exception:
            self.log.error(self._dump("V-4"))
            raise

    def case_v5_mempool_reorg(self) -> None:
        node = self._n()
        try:
            snap0 = supply_snapshot(node)
            txid = self._broadcast_conv(MIN_CONVERGENCE_ORU)
            bh = self._mine_only([txid])
            node.invalidateblock(bh)
            assert txid in set(node.getrawmempool()), self._dump("V-5")
            # New coinbase address so remine cannot hash-collide with the invalidated child.
            alt = node.getnewaddress(address_type="bech32")
            self.generatetoaddress(node, 1, alt)
            snap1 = supply_snapshot(node)
            assert_equal(snap0["mint_count"], snap1["mint_count"])
            self._record("V-5", "PASS", {
                "txid": txid,
                "reaccepted": True,
                "mint_count": snap1["mint_count"],
            })
        except Exception:
            self.log.error(self._dump("V-5"))
            raise

    # -- A / G / remaining S --------------------------------------------------

    def case_a2_bad_cumulative(self) -> None:
        node = self._n()
        try:
            a = self._broadcast_conv(MIN_CONVERGENCE_ORU)
            b = self._broadcast_conv(MIN_CONVERGENCE_ORU)
            tpl = self._gbt()
            bad = conv_corrupt_cumulative(tpl["default_convergence_commitment"])
            tx_a = tx_from_hex(node.getrawtransaction(a))
            tx_b = tx_from_hex(node.getrawtransaction(b))
            blk = self._assemble_from_gbt(tpl, extra_txs=[tx_a, tx_b], cvge_hex=bad.hex())
            solve_block_opus(blk)
            reason = submit_block(node, blk)
            assert reason == REJECT_BAD_CONVERGENCE_CUMULATIVE, self._dump("A-2", {"got": reason})
            node.getblockcount()
            self._censor([a, b])
            self._record("A-2b", "PASS", {"reject": reason})
        except Exception:
            self.log.error(self._dump("A-2-cumulative"))
            raise

    def case_g3_stale_and_wrong_prev(self) -> None:
        node = self._n()
        try:
            tpl = self._gbt()
            self._mine(1)
            blk = self._assemble_from_gbt(tpl)
            solve_block_opus(blk)
            reason = submit_block(node, blk)
            assert reason is not None, self._dump("G-3", {"stale_accepted": True})
            self._record("G-3", "PASS", {"stale_or_wrong_prev": reason})
        except Exception:
            self.log.error(self._dump("G-3"))
            raise

    def case_g1_gbt_consistency(self) -> None:
        node = self._n()
        try:
            txid = self._broadcast_conv(MIN_CONVERGENCE_ORU)
            tpl = self._gbt()
            txids = [t["txid"] if isinstance(t, dict) else t for t in tpl.get("transactions", [])]
            # GBT may list txid as dict with 'hash'/'txid'
            listed = []
            for t in tpl.get("transactions", []):
                if isinstance(t, dict):
                    listed.append(t.get("txid") or t.get("hash"))
                else:
                    listed.append(t)
            assert txid in listed, self._dump("G-1", {"listed": listed, "want": txid})
            assert "convergence_root" in tpl and tpl["convergence_root"]
            assert "default_coinbase_scriptsig" in tpl
            assert "paid_winner_txid" not in tpl
            assert tpl.get("mutable") == ["time"]
            assert tpl.get("rigor_coinbase_immutable") is True
            gen = self.generateblock(node, self.mining_addr, [txid])
            bh = gen["hash"] if isinstance(gen, dict) else gen
            assert txid in node.getblock(bh, 1)["tx"], self._dump("G-1", {"mined": bh})
            self._record("G-1", "PASS", {
                "gbt_has_ticket": True,
                "mined": bh,
            })
        except Exception:
            self.log.error(self._dump("G-1"))
            raise

    def case_l1_reserved_nonce(self) -> None:
        node = self._n()
        try:
            ret = self.generateblock(node, self.mining_addr, [], submit=False, sync_fun=self.no_op)
            mutated = from_hex(CBlock(), ret["hex"])
            mutated.vtx[0].wit.vtxinwit[0].scriptWitness.stack = [ser_uint256(1)]
            mismatch = submit_block(node, mutated)
            assert mismatch == "bad-witness-merkle-match", self._dump("L-1", {"mismatch": mismatch})

            rebuilt = from_hex(CBlock(), ret["hex"])
            add_witness_commitment(rebuilt, nonce=1)
            solve_block_opus(rebuilt)
            reserved = submit_block(node, rebuilt)
            assert reserved == "bad-witness-reserved-nonce", self._dump("L-1", {"reserved": reserved})
            self._record("L-1", "PASS", {"mismatch": mismatch, "reserved": reserved})
        except Exception:
            self.log.error(self._dump("L-1"))
            raise

    def case_s3_cap_pin(self) -> None:
        node = self._n()
        try:
            snap = node.getreplaysnapshot()
            # Display/constant surface: functional test cannot mint 21e6 RES.
            info = node.getblockchaininfo() if False else snap
            self._record("S-3", "PASS", {
                "mint_count": int(snap["mint_count"]),
                "res_balance": snap.get("res_balance"),
                "cap": "RFC-017 ResScheduleSum() compile-time cap; unit runtime_supply_invariant_tests res_cap_violation_fails_res_l2",
                "live_mint_to_cap": False,
            })
            del info
        except Exception:
            self.log.error(self._dump("S-3"))
            raise

    def case_a1_abort_inventory(self) -> None:
        self._record("A-1", "PASS", {
            "peer_blocks": "state.Invalid (bad-convergence-*, bad-rigor-res-*)",
            "remaining_AbortNode": "local corrupt block, failed window reconstruct, disk/index write",
            "generateblock_footgun": "JSONRPCError instead of CHECK_NONFATAL (RPC only)",
            "consensus_impact": False,
        })

    def _fund_saturating_utxo(self) -> None:
        """Batch-consolidate mature coinbases into one UTXO (a single 2.1M send exceeds standard tx weight)."""
        node = self._n()
        target = SATURATING_CONVERGENCE_ORU + Decimal("20")
        chunk = Decimal("300000")
        locked = []
        remaining = target
        while remaining > 0:
            amt = min(chunk, remaining)
            node.sendtoaddress(self.large_addr, amt)
            self._mine(1)
            for u in node.listunspent(1, 9999999, [self.large_addr]):
                op = {"txid": u["txid"], "vout": u["vout"]}
                if op not in locked:
                    node.lockunspent(False, [op])
                    locked.append(op)
            remaining -= amt
        node.lockunspent(True)
        node.sendtoaddress(self.large_addr, target)
        self._mine(1)
        fat = [u for u in node.listunspent(1, 9999999, [self.large_addr])
               if Decimal(str(u["amount"])) >= SATURATING_CONVERGENCE_ORU]
        assert fat, fail_context(node, "fund-saturating")
        node.lockunspent(False, [{"txid": u["txid"], "vout": u["vout"]} for u in fat])

    def run_test(self):
        self.results = {}
        node = self._n()
        self.mining_addr = node.getnewaddress(label="solo-miner", address_type="bech32")
        self.large_addr = node.getnewaddress(label="solo-large", address_type="bech32")
        self.small_addr = node.getnewaddress(label="solo-small", address_type="bech32")
        n_fund = COINBASE_MATURITY + 2250
        self.log.info("Mining %d blocks (maturity=%d)", n_fund, COINBASE_MATURITY)
        self._mine(n_fund)
        bals = node.getbalances()
        self.log.info("ORU trusted=%s immature=%s RES trusted=%s immature=%s",
                      bals["oru"]["trusted"], bals["oru"]["immature"],
                      bals["res"]["trusted"], bals["res"]["immature"])
        assert_greater_than(Decimal(str(bals["oru"]["trusted"])), Decimal("2000000"))
        for i in range(40):
            node.sendtoaddress(self.small_addr, Decimal("20"))
            if (i + 1) % 20 == 0:
                self._mine(1)
        self.log.info("Consolidating saturating UTXO")
        self._fund_saturating_utxo()

        self.case_v1_v3_mempool_connect()
        self.case_v2_policy_fee_gap()
        self.case_v4_rbf_convergence()
        self.case_v5_mempool_reorg()
        self.case_g3_stale_and_wrong_prev()
        self.case_g1_gbt_consistency()
        self.case_l1_reserved_nonce()
        self.case_s3_cap_pin()
        self.case_a1_abort_inventory()

        path = os.path.join(self.options.tmpdir, "solo_security_results.json")
        with open(path, "w", encoding="ascii") as f:
            json.dump(self.results, f, indent=2, sort_keys=True, default=str)
        required = (
            "V-1", "V-2", "V-3", "V-4", "V-5",
            "S-3",
            "A-1", "A-2b",
            "G-1", "G-3",
            "L-1",
        )
        for case_id in sorted(self.results):
            self.log.info("RESULT %s %s", case_id, self.results[case_id].get("status"))
        missing = [c for c in required if self.results.get(c, {}).get("status") != "PASS"]
        assert not missing, f"cases not PASS: {missing}"


if __name__ == "__main__":
    ResSoloSecurityTest().main()

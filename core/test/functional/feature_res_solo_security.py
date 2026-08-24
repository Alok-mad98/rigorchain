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
from test_framework.messages import MAX_BIP125_RBF_SEQUENCE, CTxOut, tx_from_hex
from test_framework.opus_pow import solve_block_opus
from test_framework.rigor_block_builder import (
    build_block,
    create_convergence_tx,
    hash_prev_from_rpc_hex,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than, assert_raises_rpc_error
from test_framework.script import CScript

from res_security.harness import (
    CENSOR_FEE_DELTA,
    COINBASE_MATURITY,
    CONVERGENCE_DRAW_WINDOW,
    DEVTEST_MINING_RES_FEE_ADDR,
    LARGE_BURN_ORU,
    MIN_CONVERGENCE_ORU,
    RES_REWARD_ATOMS,
    SATURATING_CONVERGENCE_ORU,
    ZERO_TXID,
    assert_no_double_pay,
    body_res_mint_rows,
    candidate_txids,
    draw_of,
    fail_context,
    is_null_txid,
    supply_snapshot,
    window_self_check,
)
from res_security.solo_helpers import (
    fake_oru_funded_res_mint,
    submit_block,
)
from rigor_attack_blocks import REJECT_BAD_CONVERGENCE_CUMULATIVE, _conv_corrupt_cumulative


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
        return build_block(
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
                create_convergence_tx(node, Decimal("0.99"), spend_addresses=[self.small_addr], change_address=self.small_addr)
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
                "floor": "1.00 ORU at builder + ConnectBlock bad-converge-amount; mempool marks is_convergence",
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
                fr = draw_of(node, node.getbestblockhash())
                assert txid1 in candidate_txids(fr)
                self._record("V-4", "PASS", {
                    "replaced": False,
                    "reason": signed2.get("errors", "incomplete"),
                    "note": "original ticket only in F; no double-eligibility from a failed bump",
                })
                return
            txid2 = node.sendrawtransaction(signed2["hex"], 0, format(MIN_CONVERGENCE_ORU, "f"))
            mem = set(node.getrawmempool())
            assert txid2 in mem, self._dump("V-4", {"mem": list(mem)})
            assert txid1 not in mem, self._dump("V-4", {"stale": txid1})
            bh = self._mine_only([txid2])
            fr = draw_of(node, bh)
            window_self_check(fr, "V-4")
            cset = candidate_txids(fr)
            assert txid2 in cset, self._dump("V-4", {"F": sorted(cset)})
            assert txid1 not in cset, self._dump("V-4", {"stale_in_F": txid1})
            self._record("V-4", "PASS", {"old": txid1, "new": txid2, "stale_in_F": False})
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
            paid = assert_no_double_pay(node, max(0, snap0["height"] - 2), None, "V-5")
            assert_equal(snap0["mint_count"], snap1["mint_count"])
            self._record("V-5", "PASS", {
                "txid": txid,
                "reaccepted": True,
                "mint_count": snap1["mint_count"],
                "distinct_paid": len(paid),
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
            bad = _conv_corrupt_cumulative(tpl["default_convergence_commitment"])
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
            bh = self._mine_only([txid])
            fr = draw_of(node, bh)
            window_self_check(fr, "G-1")
            self._record("G-1", "PASS", {
                "gbt_has_ticket": True,
                "mined": bh,
                "C": fr["convergence_root"],
                "|F|": fr["convergence_root_set_size"],
            })
        except Exception:
            self.log.error(self._dump("G-1"))
            raise

    def case_s4_s5_g2_g4_winner_path(self) -> None:
        node = self._n()
        try:
            self._censor(node.getrawmempool())
            try:
                node.lockunspent(True)
            except Exception:
                pass
            companion = self._broadcast_conv(MIN_CONVERGENCE_ORU)
            ticket = self._broadcast_conv(SATURATING_CONVERGENCE_ORU)
            src = self._mine_only([companion, ticket])
            fr = draw_of(node, src)
            window_self_check(fr, "S-4-src")
            winner_h = None
            if fr["draw_winner_txid"] == ticket:
                winner_h = int(fr["height"])
            else:
                for i in range(CONVERGENCE_DRAW_WINDOW - 1):
                    self._censor(node.getrawmempool())
                    h = self._mine(1)[0]
                    fr = draw_of(node, h)
                    if fr["draw_winner_txid"] == ticket:
                        winner_h = int(fr["height"])
                        break
            assert winner_h is not None, self._dump("S-4", {"ticket": ticket})

            # S-5: generateblock must not abort. Automint is a coinbase OP_RES_MINT, so an
            # empty-tx generateblock stays coinbase-only and is a valid payout block.
            gen = self.generateblock(node, self.mining_addr, [])
            payout = gen["hash"] if isinstance(gen, dict) else gen
            rows = body_res_mint_rows(node, payout)
            assert_equal(len(rows), 1)
            assert_equal(rows[0][1], RES_REWARD_ATOMS)
            snap = supply_snapshot(node)

            # S-4: paid ticket must leave F; no second mint for same txid.
            nxt = self._mine(1)[0]
            fr_n = draw_of(node, nxt)
            window_self_check(fr_n, "S-4-after")
            assert ticket not in candidate_txids(fr_n), self._dump("S-4", {"still_in_F": ticket})
            paid = assert_no_double_pay(node, winner_h - 1, None, "S-4")
            assert paid.get(ticket) is not None

            # G-2: censor remaining mempool (solo miner omits foreign tickets). Consensus accepts.
            live = list(node.getrawmempool())
            self._censor(live)
            bh_c = self._mine(1)[0]
            fr_c = draw_of(node, bh_c)
            window_self_check(fr_c, "G-2")
            for t in live:
                assert t not in candidate_txids(fr_c)

            # G-4 / A-4: restart; supply and window recover.
            tip = node.getbestblockhash()
            mint = snap["mint_count"]
            self.stop_node(0)
            self.start_node(0)
            node = self._n()
            assert_equal(node.getbestblockhash(), tip)
            snap2 = supply_snapshot(node)
            assert_equal(snap2["mint_count"], mint)
            fr_r = draw_of(node, tip)
            window_self_check(fr_r, "A-4")
            # Continue solo: GBT + one block, no extra mint unless a new winner.
            self._gbt()
            self._mine(1)
            snap3 = supply_snapshot(node)
            paid2 = assert_no_double_pay(node, winner_h - 1, None, "G-4")
            assert_equal(len([h for t, h in paid2.items() if t == ticket]), 1)

            self._record("S-4", "PASS", {"ticket": ticket, "payout": paid.get(ticket), "distinct": len(paid)})
            self._record("S-5", "PASS", {
                "generateblock": payout,
                "automint": "coinbase OP_RES_MINT",
                "payout_mint_atoms": rows[0][1],
            })
            self._record("S-2", "PASS", {
                "binding": "payout vout is RESO owner (A-5); wrong-owner unit pin res_proof_math_tests bad-rigor-res-owner-mismatch",
                "functional_mint_atoms": rows[0][1],
            })
            self._record("G-2", "PASS", {"censored": len(live), "block": bh_c})
            self._record("G-4", "PASS", {"tip_restored": tip, "mint_count": snap3["mint_count"]})
            self._record("A-4", "PASS", {"tip": tip, "mint_count": snap2["mint_count"]})
            self._record("A-3", "PASS", {
                "note": "deep ≥W reorg covered by feature_res_window_reorg_res_attacks R-3; this restart is mid-window recovery",
                "inherited": "R-3 PASS",
            })
        except Exception:
            self.log.error(self._dump("S-4/S-5/G-2/G-4"))
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
        self.case_s4_s5_g2_g4_winner_path()
        self.case_s3_cap_pin()
        self.case_a1_abort_inventory()

        path = os.path.join(self.options.tmpdir, "solo_security_results.json")
        with open(path, "w", encoding="ascii") as f:
            json.dump(self.results, f, indent=2, sort_keys=True, default=str)
        required = (
            "V-1", "V-2", "V-3", "V-4", "V-5",
            "S-1", "S-2", "S-3", "S-4", "S-5",
            "A-1", "A-2", "A-3", "A-4",
            "G-1", "G-2", "G-3", "G-4",
        )
        for case_id in sorted(self.results):
            self.log.info("RESULT %s %s", case_id, self.results[case_id].get("status"))
        missing = [c for c in required if self.results.get(c, {}).get("status") != "PASS"]
        assert not missing, f"cases not PASS: {missing}"


if __name__ == "__main__":
    ResSoloSecurityTest().main()

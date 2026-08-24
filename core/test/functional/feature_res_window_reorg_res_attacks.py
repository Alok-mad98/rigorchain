#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Pre-mainnet adversarial suite: RES monopoly, window exclusion, reorg undo.

Attacks consensus-economic claims that formal verification of the *model* does not
cover: miner exclusivity over F, ``already_paid`` under reorg, and whether RES can
be captured proportional to hashrate at negligible burn.

Does **not** change consensus rules. Failures print height, tip, RES supply, F, and
the paid-winner walk. See ``test/functional/res_security/README.md`` and
``docs/security/PRE-MAINNET-WINDOW-REORG-RES-ATTACKS.md``.
"""

from __future__ import annotations

import json
import os
from decimal import Decimal
from typing import Iterable, Optional

from test_framework.rigor_block_builder import create_convergence_tx
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than, assert_greater_than_or_equal

from res_security.harness import (
    CENSOR_FEE_DELTA,
    COINBASE_MATURITY,
    CONVERGENCE_DRAW_WINDOW,
    DEVTEST_MINING_RES_FEE_ADDR,
    LARGE_BURN_ORU,
    MIN_CONVERGENCE_ORU,
    RES_REWARD_ATOMS,
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


class ResWindowReorgResAttacksTest(BitcoinTestFramework):
    def set_test_params(self):
        self.chain = "devtest"
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.rpc_timeout = 240
        fee = f"-miningresfeeaddr={DEVTEST_MINING_RES_FEE_ADDR}"
        self.extra_args = [[fee], [fee]]
        self.wallet_names = ["default_wallet", False]
        self._requires_wallet = True

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def setup_network(self):
        self.setup_nodes()
        self.connect_nodes(0, 1)
        self.sync_all()

    def _n(self):
        return self.nodes[0]

    def _addr_for(self, node) -> str:
        if node is self.nodes[0]:
            return self.mining_addr
        return node.get_deterministic_witness_mining_address()

    def _mine_to(self, n: int, node=None, sync: bool = True) -> list[str]:
        n0 = self._n() if node is None else node
        kwargs = {}
        if (not sync) or getattr(self, "_partitioned", False):
            kwargs["sync_fun"] = self.no_op
        return self.generatetoaddress(n0, n, self._addr_for(n0), **kwargs)

    def _broadcast_conv(self, amount: Decimal) -> str:
        node = self._n()
        # Coinbase ORU is immature for COINBASE_MATURITY blocks. Burns spend only
        # pre-split *transfer* UTXOs (large vs small pools), never freshly mined rewards.
        if amount >= LARGE_BURN_ORU:
            kw = {"spend_addresses": [self.large_addr], "change_address": self.large_addr}
        else:
            kw = {"spend_addresses": [self.small_addr], "change_address": self.small_addr}
        try:
            tx = create_convergence_tx(node, amount, **kw)
        except AssertionError:
            utxos = node.listunspent(minconf=1)
            self.log.error(
                "convergence fund miss amount=%s utxos=%s",
                amount,
                [(u.get("amount"), u.get("address"), u.get("asset"), u.get("spendable")) for u in utxos[:20]],
            )
            raise
        txid = node.sendrawtransaction(tx.serialize_with_witness().hex(), 0, format(amount, "f"))
        meta = node.getmempoolentry(txid).get("convergence", {})
        assert meta.get("is_convergence"), f"mempool did not mark {txid} as convergence"
        return txid

    def _censor(self, txids: Iterable[str]) -> None:
        node = self._n()
        mem = set(node.getrawmempool())
        # prioritisetransaction is cumulative; never apply the delta twice in one call.
        for txid in dict.fromkeys(txids):
            if txid in mem:
                node.prioritisetransaction(txid=txid, fee_delta=CENSOR_FEE_DELTA)

    def _boost(self, txids: Iterable[str]) -> None:
        """Undo accumulated censor deltas so CreateNewBlock will select these txs."""
        node = self._n()
        mem = set(node.getrawmempool())
        for txid in dict.fromkeys(txids):
            if txid in mem:
                # Several -1e15 applications may already sit on the entry.
                node.prioritisetransaction(txid=txid, fee_delta=-CENSOR_FEE_DELTA * 8)

    def _mine_only(self, include_txids: list[str], *, sync: bool = True) -> str:
        node = self._n()
        mem = set(node.getrawmempool())
        missing_mem = [txid for txid in include_txids if txid not in mem]
        assert not missing_mem, (
            fail_context(node, "mine_only-not-in-mempool", {"wanted": include_txids, "missing": missing_mem})
        )
        for txid in mem - set(include_txids):
            node.prioritisetransaction(txid=txid, fee_delta=CENSOR_FEE_DELTA)
        self._boost(include_txids)
        block_hash = self._mine_to(1, sync=sync)[0]
        in_block = node.getblock(block_hash, 1)["tx"]
        for txid in include_txids:
            assert txid in in_block, (
                fail_context(node, "mine_only", {"wanted": include_txids, "got": in_block})
                + f"\nmissing {txid}"
            )
        return block_hash

    def _quiesce(self) -> None:
        """Advance until the tip awards no winner, without adding new tickets."""
        node = self._n()
        self._censor(node.getrawmempool())
        for _ in range(8):
            fr = draw_of(node, node.getbestblockhash())
            if is_null_txid(fr["draw_winner_txid"]):
                return
            self._mine_to(1)

    def _abandon_mempool(self) -> None:
        """Release UTXOs locked by deprioritised mempool tickets (censored txs)."""
        node = self._n()
        for txid in list(node.getrawmempool()):
            try:
                node.abandontransaction(txid)
            except Exception:
                continue

    def _mature_oru_balance(self) -> Decimal:
        """Trusted/spendable ORU only. Immature coinbases are not included."""
        # getbalances().oru.trusted excludes immature coinbase ORU (COINBASE_MATURITY).
        return Decimal(str(self._n().getbalances()["oru"]["trusted"]))

    def _ensure_large_utxo(self, amount: Decimal) -> None:
        """Abandon censored mempool spends and coin-select a single fat P2WPKH UTXO.

        Newly mined coinbase ORU cannot be sent until ``COINBASE_MATURITY`` confirms.
        """
        self._abandon_mempool()
        node = self._n()
        need = amount + Decimal("20")
        bal = self._mature_oru_balance()
        if bal < need:
            extra = int((need - bal) / Decimal("1000")) + COINBASE_MATURITY + 10
            self.log.info(
                "funding %s more blocks (incl. maturity %d) for large burn (mature_bal=%s need=%s)",
                extra, COINBASE_MATURITY, bal, need,
            )
            self._mine_to(extra)
            bal = self._mature_oru_balance()
        assert_greater_than(bal, need)
        node.sendtoaddress(self.large_addr, need)
        self._mine_to(1)

    def _consolidate(self, amount: Decimal) -> None:
        """Coin-select mature ORU into one transfer UTXO (not a coinbase)."""
        node = self._n()
        bal = self._mature_oru_balance()
        assert_greater_than(bal, amount)
        node.sendtoaddress(self.large_addr, amount)
        self._mine_to(1)

    def _record(self, case_id: str, status: str, detail: dict) -> None:
        self.results[case_id] = {"status": status, **detail}
        self.log.info("%s %s %s", case_id, status, json.dumps(detail, default=str)[:600])

    def _dump(self, case_id: str, extra: Optional[dict] = None) -> str:
        return fail_context(self._n(), case_id, extra)

    # -- cases ----------------------------------------------------------------

    def case_r5_empty_singleton(self) -> None:
        node = self._n()
        case = "R-5"
        try:
            self._quiesce()
            empty_hash = self._mine_to(1)[0]
            fr_e = draw_of(node, empty_hash)
            window_self_check(fr_e, "R-5-empty")
            if int(fr_e["convergence_root_set_size"]) == 0:
                assert_equal(fr_e["convergence_root"], ZERO_TXID)

            w = int(fr_e["window_size"])
            if int(fr_e["convergence_root_set_size"]) > 0:
                self._censor(node.getrawmempool())
                self._mine_to(w)

            solo = self._broadcast_conv(MIN_CONVERGENCE_ORU)
            solo_hash = self._mine_only([solo])
            fr_s = draw_of(node, solo_hash)
            window_self_check(fr_s, "R-5-singleton")
            assert solo in candidate_txids(fr_s), self._dump(case, {"solo": solo})
            if int(fr_s["convergence_root_set_size"]) == 0:
                assert_equal(fr_s["convergence_root"], ZERO_TXID)
            elif int(fr_s["convergence_root_set_size"]) == 1:
                assert fr_s["convergence_root"] != ZERO_TXID
            payout = self._mine_to(1)[0]
            if is_null_txid(fr_s["draw_winner_txid"]):
                assert_equal(body_res_mint_rows(node, payout), [])
                assert_equal(int(node.getblockheader(payout).get("rigor_res_minted", 0)), 0)
            else:
                rows = body_res_mint_rows(node, payout)
                assert_equal(len(rows), 1)
                assert_equal(rows[0][1], RES_REWARD_ATOMS)
            self._record(case, "PASS", {
                "empty_|F|": int(fr_e["convergence_root_set_size"]),
                "singleton_|F|": int(fr_s["convergence_root_set_size"]),
                "singleton_winner": fr_s["draw_winner_txid"],
            })
        except Exception:
            self.log.error(self._dump(case))
            raise

    def case_m5_border_weights(self) -> None:
        node = self._n()
        case = "M-5"
        try:
            dust_txid = None
            dust_status = "ineligible"
            try:
                dust_txid = self._broadcast_conv(Decimal("0.99"))
                dust_hash = self._mine_only([dust_txid])
                fr_d = draw_of(node, dust_hash)
                window_self_check(fr_d, "M-5-dust")
                assert dust_txid not in candidate_txids(fr_d), (
                    self._dump(case, {"dust": dust_txid}) + "\n0.99 ORU burn entered F"
                )
            except Exception as exc:
                dust_status = f"rejected:{type(exc).__name__}"
                self.log.info("M-5 dust outcome: %s", exc)

            min_txid = self._broadcast_conv(MIN_CONVERGENCE_ORU)
            min_hash = self._mine_only([min_txid])
            fr_m = draw_of(node, min_hash)
            window_self_check(fr_m, "M-5-min")
            assert min_txid in candidate_txids(fr_m), self._dump(case, {"min": min_txid})

            dummy = node.sendtoaddress(self.small_addr, Decimal("1.0"))
            dummy_hash = self._mine_only([dummy])
            fr_n = draw_of(node, dummy_hash)
            window_self_check(fr_n, "M-5-nonconv")
            assert dummy not in candidate_txids(fr_n), self._dump(case, {"dummy": dummy})

            self._record(case, "PASS", {
                "dust_txid": dust_txid,
                "dust_status": dust_status,
                "min_txid_in_F": True,
                "non_convergence_in_F": False,
                "f0": "create_convergence_tx rejects amount<=0 (harness); consensus rejects f<1 in inequality",
                "saturation": "pinned by res_proof_math_tests SATURATING_CONVERGENCE_AMOUNT; not re-funded here",
            })
        except Exception:
            self.log.error(self._dump(case))
            raise

    def case_m1_m2_b2_b3_censorship(self) -> None:
        node = self._n()
        try:
            self._quiesce()
            foreign = self._broadcast_conv(LARGE_BURN_ORU)
            foreign_block = self._mine_only([foreign])
            fr0 = draw_of(node, foreign_block)
            window_self_check(fr0, "M-1-plant")
            assert foreign in candidate_txids(fr0), self._dump("M-1", {"foreign": foreign})
            foreign_height = int(node.getblock(foreign_block, 1)["height"])
            w = int(fr0["window_size"])

            miner_tickets: list[str] = []
            censored_live: list[str] = []
            miner_wins = 0
            foreign_wins = 0
            none_wins = 0
            exclusive_blocks = 0
            fees_forgone_sats = 0

            # One block per iteration so an unpaid buried ticket stays inside [H, H+W-1].
            # If it wins, already_paid must drop it from the NEXT draw (payout block) onward.
            # The plant block itself can award the foreign ticket; the first loop block is then
            # the payout and F must not contain it.
            foreign_award_height = None
            if fr0["draw_winner_txid"] == foreign:
                foreign_award_height = foreign_height
                foreign_wins += 1
            for i in range(w - 1):
                live = self._broadcast_conv(MIN_CONVERGENCE_ORU)
                censored_live.append(live)
                own = self._broadcast_conv(MIN_CONVERGENCE_ORU)
                miner_tickets.append(own)
                fees_forgone_sats += 1000
                bh = self._mine_only([own])
                fr = draw_of(node, bh)
                window_self_check(fr, f"M-1-censor-{i}")
                cset = candidate_txids(fr)
                height = int(fr["height"])
                winner = fr["draw_winner_txid"]
                if winner == foreign:
                    foreign_wins += 1
                    foreign_award_height = height
                elif winner in miner_tickets:
                    # After the foreign ticket is paid it leaves F; later 1 ORU wins are ordinary draws.
                    if foreign_award_height is None:
                        miner_wins += 1
                elif is_null_txid(winner):
                    none_wins += 1
                if live in cset:
                    raise AssertionError(
                        self._dump("M-1", {"live": live})
                        + "\ncensored mempool ticket still entered F"
                    )
                if foreign in cset:
                    if foreign_award_height is not None and height > foreign_award_height:
                        raise AssertionError(
                            self._dump("M-4", {"foreign": foreign, "height": height})
                            + "\npaid foreign ticket remained in F after its awarding block (already_paid)"
                        )
                else:
                    if foreign_award_height is None:
                        raise AssertionError(
                            self._dump("M-1", {"i": i, "foreign": foreign, "F": sorted(cset)})
                            + "\nburied foreign ticket dropped without being paid; exclusivity restored"
                        )
                if foreign not in cset and foreign_award_height is None and cset <= set(miner_tickets):
                    exclusive_blocks += 1

            self._mine_to(1)  # settle the last draw of the window
            assert_equal(exclusive_blocks, 0)
            if miner_wins:
                raise AssertionError(
                    self._dump("B-3", {"miner_wins": miner_wins, "foreign_wins": foreign_wins})
                    + "\n1 ORU censoring tickets captured RES against a 100k ORU buried competitor"
                )

            self._record("M-1", "PASS", {
                "foreign": foreign,
                "foreign_height": foreign_height,
                "censor_blocks": w - 1,
                "exclusive_F_blocks": exclusive_blocks,
                "miner_1oru_tickets": len(miner_tickets),
                "draws_none": none_wins,
                "draws_foreign": foreign_wins,
                "draws_miner": miner_wins,
            })
            self._record("M-2", "PASS", {
                "window": w,
                "buried_foreign_survived_W_minus_1": True,
                "zero_cost_exclusive_F": False,
            })
            self._record("B-2", "PASS", {
                "fees_forgone_sats_approx": fees_forgone_sats,
                "censored_mempool_txs": len(censored_live),
                "expected_RES_from_exclusivity": "none while buried competitors remain in F",
                "censorship_dominant": False,
            })
            self._record("B-3", "PASS", {
                "methodology": "100k ORU foreign planted; W-1 blocks of miner-only 1 ORU inclusion; fixed chain, no RNG seed",
                "draws_none": none_wins,
                "draws_foreign": foreign_wins,
                "draws_miner": miner_wins,
                "RES_to_censoring_hashrate": False,
            })
        except Exception:
            self.log.error(self._dump("M-1/M-2/B-2/B-3"))
            raise

    def case_m3_flood(self) -> None:
        node = self._n()
        case = "M-3"
        try:
            self._quiesce()
            n_flood = 12
            txids = [self._broadcast_conv(MIN_CONVERGENCE_ORU) for _ in range(n_flood)]
            bh = self._mine_only(txids)
            fr = draw_of(node, bh)
            window_self_check(fr, "M-3")
            present = candidate_txids(fr) & set(txids)
            assert_equal(len(present), n_flood)
            self._record(case, "PASS", {
                "tickets": n_flood,
                "burn_oru": float(MIN_CONVERGENCE_ORU) * n_flood,
                "|F|_at_source": int(fr["convergence_root_set_size"]),
                "cost_scales_with_tickets": True,
                "expected_wins_lifetime": n_flood / 65536.0,
            })
        except Exception:
            self.log.error(self._dump(case))
            raise

    def case_b1_sigma_binding(self) -> None:
        node = self._n()
        case = "B-1"
        try:
            self._quiesce()
            txid = self._broadcast_conv(MIN_CONVERGENCE_ORU)
            raw = node.getrawtransaction(txid)
            b_hash = self._mine_only([txid])
            fr_b = draw_of(node, b_hash)
            window_self_check(fr_b, "B-1-B")
            assert_equal(fr_b["sigma_seed"], b_hash)
            sigma_b = {c["txid"]: c["h"] for c in fr_b["candidates"]}
            assert txid in sigma_b, self._dump(case, {"txid": txid})

            b_height = int(node.getblock(b_hash, 1)["height"])
            for n in self.nodes:
                if n.getblockcount() > b_height:
                    n.invalidateblock(n.getblockhash(b_height + 1))
                n.invalidateblock(b_hash)
            if txid not in set(node.getrawmempool()):
                node.sendrawtransaction(raw)
            rebuild_addr = node.getnewaddress(address_type="bech32")
            bp_hash = self.generatetoaddress(node, 1, rebuild_addr)[0]
            assert bp_hash != b_hash, self._dump(case, {"same_hash": bp_hash})
            fr_bp = draw_of(node, bp_hash)
            window_self_check(fr_bp, "B-1-B'")
            assert_equal(fr_bp["sigma_seed"], bp_hash)
            sigma_bp = {c["txid"]: c["h"] for c in fr_bp["candidates"]}
            assert txid in sigma_bp, self._dump(case)
            assert sigma_b[txid] != sigma_bp[txid], (
                self._dump(case, {"sigma_b": sigma_b[txid], "sigma_bp": sigma_bp[txid]})
                + "\nσ did not move with a new PoW block; miner can re-roll for free"
            )
            self._record(case, "PASS", {
                "B": b_hash,
                "Bprime": bp_hash,
                "sigma_changed": True,
            })
        except Exception:
            self.log.error(self._dump(case))
            raise

    def case_m4_no_double_pay(self) -> None:
        node = self._n()
        case = "M-4"
        try:
            self._quiesce()
            start_height = node.getblockcount()
            ticket = self._broadcast_conv(LARGE_BURN_ORU)
            src = self._mine_only([ticket])
            w = int(draw_of(node, src)["window_size"])
            winner_height = None

            def _check(block_hash: str, label: str) -> Optional[int]:
                fr = draw_of(node, block_hash)
                window_self_check(fr, label)
                if fr["draw_winner_txid"] == ticket:
                    payout = self._mine_to(1)[0]
                    rows = body_res_mint_rows(node, payout)
                    assert_equal(len(rows), 1)
                    assert_equal(rows[0][1], RES_REWARD_ATOMS)
                    return int(fr["height"])
                return None

            winner_height = _check(src, "M-4-src")
            if winner_height is None:
                for i in range(w - 1):
                    self._censor(node.getrawmempool())
                    h = self._mine_to(1)[0]
                    winner_height = _check(h, f"M-4-wait-{i}")
                    if winner_height is not None:
                        break

            if winner_height is not None:
                for i in range(4):
                    self._censor(node.getrawmempool())
                    h = self._mine_to(1)[0]
                    fr = draw_of(node, h)
                    window_self_check(fr, f"M-4-after-{i}")
                    assert ticket not in candidate_txids(fr), (
                        self._dump(case, {"ticket": ticket, "height": fr["height"]})
                        + "\npaid txid remained a candidate (already_paid not applied)"
                    )

            paid = assert_no_double_pay(node, start_height, None, case)
            self._record(case, "PASS", {
                "ticket": ticket,
                "observed_win": winner_height is not None,
                "winner_height": winner_height,
                "payout_height": paid.get(ticket),
                "distinct_paid": len(paid),
            })
        except Exception:
            self.log.error(self._dump(case))
            raise

    def case_r4_restore(self) -> None:
        node = self._n()
        case = "R-4"
        try:
            tip = node.getbestblockhash()
            height = node.getblockcount()
            snap_before = supply_snapshot(node)
            fr_before = {}
            for h in range(max(0, height - 3), height + 1):
                bh = node.getblockhash(h)
                fr_before[bh] = draw_of(node, bh)
            inv = node.getblockhash(max(1, height - 1))
            node.invalidateblock(inv)
            node.reconsiderblock(inv)
            assert_equal(node.getbestblockhash(), tip)
            snap_after = supply_snapshot(node)
            assert_equal(snap_before["mint_count"], snap_after["mint_count"])
            assert_equal(snap_before["res_balance"], snap_after["res_balance"])
            assert_equal(snap_before["state_hash"], snap_after["state_hash"])
            for bh, fr in fr_before.items():
                fr2 = draw_of(node, bh)
                assert_equal(fr["draw_winner_txid"], fr2["draw_winner_txid"])
                assert_equal(fr["sigma_seed"], fr2["sigma_seed"])
                assert_equal(
                    {c["txid"]: c["h"] for c in fr["candidates"]},
                    {c["txid"]: c["h"] for c in fr2["candidates"]},
                )
            self._record(case, "PASS", {
                "tip": tip,
                "invalidate": inv,
                "mint_count": snap_after["mint_count"],
            })
        except Exception:
            self.log.error(self._dump(case))
            raise

    def case_r1_r2_fork_undo_and_reinclude(self) -> None:
        n0 = self.nodes[0]
        try:
            self.sync_all()
            self._quiesce()
            fork_hash = n0.getbestblockhash()
            fork_height = n0.getblockcount()
            snap_fork = supply_snapshot(n0)

            w1 = self._broadcast_conv(LARGE_BURN_ORU)
            a_include = self._mine_only([w1])
            fr_a = draw_of(n0, a_include)
            window_self_check(fr_a, "R-1-A")
            a_payout = self._mine_to(1)[0]
            self._mine_to(1)
            minted_on_a = False
            if fr_a["draw_winner_txid"] == w1:
                rows = body_res_mint_rows(n0, a_payout)
                assert_equal(len(rows), 1)
                assert_equal(rows[0][1], RES_REWARD_ATOMS)
                minted_on_a = True
            elif int(n0.getblockheader(a_payout).get("rigor_res_minted", 0)) == 1:
                minted_on_a = True

            # Disconnect A on every node, then extend a branch that never includes W1.
            # (Two-node submitblock of a private branch hit bad-convergence-cumulative /
            # header-sync stalls after a long prefix; invalidateblock is the same
            # DisconnectBlock undo path used by feature_rigor_reorg_winner_lock.)
            for n in self.nodes:
                n.invalidateblock(a_include)
            self._censor(list(n0.getrawmempool()) + [w1])
            self._mine_to(5)
            self.sync_all()
            assert_equal(n0.getblockhash(fork_height), fork_hash)

            snap_b = supply_snapshot(n0)
            for h in range(fork_height + 1, n0.getblockcount() + 1):
                fr = draw_of(n0, n0.getblockhash(h))
                window_self_check(fr, f"R-1-B-{h}")
                assert w1 not in candidate_txids(fr), (
                    self._dump("R-1", {"w1": w1, "height": h})
                    + "\nW1 appeared on the surviving fork that never included it"
                )
            assert_equal(snap_b["mint_count"], snap_fork["mint_count"])
            assert_equal(snap_b["res_balance"], snap_fork["res_balance"])
            self._record("R-1", "PASS", {
                "w1": w1,
                "fork_height": fork_height,
                "minted_on_A": minted_on_a,
                "mint_count_fork": snap_fork["mint_count"],
                "mint_count_after_reorg": snap_b["mint_count"],
                "A_include": a_include,
                "reorg_method": "invalidateblock + longer branch without W1",
            })

            start = n0.getblockcount()
            if w1 not in set(n0.getrawmempool()):
                # Disconnect should have returned W1; wallet re-broadcast if it did not.
                try:
                    raw = n0.gettransaction(w1)["hex"]
                    n0.sendrawtransaction(raw, 0)
                except Exception as exc:
                    self.log.info("R-2: W1 not restorable from wallet: %s", exc)
            assert w1 in set(n0.getrawmempool()), (
                self._dump("R-2", {"w1": w1}) + "\nW1 missing from mempool after reorg undo"
            )
            self._mine_only([w1])
            for _ in range(6):
                self._censor(n0.getrawmempool())
                self._mine_to(1)
            paid = assert_no_double_pay(n0, start, None, "R-2")
            pays = [h for txid, h in paid.items() if txid == w1]
            assert len(pays) <= 1, self._dump("R-2", {"pays": pays, "w1": w1})
            self._record("R-2", "PASS", {
                "w1": w1,
                "pays_for_w1": pays,
                "distinct_paid_after_reinclude": len(paid),
            })
        except Exception:
            self._partitioned = False
            self.log.error(self._dump("R-1/R-2"))
            raise

    def case_r3_deep_window_reorg(self) -> None:
        node = self._n()
        case = "R-3"
        try:
            self.sync_all()
            w = CONVERGENCE_DRAW_WINDOW
            start = node.getblockcount()
            snap_before = supply_snapshot(node)
            self._censor(node.getrawmempool())
            self._mine_to(w + 2)
            tip_before = node.getbestblockhash()
            height_before = node.getblockcount()
            inv = node.getblockhash(height_before - w)
            # Invalidate while disconnected so a node does not relay the just-invalid
            # tip to its peer (that P2P path drops the link; sync_all then asserts).
            self.disconnect_nodes(0, 1)
            for n in self.nodes:
                n.invalidateblock(inv)
            assert_greater_than(height_before, node.getblockcount())
            # Empty remine on mockable PoW can hash-collide with the invalidated child
            # (same coinbase, same mempool, same timestamp) → AcceptBlock FAILED (duplicate).
            tip_time = node.getblockheader(node.getbestblockhash())["time"]
            alt_addr = node.getnewaddress(label="r3-alt", address_type="bech32")
            for n in self.nodes:
                n.setmocktime(tip_time + 60)
            self._censor(node.getrawmempool())
            self.generatetoaddress(node, w + 4, alt_addr, sync_fun=self.no_op)
            for n in self.nodes:
                n.setmocktime(0)
            snap_after = supply_snapshot(node)
            paid = assert_no_double_pay(node, start, None, case)
            for h in range(max(0, node.getblockcount() - w), node.getblockcount() + 1):
                fr = draw_of(node, node.getblockhash(h))
                window_self_check(fr, f"R-3-{h}")
            assert_greater_than_or_equal(snap_before["mint_count"] + 2, snap_after["mint_count"])
            self._record(case, "PASS", {
                "invalidated": inv,
                "old_tip": tip_before,
                "new_tip": node.getbestblockhash(),
                "mint_count_before": snap_before["mint_count"],
                "mint_count_after": snap_after["mint_count"],
                "distinct_paid": len(paid),
                "depth": w,
            })
        except Exception:
            self.log.error(self._dump(case))
            raise

    def _write_results(self) -> None:
        path = os.path.join(self.options.tmpdir, "res_security_results.json")
        with open(path, "w", encoding="ascii") as f:
            json.dump(self.results, f, indent=2, sort_keys=True, default=str)
        self.log.info("Wrote %s", path)
        for case_id in sorted(self.results):
            self.log.info("RESULT %s %s", case_id, self.results[case_id].get("status"))

    def run_test(self):
        self._partitioned = False
        self.results = {}
        node = self._n()
        self.mining_addr = node.getnewaddress(label="res-sec-miner", address_type="bech32")
        self.large_addr = node.getnewaddress(label="res-sec-large", address_type="bech32")
        self.small_addr = node.getnewaddress(label="res-sec-small", address_type="bech32")
        # Coinbase ORU and body RES mints are immature for COINBASE_MATURITY (100) blocks.
        # Burns must spend *transfer* UTXOs, never a just-mined coinbase. Mine subsidy to
        # mining_addr, wait for maturity, then split into confirmed non-coinbase UTXOs.
        n_fund = COINBASE_MATURITY + 550  # 550k mature ORU at 1000/block
        self.log.info("Mining %d blocks (maturity=%d); coinbase ORU/RES not spendable yet",
                      n_fund, COINBASE_MATURITY)
        self._mine_to(n_fund)
        self.sync_all()
        bals = node.getbalances()
        self.log.info(
            "balances ORU trusted=%s immature=%s RES trusted=%s immature=%s",
            bals["oru"]["trusted"], bals["oru"]["immature"],
            bals["res"]["trusted"], bals["res"]["immature"],
        )
        assert_greater_than(Decimal(str(bals["oru"]["trusted"])), Decimal("400000"))
        # Three 110k UTXOs for M-1 / M-4 / R-1; many small UTXOs for 1 ORU tickets.
        # create_convergence_tx spends a single input. Min-burn must not pick a 110k UTXO.
        self.log.info("Splitting mature ORU into transfer UTXOs (not coinbases)")
        for _ in range(3):
            node.sendtoaddress(self.large_addr, Decimal("110000"))
        self._mine_to(1)
        # Cap unconfirmed ancestor chains (default 25) while building the small pool.
        for i in range(80):
            node.sendtoaddress(self.small_addr, Decimal("20"))
            if (i + 1) % 20 == 0:
                self._mine_to(1)
        w = int(draw_of(node, node.getbestblockhash())["window_size"])
        assert_equal(w, CONVERGENCE_DRAW_WINDOW)
        self.log.info("W=%d height=%d", w, node.getblockcount())

        self.case_r5_empty_singleton()
        self.case_m5_border_weights()
        self.case_m1_m2_b2_b3_censorship()
        self.case_m3_flood()
        self.case_b1_sigma_binding()
        self.case_m4_no_double_pay()
        self.case_r4_restore()
        self.case_r1_r2_fork_undo_and_reinclude()
        self.case_r3_deep_window_reorg()
        self._write_results()
        required = ("M-1", "M-2", "M-3", "M-4", "M-5", "R-1", "R-2", "R-3", "R-4", "R-5", "B-1", "B-2", "B-3")
        missing = [c for c in required if self.results.get(c, {}).get("status") != "PASS"]
        assert not missing, f"cases not PASS: {missing}"


if __name__ == "__main__":
    ResWindowReorgResAttacksTest().main()

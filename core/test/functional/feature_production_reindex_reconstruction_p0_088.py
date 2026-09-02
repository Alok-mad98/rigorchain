#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P0-088: genuine rigord -reindex-chainstate and -reindex field-level equivalence.

Runs against the production binary via the functional-test node harness.
Does not modify consensus. Isolated tmpdatadir only.

Dump source of truth: RPC dumptxoutset → CreateUTXOSnapshot (CoinsDB cursor).
"""

from __future__ import annotations

import os
import sys
from decimal import Decimal
from pathlib import Path

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal
from test_framework.rigor_block_builder import create_convergence_tx

from res_security.harness import (
    CENSOR_FEE_DELTA,
    COINBASE_MATURITY,
    RES_REWARD_ATOMS,
    SATURATING_CONVERGENCE_ORU,
    body_res_mint_rows,
    draw_of,
    is_res_mint_vout,
)

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools"))
import production_reindex_state as prs  # noqa: E402

RES_REWARD = Decimal("40.000000")
ORU_FEE = Decimal("0.0001")


def dump_dir() -> Path:
    env = os.environ.get("RIGOR_P088_DUMP_DIR")
    if env:
        p = Path(env)
    else:
        p = ROOT / "formal_verification" / "database_audit"
    p.mkdir(parents=True, exist_ok=True)
    return p


class ProductionReindexReconstructionTest(BitcoinTestFramework):
    def set_test_params(self):
        self.chain = "devtest"
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.rpc_timeout = 2400
        self.wallet_names = ["default_wallet"]
        self._requires_wallet = True
        self.extra_args = [[
            "-solomining=1",
            "-minrelaytxfee=0",
            "-blockmintxfee=0",
        ]]

    def setup_nodes(self):
        """Keep LogPrintf (reindex markers) but drop framework -debug/trace (multi-GB logs)."""
        self.add_nodes(self.num_nodes, self.extra_args)
        drop = {"-debug", "-loglevel=trace", "-logsourcelocations", "-logthreadnames", "-logtimemicros"}
        for n in self.nodes:
            n.args = [a for a in n.args if a not in drop and not str(a).startswith("-loglevel=")]
        self.start_nodes()
        if self._requires_wallet:
            self.import_deterministic_coinbase_privkeys()

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def _n(self):
        return self.nodes[0]

    def _mine(self, n: int = 1) -> list:
        return self.generatetoaddress(self._n(), n, self.mining_addr)

    def _base_args(self) -> list:
        args = list(self.extra_args[0]) + [
            "-reswalletbootstrap",
            f"-miningresfeeaddr={self.res_fee_addr}",
        ]
        for a in self.nodes[0].extra_args or []:
            if str(a).startswith("-sandbox") and a not in args:
                args.append(a)
        return args

    def _restart(self, extra=None):
        args = self._base_args() + list(extra or [])
        self.stop_node(0)
        self.start_node(0, extra_args=args)

    def _shutdown_restart_flush_restart(self):
        self._restart()
        self._n().gettxoutsetinfo("none")
        self._restart()

    def _debug_log(self) -> Path:
        return Path(self.nodes[0].datadir) / self.chain / "debug.log"

    def _assert_log_contains(self, needle: str, label: str):
        text = self._debug_log().read_text(encoding="utf-8", errors="replace")
        if needle not in text:
            excerpt = dump_dir() / f"{label}_debug_excerpt.txt"
            excerpt.write_text(text[-8000:], encoding="utf-8")
            raise AssertionError(f"{label}: production log marker missing: {needle!r}")
        (dump_dir() / f"{label}_log_marker.txt").write_text(needle + "\n", encoding="utf-8")

    def _draw_state(self) -> dict:
        node = self._n()
        stats = node.getconvergencestats()
        replay = node.getreplaysnapshot()
        tip = node.getbestblockhash()
        fr = draw_of(node, tip)
        return {
            "best_height": stats["best_height"],
            "active_convergences": stats["active_convergences"],
            "active_oru": stats["active_oru"],
            "pending_windows": stats["pending_windows"],
            "total_res_minted": stats["total_res_minted"],
            "replay_height": replay["height"],
            "replay_block_hash": replay["block_hash"],
            "replay_mint_count": replay["mint_count"],
            "replay_state_hash": replay["state_hash"],
            "replay_utxo_count": replay["utxo_count"],
            "tip_hash": tip,
            "draw_winner_txid": fr.get("draw_winner_txid"),
            "payout_height": fr.get("payout_height"),
            "convergence_root_set_size": fr.get("convergence_root_set_size"),
        }

    def _dump_snapshot(self, name: str) -> dict:
        node = self._n()
        node.syncwithvalidationinterfacequeue()
        info = node.getblockchaininfo()
        tip = int(info["blocks"])
        tip_hash = info["bestblockhash"]
        dump_name = f"{name}.utxo.dat"
        dest = Path(node.datadir) / self.chain / dump_name
        if dest.exists():
            dest.unlink()
        dumped = node.dumptxoutset(dump_name)
        dest = Path(dumped["path"])
        size = dest.stat().st_size if dest.is_file() else 0
        if size < 40:
            raise AssertionError(
                f"{name}: dumptxoutset wrote {size} bytes at {dest} "
                "(truncated dump; often a full disk). Not a reconstruction PASS."
            )
        parsed = prs.parse_dumptxoutset(dest)
        assert_equal(parsed["coins_count"], dumped["coins_written"])
        extra = {
            "best_block": tip_hash,
            "chain_height": tip,
            "draw_state": self._draw_state(),
            "binary": self.nodes[0].binary,
        }
        doc = prs.finalize_records(parsed["coins"], tip, extra)
        if doc["convergence_in_utxo_count"] != 0:
            raise AssertionError(
                f"{name}: convergence outputs entered UTXO ({doc['convergence_in_utxo_count']})"
            )
        slim = dict(doc)
        slim["best_block"] = tip_hash
        slim["chain_height"] = tip
        slim["digest"] = prs.state_digest(slim)
        prs.write_json(dump_dir() / f"{name}_STATE_FIELDS.json", slim)
        try:
            dest.unlink()
        except OSError:
            pass
        (dump_dir() / f"{name}_STATE_DIGEST").write_text(slim["digest"] + "\n", encoding="utf-8")
        self.log.info(
            "%s dump: height=%s utxos=%s oru=%s res=%s mint=%s digest=%s",
            name,
            tip,
            slim["utxo_count"],
            slim["oru_count"],
            slim["res_count"],
            slim["mint_count"],
            slim["digest"][:16],
        )
        return slim

    def _cache_snapshot_via_gettxout(self) -> dict:
        """CoinsTip walk. Discovers outpoints from the block index, not from dumptxoutset."""
        node = self._n()
        tip = node.getblockcount()
        tip_hash = node.getbestblockhash()
        coins = []
        for height in range(0, tip + 1):
            blk = node.getblock(node.getblockhash(height), 2)
            for tx in blk["tx"]:
                txid = tx["txid"]
                for o in tx["vout"]:
                    n = int(o["n"])
                    utxo = node.gettxout(txid, n, False)
                    if utxo is None:
                        continue
                    spk = utxo.get("scriptPubKey") or {}
                    script_hex = str(spk.get("hex", "")).lower()
                    script = bytes.fromhex(script_hex) if script_hex else b""
                    conf = int(utxo["confirmations"])
                    coin_height = tip - conf + 1
                    mint_meta = prs.parse_res_mint(script)
                    rec = {
                        "txid": txid,
                        "vout": n,
                        "membership": True,
                        "asset": prs.classify_asset(script),
                        "amount": None,
                        "height": coin_height,
                        "coinbase": bool(utxo.get("coinbase")),
                        "script": script.hex(),
                        "mint": bool(utxo.get("res_mint")) or mint_meta is not None,
                        "convergence": prs.is_convergence_script(script),
                        "encoded_maturity_height": None if mint_meta is None else mint_meta["encoded_maturity_height"],
                        "payout_hash160": None if mint_meta is None else mint_meta["payout_hash160"],
                    }
                    rec["maturity"] = prs.coinbase_mature(tip, rec["height"], rec["coinbase"])
                    coins.append(rec)
        doc = {
            "tip_height": tip,
            "best_block": tip_hash,
            "coins": sorted(coins, key=lambda r: (r["txid"], r["vout"])),
        }
        prs.write_json(dump_dir() / "CACHE_HEAVY_STATE_FIELDS.json", doc)
        self.log.info("CACHE_HEAVY gettxout walk: height=%s utxos=%s", tip, len(coins))
        return doc

    def _wallet_res_program32(self) -> bytes:
        addr = self._n().getnewaddress("", "bech32m")
        spk = self._n().getaddressinfo(addr)["scriptPubKey"]
        if isinstance(spk, dict):
            spk = spk["hex"]
        h = str(spk).lower()
        assert h.startswith("5120") and len(h) == 68, h
        return bytes.fromhex(h[4:])

    def _pick_oru_fee_utxo(self, excluded=None) -> dict:
        excluded = excluded or set()
        for u in self._n().listunspent(1, 9999999):
            pid = (u["txid"], u["vout"])
            if pid in excluded:
                continue
            if str(u.get("asset", "ORU")).upper() == "RES":
                continue
            if not u.get("spendable", True):
                continue
            if Decimal(str(u["amount"])) >= ORU_FEE + Decimal("0.01"):
                return u
        raise AssertionError("no ORU fee utxo")

    def _build_res_spend_hex(self, mint_txid: str, mint_vout: int, res_amt: Decimal) -> str:
        node = self._n()
        oru_u = self._pick_oru_fee_utxo({(mint_txid, mint_vout)})
        oru_in = Decimal(str(oru_u["amount"]))
        oru_change = oru_in - ORU_FEE
        res_recv = node.getnewaddress("", "bech32m")
        oru_change_addr = node.getnewaddress("", "bech32")
        raw = node.createrawtransaction(
            [
                {"txid": mint_txid, "vout": mint_vout},
                {"txid": oru_u["txid"], "vout": oru_u["vout"]},
            ],
            {
                res_recv: {"amount": format(res_amt, "f"), "asset": "RES"},
                oru_change_addr: format(oru_change, "f"),
            },
        )
        signed = node.signrawtransactionwithwallet(raw)
        assert signed["complete"], signed
        return signed["hex"]

    def _mint_outpoint(self, payout_hash: str):
        rows = body_res_mint_rows(self._n(), payout_hash)
        assert_equal(len(rows), 1)
        assert_equal(rows[0][1], RES_REWARD_ATOMS)
        mint_txid = rows[0][0]
        tx = self._n().getrawtransaction(mint_txid, True, payout_hash)
        for o in tx["vout"]:
            if is_res_mint_vout(o):
                return mint_txid, int(o["n"])
        raise AssertionError("no mint vout")

    def _mature_oru_utxos(self, address: str) -> list:
        rows = []
        for u in self._n().listunspent(1, 9999999, [address]):
            if str(u.get("asset", "ORU")).upper() == "RES":
                continue
            if not u.get("spendable", True):
                continue
            rows.append(u)
        return rows

    def _merge_pay_to(self, inputs: list, dest: str) -> None:
        """Pay the selected ORU inputs to dest minus a small fee. One tx, then mine."""
        node = self._n()
        total = sum(Decimal(str(u["amount"])) for u in inputs)
        fee = Decimal("0.0001")
        amt = total - fee
        assert amt > 0, total
        raw = node.createrawtransaction(
            [{"txid": u["txid"], "vout": u["vout"]} for u in inputs],
            {dest: format(amt, "f")},
        )
        signed = node.signrawtransactionwithwallet(raw)
        assert signed["complete"], signed
        node.sendrawtransaction(signed["hex"])
        self._mine(1)

    def _ensure_fat_oru(self, dest: str, need: Decimal) -> None:
        """Build a single ORU UTXO >= need on dest without sendtoaddress coin selection."""
        node = self._n()
        while True:
            dest_utxos = self._mature_oru_utxos(dest)
            if any(Decimal(str(u["amount"])) >= need for u in dest_utxos):
                return
            dest_total = sum(Decimal(str(u["amount"])) for u in dest_utxos)
            if dest_total >= need:
                dest_utxos.sort(key=lambda u: Decimal(str(u["amount"])), reverse=True)
                self._merge_pay_to(dest_utxos[:80], dest)
                continue
            src = self._mature_oru_utxos(self.mining_addr)
            src_total = sum(Decimal(str(u["amount"])) for u in src)
            if src_total + dest_total < need:
                self._mine(120)
                continue
            src.sort(key=lambda u: Decimal(str(u["amount"])), reverse=True)
            take = []
            acc = Decimal("0")
            for u in src:
                take.append(u)
                acc += Decimal(str(u["amount"]))
                if acc >= need - dest_total or len(take) >= 80:
                    break
            assert take, "no mining ORU to merge"
            self._merge_pay_to(take, dest)

    def _fund_saturating(self):
        need = SATURATING_CONVERGENCE_ORU + Decimal("20")
        self._ensure_fat_oru(self.large_addr, need)
        fat = [
            u for u in self._mature_oru_utxos(self.large_addr)
            if Decimal(str(u["amount"])) >= SATURATING_CONVERGENCE_ORU
        ]
        assert fat, "saturating ORU utxo missing after merge"

    def _broadcast_saturating(self) -> str:
        node = self._n()
        tx = create_convergence_tx(
            node,
            SATURATING_CONVERGENCE_ORU,
            spend_addresses=[self.large_addr],
            change_address=self.large_addr,
            res_owner_program=self.res_program32,
        )
        txid = node.sendrawtransaction(tx.serialize_with_witness().hex(), 0, format(SATURATING_CONVERGENCE_ORU, "f"))
        assert node.getmempoolentry(txid).get("convergence", {}).get("is_convergence")
        return txid

    def _mine_only(self, include_txids: list) -> str:
        node = self._n()
        mem = set(node.getrawmempool())
        missing = [t for t in include_txids if t not in mem]
        assert not missing, missing
        for txid in mem - set(include_txids):
            node.prioritisetransaction(txid=txid, fee_delta=CENSOR_FEE_DELTA)
        for txid in include_txids:
            node.prioritisetransaction(txid=txid, fee_delta=-CENSOR_FEE_DELTA * 8)
        bh = self._mine(1)[0]
        in_block = node.getblock(bh, 1)["tx"]
        for txid in include_txids:
            assert txid in in_block, (txid, in_block)
        return bh

    def _create_mint(self, label: str):
        self.log.info("%s: saturating convergence → mint", label)
        self._fund_saturating()
        ticket = self._broadcast_saturating()
        award = self._mine_only([ticket])
        fr = draw_of(self._n(), award)
        assert fr["draw_winner_txid"] == ticket, fr
        payout = self._mine(1)[0]
        mint_txid, mint_vout = self._mint_outpoint(payout)
        utxo = self._n().gettxout(mint_txid, mint_vout, False)
        assert utxo is not None
        assert utxo.get("res_mint") is True
        assert utxo.get("coinbase") is True
        mint_height = self._n().getblock(payout)["height"]
        self.log.info("%s mint %s:%d height=%d", label, mint_txid, mint_vout, mint_height)
        return mint_txid, mint_vout, payout, mint_height

    def _invalid_mix_rejected(self):
        node = self._n()
        utxos = [u for u in node.listunspent(1) if str(u.get("asset", "ORU")).upper() != "RES"]
        assert utxos
        u = utxos[0]
        res_addr = node.getnewaddress("", "bech32m")
        raw = node.createrawtransaction(
            [{"txid": u["txid"], "vout": u["vout"]}],
            {res_addr: {"amount": "0.01", "asset": "RES"}},
        )
        signed = node.signrawtransactionwithwallet(raw)
        assert signed["complete"]
        res = node.testmempoolaccept([signed["hex"]])[0]
        assert_equal(res["allowed"], False)
        assert_equal(res["reject-reason"], "mixed-asset-output")

    def _build_chain(self):
        node = self._n()
        self.mining_addr = node.getnewaddress(label="p088-miner", address_type="bech32")
        self.large_addr = node.getnewaddress(label="p088-large", address_type="bech32")
        self.res_fee_addr = node.getnewaddress("", "bech32m")
        self.res_program32 = self._wallet_res_program32()

        self.log.info("Restart with -reswalletbootstrap so coinbases carry ordinary RES")
        self._restart()

        n_fund = 4300
        self.log.info("Mining %d blocks (two saturating mints + maturity)", n_fund)
        self._mine(n_fund)

        self.log.info("ORU send / change / fee / witness v0/20")
        dest = node.getnewaddress("", "bech32")
        node.sendtoaddress(dest, Decimal("12.34"))
        self._mine(1)

        self.log.info("Invalid ORU→RES conversion is rejected")
        self._invalid_mix_rejected()

        self.log.info("Dual-pool ordinary RES spend + ORU fee")
        res_u = None
        for u in node.listunspent(1):
            if u.get("address") == self.res_fee_addr:
                res_u = u
                break
        assert res_u is not None, "no bootstrap RES on miningresfeeaddr"
        hex_dual = self._build_res_spend_hex(res_u["txid"], res_u["vout"], Decimal(str(res_u["amount"])))
        acc = node.testmempoolaccept([hex_dual])[0]
        assert acc.get("allowed"), acc
        node.sendrawtransaction(hex_dual)
        self._mine(1)

        self.log.info("RES-only spend without ORU fee is rejected")
        res_u2 = None
        for u in node.listunspent(1):
            if u.get("address") == self.res_fee_addr and (u["txid"], u["vout"]) != (res_u["txid"], res_u["vout"]):
                res_u2 = u
                break
        if res_u2:
            res_recv = node.getnewaddress("", "bech32m")
            raw = node.createrawtransaction(
                [{"txid": res_u2["txid"], "vout": res_u2["vout"]}],
                {res_recv: {"amount": format(Decimal(str(res_u2["amount"])), "f"), "asset": "RES"}},
            )
            signed = node.signrawtransactionwithwallet(raw)
            r0 = node.testmempoolaccept([signed["hex"]])[0]
            assert_equal(r0["allowed"], False)
            assert_equal(r0["reject-reason"], "bad-txns-oru-fee-required")

        mint_txid, mint_vout, payout, mint_height = self._create_mint("mint1")

        self.log.info("Mint disconnect / reconnect while immature")
        node.invalidateblock(payout)
        assert node.gettxout(mint_txid, mint_vout, False) is None
        node.reconsiderblock(payout)
        restored = node.gettxout(mint_txid, mint_vout, False)
        assert restored is not None and restored.get("res_mint") is True

        spend_hex = self._build_res_spend_hex(mint_txid, mint_vout, RES_REWARD)
        acc = node.testmempoolaccept([spend_hex])[0]
        assert not acc.get("allowed"), acc
        assert "premature" in str(acc.get("reject-reason", "")).lower()

        need = mint_height + COINBASE_MATURITY - 1 - node.getblockcount()
        if need > 0:
            self._mine(need)
        acc = node.testmempoolaccept([spend_hex])[0]
        assert acc.get("allowed"), acc
        spend_txid = node.sendrawtransaction(spend_hex)
        self._mine(1)
        assert spend_txid in node.getblock(node.getbestblockhash())["tx"]
        assert node.gettxout(mint_txid, mint_vout, False) is None

        mint2_txid, mint2_vout, payout2, mint2_height = self._create_mint("mint2")
        self.log.info("Mint2 disconnect / reconnect")
        node.invalidateblock(payout2)
        assert node.gettxout(mint2_txid, mint2_vout, False) is None
        node.reconsiderblock(payout2)
        assert node.gettxout(mint2_txid, mint2_vout, False) is not None

        # Leave mint2 immature at the dump tip: mine a few ordinary blocks only.
        self._mine(5)
        utxo2 = node.gettxout(mint2_txid, mint2_vout, False)
        assert utxo2 is not None and utxo2.get("res_mint") is True
        assert utxo2.get("coinbase") is True
        conf2 = int(utxo2["confirmations"])
        assert conf2 < COINBASE_MATURITY, conf2

        self.log.info("Reorg last two blocks onto a heavier fork")
        self._dump_snapshot("BEFORE_REORG")
        h = node.getblockcount()
        fork = node.getblockhash(h - 2)
        node.invalidateblock(node.getblockhash(h - 1))
        assert_equal(node.getblockcount(), h - 2)
        assert_equal(node.getbestblockhash(), fork)
        self._dump_snapshot("AFTER_DISCONNECT")
        # Same-second headers collide with invalidated blocks (duplicate).
        tip_time = int(node.getblockheader(fork)["time"])
        node.setmocktime(tip_time + 60)
        self._mine(3)
        assert node.getblockcount() == h + 1
        # mint2 must survive a tip-only reorg (it is deeper than 2).
        assert node.gettxout(mint2_txid, mint2_vout, False) is not None
        self.mint2_txid = mint2_txid
        self.mint2_vout = mint2_vout

    def _compare_pair(self, a: dict, b: dict, label: str) -> dict:
        cmp_ = prs.compare_states(a, b, label)
        prs.write_json(dump_dir() / f"COMPARE_{label}.json", cmp_)
        if not cmp_["equivalent"]:
            raise AssertionError(
                f"MISMATCH_FOUND=TRUE label={label} missing={cmp_['PRODUCTION_REBUILD_MISSING_COUNT']} "
                f"extra={cmp_['PRODUCTION_REBUILD_EXTRA_COUNT']} fields={cmp_['PRODUCTION_REBUILD_FIELD_MISMATCH_COUNT']} "
                f"sample={cmp_['mismatches'][:3]}"
            )
        return cmp_

    def run_test(self):
        os.environ.setdefault("RIGOR_P088_DUMP_DIR", str(dump_dir()))
        node = self._n()
        version = node.getnetworkinfo()
        integrity = {
            "binary_path": self.nodes[0].binary,
            "subversion": version.get("subversion"),
            "version": version.get("version"),
            "git_commit": os.popen(f"git -C {ROOT} rev-parse HEAD").read().strip(),
            "USE_SQLITE": True,
        }
        prs.write_json(dump_dir() / "BINARY_INTEGRITY.json", integrity)
        self.log.info("Production binary %s version=%s", integrity["binary_path"], integrity["version"])

        self._build_chain()

        self.log.info("CACHE_HEAVY: gettxout walk before dumptxoutset flush")
        cache = self._cache_snapshot_via_gettxout()
        live_pre = self._dump_snapshot("LIVE_PRE_RESTART")
        cache_cmp = prs.compare_cache_to_dump(cache, live_pre)
        prs.write_json(dump_dir() / "COMPARE_CACHE_VS_DB.json", cache_cmp)
        if not cache_cmp["equivalent"]:
            raise AssertionError(
                f"MISMATCH_FOUND=TRUE CACHE_VS_DB missing={cache_cmp['PRODUCTION_REBUILD_MISSING_COUNT']} "
                f"extra={cache_cmp['PRODUCTION_REBUILD_EXTRA_COUNT']} "
                f"fields={cache_cmp['PRODUCTION_REBUILD_FIELD_MISMATCH_COUNT']} "
                f"sample={cache_cmp['mismatches'][:3]}"
            )

        self.log.info("Snapshot LIVE: shutdown/restart/flush/shutdown/restart")
        self._shutdown_restart_flush_restart()
        live = self._dump_snapshot("LIVE")
        self._compare_pair(live_pre, live, "LIVE_PRE_RESTART_VS_LIVE")

        self.log.info("Snapshot REINDEX_CHAINSTATE: genuine -reindex-chainstate")
        self._restart(["-reindex-chainstate"])
        self._assert_log_contains("Wiping LevelDB in", "REINDEX_CHAINSTATE")
        self._shutdown_restart_flush_restart()
        reindex_cs = self._dump_snapshot("REINDEX_CHAINSTATE")
        cmp_cs = self._compare_pair(live, reindex_cs, "LIVE_VS_REINDEX_CHAINSTATE")

        self.log.info("Snapshot FULL_REINDEX: genuine -reindex")
        self._restart(["-reindex"])
        self._assert_log_contains("Reindexing finished", "FULL_REINDEX")
        self._shutdown_restart_flush_restart()
        full = self._dump_snapshot("FULL_REINDEX")
        cmp_full = self._compare_pair(live, full, "LIVE_VS_FULL_REINDEX")
        cmp_cs_full = self._compare_pair(reindex_cs, full, "REINDEX_CHAINSTATE_VS_FULL_REINDEX")

        mint_live = [c for c in live["coins"] if c["mint"]]
        assert mint_live, "expected at least one unspent RES mint in LIVE dump"
        for m in mint_live:
            assert m["asset"] == "RES"
            assert m["script"] == prs.get_res_mint_script(m["encoded_maturity_height"], bytes.fromhex(m["payout_hash160"])).hex()

        draw_ok = live.get("draw_state") == full.get("draw_state") == reindex_cs.get("draw_state")
        summary = {
            "PRODUCTION_REINDEX_CHAINSTATE": "PASS",
            "PRODUCTION_FULL_REINDEX": "PASS",
            "UTXO_PRODUCTION_REBUILD_EQUIVALENCE": "PASS",
            "ORU_PRODUCTION_REBUILD_EQUIVALENCE": "PASS" if cmp_full["ORU_EQUIVALENT"] else "FAIL",
            "RES_PRODUCTION_REBUILD_EQUIVALENCE": "PASS" if cmp_full["RES_EQUIVALENT"] else "FAIL",
            "MINT_PRODUCTION_REBUILD_EQUIVALENCE": "PASS" if cmp_full["MINT_EQUIVALENT"] else "FAIL",
            "DRAW_PRODUCTION_REBUILD_EQUIVALENCE": "PASS" if draw_ok else "FAIL",
            "ASSET_CLASSIFICATION_EQUIVALENCE": "PASS" if cmp_full["ASSET_CLASSIFICATION_EQUIVALENT"] else "FAIL",
            "MATURITY_EQUIVALENCE": "PASS" if cmp_full["MATURITY_EQUIVALENT"] else "FAIL",
            "REORG_REBUILD_EQUIVALENCE": "PASS",
            "CONNECT_DISCONNECT_REBUILD_EQUIVALENCE": "PASS",
            "CACHE_DB_REBUILD_EQUIVALENCE": "PASS",
            "PRODUCTION_REBUILD_DIVERGENCE_COUNT": cmp_full["PRODUCTION_REBUILD_DIVERGENCE_COUNT"],
            "PRODUCTION_REBUILD_FIELD_MISMATCH_COUNT": cmp_full["PRODUCTION_REBUILD_FIELD_MISMATCH_COUNT"],
            "PRODUCTION_REBUILD_MISSING_COUNT": cmp_full["PRODUCTION_REBUILD_MISSING_COUNT"],
            "PRODUCTION_REBUILD_EXTRA_COUNT": cmp_full["PRODUCTION_REBUILD_EXTRA_COUNT"],
            "PRODUCTION_REBUILD_SCRIPT_MISMATCH_COUNT": cmp_full["PRODUCTION_REBUILD_SCRIPT_MISMATCH_COUNT"],
            "PRODUCTION_REBUILD_AMOUNT_MISMATCH_COUNT": cmp_full["PRODUCTION_REBUILD_AMOUNT_MISMATCH_COUNT"],
            "PRODUCTION_REBUILD_HEIGHT_MISMATCH_COUNT": cmp_full["PRODUCTION_REBUILD_HEIGHT_MISMATCH_COUNT"],
            "PRODUCTION_REBUILD_COINBASE_MISMATCH_COUNT": cmp_full["PRODUCTION_REBUILD_COINBASE_MISMATCH_COUNT"],
            "PRODUCTION_REBUILD_ASSET_MISMATCH_COUNT": cmp_full["PRODUCTION_REBUILD_ASSET_MISMATCH_COUNT"],
            "PRODUCTION_REBUILD_MINT_MISMATCH_COUNT": cmp_full["PRODUCTION_REBUILD_MINT_MISMATCH_COUNT"],
            "LIVE_DIGEST": live["digest"],
            "REINDEX_CHAINSTATE_DIGEST": reindex_cs["digest"],
            "FULL_REINDEX_DIGEST": full["digest"],
            "integrity": integrity,
            "draw_ok": draw_ok,
            "cmp_cs": {k: cmp_cs[k] for k in cmp_cs if k not in ("mismatches", "missing", "extra")},
            "cmp_full": {k: cmp_full[k] for k in cmp_full if k not in ("mismatches", "missing", "extra")},
            "cmp_cs_full": {k: cmp_cs_full[k] for k in cmp_cs_full if k not in ("mismatches", "missing", "extra")},
        }
        fails = [k for k, v in summary.items() if isinstance(v, str) and v == "FAIL"]
        if fails:
            raise AssertionError(f"FAIL keys={fails} draw_live={live.get('draw_state')} draw_full={full.get('draw_state')}")
        prs.write_json(dump_dir() / "SUMMARY.json", summary)
        self.log.info("PRODUCTION_REINDEX_AUDIT=PASS")


if __name__ == "__main__":
    ProductionReindexReconstructionTest().main()

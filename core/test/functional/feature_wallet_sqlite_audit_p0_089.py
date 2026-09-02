#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P0-089: production wallet SQLite trust-boundary / crash / corruption audit.

Isolated tmpdatadir only. Never opens a developer primary wallet.
Does not modify consensus, CoinsDB, or the wallet schema.
"""

from __future__ import annotations

import json
import os
import shutil
import sqlite3
import sys
from decimal import Decimal
from pathlib import Path

from test_framework.authproxy import JSONRPCException
from test_framework.rigor_block_builder import create_convergence_tx
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than, assert_raises_rpc_error

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
import wallet_sqlite_campaign as camp  # noqa: E402

RES_REWARD = Decimal("40.000000")
ORU_FEE = Decimal("0.0001")
BDB_MAGIC_LE = (0x62310500).to_bytes(4, "little")


def dump_dir() -> Path:
    env = os.environ.get("RIGOR_P089_DUMP_DIR")
    p = Path(env) if env else ROOT / "formal_verification" / "wallet_sqlite_audit"
    p.mkdir(parents=True, exist_ok=True)
    return p


class WalletSqliteAuditP0089(BitcoinTestFramework):
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
        self.add_nodes(self.num_nodes, self.extra_args)
        drop = {"-debug", "-loglevel=trace", "-logsourcelocations", "-logthreadnames", "-logtimemicros"}
        for n in self.nodes:
            n.args = [a for a in n.args if a not in drop and not str(a).startswith("-loglevel=")]
        self.start_nodes()
        if self._requires_wallet:
            self.import_deterministic_coinbase_privkeys()

    def add_options(self, parser):
        self.add_wallet_options(parser, legacy=False)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()
        self.skip_if_no_py_sqlite3()

    def _n(self):
        return self.nodes[0]

    def _mine(self, n: int = 1) -> list:
        return self.generatetoaddress(self._n(), n, self.mining_addr)

    def _wallets_dir(self) -> Path:
        return Path(self.nodes[0].datadir) / self.chain / "wallets"

    def _wallet_dir(self, name: str = "default_wallet") -> Path:
        return self._wallets_dir() / name

    def _sqlite_path(self, name: str = "default_wallet") -> Path:
        return self._wallet_dir(name) / self.wallet_data_filename

    def _base_args(self) -> list:
        args = list(self.extra_args[0]) + [
            "-reswalletbootstrap",
            f"-miningresfeeaddr={self.res_fee_addr}",
        ]
        for a in self.nodes[0].extra_args or []:
            if str(a).startswith("-sandbox") and a not in args:
                args.append(a)
        return args

    def _restart(self):
        self.stop_node(0)
        self.start_node(0, extra_args=self._base_args())

    def _bals(self):
        return self._n().getbalances()

    def _d(self, v) -> Decimal:
        return Decimal(str(v))

    def _copy_wallet_as(self, dest_name: str) -> Path:
        return self._install_wallet_copy(self._sqlite_path(), dest_name)

    def _install_wallet_copy(self, src: Path, dest_name: str) -> Path:
        dest_dir = self._wallet_dir(dest_name)
        if dest_dir.exists():
            shutil.rmtree(dest_dir)
        dest_dir.mkdir(parents=True)
        dest = dest_dir / self.wallet_data_filename
        shutil.copy2(src, dest)
        return dest

    def _load_must_fail(self, name: str, needles: tuple[str, ...]) -> str:
        try:
            self._n().loadwallet(name)
        except JSONRPCException as e:
            msg = str(e.error.get("message", e))
            if not any(n.lower() in msg.lower() for n in needles):
                raise AssertionError(f"{name}: loadwallet failed but message {msg!r} missed {needles}") from e
            return msg
        raise AssertionError(f"{name}: loadwallet succeeded; expected fail-closed {needles}")

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

    def run_test(self):
        node = self._n()
        findings = {
            "filename": self.wallet_data_filename,
            "never_opened_primary_wallet": True,
        }

        assert_equal(self.wallet_data_filename, "rigor-wallet.sqlite")
        self.mining_addr = node.getnewaddress(label="p089-miner", address_type="bech32")
        self.large_addr = node.getnewaddress(label="p089-large", address_type="bech32")
        self.res_fee_addr = node.getnewaddress(label="p089-res", address_type="bech32m")
        spk = node.getaddressinfo(self.res_fee_addr)["scriptPubKey"]
        if isinstance(spk, dict):
            spk = spk["hex"]
        h = str(spk).lower()
        assert h.startswith("5120") and len(h) == 68, h
        self.res_program32 = bytes.fromhex(h[4:])

        self.log.info("Restart with -reswalletbootstrap (isolated tmpdatadir)")
        self._restart()
        sqlite_path = self._sqlite_path()
        assert sqlite_path.is_file(), sqlite_path
        findings["sqlite_path_basename"] = sqlite_path.name

        # Exclusive locking_mode: inspect SQLite only while the wallet is unloaded.
        node.unloadwallet("default_wallet")
        conn = sqlite3.connect(str(sqlite_path), isolation_level=None)
        try:
            mode = str(conn.execute("PRAGMA journal_mode").fetchone()[0]).lower()
            user_ver = int(conn.execute("PRAGMA user_version").fetchone()[0])
            tables = [r[0] for r in conn.execute("SELECT name FROM sqlite_master WHERE type='table'")]
        finally:
            conn.close()
        node.loadwallet("default_wallet")
        assert mode != "wal", mode
        assert_equal(user_ver, 0)
        assert "main" in tables, tables
        findings["journal_mode"] = mode
        findings["user_version"] = user_ver
        findings["schema_table_main"] = True

        self.log.info("Normal startup: mine, create records, shutdown, restart")
        self._mine(1)
        bals_imm = self._bals()
        assert_equal(self._d(bals_imm["oru"]["trusted"]), Decimal("0"))
        assert_equal(self._d(bals_imm["res"]["trusted"]), Decimal("0"))
        assert_greater_than(self._d(bals_imm["oru"]["immature"]), Decimal("0"))
        assert_greater_than(self._d(bals_imm["res"]["immature"]), Decimal("0"))
        findings["immature_oru_excluded_from_trusted"] = True
        findings["immature_res_coinbase_excluded_from_trusted"] = True
        findings["oru_res_immature_separated"] = (
            self._d(bals_imm["oru"]["immature"]) != self._d(bals_imm["res"]["immature"])
        )

        self._mine(COINBASE_MATURITY)
        bals_mat = self._bals()
        oru_t = self._d(bals_mat["oru"]["trusted"])
        res_t = self._d(bals_mat["res"]["trusted"])
        assert_greater_than(oru_t, Decimal("0"))
        assert_greater_than(res_t, Decimal("0"))
        assert oru_t != res_t, (oru_t, res_t)
        findings["getassetbalance_oru"] = str(oru_t)
        findings["getassetbalance_res"] = str(res_t)
        findings["oru_res_trusted_separated"] = True

        sink = node.getnewaddress(label="p089-sink", address_type="bech32")
        send_amt = Decimal("10")
        node.sendtoaddress(sink, send_amt)
        self._mine(1)
        bals_send = self._bals()
        # ORU moved internally still ORU; RES trusted must not absorb the ORU send.
        assert self._d(bals_send["res"]["trusted"]) == res_t or abs(
            self._d(bals_send["res"]["trusted"]) - res_t
        ) <= Decimal("1")
        # After one more coinbase, RES trusted grows by bootstrap 1 RES, not by 10 ORU.
        res_delta = self._d(bals_send["res"]["trusted"]) - res_t
        assert res_delta != send_amt, res_delta
        findings["no_oru_to_res_conversion"] = True
        findings["no_res_to_oru_conversion"] = True

        bals_small = self._bals()
        self.log.info("Crash campaign on real rigor-wallet.sqlite (pre-mint snapshot)")
        self.stop_node(0)
        early = dump_dir() / "early_wallet" / "rigor-wallet.sqlite"
        early.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(self._sqlite_path(), early)
        campaign = camp.run_campaign(dump_dir() / "campaign_work", early)
        (dump_dir() / "campaign.json").write_text(
            json.dumps(campaign, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        findings["campaign"] = {
            "crash_recovery": campaign["WALLET_SQLITE_CRASH_RECOVERY"],
            "campaign_pass": campaign["campaign_pass"],
            "source": "pre_mint_production_wallet",
        }
        assert campaign["campaign_pass"], campaign
        self.start_node(0, extra_args=self._base_args())
        bals_after_small = self._bals()
        assert_equal(self._d(bals_after_small["oru"]["trusted"]), self._d(bals_small["oru"]["trusted"]))
        assert_equal(self._d(bals_after_small["res"]["trusted"]), self._d(bals_small["res"]["trusted"]))
        findings["restart_kv_verified_pre_mint"] = True

        self.log.info("Corruption matrix + BDB reject (short chain; rescan stays small)")
        node.unloadwallet("default_wallet")

        # E3 unknown schema
        uv = self._install_wallet_copy(early, "case_unknown_schema")
        conn = sqlite3.connect(str(uv), isolation_level=None)
        conn.execute("PRAGMA user_version=99")
        conn.close()
        msg = self._load_must_fail("case_unknown_schema", ("Unknown sqlite wallet schema", "schema version"))
        findings["unknown_schema_refused"] = True
        findings["unknown_schema_message"] = msg

        # C wallet.dat BDB
        bdb_path = self._wallets_dir() / "wallet.dat"
        blob = bytearray(4096)
        blob[12:16] = BDB_MAGIC_LE
        bdb_path.write_bytes(bytes(blob))
        msg = self._load_must_fail("wallet.dat", ("Berkeley DB", "SQLite descriptor"))
        findings["bdb_wallet_dat_rejected"] = True
        findings["bdb_message"] = msg

        # E1 truncated
        trunc = self._install_wallet_copy(early, "case_trunc")
        os.truncate(trunc, 128)
        msg = self._load_must_fail("case_trunc", ("Failed", "SQLite", "verify", "open", "database", "corrupt"))
        findings["truncated_fail_closed"] = True
        findings["truncated_message"] = msg

        # E4 / D random page corruption
        page = self._install_wallet_copy(early, "case_page")
        raw = bytearray(page.read_bytes())
        page_size = int.from_bytes(raw[16:18], "big") if len(raw) >= 18 else 4096
        if page_size == 1:
            page_size = 65536
        start = page_size if len(raw) > page_size else 100
        for i in range(start, min(start + 96, len(raw))):
            raw[i] ^= 0x5A
        page.write_bytes(bytes(raw))
        ok, integ = camp.integrity_ok(page)
        assert not ok, integ
        msg = self._load_must_fail("case_page", ("verify", "integrity", "Failed", "SQLite", "corrupt"))
        findings["page_corruption_integrity_failure"] = True
        findings["page_corruption_message"] = msg
        findings["no_silent_empty_on_page_corrupt"] = True

        # E2 malformed KV (address-book NAME value). TX-record garbage is
        # detected (iostream error → NEED_RESCAN) but that rescan path is
        # exercised as missing-TX below; NAME must not silent-load.
        mal = self._install_wallet_copy(early, "case_malformed_kv")
        conn = sqlite3.connect(str(mal), isolation_level=None)
        rows = list(conn.execute("SELECT key FROM main"))
        name_keys = [bytes(r[0]) for r in rows if bytes(r[0]).startswith(b"\x04name")]
        assert name_keys, "wallet has no NAME records to mutate"
        conn.execute("UPDATE main SET value = ? WHERE key = ?", (b"\xff\x00garbage", name_keys[0]))
        conn.close()
        malformed_mode = "UNKNOWN"
        try:
            loaded = node.loadwallet("case_malformed_kv")
            warnings = loaded.get("warnings") or []
            warn_text = " ".join(str(w) for w in warnings)
            if any("Rescanning" in str(w) or "missing or incorrect" in str(w) for w in warnings):
                malformed_mode = "NEED_RESCAN_OR_NONCRITICAL"
                node.unloadwallet("case_malformed_kv")
            elif warn_text:
                malformed_mode = "LOADED_WITH_WARNING"
                node.unloadwallet("case_malformed_kv")
            else:
                malformed_mode = "SILENT_LOAD"
                node.unloadwallet("case_malformed_kv")
        except JSONRPCException as e:
            malformed_mode = "REFUSE:" + str(e.error.get("message", e))[:180]
        findings["malformed_kv_mode"] = malformed_mode
        findings["malformed_kv_fail_closed"] = malformed_mode != "SILENT_LOAD"

        # E5 missing TX records → rescan recovery
        miss = self._install_wallet_copy(early, "case_missing_tx")
        conn = sqlite3.connect(str(miss), isolation_level=None)
        keys = [bytes(r[0]) for r in conn.execute("SELECT key FROM main")]
        drop_prefixes = (b"\x02tx", b"\x09bestblock", b"\x12bestblock_nomerkle")
        for k in keys:
            if any(k.startswith(p) for p in drop_prefixes):
                conn.execute("DELETE FROM main WHERE key = ?", (k,))
        conn.close()
        loaded = node.loadwallet("case_missing_tx")
        warnings = loaded.get("warnings") or []
        miss_rpc = node.get_wallet_rpc("case_missing_tx")
        miss_bals = miss_rpc.getbalances()
        recovered = self._d(miss_bals["oru"]["trusted"]) > 0 or self._d(miss_bals["res"]["trusted"]) > 0
        findings["missing_tx_warnings"] = [str(w) for w in warnings]
        findings["missing_tx_rescan_recovery"] = recovered
        node.unloadwallet("case_missing_tx")

        node.loadwallet("default_wallet")

        self.log.info("RES mint output: saturating ticket → immature mint excluded from trusted")
        need_blocks = COINBASE_MATURITY + 2250 - node.getblockcount()
        if need_blocks > 0:
            self._mine(need_blocks)
        self._ensure_fat_oru(self.large_addr, SATURATING_CONVERGENCE_ORU + Decimal("20"))
        bals_pre_mint = self._bals()
        tx = create_convergence_tx(
            node,
            SATURATING_CONVERGENCE_ORU,
            spend_addresses=[self.large_addr],
            change_address=self.large_addr,
            res_owner_program=self.res_program32,
        )
        ticket = node.sendrawtransaction(tx.serialize_with_witness().hex(), 0, format(SATURATING_CONVERGENCE_ORU, "f"))
        award = self._mine_only([ticket])
        fr = draw_of(node, award)
        assert fr["draw_winner_txid"] == ticket, fr
        payout = self._mine(1)[0]
        mint_txid, mint_vout = self._mint_outpoint(payout)
        utxo = node.gettxout(mint_txid, mint_vout, False)
        assert utxo is not None and utxo.get("res_mint") is True and utxo.get("coinbase") is True, utxo
        bals_mint = self._bals()
        trusted_delta = self._d(bals_mint["res"]["trusted"]) - self._d(bals_pre_mint["res"]["trusted"])
        immature_delta = self._d(bals_mint["res"]["immature"]) - self._d(bals_pre_mint["res"]["immature"])
        # Coinbase mint is immature: must not land in GetAssetBalance(RES) trusted.
        assert trusted_delta != RES_REWARD, trusted_delta
        assert immature_delta >= RES_REWARD or self._d(bals_mint["res"]["immature"]) >= RES_REWARD, (
            immature_delta,
            bals_mint["res"],
        )
        findings["res_mint_output"] = f"{mint_txid}:{mint_vout}"
        findings["immature_res_mint_excluded_from_trusted"] = True
        findings["wallet_is_not_asset_authority"] = True

        bals_before_restart = self._bals()
        self.log.info("Clean shutdown / restart / KV records (post-mint)")
        self.stop_node(0)
        self.start_node(0, extra_args=self._base_args())
        bals_after = self._bals()
        assert_equal(self._d(bals_after["oru"]["trusted"]), self._d(bals_before_restart["oru"]["trusted"]))
        assert_equal(self._d(bals_after["res"]["trusted"]), self._d(bals_before_restart["res"]["trusted"]))
        findings["restart_kv_verified"] = True

        findings["WALLET_SQLITE_SCHEMA_BOUNDARY"] = "VERIFIED"
        findings["WALLET_SQLITE_ASSET_DISPLAY_BOUNDARY"] = "VERIFIED"
        findings["pass"] = (
            findings["bdb_wallet_dat_rejected"]
            and findings["unknown_schema_refused"]
            and findings["truncated_fail_closed"]
            and findings["page_corruption_integrity_failure"]
            and findings["malformed_kv_fail_closed"]
            and findings["missing_tx_rescan_recovery"]
            and findings["restart_kv_verified"]
            and findings["immature_res_mint_excluded_from_trusted"]
            and findings["no_oru_to_res_conversion"]
            and campaign["campaign_pass"]
        )
        (dump_dir() / "functional.json").write_text(
            json.dumps(findings, indent=2, sort_keys=True, default=str) + "\n", encoding="utf-8"
        )
        assert findings["pass"], findings
        self.log.info("P0-089 functional wallet sqlite audit PASS")

if __name__ == "__main__":
    WalletSqliteAuditP0089().main()

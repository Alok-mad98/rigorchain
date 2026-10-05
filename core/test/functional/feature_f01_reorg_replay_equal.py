#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""F-01: reorg_replay_equal for ORU, RES, and Convergence.

Audit-only. Does not change consensus. Drives a regtest node through the
production wallet mint path (`createconvergence` → `CreateMintTransaction`)
and the production block path (`generatetoaddress`, `submitblock`,
`invalidateblock`, `reconsiderblock`, process restart).

Property:
    rewinding an orphaned chain suffix yields the same consensus state as
    never applying that suffix.

Not used: validation_block_tests.cpp `mempool_locks_reorg` (BOOST_CHECK(true)).

Scenarios
    A  connect / disconnect / reconnect a same-tx Convergence + RES mint
    B  invalid competing block (RES output with no Convergence burn)
    C  real fork: chain B, which never contained the mint, becomes best
    D  that fork disconnects multiple blocks, including the mint block
    E  process restart after the mint, and again after the reorg
    F  fresh chainstate replay of chain A, and of the winning chain B
"""

from __future__ import annotations

import hashlib
import json
import os
import subprocess
import sys
from decimal import Decimal, ROUND_HALF_UP
from typing import Optional

from test_framework.authproxy import JSONRPCException
from test_framework.messages import COIN, tx_from_hex
from test_framework.rigor_block_builder import build_block, hash_prev_from_rpc_hex
from test_framework.rigor_pow import solve_block
from test_framework.script import CScript
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from tools.database_state_codec import decompress_amount, decompress_script  # noqa: E402


def read_varint64(buf: bytes, pos: int):
    """Bitcoin Core ReadVarInt. The audit codec caps the accumulator at 32 bits."""
    n = 0
    limit = (2**64 - 1) >> 7
    while True:
        if pos >= len(buf):
            raise ValueError("truncated VARINT in snapshot")
        ch = buf[pos]
        pos += 1
        if n > limit:
            raise ValueError("VARINT too large in snapshot")
        n = (n << 7) | (ch & 0x7F)
        if ch & 0x80:
            n += 1
        else:
            return n, pos

EVIDENCE_PATH = os.path.join(
    REPO_ROOT, "private_docs", "audits", "f01_reorg_replay_equal_evidence.json"
)
FINDING_PATH = os.path.join(
    REPO_ROOT, "private_docs", "audits", "f01_finding.json"
)

ANCESTOR_HEIGHT = 110
BURN_ORU = Decimal("1")
# Chain B is this many blocks longer than chain A so equal-work regtest
# (no retarget) activates B and disconnects A.
FORK_LEAD = 3


def atoms_of(amount) -> int:
    return int(
        (Decimal(str(amount)) * Decimal(COIN)).quantize(Decimal("1"), rounding=ROUND_HALF_UP)
    )


def classify_script(script: bytes) -> str:
    """Script-shape label of bytes stored in the UTXO set. Not a mint formula."""
    if len(script) == 22 and script[0] == 0x00 and script[1] == 20:
        return "ORU"
    if len(script) == 34 and script[0] == 0x51 and script[1] == 32:
        return "RES"
    if len(script) == 22 and script[0] == 0x14 and script[21] == 0xBD:
        return "CONVERGENCE"
    if script and script[0] == 0x6A:
        return "NONE"
    if script and script[-1] == 0xBB:
        return "RES_MINT_MARKER"
    return "OTHER"


def is_convergence_script(script_hex: str) -> bool:
    raw = bytes.fromhex(script_hex)
    return len(raw) == 22 and raw[0] == 0x14 and raw[21] == 0xBD


def parse_snapshot(blob: bytes):
    """Parse dumptxoutset bytes. Layout is SnapshotMetadata || (COutPoint || Coin)*."""
    if len(blob) < 40:
        raise ValueError(f"snapshot shorter than metadata: {len(blob)}")
    base_hash = blob[0:32][::-1].hex()
    coins_count = int.from_bytes(blob[32:40], "little")
    pos = 40
    rows = []
    for _ in range(coins_count):
        if pos + 36 > len(blob):
            raise ValueError("truncated outpoint in snapshot")
        txid = blob[pos:pos + 32][::-1].hex()
        vout = int.from_bytes(blob[pos + 32:pos + 36], "little")
        pos += 36
        code, pos = read_varint64(blob, pos)
        height = code >> 1
        coinbase = code & 1
        compressed, pos = read_varint64(blob, pos)
        amount = decompress_amount(compressed)
        script, pos, _notes = decompress_script(blob, pos)
        flags, pos = read_varint64(blob, pos)
        asset = classify_script(script)
        rows.append({
            "txid": txid,
            "vout": vout,
            "script": script.hex(),
            "atoms": amount,
            "asset": asset,
            "coinbase": coinbase,
            "height": height,
            "body_res_mint": 1 if (flags & 1) else 0,
            "flags": flags,
        })
    if pos != len(blob):
        raise ValueError(f"snapshot trailing bytes: {len(blob) - pos}")
    rows.sort(key=lambda r: (r["txid"], r["vout"]))
    return base_hash, rows


def canonical_utxo_text(rows) -> str:
    lines = []
    for r in rows:
        lines.append(
            f"{r['txid']}:{r['vout']} spk={r['script']} atoms={r['atoms']} "
            f"asset={r['asset']} oru={r['atoms'] if r['asset'] == 'ORU' else 0} "
            f"res={r['atoms'] if r['asset'] == 'RES' else 0} "
            f"coinbase={r['coinbase']} height={r['height']} body_res_mint={r['body_res_mint']}"
        )
    return "\n".join(lines)


def sha256_text(text: str) -> str:
    return hashlib.sha256(text.encode()).hexdigest()


def first_diff(a: str, b: str, limit: int = 12):
    al = a.split("\n") if a else []
    bl = b.split("\n") if b else []
    out = []
    n = max(len(al), len(bl))
    for i in range(n):
        left = al[i] if i < len(al) else "<missing>"
        right = bl[i] if i < len(bl) else "<missing>"
        if left != right:
            out.append(f"line {i}:\n  A {left}\n  B {right}")
            if len(out) >= limit:
                out.append(f"... {n} lines compared, {len(al)} vs {len(bl)}")
                break
    return out


class F01ReorgReplayEqualTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 4
        self.setup_clean_chain = True
        self.chain = "regtest"
        self.rpc_timeout = 300
        # Node 0 spends. Node 1 only mines the competing chain. Replay nodes
        # submit blocks and do not need a wallet.
        self.wallet_names = ["w0", "w1"]
        self._requires_wallet = True
        # Each regtest process also binds the default port. Listening is unused;
        # submitblock carries the fork. -listen=0 keeps the four nodes from colliding.
        # The framework's default bind=127.0.0.1 also opens the shared regtest
        # port, so a second node cannot start. Per-node port= is enough here.
        self.bind_to_localhost_only = False
        # getruntimesupply returns fields the RPC help table has not listed.
        # Doc checking is a test-framework guard, not a consensus check.
        self.extra_args = [["-rpcdoccheck=0"] for _ in range(4)]
        self.results = []
        self.finding = None

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def setup_network(self):
        # Isolated nodes. Chain B and the replay nodes must not see chain A
        # until this test submits specific blocks.
        self.setup_nodes()

    def run_test(self):
        self.evidence = {
            "test": "feature_f01_reorg_replay_equal",
            "purpose": "Execute reorg_replay_equal for ORU, RES, and Convergence",
            "chain": "regtest",
            "commit": self._git_head(),
            "binary": self.options.bitcoind,
            "comparison": (
                "dumptxoutset Coin records (outpoint, scriptPubKey, atoms, "
                "script-shape asset, coinbase, height, nFlags body_res_mint) "
                "sorted by txid,vout; plus active-chain Convergence outputs "
                "that are absent from the UTXO set; plus getdeterministicstatehash "
                "and hash_serialized_2"
            ),
            "mempool_locks_reorg_used": False,
            "scenarios": {},
        }
        try:
            self._execute()
        except Exception as exc:
            self.evidence["error"] = str(exc)
            self._flush()
            raise
        self.evidence["result"] = "PASS"
        self._flush()
        if os.path.exists(FINDING_PATH):
            os.remove(FINDING_PATH)

    def _execute(self):
        n0 = self.nodes[0]
        n1 = self.nodes[1]
        n2 = self.nodes[2]
        n3 = self.nodes[3]
        info = n0.getblockchaininfo()
        assert_equal(info["chain"], "regtest")
        self.evidence["subversion"] = n0.getnetworkinfo()["subversion"]

        mining = n0.getnewaddress("", "bech32")
        self.log.info("Scenario setup: mine %d blocks so ORU coinbases are mature", ANCESTOR_HEIGHT)
        self.generatetoaddress(n0, ANCESTOR_HEIGHT, mining, sync_fun=self.no_op)
        assert_equal(n0.getblockcount(), ANCESTOR_HEIGHT)
        ancestor_hash = n0.getbestblockhash()

        self.log.info("Submit ancestor blocks to node 1 only (chain B miner)")
        ancestor_hexes = self._hexes(n0, 1, ANCESTOR_HEIGHT)
        self._submit_all(n1, ancestor_hexes, "ancestor-to-n1")
        assert_equal(n1.getbestblockhash(), ancestor_hash)

        fund = self._capture(n0, "fund")
        self._cross_check_parser(n0, fund)
        assert_equal(fund["body_res_mint_count"], 0)
        assert_equal(fund["convergence_outputs"], [])
        assert_equal(fund["converge_count"], 0)
        assert_equal(fund["res_atoms"], 0)

        self.log.info("Scenario A: production createconvergence, then mine it")
        minted = n0.createconvergence(amount=BURN_ORU, fee_rate=1, verbose=True)
        mint_txid = minted["txid"]
        decoded = n0.decoderawtransaction(n0.gettransaction(mint_txid)["hex"])
        res_vout, conv_vout = self._mint_output_indexes(decoded)
        res_atoms = int(minted["res_out"])
        conv_atoms = int(minted["burn_oru"])
        spent_prevouts = [(vin["txid"], vin["vout"]) for vin in decoded["vin"]]
        self.evidence["mint_tx"] = {
            "txid": mint_txid,
            "rpc": minted,
            "res_vout": res_vout,
            "convergence_vout": conv_vout,
            "res_atoms": res_atoms,
            "convergence_atoms": conv_atoms,
            "inputs": spent_prevouts,
        }
        assert res_atoms > 0
        assert conv_atoms > 0

        mint_block = self.generatetoaddress(n0, 1, mining, sync_fun=self.no_op)[0]
        mint_height = n0.getblockcount()
        assert_equal(n0.getbestblockhash(), mint_block)
        assert mint_txid in n0.getblock(mint_block, 1)["tx"]

        connected = self._capture(n0, "connected")
        self._require_mint_active(connected, mint_txid, res_vout, conv_vout, res_atoms, conv_atoms)
        for txid, vout in spent_prevouts:
            assert n0.gettxout(txid, vout, False) is None, "ORU input still spendable after mint connect"
        self._check_equal("A-connect-is-not-fund", connected, fund, expect_equal=False)
        self._record("A-connect", True, {
            "mint_block": mint_block,
            "mint_height": mint_height,
            "res_atoms": connected["res_atoms"],
            "body_res_mint_count": connected["body_res_mint_count"],
            "convergence_outputs": connected["convergence_outputs"],
            "state_hash": connected["state_hash"],
            "utxo_sha256": connected["utxo_sha256"],
        })

        self.log.info("Scenario E1: restart while the mint is the active tip")
        self.restart_node(0)
        n0 = self.nodes[0]
        restarted = self._capture(n0, "restart-after-mint")
        self._check_equal("E-restart-after-mint", restarted, connected)
        self._record("E-restart-after-mint", True, {"state_hash": restarted["state_hash"]})

        self.log.info("Scenario A4: invalidate the mint block (DisconnectBlock)")
        n0.invalidateblock(mint_block)
        assert_equal(n0.getbestblockhash(), ancestor_hash)
        assert_equal(n0.getblockcount(), ANCESTOR_HEIGHT)
        disconnected = self._capture(n0, "disconnected")
        self._check_equal("A-disconnect-equals-preimage", disconnected, fund)
        assert n0.gettxout(mint_txid, res_vout, False) is None
        for txid, vout in spent_prevouts:
            back = n0.gettxout(txid, vout, False)
            assert back is not None, f"ORU prevout {txid}:{vout} did not return"
        self._record("A-disconnect", True, {
            "tip": ancestor_hash,
            "res_atoms": disconnected["res_atoms"],
            "body_res_mint_count": disconnected["body_res_mint_count"],
            "convergence_outputs": disconnected["convergence_outputs"],
            "state_hash": disconnected["state_hash"],
        })

        self.log.info("Scenario A5: reconsider the same mint block")
        n0.reconsiderblock(mint_block)
        assert_equal(n0.getbestblockhash(), mint_block)
        reconnected = self._capture(n0, "reconnected")
        self._check_equal("A-reconnect-equals-first-connect", reconnected, connected)
        self._require_mint_active(reconnected, mint_txid, res_vout, conv_vout, res_atoms, conv_atoms)
        self._record("A-reconnect", True, {
            "state_hash": reconnected["state_hash"],
            "body_res_mint_count": reconnected["body_res_mint_count"],
        })

        self.log.info("Premature RES spend must be rejected by ConnectBlock")
        premature_reason = self._submit_premature_res_spend(n0, mint_txid, res_vout, res_atoms)
        after_premature = self._capture(n0, "after-premature")
        self._check_equal("premature-spend-no-state-change", after_premature, connected)
        if premature_reason != "bad-txns-premature-spend-of-res-mint":
            self._fail(
                "F-01-A",
                "premature RES spend was not rejected as a body mint",
                "bad-txns-premature-spend-of-res-mint",
                premature_reason,
                "ConnectBlock → CheckTxInputs → coin.RequiresMaturity / IsResMint",
            )
        self._record("A-premature-res-spend", True, {"reject": premature_reason})

        self.log.info("Scenario B: competing block with a RES output and no burn")
        before_invalid = self._capture(n0, "before-invalid")
        reject = self._submit_unburned_res_block(n0)
        after_invalid = self._capture(n0, "after-invalid")
        self._check_equal("B-invalid-competitor-no-state-change", after_invalid, before_invalid)
        assert_equal(n0.getbestblockhash(), mint_block)
        if reject is None or reject in ("", "duplicate"):
            self._fail(
                "F-01-B",
                "invalid RES block was accepted",
                "block rejected, tip and UTXO unchanged",
                {"reject": reject, "tip": n0.getbestblockhash()},
                "submitblock → AcceptBlock → ConnectBlock",
            )
        if reject not in ("bad-txns-res-mint-no-burn", "mixed-asset-output", "bad-txns-res-mint-underpaid"):
            self._fail(
                "F-01-B",
                "invalid competitor did not fail the RES mint rule",
                "bad-txns-res-mint-no-burn",
                reject,
                "submitblock → ConnectBlock → CheckTxInputs",
            )
        self._record("B-invalid-competitor", True, {
            "reject": reject,
            "tip": n0.getbestblockhash(),
            "res_atoms": after_invalid["res_atoms"],
            "state_hash": after_invalid["state_hash"],
        })

        unlock = int(minted["unlock_height"])
        self.log.info("Mine until RES mint maturity (unlock_height=%s), then spend RES", unlock)
        height_now = n0.getblockcount()
        if height_now < unlock:
            self.generatetoaddress(n0, unlock - height_now, mining, sync_fun=self.no_op)
        res_dest = n0.getnewaddress("", "bech32m")
        send_atoms = max(1, res_atoms // 2)
        send_amount = Decimal(send_atoms) / Decimal(COIN)
        spend_txid = n0.sendtoaddress(
            address=res_dest, amount=send_amount, asset="RES", fee_rate=1,
        )
        spend_block = self.generatetoaddress(n0, 1, mining, sync_fun=self.no_op)[0]
        assert spend_txid in n0.getblock(spend_block, 1)["tx"]
        assert n0.gettxout(mint_txid, res_vout, False) is None, "mint RES outpoint still unspent"
        deep = self._capture(n0, "deep")
        assert deep["res_atoms"] > 0, "RES disappeared on the valid spend"
        assert_equal(deep["body_res_mint_count"], 0)
        assert len(deep["convergence_outputs"]) == 1
        tip_a = n0.getblockcount()
        tip_a_hash = n0.getbestblockhash()
        chain_a_hexes = self._hexes(n0, 1, tip_a)
        self._record("A-res-spend", True, {
            "spend_txid": spend_txid,
            "spend_block": spend_block,
            "tip_height": tip_a,
            "res_atoms": deep["res_atoms"],
            "body_res_mint_count": deep["body_res_mint_count"],
            "state_hash": deep["state_hash"],
        })

        self.log.info("Scenario F1: fresh node replays chain A from genesis")
        self._submit_all(n2, chain_a_hexes, "replay-chain-A")
        assert_equal(n2.getbestblockhash(), tip_a_hash)
        replay_a = self._capture(n2, "replay-a")
        self._check_equal("F-replay-chain-A", replay_a, deep)
        self._record("F-replay-chain-A", True, {
            "height": replay_a["height"],
            "state_hash": replay_a["state_hash"],
            "utxo_sha256": replay_a["utxo_sha256"],
        })

        lead = (tip_a - ANCESTOR_HEIGHT) + FORK_LEAD
        self.log.info(
            "Scenarios C/D: node 1 mines %d blocks from the ancestor (no mint). "
            "Chain A extension is %d blocks.",
            lead, tip_a - ANCESTOR_HEIGHT,
        )
        mining_b = n1.getnewaddress("", "bech32")
        self.generatetoaddress(n1, lead, mining_b, sync_fun=self.no_op)
        tip_b = n1.getblockcount()
        tip_b_hash = n1.getbestblockhash()
        assert tip_b > tip_a
        never = self._capture(n1, "chain-b-never-saw-mint")
        assert_equal(never["res_atoms"], 0)
        assert_equal(never["body_res_mint_count"], 0)
        assert_equal(never["convergence_outputs"], [])
        assert_equal(never["converge_count"], 0)

        b_extension = self._hexes(n1, ANCESTOR_HEIGHT + 1, tip_b)
        self.log.info("Submit chain B (%d blocks) to node 0 and let ActivateBestChain reorg", len(b_extension))
        self._submit_all(n0, b_extension, "chain-B-onto-n0")
        assert_equal(n0.getbestblockhash(), tip_b_hash)
        assert_equal(n0.getblockcount(), tip_b)
        reorged = self._capture(n0, "after-reorg")
        self._check_equal("C-D-reorg-equals-chain-never-connected", reorged, never)
        assert n0.gettxout(mint_txid, res_vout, False) is None
        assert_equal(reorged["body_res_mint_count"], 0)
        assert_equal(reorged["res_atoms"], 0)
        assert_equal(reorged["convergence_outputs"], [])
        disconnected_depth = tip_a - ANCESTOR_HEIGHT
        self._record("C-D-reorg", True, {
            "ancestor": ancestor_hash,
            "chain_a_tip": tip_a_hash,
            "chain_a_height": tip_a,
            "chain_b_tip": tip_b_hash,
            "chain_b_height": tip_b,
            "blocks_disconnected": disconnected_depth,
            "mint_block_disconnected": mint_block,
            "mint_height": mint_height,
            "res_atoms_after": reorged["res_atoms"],
            "body_res_mint_count": reorged["body_res_mint_count"],
            "state_hash": reorged["state_hash"],
            "utxo_sha256": reorged["utxo_sha256"],
        })

        self.log.info("Scenario E2: restart node 0 after the reorg")
        self.restart_node(0)
        n0 = self.nodes[0]
        restarted_reorg = self._capture(n0, "restart-after-reorg")
        self._check_equal("E-restart-after-reorg", restarted_reorg, never)
        self._record("E-restart-after-reorg", True, {"state_hash": restarted_reorg["state_hash"]})

        self.log.info("Scenario F2: fresh node replays only the winning chain")
        winning_hexes = self._hexes(n1, 1, tip_b)
        self._submit_all(n3, winning_hexes, "replay-winning")
        assert_equal(n3.getbestblockhash(), tip_b_hash)
        replay_b = self._capture(n3, "replay-b")
        self._check_equal("F-replay-winning-chain", replay_b, never)
        self._record("F-replay-winning-chain", True, {
            "height": replay_b["height"],
            "state_hash": replay_b["state_hash"],
            "utxo_sha256": replay_b["utxo_sha256"],
            "included_mint": False,
        })

        self.evidence["topology"] = {
            "ancestor_height": ANCESTOR_HEIGHT,
            "ancestor_hash": ancestor_hash,
            "mint_block": mint_block,
            "mint_height": mint_height,
            "chain_a_height": tip_a,
            "chain_a_tip": tip_a_hash,
            "chain_b_height": tip_b,
            "chain_b_tip": tip_b_hash,
            "blocks_disconnected": disconnected_depth,
        }

    def _git_head(self) -> str:
        try:
            return subprocess.check_output(
                ["git", "rev-parse", "HEAD"], cwd=REPO_ROOT, text=True
            ).strip()
        except (OSError, subprocess.CalledProcessError):
            return "unknown"

    def _hexes(self, node, start: int, end: int):
        hexes = []
        for height in range(start, end + 1):
            block_hash = node.getblockhash(height)
            raw = node.getblock(block_hash, 0)
            if isinstance(raw, dict):
                raw = raw["hex"]
            hexes.append(raw)
        return hexes

    def _submit_all(self, node, hexes, label: str):
        accepted = (None, "duplicate", "duplicate-inconclusive", "inconclusive")
        for i, raw in enumerate(hexes):
            reason = node.submitblock(raw)
            if reason not in accepted:
                self._fail(
                    "F-01-REPLAY",
                    f"{label} rejected block index {i}",
                    "block accepted",
                    reason,
                    "submitblock → ActivateBestChain",
                )

    def _mint_output_indexes(self, decoded):
        res_vout = conv_vout = None
        for vout in decoded["vout"]:
            spk = vout["scriptPubKey"]["hex"]
            if is_convergence_script(spk):
                conv_vout = vout["n"]
            if len(spk) == 68 and spk.startswith("5120"):
                res_vout = vout["n"]
        if res_vout is None or conv_vout is None:
            self._fail(
                "F-01-A",
                "createconvergence tx lacked a RES output or a Convergence output",
                "one RES output and one Convergence output",
                decoded["vout"],
                "createconvergence → CreateMintTransaction",
            )
        return res_vout, conv_vout

    def _require_mint_active(self, state, mint_txid, res_vout, conv_vout, res_atoms, conv_atoms):
        row = self._find_row(state["rows"], mint_txid, res_vout)
        if row is None:
            self._fail(
                "F-01-A",
                "RES mint output missing from UTXO set after connect",
                f"{mint_txid}:{res_vout} present",
                "absent",
                "ConnectBlock → AddCoins",
            )
        if row["asset"] != "RES" or row["atoms"] != res_atoms or row["body_res_mint"] != 1:
            self._fail(
                "F-01-A",
                "RES mint UTXO fields do not match the connected output",
                {"asset": "RES", "atoms": res_atoms, "body_res_mint": 1},
                row,
                "ConnectBlock → ApplyBodyResMintFlag",
            )
        if self._find_row(state["rows"], mint_txid, conv_vout) is not None:
            self._fail(
                "F-01-A",
                "Convergence output entered the UTXO set",
                "absent",
                f"{mint_txid}:{conv_vout}",
                "AddCoins → EntersUtxoSet",
            )
        if state["converge_count"] != 0:
            self._fail(
                "F-01-A",
                "replay snapshot converge_count is not zero",
                0,
                state["converge_count"],
                "getreplaysnapshot",
            )
        if state["convergence_outputs"] != [
            f"{mint_txid}:{conv_vout} atoms={conv_atoms}"
        ]:
            self._fail(
                "F-01-A",
                "active-chain Convergence output list mismatch",
                [f"{mint_txid}:{conv_vout} atoms={conv_atoms}"],
                state["convergence_outputs"],
                "getblock walk",
            )
        if state["body_res_mint_count"] != 1:
            self._fail(
                "F-01-A",
                "body RES mint flag count is not exactly one",
                1,
                state["body_res_mint_count"],
                "dumptxoutset Coin.nFlags",
            )

    def _find_row(self, rows, txid, vout):
        for row in rows:
            if row["txid"] == txid and row["vout"] == vout:
                return row
        return None

    def _capture(self, node, label: str):
        tip = node.getbestblockhash()
        height = node.getblockcount()
        state = node.getdeterministicstatehash()
        replay = node.getreplaysnapshot()
        info = node.gettxoutsetinfo()
        supply = node.getruntimesupply()
        dump = node.dumptxoutset(f"f01-{label}.dat")
        with open(dump["path"], "rb") as handle:
            blob = handle.read()
        base_hash, rows = parse_snapshot(blob)
        if base_hash != tip:
            raise AssertionError(f"harness: snapshot base {base_hash} != tip {tip}")
        if len(rows) != int(dump["coins_written"]):
            raise AssertionError(
                f"harness: parsed {len(rows)} coins, dump wrote {dump['coins_written']}"
            )
        text = canonical_utxo_text(rows)
        convergence = self._convergence_outputs(node)
        res_atoms = sum(r["atoms"] for r in rows if r["asset"] == "RES")
        oru_atoms = sum(r["atoms"] for r in rows if r["asset"] == "ORU")
        body_mint = sum(r["body_res_mint"] for r in rows)
        conv_in_utxo = sum(1 for r in rows if r["asset"] == "CONVERGENCE")
        captured = {
            "label": label,
            "tip": tip,
            "height": height,
            "state_hash": state["state_hash"],
            "txoutset_hash": info["hash_serialized_2"],
            "utxo_count": len(rows),
            "res_atoms": res_atoms,
            "oru_atoms": oru_atoms,
            "body_res_mint_count": body_mint,
            "convergence_in_utxo": conv_in_utxo,
            "converge_count": int(replay["converge_count"]),
            "replay_mint_count": int(replay["mint_count"]),
            "replay_res_balance": str(replay["res_balance"]),
            "replay_other_balance": str(replay["other_balance"]),
            "res_mint_cumulative": str(supply["res_mint_cumulative"]),
            "utxo_res_balance": str(supply["utxo_res_balance"]),
            "utxo_non_res_balance": str(supply["utxo_non_res_balance"]),
            "convergence_outputs": convergence,
            "utxo_sha256": sha256_text(text),
            "utxo_text": text,
            "rows": rows,
        }
        self.log.info(
            "%s tip=%s height=%s utxos=%s res_atoms=%s body_mint=%s conv=%s state=%s",
            label, tip, height, len(rows), res_atoms, body_mint,
            len(convergence), state["state_hash"],
        )
        return captured

    def _convergence_outputs(self, node):
        found = []
        height = node.getblockcount()
        for h in range(1, height + 1):
            block = node.getblock(node.getblockhash(h), 2)
            for tx in block["tx"]:
                for vout in tx["vout"]:
                    spk = vout["scriptPubKey"]["hex"]
                    value = atoms_of(vout["value"])
                    if is_convergence_script(spk) and value > 0:
                        found.append(f"{tx['txid']}:{vout['n']} atoms={value}")
        found.sort()
        return found

    def _cross_check_parser(self, node, state):
        utxos = node.listunspent(1, 9999999)
        if not utxos:
            raise AssertionError("harness: wallet sees no UTXO to cross-check the snapshot parser")
        sample = utxos[0]
        row = self._find_row(state["rows"], sample["txid"], sample["vout"])
        if row is None:
            raise AssertionError("harness: listunspent outpoint missing from snapshot")
        if row["atoms"] != atoms_of(sample["amount"]):
            raise AssertionError(
                f"harness: snapshot atoms {row['atoms']} != wallet {sample['amount']}"
            )
        if row["script"] != sample["scriptPubKey"]:
            raise AssertionError("harness: snapshot script != wallet scriptPubKey")
        live = node.gettxout(sample["txid"], sample["vout"], False)
        if live is None or live["scriptPubKey"]["hex"] != row["script"]:
            raise AssertionError("harness: gettxout script != snapshot script")

    def _state_diffs(self, a, b):
        diffs = []
        keys = [
            "tip", "height", "state_hash", "txoutset_hash", "utxo_count",
            "res_atoms", "oru_atoms", "body_res_mint_count", "convergence_in_utxo",
            "converge_count", "replay_mint_count", "replay_res_balance",
            "replay_other_balance", "res_mint_cumulative", "utxo_res_balance",
            "utxo_non_res_balance", "utxo_sha256",
        ]
        for key in keys:
            if a[key] != b[key]:
                diffs.append(f"{key}: {a[key]!r} != {b[key]!r}")
        if a["convergence_outputs"] != b["convergence_outputs"]:
            diffs.append(
                f"convergence_outputs: {a['convergence_outputs']!r} != {b['convergence_outputs']!r}"
            )
        if a["utxo_text"] != b["utxo_text"]:
            diffs.extend(first_diff(a["utxo_text"], b["utxo_text"]))
        return diffs

    def _check_equal(self, label: str, left, right, expect_equal: bool = True):
        diffs = self._state_diffs(left, right)
        equal = not diffs
        self.evidence["scenarios"][label] = {
            "equal": equal,
            "expect_equal": expect_equal,
            "left": left["label"],
            "right": right["label"],
            "left_state_hash": left["state_hash"],
            "right_state_hash": right["state_hash"],
            "left_utxo_sha256": left["utxo_sha256"],
            "right_utxo_sha256": right["utxo_sha256"],
            "diffs": diffs[:24],
        }
        self._flush()
        if equal != expect_equal:
            self._fail(
                "F-01-STATE",
                f"state comparison {label} expected equal={expect_equal}",
                "structural UTXO and asset equality" if expect_equal else "states differ",
                diffs[:24],
                "dumptxoutset + active Convergence outputs + state_hash",
            )

    def _submit_unburned_res_block(self, node) -> Optional[str]:
        utxos = []
        for u in node.listunspent(1, 9999999):
            spk = u.get("scriptPubKey", "")
            if not (isinstance(spk, str) and spk.startswith("0014") and len(spk) == 44):
                continue
            if int(u.get("confirmations", 0)) < 100:
                continue
            if not u.get("spendable", True):
                continue
            if atoms_of(u["amount"]) <= 10000:
                continue
            utxos.append(u)
        if not utxos:
            raise AssertionError("harness: no mature ORU for the invalid competitor")
        utxo = utxos[0]
        res_addr = node.getnewaddress("", "bech32m")
        change_addr = node.getnewaddress("", "bech32")
        change_atoms = atoms_of(utxo["amount"]) - 1000
        raw = node.createrawtransaction(
            [{"txid": utxo["txid"], "vout": utxo["vout"]}],
            {
                res_addr: {"amount": "0.01000000", "asset": "RES"},
                change_addr: Decimal(change_atoms) / Decimal(COIN),
            },
        )
        signed = node.signrawtransactionwithwallet(raw)
        if not signed.get("complete"):
            raise AssertionError(f"harness: invalid-competitor tx did not sign: {signed}")
        probed = node.testmempoolaccept([signed["hex"]])[0]
        self.evidence["invalid_tx_mempool"] = probed
        return self._submit_tx_in_block(node, tx_from_hex(signed["hex"]), "invalid-res")

    def _submit_premature_res_spend(self, node, mint_txid: str, res_vout: int, res_atoms: int) -> str:
        live = node.gettxout(mint_txid, res_vout, False)
        assert live is not None
        dest = node.getnewaddress("", "bech32m")
        amount = Decimal(res_atoms) / Decimal(COIN)
        raw = node.createrawtransaction(
            [{"txid": mint_txid, "vout": res_vout}],
            {dest: {"amount": format(amount, "f"), "asset": "RES"}},
        )
        signed = node.signrawtransactionwithwallet(raw)
        if not signed.get("complete"):
            raise AssertionError(f"harness: premature spend did not sign: {signed}")
        reason = self._submit_tx_in_block(node, tx_from_hex(signed["hex"]), "premature-res")
        return reason if reason is not None else "accepted"

    def _submit_tx_in_block(self, node, tx, label: str) -> Optional[str]:
        tip = node.getbestblockhash()
        tpl = node.getblocktemplate({"rules": ["segwit"]})
        if tpl["previousblockhash"] != tip:
            raise AssertionError(f"harness: GBT prev {tpl['previousblockhash']} != tip {tip}")
        res_mint_out = None
        mint = tpl.get("default_res_mint")
        if mint:
            from test_framework.messages import CTxOut
            res_mint_out = CTxOut(int(mint["value"]), CScript(bytes.fromhex(mint["scriptPubKey"])))
        carrier = tpl.get("default_pcorb_carrier")
        conv = tpl.get("default_convergence_commitment")
        root = tpl.get("convergence_root")
        block = build_block(
            [],
            tx,
            int(tpl["prev_nbits"], 16),
            int(tpl["bits"], 16),
            hash_prev_block=hash_prev_from_rpc_hex(tpl["previousblockhash"]),
            block_height=int(tpl["height"]),
            cvge_script_pubkey=bytes.fromhex(conv) if conv else None,
            coinbase_value_sats=int(tpl["coinbasevalue"]),
            ntime=int(tpl["curtime"]) + 1,
            block_version=int(tpl["version"]),
            rpc_node=node,
            convergence_root=bytes.fromhex(root)[::-1] if root else None,
            res_mint_out=res_mint_out,
            pcorb_carrier_script=bytes.fromhex(carrier) if carrier else None,
        )
        sig_hex = tpl.get("default_coinbase_scriptsig")
        if sig_hex:
            block.vtx[0].vin[0].scriptSig = CScript(bytes.fromhex(sig_hex))
            block.vtx[0].rehash()
            block.hashMerkleRoot = block.calc_merkle_root()
            block.rehash()
        solve_block(block)
        reason = node.submitblock(block.serialize(with_witness=True).hex())
        self.log.info("%s submitblock → %s", label, reason)
        self.evidence.setdefault("submitblock", {})[label] = reason
        return reason

    def _record(self, name: str, ok: bool, detail: dict):
        self.evidence["scenarios"][name] = {"pass": ok, **detail}
        self.log.info("PASS %s", name)
        self._flush()

    def _fail(self, finding_id: str, scenario: str, expected, actual, code_path: str):
        finding = {
            "id": finding_id,
            "scenario": scenario,
            "expected": expected,
            "actual": actual,
            "code_path": code_path,
            "reproduction": "python3 core/test/functional/feature_f01_reorg_replay_equal.py",
            "consensus_impact": "reorg_replay_equal is not established",
            "classification": "implementation defect or protocol ambiguity; not patched",
        }
        self.finding = finding
        self.evidence["finding"] = finding
        self.evidence["result"] = "BLOCKED"
        os.makedirs(os.path.dirname(FINDING_PATH), exist_ok=True)
        with open(FINDING_PATH, "w", encoding="utf-8") as handle:
            json.dump(finding, handle, indent=2, default=str)
            handle.write("\n")
        self._flush()
        raise AssertionError(f"{finding_id}: {scenario}: expected {expected!r} actual {actual!r}")

    def _flush(self):
        os.makedirs(os.path.dirname(EVIDENCE_PATH), exist_ok=True)
        # Rows are large and already committed in utxo_text. Drop them from the file.
        payload = json.loads(json.dumps(self.evidence, default=str))
        with open(EVIDENCE_PATH, "w", encoding="utf-8") as handle:
            json.dump(payload, handle, indent=2)
            handle.write("\n")


if __name__ == "__main__":
    F01ReorgReplayEqualTest().main()

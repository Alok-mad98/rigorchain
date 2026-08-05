#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Attack model → real block → ``submitblock`` → exact reject reason (consensus ground truth).

This file follows the *shape* of a minimal ``bitcoind`` functional template (mine tip,
assemble ``CBlock``, ``submitblock``, ``assert_equal`` on reject string) while staying
compatible with **Rigor** rules: witness commitment, CVGE coinbase commitment, convergence txs,
and descriptor-wallet devtest setup.

**Why not the bare ``create_block`` + fake ``ORB2`` snippet alone?** On Rigor,
``ContextualCheckBlock`` enforces convergence commitment / convergence draw **before** RIGOR coinbase checks.
Blocks without a valid CVGE path are rejected as ``bad-convergence-missing`` / ``bad-convergence-*``,
so mutations that skip real template wiring fail early. RIGOR target checks use
``bad-rigor-commitment-missing`` / ``bad-rigor-target-mismatch``; RES body mint proofs use
``bad-rigor-res-*`` on body transactions when exercised.

Optional deeper paths (``bad-rigor-multiple-res`` with two valid signed RES mints) need
spendable RES UTXOs; see ``feature_rigor_rng_wallet.py``.
"""

import json
import os
import subprocess
import sys
from decimal import Decimal
from typing import List, Optional, Tuple

from test_framework.rigor_block_builder import (
    build_block,
    create_convergence_tx,
    hash_prev_from_rpc_hex,
    make_rigor_commitment_script,
    make_convergence_script,
)
from test_framework.opus_pow import solve_block_opus
from test_framework.messages import CTransaction, tx_from_hex
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import append_config, assert_equal, get_datadir_path

# RES receive script for CreateNewBlock once mempool has convergence-draw winners.
# Same deterministic witness-v1-RES bech32m address used by other devtest tests.
from feature_res_core_convergence_draw import DEVTEST_MINING_RES_FEE_ADDR  # noqa: E402

# Consensus-aligned winner selection (same inequality + argmin as ``attack_sim`` / node).
# Attack helpers live under ``test/rigor_attack/``.
_AD_ATTACK = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "rigor_attack"))
if _AD_ATTACK not in sys.path:
    sys.path.insert(0, _AD_ATTACK)
from attack_sim import COIN, MAX_RES_MINT_SIM_SUPPLY, collect_convergence_winner_txid  # noqa: E402
# ``attack_sim`` inserts ``test/rigor`` for ``rigor_model``.
from rigor_model import convergenceCandidate, compute_convergence_hash, compute_convergence_root_bytes  # noqa: E402


# ---------------------------------------------------------------------------
# Consensus reject strings (exact ``submitblock`` return values)
# ---------------------------------------------------------------------------
REJECT_BAD_RIGOR_COMMITMENT_MISSING = "bad-rigor-commitment-missing"
REJECT_BAD_RIGOR_TARGET_MISMATCH = "bad-rigor-target-mismatch"
REJECT_BAD_RIGOR_MULTIPLE_RNG = "bad-rigor-multiple-res"
REJECT_BAD_CONVERGENCE_CUMULATIVE = "bad-convergence-cumulative"
REJECT_BAD_CONVERGENCE_DRAW_CANDIDATE_ROOT = "bad-convergence-draw-candidate-root"
REJECT_BAD_CONVERGENCE_MISSING = "bad-convergence-missing"


# ---------------------------------------------------------------------------
# RIGOR / convergence helpers (``compute_convergence_hash`` from ``rigor_model``)
# ---------------------------------------------------------------------------
def build_rigor_commitment(_winner_unused: bytes, res_draw_target_nbits: int):
    """``OP_RETURN`` payload ``RIGOR`` + uint32 LE compact draw target (8 bytes; winner binds in RES2 claim witness)."""
    return make_rigor_commitment_script(int(res_draw_target_nbits) & 0xFFFFFFFF)


def gfr_res_probability_target_hex(fr: dict) -> str:
    """Draw target ``T`` from ``getconvergenceresult`` (``res_probability_target``)."""
    v = fr.get("res_probability_target")
    if v is not None:
        return v
    raise KeyError("getconvergenceresult missing res_probability_target")


def _conv_payload_bounds(conv_script_hex: str):
    """Return ``(raw_bytearray, payload_start, payload_end)`` for CVGE v2 OP_RETURN push.

    ``convergenceCommitmentScript`` uses ``CScript() << OP_RETURN << payload``; for a 45-byte payload
    Bitcoin uses a direct push (opcode ``0x2d``), not ``OP_PUSHDATA1``. Accept both encodings.
    """
    raw = bytearray(bytes.fromhex(conv_script_hex))
    assert raw[0] == 0x6A  # OP_RETURN
    i = 1
    op = raw[i]
    OP_PUSHDATA1 = 0x4C
    OP_PUSHDATA2 = 0x4D
    OP_PUSHDATA4 = 0x4E
    if op < OP_PUSHDATA1:
        ln = op
        start = i + 1
    elif op == OP_PUSHDATA1:
        ln = raw[i + 1]
        start = i + 2
    elif op == OP_PUSHDATA2:
        ln = int.from_bytes(raw[i + 1 : i + 3], "little")
        start = i + 3
    elif op == OP_PUSHDATA4:
        ln = int.from_bytes(raw[i + 1 : i + 5], "little")
        start = i + 5
    else:
        raise AssertionError(f"unexpected push opcode after OP_RETURN: 0x{op:02x}")
    assert ln == 45, f"CVGE v2 payload must be 45 bytes, got push len {ln}"
    return raw, start, start + ln


def _conv_corrupt_cumulative(conv_script_hex: str) -> bytes:
    raw, p0, _p1 = _conv_payload_bounds(conv_script_hex)
    off = p0 + 5
    cur = int.from_bytes(raw[off : off + 8], "little")
    raw[off : off + 8] = (cur + 1).to_bytes(8, "little")
    return bytes(raw)


def _conv_corrupt_draw_candidate_root(conv_script_hex: str) -> bytes:
    raw, p0, _p1 = _conv_payload_bounds(conv_script_hex)
    off = p0 + 13 + 10
    raw[off] ^= 0x01
    return bytes(raw)


def _run_convergence_helper(test_framework, node_idx: int, fund_address: Optional[str] = None) -> dict:
    srcdir = test_framework.config["environment"]["SRCDIR"]
    cli = test_framework.options.bitcoincli
    helper = os.path.join(srcdir, "contrib", "devtools", "devtest-create-convergence-tx.py")
    datadir = test_framework.nodes[node_idx].datadir
    wallet = test_framework.wallet_names[node_idx]
    cmd = [
        sys.executable,
        helper,
        "--cli",
        cli,
        "--no-chain-arg",
        f"--cli-extra=-datadir={datadir}",
        "--convergence-orum",
        "1.0",
        "--change-type",
        "bech32",
        "--json",
    ]
    if wallet:
        cmd.append(f"--cli-extra=-rpcwallet={wallet}")
    if fund_address:
        cmd.extend(["--fund-address", fund_address])
    r = subprocess.run(
        cmd,
        cwd=test_framework.options.tmpdir,
        capture_output=True,
        text=True,
        timeout=120,
    )
    if r.returncode != 0:
        raise AssertionError("convergence helper failed:\n" + (r.stderr or r.stdout or "no output"))
    return json.loads(r.stdout)


def _submit(node, block) -> str:
    """``submitblock`` result: ``None`` if accepted, else reject reason string."""
    return node.submitblock(hexdata=block.serialize(with_witness=True).hex())


def _json_norm(obj) -> str:
    """Stable JSON for comparing nested RPC objects.

    ``AuthServiceProxy`` turns JSON numbers for amounts into ``Decimal``; normalize before
    ``json.dumps`` so ``getconvergenceresult`` (``STR_AMOUNT`` fields like ``min_convergence``, ``convergence_amount``)
    compares cleanly between two RPC calls (hash vs height).
    """
    def _canonical(x):
        if isinstance(x, dict):
            return {k: _canonical(v) for k, v in x.items()}
        if isinstance(x, list):
            return [_canonical(v) for v in x]
        if isinstance(x, Decimal):
            return format(x, "f")
        return x

    return json.dumps(_canonical(obj), sort_keys=True)


def _rpc_uint256_hex_to_le_bytes(h: str) -> bytes:
    """RPC ``uint256::GetHex()`` string → 32-byte LE blob (matches ``HashWriter`` / ``Reconvergence`` preimage)."""
    return bytes.fromhex(h)[::-1]


def _amount_str_to_sats(s: str) -> int:
    return int(Decimal(s) * COIN)


def _winner_le_to_rpc_hex(w: Optional[bytes]) -> str:
    if w is None:
        return "00" * 32
    return w[::-1].hex()


def _convergence_tx_rows_from_convergenceresult(fr) -> List[Tuple[bytes, int]]:
    """``(txid_internal_le, convergence_amount_sats)`` for txs with ``draw_eligible`` (matches ``IsconvergenceRngCandidate``)."""
    out: List[Tuple[bytes, int]] = []
    for c in fr["candidates"]:
        if not c.get("draw_eligible", False):
            continue
        amt = _amount_str_to_sats(c["convergence_amount"])
        if amt < COIN:
            continue
        txid_le = bytes.fromhex(c["txid"])[::-1]
        out.append((txid_le, amt))
    return out


def recompute_winner_from_chain_context(node, fr) -> Optional[bytes]:
    """
    Recompute the convergence draw winner from the same inputs the node uses for
    ``getconvergenceresult``: the σ seed, the payout height, the RES draw target ``T``,
    ``res_remaining_supply`` and the convergence tx set.

    SECURITY-001-A8: the seed is the awarding block's own hash and the height fed to σ is
    the payout height (one past the awarding block), both reported directly by the RPC.
    """
    del node
    # RPC ``sigma_seed`` / ``res_probability_target`` use ``uint256::GetHex()`` (big-endian hex);
    # preimage bytes match consensus ``uint256`` memory order (LE) — same convention as
    # ``_rpc_uint256_hex_to_le_bytes``.
    bind = _rpc_uint256_hex_to_le_bytes(fr["sigma_seed"])
    height = int(fr["payout_height"])
    draw_target_le = _rpc_uint256_hex_to_le_bytes(gfr_res_probability_target_hex(fr))
    remaining_res_supply = MAX_RES_MINT_SIM_SUPPLY
    rows = _convergence_tx_rows_from_convergenceresult(fr)
    return collect_convergence_winner_txid(
        rows,
        bind,
        height,
        draw_target_le,
        remaining_res_supply,
        enforce_min_two_candidates=False,
    )


class RigorAttackBlocksTest(BitcoinTestFramework):
    def set_test_params(self):
        self.chain = "devtest"
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [[], []]
        self.wallet_names = ["default_wallet", False]
        self._requires_wallet = True

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def setup_chain(self):
        super().setup_chain()
        # CreateNewBlock requires a RES receive script as soon as a convergence-draw winner
        # appears in mempool (BlockAssembler / validation.cpp). Without -miningresfeeaddr
        # the GBT / generatetoaddress call fails with "convergence draw winner present but
        # no RES receive script". Mirrors feature_rigor_convergence_rng_consensus.py.
        line = f"miningresfeeaddr={DEVTEST_MINING_RES_FEE_ADDR}"
        for i in range(self.num_nodes):
            append_config(get_datadir_path(self.options.tmpdir, i), [line])

    def _assert_rejected_exact(self, node, block, expected: str):
        r = _submit(node, block)
        assert r is not None, "expected invalid block to be rejected"
        assert_equal(r, expected)

    def _setup_chain(self, node):
        # Mine coinbase to a wallet-owned bech32 (P2WPKH) address so the convergence helper's
        # listunspent query finds spendable ORU-domain UTXOs. The deterministic witness
        # mining address is not part of the default wallet keypool on every devtest path.
        mining_addr = node.getnewaddress("", "bech32")
        self.generatetoaddress(node, 130, mining_addr)
        # Stash for later cases that need to mine maturity blocks between rejects.
        self._wallet_mining_addr = mining_addr

    def _fresh_convergence_pair_and_template(self, node):
        res_a = _run_convergence_helper(self, 0)
        res_b = _run_convergence_helper(self, 0)
        assert res_a["txid"] != res_b["txid"]
        self.sync_mempools()
        tpl = node.getblocktemplate({"rules": ["segwit"]})
        assert "default_convergence_commitment" in tpl, "GBT must include CVGE for attack assembly"
        return tpl, res_a, res_b

    def _assemble_attack_block(self, node, tpl, res_a, res_b, **kwargs):
        tx_a = tx_from_hex(res_a["raw_signed"])
        tx_b = tx_from_hex(res_b["raw_signed"])
        h = int(tpl["height"])
        hp = hash_prev_from_rpc_hex(tpl["previousblockhash"])
        nbits = int(tpl["bits"], 16)
        conv_hex = tpl["default_convergence_commitment"]
        # The RIGOR coinbase commitment carries the PARENT header's compact nBits, and ConnectBlock
        # pins it to exactly that (`bad-rigor-target-mismatch`). It is not the draw target T: a
        # block built with compact(T) here is rejected on the target rule before reaching whatever
        # the case actually means to exercise, since the RIGOR check runs first.
        good_compact = int(tpl["prev_nbits"], 16)
        # C spans the whole draw window since A-9-C, so it cannot be derived from this block's
        # body. The template's set and this block's are the same — both are the mempool's two
        # convergence transactions — so the root the node published for the template is the one
        # ConnectBlock will demand here (`bad-cb-convergence-root`).
        committed_root = bytes.fromhex(tpl["convergence_root"])[::-1]
        return build_block(
            [tx_a, tx_b],
            kwargs.get("res_claim_tx"),
            kwargs.get("commitment", good_compact),
            nbits,
            hash_prev_block=hp,
            block_height=h,
            cvge_script_pubkey=kwargs.get("cvge_script_pubkey", bytes.fromhex(conv_hex)),
            coinbase_value_sats=int(tpl["coinbasevalue"]),
            ntime=int(tpl["curtime"]),
            block_version=int(tpl["version"]),
            rpc_node=node,
            convergence_root=committed_root,
        )

    def _mine_block_with_two_convergences(self, node):
        """Broadcast two signed convergence transactions and mine exactly one block that includes both."""
        res_a = _run_convergence_helper(self, 0)
        res_b = _run_convergence_helper(self, 0)
        assert res_a["txid"] != res_b["txid"]
        self.sync_mempools()
        mining_addr = node.get_deterministic_witness_mining_address()
        out = self.generatetoaddress(node, 1, mining_addr)
        assert len(out) == 1
        block_hash = out[0]
        txids = node.getblock(block_hash, 1)["tx"]
        assert res_a["txid"] in txids and res_b["txid"] in txids
        return block_hash, res_a, res_b

    # -----------------------------------------------------------------------
    # A — Real convergence transactions + recomputed winner (not only CVGE hex from GBT)
    # -----------------------------------------------------------------------
    def case_a_recomputed_winner_matches_node(self, node, convergence_block_hash: str):
        self.log.info(
            "A: mined convergence block → Python collect_convergence_winner_txid matches RPC ``draw_winner_txid``"
        )
        fr = node.getconvergenceresult(convergence_block_hash)
        w_py = recompute_winner_from_chain_context(node, fr)
        committed = fr["draw_winner_txid"].lower()
        assert_equal(committed, _winner_le_to_rpc_hex(w_py).lower())

    # -----------------------------------------------------------------------
    # B — SECURITY-001-A8: σ is seeded by the parent's proof of work
    # -----------------------------------------------------------------------
    def case_b_sigma_is_seeded_by_parent_pow(self, node, fr):
        """The whole σ preimage is fixed by the awarding block, so re-rolling costs a PoW.

        Before Sigma-PoW-01 the producer of the paying block chose the seed, the candidate set
        and C while assembling that block, and could re-roll σ until it won — a few milliseconds
        of CPU per block. Now the seed is the awarding block's hash and the set is the one that
        block committed, so the only way to move σ is to mine a different awarding block.
        """
        del node
        self.log.info("B: σ depends on the awarding block hash and the payout height, and on nothing else")
        rows = _convergence_tx_rows_from_convergenceresult(fr)
        if len(rows) < 2:
            self.log.info("B: skip (need ≥2 convergence rows)")
            return
        assert_equal(fr["sigma_seed"], fr["hash"])
        assert_equal(int(fr["payout_height"]), int(fr["height"]) + 1)

        bind = _rpc_uint256_hex_to_le_bytes(fr["sigma_seed"])
        height = int(fr["payout_height"])
        draw_target_le = _rpc_uint256_hex_to_le_bytes(gfr_res_probability_target_hex(fr))
        remaining_res_supply = MAX_RES_MINT_SIM_SUPPLY
        base_w = collect_convergence_winner_txid(
            rows,
            bind,
            height,
            draw_target_le,
            remaining_res_supply,
            enforce_min_two_candidates=False,
        )
        tx0 = rows[0][0]
        croot = compute_convergence_root_bytes([convergenceCandidate(txid) for txid, _ in rows])
        h0_real = compute_convergence_hash(tx0, bind, height, croot)

        # A one-bit change in the seed — i.e. a different awarding block — moves σ, and may
        # move the winner with it. That is the cost the attacker now has to pay per re-roll.
        seed_flipped = bytes([bind[0] ^ 0x01]) + bind[1:]
        assert compute_convergence_hash(tx0, seed_flipped, height, croot) != h0_real, (
            "σ must depend on the awarding block hash, or the draw is free to grind"
        )
        # Recomputing with the real inputs is deterministic and agrees with itself.
        assert_equal(
            base_w,
            collect_convergence_winner_txid(
                rows,
                bind,
                height,
                draw_target_le,
                remaining_res_supply,
                enforce_min_two_candidates=False,
            ),
        )
        w_alt_seed = collect_convergence_winner_txid(
            rows,
            seed_flipped,
            height,
            draw_target_le,
            remaining_res_supply,
            enforce_min_two_candidates=False,
        )
        if base_w is not None and w_alt_seed is not None and w_alt_seed != base_w:
            self.log.info("B: a different awarding block hash flips the winner (one proof of work per re-roll)")
        else:
            self.log.info("B: seed change did not flip the winner for this draw (σ still moved)")

    # -----------------------------------------------------------------------
    # C — Same block identity across nodes / RPC aliases / invalidate+reconsider
    # -----------------------------------------------------------------------
    def case_c_convergenceresult_hash_matches_height(self, node, convergence_block_hash: str):
        self.log.info("C: getconvergenceresult(hash) == getconvergenceresult(height) on the same block")
        by_hash = node.getconvergenceresult(convergence_block_hash)
        height = int(by_hash["height"])
        # RPCArg::STR: pass decimal height as JSON string (e.g. "100"), not a JSON number.
        by_h = node.getconvergenceresult(str(height))
        assert_equal(_json_norm(by_hash), _json_norm(by_h))

    def case_c_two_nodes_identical_convergenceresult(self, convergence_block_hash: str):
        self.log.info("C: two synced nodes return identical getconvergenceresult for the same block hash")
        a = self.nodes[0].getconvergenceresult(convergence_block_hash)
        b = self.nodes[1].getconvergenceresult(convergence_block_hash)
        assert_equal(_json_norm(a), _json_norm(b))

    def case_c_invalidate_reconsider_convergenceresult_stable(self, convergence_block_hash: str):
        self.log.info(
            "C: both nodes invalidate convergence block + reconsider → getconvergenceresult JSON unchanged"
        )
        gr0 = _json_norm(self.nodes[0].getconvergenceresult(convergence_block_hash))
        mining_addr = self.nodes[0].get_deterministic_witness_mining_address()
        child = self.generatetoaddress(self.nodes[0], 1, mining_addr)[0]
        self.sync_all()
        for n in self.nodes:
            n.invalidateblock(convergence_block_hash)
        self.sync_all()
        for n in self.nodes:
            n.reconsiderblock(convergence_block_hash)
        self.sync_all()
        gr1 = _json_norm(self.nodes[0].getconvergenceresult(convergence_block_hash))
        assert_equal(gr0, gr1)
        for n in self.nodes:
            n.reconsiderblock(child)
        self.sync_all()

    # -----------------------------------------------------------------------
    # Case 1 — CVGE cumulative lie (split / mis-report total Mint Cost)
    # -----------------------------------------------------------------------
    def case_01_conv_cumulative_lie(self, node, tpl, res_a, res_b):
        self.log.info("Case 1: CVGE cumulative convergence mismatch → %s", REJECT_BAD_CONVERGENCE_CUMULATIVE)
        bad_conv = _conv_corrupt_cumulative(tpl["default_convergence_commitment"])
        b = self._assemble_attack_block(node, tpl, res_a, res_b, cvge_script_pubkey=bad_conv)
        solve_block_opus(b)
        self._assert_rejected_exact(node, b, REJECT_BAD_CONVERGENCE_CUMULATIVE)

    # -----------------------------------------------------------------------
    # Case 2 — CVGE draw root / preimage lane
    # -----------------------------------------------------------------------
    def case_02_conv_draw_candidate_root_tamper(self, node, tpl, res_a, res_b):
        self.log.info("Case 2: CVGE ConvergenceDrawCandidates root tamper → %s", REJECT_BAD_CONVERGENCE_DRAW_CANDIDATE_ROOT)
        bad_conv = _conv_corrupt_draw_candidate_root(tpl["default_convergence_commitment"])
        b = self._assemble_attack_block(node, tpl, res_a, res_b, cvge_script_pubkey=bad_conv)
        solve_block_opus(b)
        self._assert_rejected_exact(node, b, REJECT_BAD_CONVERGENCE_DRAW_CANDIDATE_ROOT)

    # -----------------------------------------------------------------------
    # Case 3 — RIGOR commitment output missing (non-genesis chain)
    # -----------------------------------------------------------------------
    def case_03_bad_rigor_commitment_missing(self, node, tpl, res_a, res_b):
        self.log.info("Case 3: omit RIGOR OP_RETURN → %s", REJECT_BAD_RIGOR_COMMITMENT_MISSING)
        b = self._assemble_attack_block(node, tpl, res_a, res_b, commitment=None)
        solve_block_opus(b)
        self._assert_rejected_exact(node, b, REJECT_BAD_RIGOR_COMMITMENT_MISSING)

    # -----------------------------------------------------------------------
    # Case 4 — CVGE output missing / not parseable (ContextualCheckBlock)
    # -----------------------------------------------------------------------
    def case_04_bad_convergence_missing(self, node, tpl, res_a, res_b):
        self.log.info("Case 4: replace CVGE with minimal OP_RETURN → %s", REJECT_BAD_CONVERGENCE_MISSING)
        bad_conv = bytes.fromhex("6a0100")  # OP_RETURN OP_0
        b = self._assemble_attack_block(node, tpl, res_a, res_b, cvge_script_pubkey=bad_conv)
        solve_block_opus(b)
        self._assert_rejected_exact(node, b, REJECT_BAD_CONVERGENCE_MISSING)

    # -----------------------------------------------------------------------
    # Case 5 — RIGOR res_draw_target_nbits (compact) mismatch
    # -----------------------------------------------------------------------
    def case_05_bad_target_prev_nbits_mismatch(self, node, tpl, res_a, res_b):
        # The RES inequality RHS is the constitutional constant, and since SECURITY-001-A8
        # `target_prev` is no longer a σ input either. The coinbase RIGOR commitment still
        # carries the compact `nBits`, and ConnectBlock still pins it to the parent header,
        # so a mismatch here is still a reject — it just no longer bears on σ.
        self.log.info("Case 5: RIGOR target_prev_nbits coinbase commitment mismatch -> %s", REJECT_BAD_RIGOR_TARGET_MISMATCH)
        bad_compact = 0x01020304
        b = self._assemble_attack_block(node, tpl, res_a, res_b, commitment=bad_compact)
        solve_block_opus(b)
        self._assert_rejected_exact(node, b, REJECT_BAD_RIGOR_TARGET_MISMATCH)

    def run_test(self):
        node = self.nodes[0]
        self.log.info("Mining initial chain (maturity + subsidy for convergence helper)")
        self._setup_chain(node)
        self.sync_all()

        # Template helpers sanity (not used for reject selection — node is ground truth).
        assert len(compute_convergence_hash(bytes(32), bytes(32), 1, bytes(32))) == 32
        assert len(bytes(build_rigor_commitment(bytes(32), 0x1D00FFFF))) >= 10

        self.log.info("========== A / B / C (convergence winner path + σ seed + reorg RPC) ==========")
        fb_hash, _ra, _rb = self._mine_block_with_two_convergences(node)
        self.sync_all()
        fr = node.getconvergenceresult(fb_hash)
        self.case_a_recomputed_winner_matches_node(node, fb_hash)
        self.case_b_sigma_is_seeded_by_parent_pow(node, fr)
        self.case_c_convergenceresult_hash_matches_height(node, fb_hash)
        self.case_c_two_nodes_identical_convergenceresult(fb_hash)
        self.case_c_invalidate_reconsider_convergenceresult_stable(fb_hash)

        mining_addr = getattr(self, "_wallet_mining_addr", None) or node.getnewaddress("", "bech32")
        cases = (
            ("1", self.case_01_conv_cumulative_lie),
            ("2", self.case_02_conv_draw_candidate_root_tamper),
            ("3", self.case_03_bad_rigor_commitment_missing),
            ("4", self.case_04_bad_convergence_missing),
            ("5", self.case_05_bad_target_prev_nbits_mismatch),
        )
        for label, fn in cases:
            tpl, res_a, res_b = self._fresh_convergence_pair_and_template(node)
            self.log.info("========== Case %s ==========", label)
            fn(node, tpl, res_a, res_b)
            self.generatetoaddress(node, 1, mining_addr)

        self.log.info(
            "Optional: %s needs two valid RES mint body txs (see ``feature_rigor_rng_wallet.py``).",
            REJECT_BAD_RIGOR_MULTIPLE_RNG,
        )

        self.log.info("Helper sanity: ``make_convergence_script`` + ``create_convergence_tx`` → ``testmempoolaccept``")
        assert len(make_convergence_script(int(COIN))) >= 1
        ftx = create_convergence_tx(node, Decimal("1.0"))
        ta = node.testmempoolaccept([ftx.serialize_with_witness().hex()])[0]
        assert ta["allowed"], ta

        self.log.info("Control: resubmit tip block → duplicate")
        tip = node.getbestblockhash()
        h = node.getblock(tip, 0)
        assert_equal(node.submitblock(hexdata=h), "duplicate")


if __name__ == "__main__":
    RigorAttackBlocksTest().main()

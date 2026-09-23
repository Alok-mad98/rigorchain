#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Shared harness helpers for fault-injection tests (harness-only; no consensus edits).

Signature / witness routing (consensus facts; see ``src/script/interpreter.cpp``)
---------------------------------------------------------------------------
* **ORU native (Ed25519)** — witness spends whose stack pubkey is ``0x00 || ed25519_pk``
  (33 bytes, first byte ``0x00``) are verified with ``CheckEd25519Signature``.
* **Bitcoin-compatible witness v0 (ECDSA)** — compressed secp256k1 pubkeys (``0x02`` / ``0x03``)
  on ``SigVersion::WITNESS_V0`` use ``CheckECDSASignature`` (BIP143). This path exists for
  compatibility; it is **not** the ORU-native identity key shape.
* **MiniWallet** (``test_framework.wallet``, ``ADDRESS_OP_TRUE``) builds **standard P2WPKH**
  and signs with **secp256k1 ECDSA** over BIP143 — i.e. the **compatibility** path only.
  MiniWallet is a **test helper**, not a consensus-critical ORU signing primitive.
* **Legacy scriptSig ECDSA** — disabled on RigorChain for non-witness paths
  (``SCRIPT_ERR_SECP256K1_DISABLED``).

Layering (post-freeze harness isolation)
----------------------------------------
* **Layer-A (consensus fault)** — invalid ``submitblock`` / convergence commitment / convergence coinbase
  rejects (FAULT-003). Run standalone via ``feature_fault_consensus_only.py``.
  **Note:** FAULT-001 and FAULT-002 in the tree still use ``MiniWallet`` + mempool policy
  vectors; they are treated as **Layer-B compatibility** until refactored off MiniWallet.
* **Layer-B (compat / policy / mempool)** — mempool policy, MiniWallet witness hostility
  (FAULT-001/002), P2P relay/orphans/SIGKILL/mempool-order (FAULT-004…008).
  These remain in ``feature_fault_injection.py`` (full suite).

Replay / deterministic state capture (STEP-0202, triplet fingerprints) is shared; this
module does **not** change replay, RPL2, or ``state_hash`` semantics — only factors common
setup for both layers.
"""

from __future__ import annotations

import importlib.util
import os
from decimal import Decimal

from test_framework.messages import tx_from_hex
from test_framework.rigor_block_builder import build_block, create_convergence_tx, hash_prev_from_rpc_hex
from test_framework.util import append_config, get_datadir_path

import test_framework.fault_injection_harness as fh

from res_security.harness import DEVTEST_MINING_RES_FEE_ADDR  # noqa: I252


SYNC_TIMEOUT = 120


def _load_replay_failure_capture():
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "feature_replay_failure_capture.py")
    spec = importlib.util.spec_from_file_location("feature_replay_failure_capture", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def assemble_attack_block(node, tpl, res_a, res_b, **kwargs):
    tx_a = tx_from_hex(res_a["raw_signed"])
    tx_b = tx_from_hex(res_b["raw_signed"])
    h = int(tpl["height"])
    hp = hash_prev_from_rpc_hex(tpl["previousblockhash"])
    nbits = int(tpl["bits"], 16)
    conv_hex = tpl["default_convergence_commitment"]
    # The RIGOR coinbase commitment carries the PARENT header's compact nBits — not the draw
    # target T — and ConnectBlock pins it to exactly that (`bad-rigor-target-mismatch`).
    good_compact = int(tpl["prev_nbits"], 16)
    # Since A-9-C, C spans the whole draw window and cannot be derived from this block's body.
    # The template's convergence set and this block's are the same two mempool transactions, so
    # the node's published root is the one it will demand (`bad-cb-convergence-root`).
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


class FaultInjectionShared:
    """Node setup + RIGOR convergence helpers shared by Layer-A and Layer-B harnesses."""

    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True
        self.chain = "devtest"
        self.extra_args = [[], [], []]
        self.wallet_names = ["default_wallet", False, False]
        self._requires_wallet = True

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def setup_chain(self):
        super().setup_chain()
        line = f"miningresfeeaddr={DEVTEST_MINING_RES_FEE_ADDR}"
        for i in range(self.num_nodes):
            append_config(get_datadir_path(self.options.tmpdir, i), [line])

    def import_deterministic_coinbase_privkeys(self):
        for i in range(self.num_nodes):
            wn = (
                self.default_wallet_name
                if self.wallet_names is None
                else self.wallet_names[i]
                if i < len(self.wallet_names)
                else False
            )
            if wn is False:
                continue
            n = self.nodes[i]
            if wn is not None:
                n.createwallet(
                    wallet_name=wn,
                    descriptors=self.options.descriptors,
                    load_on_startup=True,
                )

    def setup_network(self):
        self.setup_nodes()
        self.connect_nodes(1, 0)
        self.connect_nodes(2, 0)

    def _fault_dir(self) -> str:
        return os.path.join(self.options.tmpdir, "fault_injection")

    def _trace_path(self) -> str:
        return os.path.join(self._fault_dir(), "fault_trace.tsv")

    def _maybe_capture_divergence(self, node_a, node_b, node_c, reason: str):
        fps = [fh.consensus_fingerprint(n) for n in (node_a, node_b, node_c)]
        if fps[0]["state_hash"] == fps[1]["state_hash"] == fps[2]["state_hash"]:
            return
        mod = _load_replay_failure_capture()
        max_h = min(n.getblockcount() for n in (node_a, node_b, node_c))
        div = mod.find_first_divergence_height(node_a, node_b, max_h)
        out = os.path.join(self.options.tmpdir, "fault_injection_divergence_capture")
        mod.capture_divergence(
            self.log,
            out,
            {"A": node_a, "B": node_b, "C": node_c},
            div,
            harness_failure_reason=reason,
        )

    def _assert_triplet(self, na, nb, nc, ctx: str):
        try:
            fh.assert_fingerprints_equal(
                fh.consensus_fingerprint(na),
                fh.consensus_fingerprint(nb),
                fh.consensus_fingerprint(nc),
                ctx,
            )
        except AssertionError:
            self._maybe_capture_divergence(na, nb, nc, ctx)
            raise

    def _trace_row(self, fault_id: str, case_id: str, field: str, value: str):
        fh.append_fault_trace(
            self._trace_path(),
            "\t".join([fault_id, case_id, field, value]),
        )

    def _mine_baseline(self, na, nb, nc, nblocks: int):
        addr = na.get_deterministic_witness_mining_address()
        chunk = 5
        for i in range(0, nblocks, chunk):
            k = min(chunk, nblocks - i)
            self.generatetoaddress(
                na,
                k,
                addr,
                sync_fun=lambda: self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT),
            )
        fh.drain(na, nb, nc)

    def _setup_rigor_attack_chain(self, node):
        self._convergence_fund_address = None
        wname = (
            self.wallet_names[node.index]
            if node.index < len(self.wallet_names)
            else None
        )
        if wname:
            w = node.get_wallet_rpc(wname)
            wallet_addr = w.getnewaddress(address_type="bech32")
            self._convergence_fund_address = wallet_addr
            self.generatetoaddress(node, 130, wallet_addr)
        else:
            mining_addr = node.get_deterministic_witness_mining_address()
            self.generatetoaddress(node, 130, mining_addr)

    def _fresh_convergence_pair_and_template(self, node):
        fund = getattr(self, "_convergence_fund_address", None)
        if not fund:
            raise AssertionError("FAULT-003 requires _convergence_fund_address from _setup_rigor_attack_chain")
        wname = self.wallet_names[0]
        if not wname:
            raise AssertionError("FAULT-003 expects default_wallet on node0")

        def _broadcast_one():
            tx = create_convergence_tx(
                node,
                Decimal("1.0"),
                spend_addresses=[fund],
                change_address=fund,
            )
            raw = tx.serialize_with_witness().hex()
            txid = node.sendrawtransaction(raw, 0, "1.0")
            entry = node.getmempoolentry(txid)
            meta = entry.get("convergence", {})
            assert meta.get("is_convergence") and meta.get("meets_min_convergence"), entry
            return {"txid": txid, "raw_signed": raw, "raw_unsigned": raw}

        res_a = _broadcast_one()
        res_b = _broadcast_one()
        assert res_a["txid"] != res_b["txid"]
        self.sync_mempools()
        tpl = node.getblocktemplate({"rules": ["segwit"]})
        assert "default_convergence_commitment" in tpl
        return tpl, res_a, res_b

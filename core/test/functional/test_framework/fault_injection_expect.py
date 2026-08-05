#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Frozen expected strings for fault-injection harnesses (policy / RPC vocabulary).

CI MUST run ``feature_fault_injection.py --descriptors`` (see .github/workflows).

When Bitcoin Core / RIGOR changes mempool or RPC error text, update this file
and the harness assertions — do **not** relax tests to ``allowed is False``
without checking the reject reason (Constitution: explicit behavior).
"""

# --- testmempoolaccept ``reject-reason`` (TxValidationState / policy) ----------

MEMPOOL_BAD_TXNS_INPUTS_DUPLICATE = "bad-txns-inputs-duplicate"
"""Consensus: duplicate prevouts within one transaction (CVE-2018-17144 family)."""

MEMPOOL_TX_SIZE_NON_STANDARD = "tx-size"
"""Policy: ``IsStandardTx`` weight exceeds ``MAX_STANDARD_TX_WEIGHT``."""

MEMPOOL_MIN_RELAY_FEE_NOT_MET_ORUM = "min relay fee not met (orum)"
"""RIGOR ORUM-domain relay floor (may precede ``tx-size`` if fee is too low for ``GetTxSize()``)."""

MEMPOOL_MIN_FEE_NOT_MET_ORUM = "mempool min fee not met (orum)"
"""RIGOR ORUM mempool minimum fee (congestion path; rare in isolated functional tests)."""

MEMPOOL_RPC_DECODE_FRAGMENT = "TX decode failed"
"""Both ``sendrawtransaction`` and ``testmempoolaccept`` decode failures."""

SENDRAW_MAX_UNSPENDABLE_FRAGMENT = "maxunspendable"
"""Substring of ``TransactionError::MAX_UNSPENDABLE_EXCEEDED`` user-visible text."""

# --- Script verification failures (prefix-stable; parenthetical detail may change) ---

MEMPOOL_SCRIPT_VERIFY_FAILED_PREFIX = "mandatory-script-verify-flag-failed"
"""Consensus script failure after deserialize (P2WPKH / witness programs)."""

MEMPOOL_NON_MANDATORY_SCRIPT_PREFIX = "non-mandatory-script-verify-flag"
"""Policy script failure (standardness) — some witness faults surface here."""

MEMPOOL_BAD_TXNS_VOUT_EMPTY = "bad-txns-vout-empty"
"""Consensus: ``CheckTransaction`` rejects zero outputs on non-coinbase."""

SUBMITBLOCK_BAD_TXNS_DUPLICATE = "bad-txns-duplicate"
"""Block merkle mutation / duplicate non-coinbase tx identity (CVE-2012-2459 family)."""

SUBMITBLOCK_BAD_TXNS_INPUTS_MISSINGORSPENT = "bad-txns-inputs-missingorspent"
"""ConnectBlock: a later tx spends prevouts already consumed earlier in the same block (e.g. two identical convergence txs)."""

SUBMITBLOCK_BAD_RIGOR_RES_WINNER_MISMATCH = "bad-rigor-res-winner-mismatch"
"""ConnectBlock: draw winner vs body RES mint count coupling (fires before CVGE ConvergenceDrawCandidates root byte match)."""

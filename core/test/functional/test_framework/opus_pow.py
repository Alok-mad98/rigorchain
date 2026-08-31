# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Compatibility shim. Consensus PoW is SPEC-008 SHA3-256; see rigor_pow.py."""

from test_framework.rigor_pow import (  # noqa: F401
    header80_from_block,
    rigor_pow_digest_hex,
    rigor_pow_digest_int,
    solve_block,
    solve_block_opus,
)

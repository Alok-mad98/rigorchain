#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""CVGE coinbase tamper helpers for submitblock tests (not lottery draw)."""

from __future__ import annotations

REJECT_BAD_CONVERGENCE_CUMULATIVE = "bad-convergence-cumulative"
REJECT_BAD_CONVERGENCE_MISSING = "bad-convergence-missing"


def conv_payload_bounds(conv_script_hex: str):
    raw = bytearray.fromhex(conv_script_hex)
    assert raw[0] == 0x6A  # RIGOR_MARKER_RETURN
    i = 1
    op = raw[i]
    push1, push2, push4 = 0x4C, 0x4D, 0x4E
    if op < push1:
        ln = op
        start = i + 1
    elif op == push1:
        ln = raw[i + 1]
        start = i + 2
    elif op == push2:
        ln = int.from_bytes(raw[i + 1 : i + 3], "little")
        start = i + 3
    elif op == push4:
        ln = int.from_bytes(raw[i + 1 : i + 5], "little")
        start = i + 5
    else:
        raise AssertionError(f"unexpected push opcode after RIGOR_MARKER_RETURN: 0x{op:02x}")
    assert ln == 45, f"CVGE v2 payload must be 45 bytes, got push len {ln}"
    return raw, start, start + ln


def conv_corrupt_cumulative(conv_script_hex: str) -> bytes:
    raw, p0, _p1 = conv_payload_bounds(conv_script_hex)
    off = p0 + 5
    cur = int.from_bytes(raw[off : off + 8], "little")
    raw[off : off + 8] = (cur + 1).to_bytes(8, "little")
    return bytes(raw)


def submit_block_hex(node, block) -> str:
    """submitblock result: None if accepted, else reject reason string."""
    return node.submitblock(hexdata=block.serialize(with_witness=True).hex())

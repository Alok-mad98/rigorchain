# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""SPEC-008 SHA3-256 proof-of-work for functional tests.

Matches ``src/rigor_kernel/pow_digest.cpp`` and
``formal_verification/coq/PowDigest.v``: SHA3-256("RGRPOW01" || header_80).
"""

from __future__ import annotations

import hashlib
from typing import Final

POW_DOMAIN_TAG: Final[bytes] = b"RGRPOW01"
POW_HEADER_LEN: Final[int] = 80
POW_PREIMAGE_LEN: Final[int] = 88

# Coq / C++ / Python zero-header digest (little-endian integer, display hex).
_ZERO_HEADER_DIGEST_HEX: Final[str] = (
    "fc04ded6f5d9f677b57536b4751c512d9e817b9dcc34e866945b8fdfc17f05dd"
)


def rigor_pow_digest_bytes(header80: bytes) -> bytes:
    if len(header80) != POW_HEADER_LEN:
        raise ValueError(f"header must be {POW_HEADER_LEN} bytes, got {len(header80)}")
    return hashlib.sha3_256(POW_DOMAIN_TAG + header80).digest()


def rigor_pow_digest_int(header80: bytes) -> int:
    return int.from_bytes(rigor_pow_digest_bytes(header80), "little")


def rigor_pow_digest_hex(header80: bytes) -> str:
    """Display-order hex, matching ``uint256::GetHex`` / RPC ``pow_hash``."""
    return f"{rigor_pow_digest_int(header80):064x}"


def header80_from_block(block) -> bytes:
    return bytes(block.serialize(with_witness=False)[:POW_HEADER_LEN])


def _self_test() -> None:
    assert rigor_pow_digest_hex(bytes(POW_HEADER_LEN)) == _ZERO_HEADER_DIGEST_HEX
    empty = hashlib.sha3_256(b"").digest()
    assert empty.hex() == "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a"


_self_test()


def solve_block(block) -> None:
    """Brute-force ``nNonce`` / ``nTime`` until the SPEC-008 digest is under target.

    Mutates ``block`` in place (same role as ``CBlock.solve``).
    """
    from test_framework.messages import uint256_from_compact

    target = uint256_from_compact(block.nBits)
    block.rehash()
    while True:
        if rigor_pow_digest_int(header80_from_block(block)) <= target:
            return
        block.nNonce = (block.nNonce + 1) & 0xFFFFFFFF
        if block.nNonce == 0:
            block.nTime += 1
        block.rehash()


# Temporary alias so existing functional tests keep importing the old name.
solve_block_opus = solve_block

# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""OpuHash (``GetOpusHashFromBaseHash``) for functional tests.

Consensus matches ``src/pow_opus.cpp`` + ``src/crypto/chacha20.cpp`` (ChaCha20
key = 32-byte ``uint256`` memory order, IV/counter zero, 64 KiB keystream).
"""

from __future__ import annotations

import hashlib
import struct
from typing import Final

# Must match consensus/rigor_consensus_lock.h + pow_opus.cpp
OPUS_POW_BUFFER_SIZE: Final[int] = 64 * 1024
OPUS_POW_MIX_ROUNDS: Final[int] = 256
OPUS_POW_BUFFER_MASK: Final[int] = OPUS_POW_BUFFER_SIZE - 1

# CHashWriter(..., PROTOCOL_VERSION) in pow_opus.cpp includes <version.h>
PROTOCOL_VERSION: Final[int] = 70016


def _ser_uint256_le(u: int) -> bytes:
    b = bytearray(32)
    x = u & ((1 << 256) - 1)
    for i in range(8):
        struct.pack_into("<I", b, i * 4, x & 0xFFFFFFFF)
        x >>= 32
    return bytes(b)


def _uint256_from_bytes_le(b: bytes) -> int:
    assert len(b) == 32
    r = 0
    for i, w in enumerate(struct.unpack("<8I", b)):
        r |= int(w) << (32 * i)
    return r


def _hash256(data: bytes) -> bytes:
    return hashlib.sha256(hashlib.sha256(data).digest()).digest()


def _chacha20_keystream(key32: bytes, nbytes: int) -> bytes:
    """Bitcoin ``ChaCha20`` with ``SetIV(0)``, ``Seek64(0)``, same layout as ``crypto/chacha20.cpp``."""

    def rotl32(v: int, c: int) -> int:
        return ((v << c) & 0xFFFFFFFF) | (v >> (32 - c))

    def quarter_round(s, a, b, c, d):
        s[a] = (s[a] + s[b]) & 0xFFFFFFFF
        s[d] = rotl32(s[d] ^ s[a], 16)
        s[c] = (s[c] + s[d]) & 0xFFFFFFFF
        s[b] = rotl32(s[b] ^ s[c], 12)
        s[a] = (s[a] + s[b]) & 0xFFFFFFFF
        s[d] = rotl32(s[d] ^ s[a], 8)
        s[c] = (s[c] + s[d]) & 0xFFFFFFFF
        s[b] = rotl32(s[b] ^ s[c], 7)

    assert len(key32) == 32
    out = bytearray()
    # ChaCha20 state words j4..j15 — see ChaCha20Aligned::Keystream64 in chacha20.cpp
    j4 = struct.unpack("<I", key32[0:4])[0]
    j5 = struct.unpack("<I", key32[4:8])[0]
    j6 = struct.unpack("<I", key32[8:12])[0]
    j7 = struct.unpack("<I", key32[12:16])[0]
    j8 = struct.unpack("<I", key32[16:20])[0]
    j9 = struct.unpack("<I", key32[20:24])[0]
    j10 = struct.unpack("<I", key32[24:28])[0]
    j11 = struct.unpack("<I", key32[28:32])[0]
    j12 = 0
    j13 = 0
    j14 = 0
    j15 = 0

    while len(out) < nbytes:
        x0, x1, x2, x3 = 0x61707865, 0x3320646E, 0x79622D32, 0x6B206574
        x4, x5, x6, x7 = j4, j5, j6, j7
        x8, x9, x10, x11 = j8, j9, j10, j11
        x12, x13, x14, x15 = j12, j13, j14, j15
        for _ in range(10):
            quarter_round(x := [x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15], 0, 4, 8, 12)
            quarter_round(x, 1, 5, 9, 13)
            quarter_round(x, 2, 6, 10, 14)
            quarter_round(x, 3, 7, 11, 15)
            quarter_round(x, 0, 5, 10, 15)
            quarter_round(x, 1, 6, 11, 12)
            quarter_round(x, 2, 7, 8, 13)
            quarter_round(x, 3, 4, 9, 14)
            x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15 = x
        x0 = (x0 + 0x61707865) & 0xFFFFFFFF
        x1 = (x1 + 0x3320646E) & 0xFFFFFFFF
        x2 = (x2 + 0x79622D32) & 0xFFFFFFFF
        x3 = (x3 + 0x6B206574) & 0xFFFFFFFF
        x4 = (x4 + j4) & 0xFFFFFFFF
        x5 = (x5 + j5) & 0xFFFFFFFF
        x6 = (x6 + j6) & 0xFFFFFFFF
        x7 = (x7 + j7) & 0xFFFFFFFF
        x8 = (x8 + j8) & 0xFFFFFFFF
        x9 = (x9 + j9) & 0xFFFFFFFF
        x10 = (x10 + j10) & 0xFFFFFFFF
        x11 = (x11 + j11) & 0xFFFFFFFF
        x12 = (x12 + j12) & 0xFFFFFFFF
        x13 = (x13 + j13) & 0xFFFFFFFF
        x14 = (x14 + j14) & 0xFFFFFFFF
        x15 = (x15 + j15) & 0xFFFFFFFF
        block64 = b"".join(struct.pack("<I", w) for w in (x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15))
        take = min(64, nbytes - len(out))
        out.extend(block64[:take])
        j12 = (j12 + 1) & 0xFFFFFFFF
        if j12 == 0:
            j13 = (j13 + 1) & 0xFFFFFFFF
    return bytes(out[:nbytes])


def _compute_opus_mix(base_u256: int) -> bytes:
    key = _ser_uint256_le(base_u256)
    mem = _chacha20_keystream(key, OPUS_POW_BUFFER_SIZE)
    current = base_u256
    for _rnd in range(OPUS_POW_MIX_ROUNDS):
        val = current & 0xFFFFFFFF
        idx = val & OPUS_POW_BUFFER_MASK
        if idx > OPUS_POW_BUFFER_SIZE - 8:
            idx = OPUS_POW_BUFFER_SIZE - 8
        mem_val = struct.unpack_from("<Q", mem, idx)[0]
        ser = _ser_uint256_le(current) + struct.pack("<Q", mem_val)
        current = _uint256_from_bytes_le(_hash256(ser))
    return _ser_uint256_le(current)


def get_opus_pow_hash_bytes_from_base_uint256(base_u256: int) -> bytes:
    """Return 32-byte consensus OpuHash digest (``GetOpusHashFromBaseHash``)."""
    mixed = _compute_opus_mix(base_u256)
    ser = _ser_uint256_le(base_u256) + mixed
    return _hash256(ser)


def get_opus_pow_hash_int_from_base_uint256(base_u256: int) -> int:
    return _uint256_from_bytes_le(get_opus_pow_hash_bytes_from_base_uint256(base_u256))


def _self_test() -> None:
    # src/test/pow_tests.cpp OpuHash_v3plus_reference_vectors
    b0 = 1
    e0 = int(
        "0cc4ff55b1598a19689b97bd06cdb7c6abe8799bfd5e1554fbe44b606c265780",
        16,
    )
    b1 = (1 << 256) - 1
    e1 = int(
        "4e96acde7248b3483051a3233a261b7e25cbfc41851093d71a2fde437124d4f7",
        16,
    )
    assert get_opus_pow_hash_int_from_base_uint256(b0) == e0
    assert get_opus_pow_hash_int_from_base_uint256(b1) == e1


_self_test()


def solve_block_opus(block) -> None:
    """Brute-force ``nNonce`` / ``nTime`` until ``CheckProofOfWork`` (OpuHash) passes.

    Mutates ``block`` in place (same role as ``CBlock.solve`` on Rigor chains).
    """
    from test_framework.messages import uint256_from_compact

    target = uint256_from_compact(block.nBits)
    block.rehash()
    while True:
        hdr = block.serialize(with_witness=False)[:80]
        base = _uint256_from_bytes_le(_hash256(hdr))
        powh = get_opus_pow_hash_int_from_base_uint256(base)
        if powh <= target:
            return
        block.nNonce = (block.nNonce + 1) & 0xFFFFFFFF
        if block.nNonce == 0:
            block.nTime += 1
        block.rehash()

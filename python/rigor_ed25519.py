# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""libsodium Ed25519 for golden-vector generation. Harness only."""
from __future__ import annotations

import ctypes
import ctypes.util

_PUB = 32
_SEC = 64
_SIG = 64
_SEED = 32

_lib = None


def _sodium() -> ctypes.CDLL:
    global _lib
    if _lib is not None:
        return _lib
    path = ctypes.util.find_library("sodium") or "libsodium.so.23"
    lib = ctypes.CDLL(path)
    if lib.sodium_init() < 0:
        raise RuntimeError("sodium_init failed")
    _lib = lib
    return lib


def seed_to_keypair(seed: bytes) -> tuple[bytes, bytes]:
    if len(seed) != _SEED:
        raise ValueError("seed must be 32 bytes")
    lib = _sodium()
    pk = (ctypes.c_ubyte * _PUB)()
    sk = (ctypes.c_ubyte * _SEC)()
    seed_b = (ctypes.c_ubyte * _SEED).from_buffer_copy(seed)
    if lib.crypto_sign_seed_keypair(pk, sk, seed_b) != 0:
        raise RuntimeError("crypto_sign_seed_keypair failed")
    return bytes(pk), bytes(sk)


def sign_detached(message: bytes, secret_key: bytes) -> bytes:
    if len(secret_key) != _SEC:
        raise ValueError("secret key must be 64 bytes")
    lib = _sodium()
    sig = (ctypes.c_ubyte * _SIG)()
    slen = ctypes.c_ulonglong()
    msg = (ctypes.c_ubyte * len(message)).from_buffer_copy(message) if message else (ctypes.c_ubyte * 0)()
    sk = (ctypes.c_ubyte * _SEC).from_buffer_copy(secret_key)
    if lib.crypto_sign_detached(sig, ctypes.byref(slen), msg, ctypes.c_ulonglong(len(message)), sk) != 0:
        raise RuntimeError("crypto_sign_detached failed")
    return bytes(sig)


def verify_detached(signature: bytes, message: bytes, pubkey: bytes) -> bool:
    if len(signature) != _SIG or len(pubkey) != _PUB:
        return False
    lib = _sodium()
    sig = (ctypes.c_ubyte * _SIG).from_buffer_copy(signature)
    pk = (ctypes.c_ubyte * _PUB).from_buffer_copy(pubkey)
    msg = (ctypes.c_ubyte * len(message)).from_buffer_copy(message) if message else (ctypes.c_ubyte * 0)()
    rc = lib.crypto_sign_verify_detached(sig, msg, ctypes.c_ulonglong(len(message)), pk)
    return rc == 0

# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Ed25519 via libsodium — matches ``src/crypto/ed25519_provider.cpp``.

Witness v0 ORU spends are ``<64-byte sig> <32-byte pk>``. CPubKey on the
descriptor/wallet side is the 33-byte wrapper ``0x00 || pk32``.
"""

from __future__ import annotations

import ctypes
import ctypes.util
import os
from typing import Final

PUBKEY_SIZE: Final[int] = 32
SECRETKEY_SIZE: Final[int] = 64
SEED_SIZE: Final[int] = 32
SIGNATURE_SIZE: Final[int] = 64
# Wallet/descriptor CPubKey: 0x00 || 32-byte payload
WRAPPED_PUBKEY_SIZE: Final[int] = 33
ORUM_PUBKEY_PREFIX: Final[int] = 0x00


def _load_sodium() -> ctypes.CDLL:
    names = []
    found = ctypes.util.find_library("sodium")
    if found:
        names.append(found)
    names.extend(["libsodium.so", "libsodium.so.23", "libsodium.so.26", "libsodium.dylib"])
    last_err = None
    for name in names:
        try:
            lib = ctypes.cdll.LoadLibrary(name)
            if lib.sodium_init() < 0:
                raise OSError("sodium_init failed")
            return lib
        except OSError as e:
            last_err = e
            continue
    raise OSError(
        "libsodium is required for RIGOR functional-test Ed25519 signing "
        f"(tried {names}): {last_err}"
    )


_SODIUM = _load_sodium()
_SODIUM.crypto_sign_seed_keypair.argtypes = [
    ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p,
]
_SODIUM.crypto_sign_seed_keypair.restype = ctypes.c_int
_SODIUM.crypto_sign_detached.argtypes = [
    ctypes.c_char_p, ctypes.POINTER(ctypes.c_ulonglong),
    ctypes.c_char_p, ctypes.c_ulonglong,
    ctypes.c_char_p,
]
_SODIUM.crypto_sign_detached.restype = ctypes.c_int
_SODIUM.crypto_sign_verify_detached.argtypes = [
    ctypes.c_char_p, ctypes.c_char_p, ctypes.c_ulonglong, ctypes.c_char_p,
]
_SODIUM.crypto_sign_verify_detached.restype = ctypes.c_int


def seed_to_keypair(seed: bytes) -> tuple[bytes, bytes]:
    """Return ``(pk32, sk64)`` from a 32-byte seed (``crypto_sign_seed_keypair``)."""
    if len(seed) != SEED_SIZE:
        raise ValueError("Ed25519 seed must be 32 bytes")
    pk = ctypes.create_string_buffer(PUBKEY_SIZE)
    sk = ctypes.create_string_buffer(SECRETKEY_SIZE)
    if _SODIUM.crypto_sign_seed_keypair(pk, sk, seed) != 0:
        raise ValueError("crypto_sign_seed_keypair failed")
    return pk.raw, sk.raw


def sign_detached(message: bytes, secret_key: bytes) -> bytes:
    if len(secret_key) != SECRETKEY_SIZE:
        raise ValueError("Ed25519 secret key must be 64 bytes")
    sig = ctypes.create_string_buffer(SIGNATURE_SIZE)
    if _SODIUM.crypto_sign_detached(sig, None, message, len(message), secret_key) != 0:
        raise ValueError("crypto_sign_detached failed")
    return sig.raw


def verify_detached(signature: bytes, message: bytes, pubkey: bytes) -> bool:
    if len(signature) != SIGNATURE_SIZE or len(pubkey) != PUBKEY_SIZE:
        return False
    return _SODIUM.crypto_sign_verify_detached(signature, message, len(message), pubkey) == 0


def wrap_cpubkey(pk32: bytes) -> bytes:
    if len(pk32) != PUBKEY_SIZE:
        raise ValueError("Ed25519 payload must be 32 bytes")
    return bytes([ORUM_PUBKEY_PREFIX]) + pk32


def payload_from_cpubkey(wrapped: bytes) -> bytes:
    if len(wrapped) == PUBKEY_SIZE:
        return wrapped
    if len(wrapped) == WRAPPED_PUBKEY_SIZE and wrapped[0] == ORUM_PUBKEY_PREFIX:
        return wrapped[1:]
    raise ValueError("expected 32-byte Ed25519 pk or 0x00-wrapped 33-byte CPubKey")


class Ed25519Key:
    """32-byte seed → libsodium Ed25519 keypair. Signing is detached (64 bytes, no hashtype)."""

    def __init__(self, seed: bytes | None = None):
        if seed is None:
            seed = os.urandom(SEED_SIZE)
        if len(seed) != SEED_SIZE:
            raise ValueError("Ed25519 seed must be 32 bytes")
        self._seed = seed
        self._pk32, self._sk64 = seed_to_keypair(seed)

    @classmethod
    def from_seed_int(cls, n: int) -> "Ed25519Key":
        return cls(n.to_bytes(SEED_SIZE, "big"))

    def get_seed(self) -> bytes:
        return self._seed

    def get_pubkey_bytes(self) -> bytes:
        """32-byte witness-stack pubkey."""
        return self._pk32

    def get_cpubkey_bytes(self) -> bytes:
        """33-byte ``0x00 || pk32`` for wpkh() descriptors."""
        return wrap_cpubkey(self._pk32)

    def sign(self, message: bytes) -> bytes:
        return sign_detached(message, self._sk64)

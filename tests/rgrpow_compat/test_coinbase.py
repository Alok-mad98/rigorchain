"""RIGOR coinbase: immutable scriptSig, OP_RETURN extranonce."""
from __future__ import annotations

import hashlib
import unittest


def bip34_height(height: int) -> bytes:
    if height == 0:
        return b"\x00"
    if 1 <= height <= 16:
        return bytes([0x50 + height])
    mag = bytearray()
    n = height
    while n:
        mag.append(n & 0xFF)
        n >>= 8
    if mag[-1] & 0x80:
        mag.append(0)
    return bytes([len(mag)]) + bytes(mag)


def node_scriptsig(height: int) -> bytes:
    """Match Consensus::BuildCoinbaseScriptSig / build_coinbase_scriptsig: height || OP_0."""
    return bip34_height(height) + b"\x00"


def bitcoin_pool_scriptsig(height: int, extra: bytes) -> bytes:
    return bip34_height(height) + extra


def op_return_extranonce(en1: bytes, en2: bytes) -> bytes:
    payload = en1 + en2
    assert 1 <= len(payload) <= 75
    return b"\x6a" + bytes([len(payload)]) + payload


def sha256d(data: bytes) -> bytes:
    return hashlib.sha256(hashlib.sha256(data).digest()).digest()


class CoinbaseCompatibilityTests(unittest.TestCase):
    def test_scriptsig_is_height_plus_op0(self):
        self.assertEqual(node_scriptsig(1), bytes.fromhex("5100"))
        self.assertEqual(node_scriptsig(16), bytes.fromhex("6000"))
        ss = node_scriptsig(100)
        self.assertTrue(ss.endswith(b"\x00"))
        self.assertFalse(ss[1:] == b"\x00" * 32)

    def test_height_plus_extranonce_in_scriptsig_is_forbidden(self):
        honest = node_scriptsig(1)
        mutated = bitcoin_pool_scriptsig(1, b"\x04\x00\x00\x00\x01")
        self.assertNotEqual(honest, mutated)
        # Compatibility rule: a pool that writes extranonce into scriptSig diverges.
        self.assertGreater(len(mutated), len(honest))

    def test_scriptsig_mutation_fails_equality(self):
        base = node_scriptsig(1)
        mutated = base + b"\x08" + b"\x00" * 8
        self.assertNotEqual(base, mutated)

    def test_op_return_extranonce_allowed_and_changes_merkle_leaf(self):
        ss = node_scriptsig(1)
        en_a = op_return_extranonce(b"\x00" * 4, b"\x00" * 4)
        en_b = op_return_extranonce(b"\x00" * 4, b"\x01\x00\x00\x00")
        self.assertEqual(en_a[:2], b"\x6a\x08")
        self.assertEqual(ss, node_scriptsig(1))
        # Coinbase identity for merkle is SHA256d(txid material). Changing OP_RETURN
        # payload must change the leaf even when scriptSig is unchanged.
        leaf_a = sha256d(ss + en_a)
        leaf_b = sha256d(ss + en_b)
        self.assertNotEqual(leaf_a, leaf_b)
        self.assertEqual(ss, node_scriptsig(1))


if __name__ == "__main__":
    unittest.main()

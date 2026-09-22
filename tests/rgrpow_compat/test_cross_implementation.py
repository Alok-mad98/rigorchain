"""Cross-check C++ kernel, miner C, Python, and Rust share-validator construction."""
from __future__ import annotations

import unittest

from rgrpow_lib import hash_c, hash_cpp, hash_rust, opuhash, vector_by_id


class CrossImplementationTests(unittest.TestCase):
    def _check(self, vid: str):
        v = vector_by_id(vid)
        header = bytes.fromhex(v["header_hex"])
        want = bytes.fromhex(v["digest_hex"])
        py = opuhash(header)
        c = hash_c(header)
        cpp = hash_cpp(header)
        diffs = []
        if py != want:
            diffs.append(f"python={py.hex()}")
        if c != want:
            diffs.append(f"miner_c={c.hex()}")
        if cpp != want:
            diffs.append(f"kernel_cpp={cpp.hex()}")
        rust = hash_rust(header)
        if rust is not None and rust != want:
            diffs.append(f"rust={rust.hex()}")
        if diffs:
            self.fail(
                f"vector {vid} consensus mismatch want={want.hex()} " + " ".join(diffs)
            )
        return rust is not None

    def test_vector_001(self):
        self._check("001")

    def test_vector_002(self):
        self._check("002")

    def test_vector_003(self):
        self._check("003")

    def test_all_implementations_agree(self):
        rust_ok = True
        for vid in ("001", "002", "003"):
            rust_ok = self._check(vid) and rust_ok
        header = bytes.fromhex(vector_by_id("001")["header_hex"])
        self.assertEqual(hash_c(header), hash_cpp(header))
        self.assertEqual(hash_c(header), opuhash(header))
        rust = hash_rust(header)
        if rust is None:
            self.skipTest("Rust hash80_rust CLI not built (cargo unavailable)")
        self.assertEqual(rust, hash_cpp(header))


if __name__ == "__main__":
    unittest.main()

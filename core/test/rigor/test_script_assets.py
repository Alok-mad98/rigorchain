#!/usr/bin/env python3
import unittest

from rigor_model import (
    CVG2,
    ORB2,
    SCRIPT_OP_RETURN,
    SCRIPT_WITNESS_V1,
    Script,
    is_asset_script,
    is_convergence_script,
    is_res_mint_script,
)


class TestScriptAssets(unittest.TestCase):
    """Layer 1: script/asset invariants only."""

    def test_cvg2_and_orb2_are_distinct_asset_types(self):
        convergence = Script(SCRIPT_WITNESS_V1, CVG2 + bytes(28))
        res_asset = Script(SCRIPT_WITNESS_V1, ORB2 + bytes(32))
        self.assertTrue(is_convergence_script(convergence))
        self.assertFalse(is_res_mint_script(convergence))
        self.assertTrue(is_res_mint_script(res_asset))
        self.assertFalse(is_convergence_script(res_asset))

    def test_op_return_is_not_asset(self):
        opret = Script(SCRIPT_OP_RETURN, b"NOTE" + bytes(32))
        self.assertFalse(is_asset_script(opret))


if __name__ == "__main__":
    unittest.main()

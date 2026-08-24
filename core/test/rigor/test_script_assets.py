#!/usr/bin/env python3
import unittest

from rigor_model import (
    RES2,
    SCRIPT_OP_CONVERGE,
    SCRIPT_OP_RETURN,
    SCRIPT_WITNESS_V1,
    Script,
    is_asset_script,
    is_convergence_script,
    is_res_mint_script,
)


class TestScriptAssets(unittest.TestCase):
    """Layer 1: script/asset invariants only."""

    def test_converge_and_res_are_distinct_asset_types(self):
        convergence = Script(SCRIPT_OP_CONVERGE, bytes(20))
        res_prefixed = Script(SCRIPT_WITNESS_V1, RES2 + bytes(28))
        res_native = Script(SCRIPT_WITNESS_V1, bytes(32))
        stale_36 = Script(SCRIPT_WITNESS_V1, RES2 + bytes(32))
        self.assertTrue(is_convergence_script(convergence))
        self.assertFalse(is_res_mint_script(convergence))
        self.assertFalse(is_res_mint_script(res_prefixed))
        self.assertTrue(is_res_mint_script(res_native))
        self.assertFalse(is_convergence_script(res_prefixed))
        self.assertFalse(is_convergence_script(res_native))
        self.assertFalse(is_res_mint_script(stale_36))

    def test_op_return_is_not_asset(self):
        opret = Script(SCRIPT_OP_RETURN, b"NOTE" + bytes(32))
        self.assertFalse(is_asset_script(opret))


if __name__ == "__main__":
    unittest.main()

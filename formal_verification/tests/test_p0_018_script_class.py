# Copyright (c) 2026 The RigorChain developers
"""P0-018 M-3: Python spec classifier matches C++ ClassifyRigorTxout admit-set."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "spec"))
from rigor_consensus_spec import (  # noqa: E402
    asset_from_script,
    get_res_mint_script,
    is_allowed_script,
    is_converge_script,
    is_res_mint_script,
    is_res_script,
)


def _oru():
    return bytes([0x00, 0x14]) + bytes([0x33]) * 20


def _res():
    return bytes([0x51, 0x20]) + bytes([0x44]) * 32


def _mint():
    return get_res_mint_script(100, bytes([0x55]) * 20)


def _converge():
    return bytes([0x14]) + bytes([0x66]) * 20 + bytes([0xBD])


def _cvg2():
    return bytes([0x51, 0x20]) + b"CVG2" + bytes(28)


def _res2():
    return bytes([0x51, 0x20]) + b"RES2" + bytes(28)


def test_p0_018_classifier_corpus():
    assert is_allowed_script(_oru()) and not is_res_script(_oru())
    assert asset_from_script(_oru()) == "ORU"

    assert is_allowed_script(_res()) and is_res_script(_res())
    assert asset_from_script(_res()) == "RES"

    assert is_res_mint_script(_mint())
    assert is_allowed_script(_mint()) and is_res_script(_mint())
    assert asset_from_script(_mint()) == "RES"

    assert is_converge_script(_converge())
    assert is_allowed_script(_converge()) and not is_res_script(_converge())
    assert asset_from_script(_converge()) == "ORU"

    assert not is_allowed_script(_cvg2())
    assert asset_from_script(_cvg2()) == "INVALID"

    assert not is_allowed_script(_res2())
    assert not is_res_script(_res2())
    assert asset_from_script(_res2()) == "INVALID"

    p2wsh = bytes([0x00, 0x20]) + bytes(32)
    assert not is_allowed_script(p2wsh)
    assert asset_from_script(p2wsh) == "INVALID"

    p2pkh = bytes([0x76, 0xA9, 0x14]) + bytes(20) + bytes([0x88, 0xAC])
    assert not is_allowed_script(p2pkh)
    assert asset_from_script(p2pkh) == "INVALID"

    opreturn = bytes([0x6A, 0x01, 0x78])
    assert not is_allowed_script(opreturn)
    assert asset_from_script(opreturn) == "NONE"

    opreturn_dup = bytes([0x6A, 0x76])  # OP_RETURN OP_DUP: not push-only
    assert not is_allowed_script(opreturn_dup)
    assert asset_from_script(opreturn_dup) == "INVALID"

    from rigor_consensus_spec import classify_script_reject, created_script_shape_ok
    assert classify_script_reject(p2pkh, "INVALID") is None
    assert classify_script_reject(_oru(), "INVALID") == "asset_tag_mismatch"
    assert asset_from_script(b"") == "INVALID"
    assert created_script_shape_ok({"value": 1, "script": []}) is False
    assert created_script_shape_ok({"value": 0, "script": [0x6A]}) is True

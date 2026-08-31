#!/usr/bin/env python3
"""PHASE-3 oracle: Python spec vs phase3_golden_vectors.json."""

from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent / "spec"))
import rigor_consensus_spec as spec  # noqa: E402

VECTORS = Path(__file__).parent.parent / "phase3_golden_vectors.json"

# Families whose shape changed with SECURITY-001-A8. A vectors file still carrying the
# pre-A-8 shape is not a spec failure but a stale artifact, and is reported as such.
STALE_MODULES = {"SigmaCR01"}
STALE_KEYS = {"target_prev", "bind_hex"}


def decode_txids(inp: dict) -> list[bytes]:
    if inp.get("encoding") == "sethex":
        return [spec.parse_hex256_sethex(h) for h in inp["txids_hex"]]
    return [bytes.fromhex(h) for h in inp["txids_hex"]]


def check_vector(vec: dict) -> tuple[bool, str]:
    vid = vec["id"]
    mod = vec["module"]
    inp = vec["input"]
    exp = vec["expected"]

    if mod == "ConvergenceRoot":
        got = spec.compute_convergence_root_from_txids(decode_txids(inp)).hex()
        return got == exp["convergence_root"], f"{vid} root"

    if mod == "DrawCandidateRoot":
        kind = inp.get("kind", "flags")
        if kind == "incomplete":
            return exp["draw_candidate_root"] is None, f"{vid} dcr-incomplete"
        if kind == "genesis":
            got = spec.convergence_draw_eligible_root([], []).hex()
            return got == exp["draw_candidate_root"], f"{vid} dcr-genesis"
        if kind == "exclusion":
            cands = [bytes.fromhex(h) for h in inp["candidates_hex"]]
            paid = [bytes.fromhex(h) for h in inp["already_paid_hex"]]
            surv = spec.exclude_already_paid(cands, paid)
            got = spec.convergence_draw_eligible_root(surv, [True] * len(surv)).hex()
            return got == exp["draw_candidate_root"], f"{vid} dcr-exclusion"
        if kind == "e2e":
            txids = decode_txids(inp)
            fees = inp["fees"]
            prev = bytes.fromhex(inp["prev_block_hash_hex"])
            t_int = spec.res_probability_target_from_bytes(bytes.fromhex(inp["T_hex"]))
            paid = [bytes.fromhex(h) for h in inp.get("already_paid_hex", [])]
            work, work_fees = txids, fees
            if paid:
                work = spec.exclude_already_paid(txids, paid)
                work_fees = [fees[txids.index(x)] for x in work]
            r = spec.run_selection(work, work_fees, inp["height"], prev, t_int)
            dcr = spec.convergence_draw_eligible_root(work, r.eligible).hex()
            if r.winner_index is None or r.winner_index < 0:
                orig_win = -1 if r.winner_index is None else r.winner_index
            else:
                orig_win = txids.index(work[r.winner_index])
            ok = (
                dcr == exp["draw_candidate_root"]
                and r.convergence_root.hex() == exp["convergence_root"]
                and r.eligible == exp["eligible"]
                and orig_win == exp["winner_index"]
            )
            return ok, f"{vid} dcr-e2e"
        if kind == "contrast":
            a = spec.convergence_draw_eligible_root(
                [bytes.fromhex(h) for h in inp["txids_hex"]], inp["eligible"]
            )
            b = spec.convergence_draw_eligible_root(
                [bytes.fromhex(h) for h in inp["other_txids_hex"]], inp["other_eligible"]
            )
            ok = (
                a.hex() == exp["draw_candidate_root"]
                and b.hex() == exp["other_draw_candidate_root"]
                and (a != b) is exp["differ"] is True
            )
            return ok, f"{vid} dcr-contrast"
        if kind == "shape":
            txids = decode_txids(inp)
            got = spec.convergence_draw_eligible_root(txids, inp["eligible"])
            ok = got.hex() == exp["draw_candidate_root"]
            if inp.get("neq_zero") or exp.get("neq_zero"):
                ok = ok and got != bytes(32)
            if inp.get("neq_c") or exp.get("neq_c"):
                ok = ok and got != spec.compute_convergence_root_from_txids(txids)
            return ok, f"{vid} dcr-shape"
        if kind == "e2e_mismatch":
            txids = decode_txids(inp)
            fees = inp["fees"]
            prev = bytes.fromhex(inp["prev_block_hash_hex"])
            t_int = spec.res_probability_target_from_bytes(bytes.fromhex(inp["T_hex"]))
            paid = [bytes.fromhex(h) for h in inp.get("already_paid_hex", [])]
            work = spec.exclude_already_paid(txids, paid)
            work_fees = [fees[txids.index(x)] for x in work]
            r = spec.run_selection(work, work_fees, inp["height"], prev, t_int)
            r_wrong = spec.run_selection(txids, fees, inp["height"], prev, t_int)
            dcr = spec.convergence_draw_eligible_root(work, r.eligible)
            dcr_w = spec.convergence_draw_eligible_root(txids, r_wrong.eligible)
            orig_win = -1 if r.winner_index is None or r.winner_index < 0 else txids.index(work[r.winner_index])
            ok = (
                dcr.hex() == exp["draw_candidate_root"]
                and r.convergence_root.hex() == exp["convergence_root"]
                and r.eligible == exp["eligible"]
                and orig_win == exp["winner_index"]
                and dcr_w.hex() == exp["wrong_draw_candidate_root"]
                and r_wrong.winner_index == exp["wrong_winner_index"]
                and (dcr != dcr_w and orig_win != r_wrong.winner_index) is exp["differ"] is True
            )
            return ok, f"{vid} dcr-e2e-mismatch"
        if kind == "sigma_perturb":
            txids = decode_txids(inp)
            fees = inp["fees"]
            prev = bytes.fromhex(inp["prev_block_hash_hex"])
            t_int = spec.res_probability_target_from_bytes(bytes.fromhex(inp["T_hex"]))
            r = spec.run_selection(txids, fees, inp["height"], prev, t_int)
            dcr = spec.convergence_draw_eligible_root(txids, r.eligible)
            alt = inp["alt"]
            alt_prev = bytes.fromhex(alt["prev_block_hash_hex"]) if "prev_block_hash_hex" in alt else prev
            alt_h = int(alt.get("height", inp["height"]))
            if "convergence_root_hex" in alt:
                r_alt = spec.compute_convergence_draw_selection(
                    txids, fees, bytes.fromhex(alt["convergence_root_hex"]), alt_prev, alt_h, t_int
                )
            else:
                r_alt = spec.run_selection(txids, fees, alt_h, alt_prev, t_int)
            ok = (
                dcr.hex() == exp["draw_candidate_root"]
                and r.convergence_root.hex() == exp["convergence_root"]
                and r.eligible == exp["eligible"]
                and r.winner_index == exp["winner_index"]
                and r_alt.winner_index == exp["alt_winner_index"]
                and (r.winner_index != r_alt.winner_index) is exp["differ"] is True
            )
            return ok, f"{vid} dcr-sigma-perturb"
        txids = decode_txids(inp)
        elig = inp["eligible"]
        got = spec.convergence_draw_eligible_root(txids, elig).hex()
        return got == exp["draw_candidate_root"], f"{vid} dcr"

    if mod == "SigmaPoW01":
        got = spec.compute_pcobr_hash(
            bytes.fromhex(inp["txid_hex"]),
            bytes.fromhex(inp["prev_block_hash_hex"]),
            inp["height"],
            bytes.fromhex(inp["convergence_root_hex"]),
        ).hex()
        return got == exp["sigma"], f"{vid} sigma"

    if mod == "Inequality":
        h = spec.hash_hex_to_uint256_bytes(inp["h_hex"])
        t_int = spec.res_probability_target_from_bytes(bytes.fromhex(inp["T_hex"]))
        got = spec.check_inequality(h, t_int, inp["f"])
        return got == exp["eligible"], f"{vid} inq"

    if mod == "RunSelection":
        txids = [spec.parse_hex256_sethex(h) for h in inp["txids_hex"]]
        prev_block_hash = spec.parse_hex256_sethex(inp["prev_block_hash_hex"])
        t_int = spec.res_probability_target_from_bytes(bytes.fromhex(inp["T_hex"]))
        r = spec.run_selection(txids, inp["fees"], inp["height"], prev_block_hash, t_int)
        got = {
            "convergence_root": r.convergence_root.hex(),
            "sigmas": [s.hex() for s in r.sigma_hashes],
            "eligible": r.eligible,
            "winner_index": r.winner_index,
        }
        return got == exp, f"{vid} run"

    if mod == "Replay":
        # Flipping one seed bit must move σ: a draw is bound to exactly one parent.
        base = spec.parse_hex256_sethex("aa" * 32)
        flipped = bytearray(base)
        bit = inp["seed_bit"]
        flipped[bit // 8] ^= 1 << (bit % 8)
        txid0 = spec.create_test_txid(0)
        differ = spec.compute_pcobr_hash(txid0, base, 50, bytes(32)) != spec.compute_pcobr_hash(
            txid0, bytes(flipped), 50, bytes(32)
        )
        return differ is exp["differ"] is True, f"{vid} replay"

    if mod == "ReorgBind":
        # The seed is the parent hash at depth 1 for every payout height >= 1; there is no
        # bootstrap window in which σ would take a substituted preimage.
        if exp["seed_depth"] != spec.CONVERGENCE_DRAW_SEED_DEPTH:
            return False, f"{vid} seed depth"
        got = spec.compute_pcobr_hash(
            bytes.fromhex(inp["txid_hex"]),
            bytes.fromhex(inp["prev_block_hash_hex"]),
            inp["payout_height"],
            bytes(32),
        ).hex()
        return got == exp["sigma"], f"{vid} seed bind"

    if mod == "WindowExclusion":
        cands = [bytes.fromhex(h) for h in inp["candidates_hex"]]
        paid = [bytes.fromhex(h) for h in inp["already_paid_hex"]]
        got = [x.hex() for x in spec.exclude_already_paid(cands, paid)]
        return got == exp["surviving_hex"] and (len(cands) - len(got)) == exp["dropped"], f"{vid} a9c"

    if mod == "SigmaEncoder":
        pre = spec.encode_sigma_preimage(
            bytes.fromhex(inp["txid_hex"]),
            bytes.fromhex(inp["prev_block_hash_hex"]),
            inp["height"],
            bytes.fromhex(inp["convergence_root_hex"]),
        )
        ok = (
            pre.hex() == exp["preimage_hex"]
            and len(pre) == exp["length"] == 100
            and pre[0:32].hex() == exp["txid"]
            and pre[32:64].hex() == exp["croot"]
            and pre[64:96].hex() == exp["prev"]
            and pre[96:100].hex() == exp["height_le"]
            and spec.sha256d(pre).hex() == exp["sigma"]
        )
        return ok, f"{vid} encoder"

    if mod == "WindowIncomplete":
        if inp["complete"] is False:
            return exp["draw"] is None, f"{vid} incomplete"
        return exp["draw"] == "empty_ok", f"{vid} genesis"

    if mod == "HashKernel":
        got = spec.sha256d(bytes.fromhex(inp["payload_hex"])).hex()
        return got == exp["sha256d"], f"{vid} hash"

    if mod == "Sha256Fips":
        import hashlib
        got = hashlib.sha256(bytes.fromhex(inp["payload_hex"])).hexdigest()
        return got == exp["sha256"], f"{vid} fips"

    if mod == "SupplyScaffold":
        # Was rubber-stamped as doc-only. RFC-017 made the mint amount a function of height, so
        # there is now something worth checking: that the vector's RES_after agrees with the
        # scheduled reward at the vector's own height, under the cap rule.
        w = spec.get_res_mint_reward(inp["height"])
        res = inp["RES"]
        after = res + (w if inp["mint"] and res + w <= spec.RES_MAX_SUPPLY else 0)
        return after == exp["RES_after"], f"{vid} supply"

    if mod == "NegativeCore":
        if exp.get("accepted") is not False:
            return False, f"{vid} expected.accepted must be false"
        try:
            got = spec.eval_negative_core(inp)
        except (KeyError, ValueError, TypeError) as exc:
            return False, f"{vid} eval error: {exc}"
        ok = (
            got.get("accepted") is False
            and got.get("reject") == exp.get("reject")
            and got.get("W") == exp.get("W")
        )
        return ok, f"{vid} neg {got.get('reject')}"

    if mod == "InvalidCvg":
        if exp.get("accepted") is not False:
            return False, f"{vid} expected.accepted must be false"
        try:
            got = spec.eval_invalid_cvg(inp)
        except (KeyError, ValueError, TypeError) as exc:
            return False, f"{vid} eval error: {exc}"
        ok = got.get("accepted") is False and got.get("reject") == exp.get("reject")
        for key in ("convergence_root", "bad_digest", "collapsed_root"):
            if key in exp:
                ok = ok and got.get(key) == exp.get(key)
        return ok, f"{vid} icvg {got.get('reject')}"

    if mod == "ScriptMintNeg":
        if exp.get("accepted") is not False:
            return False, f"{vid} expected.accepted must be false"
        try:
            got = spec.eval_script_mint_neg(inp)
        except (KeyError, ValueError, TypeError) as exc:
            return False, f"{vid} eval error: {exc}"
        ok = got.get("accepted") is False and got.get("reject") == exp.get("reject")
        for key in ("is_allowed", "is_res", "asset", "dual_pool_reject", "mint_shape_would_pass", "W"):
            if key in exp:
                ok = ok and got.get(key) == exp.get(key)
        return ok, f"{vid} smw {got.get('reject')}"

    if mod == "ScriptAsset":
        try:
            got = spec.eval_script_asset(inp)
        except (KeyError, ValueError, TypeError) as exc:
            return False, f"{vid} eval error: {exc}"
        ok = (
            got.get("asset") == exp.get("asset")
            and got.get("is_allowed") == exp.get("is_allowed")
            and got.get("is_res") == exp.get("is_res")
        )
        return ok, f"{vid} sass {got.get('asset')}"

    if mod == "EdgeCross":
        if exp.get("accepted") is not False:
            return False, f"{vid} expected.accepted must be false"
        try:
            got = spec.eval_edge_cross(inp)
        except (KeyError, ValueError, TypeError) as exc:
            return False, f"{vid} eval error: {exc}"
        ok = got.get("accepted") is False and got.get("reject") == exp.get("reject")
        for key in (
            "W",
            "W_neighbour",
            "dual_pool_reject",
            "shape_reject",
            "root_reject",
            "mint_reject",
            "cap_reject",
            "convergence_root",
            "bad_digest",
            "draw_candidate_root",
            "surviving_hex",
            "dropped",
            "winner_index",
            "full_winner_index",
            "c_is_zero",
            "dcr_neq_zero",
            "dcr_neq_c",
            "obligation_reject",
            "draw_won",
            "mint_count",
        ):
            if key in exp:
                ok = ok and got.get(key) == exp.get(key)
        return ok, f"{vid} xfam {got.get('reject')}"

    if mod == "BlockTrace":
        return True, f"{vid} doc-only"

    return False, f"{vid} unknown module {mod}"


def stale_reason(vec: dict) -> str | None:
    if vec["module"] in STALE_MODULES:
        return f"module {vec['module']} was renamed to SigmaPoW01"
    present = STALE_KEYS & set(vec["input"])
    if present:
        return f"input carries pre-A-8 key(s) {sorted(present)}"
    return None


def main() -> None:
    if not VECTORS.is_file():
        print(f"Missing {VECTORS}; run generate_phase3_vectors.py first")
        sys.exit(2)
    data = json.loads(VECTORS.read_text())

    lock_path = Path(__file__).parent.parent / "golden_lock.json"
    lock = json.loads(lock_path.read_text())
    # Fail-closed on the living lock before evaluating (oracle does not rewrite
    # artifacts; generators must still refuse to write on the same mismatch).
    if data.get("tag") != lock["tag"]:
        print(f"FAIL golden lock: JSON tag {data.get('tag')!r} != lock {lock['tag']!r}")
        sys.exit(1)
    if data.get("count") != lock["shared_vectors"] or len(data["vectors"]) != lock["shared_vectors"]:
        print(
            f"FAIL golden lock: oracle saw count={data.get('count')} "
            f"len={len(data['vectors'])}, lock wants {lock['shared_vectors']}"
        )
        sys.exit(1)
    by_mod = {}
    for vec in data["vectors"]:
        by_mod[vec["module"]] = by_mod.get(vec["module"], 0) + 1
    lock_failed = 0
    for fam, fam_spec in lock["must_enforce_families"].items():
        got = by_mod.get(fam, 0)
        if got != fam_spec["vectors"]:
            print(f"FAIL golden lock: {fam} has {got} rows, lock wants {fam_spec['vectors']}")
            lock_failed += 1
    vec_by_id = {v["id"]: v for v in data["vectors"]}
    flag_true = sum(1 for v in data["vectors"] if v.get("coq_enforce") is True)
    if flag_true != lock["json_coq_enforce_true"]:
        print(
            f"FAIL golden lock: coq_enforce:true = {flag_true} != lock "
            f"{lock['json_coq_enforce_true']}"
        )
        lock_failed += 1
    for vid in lock["surface_splits"]["omit_coq_enforce_key_enforced"]:
        if vid not in vec_by_id:
            print(f"FAIL golden lock: omit-key id missing: {vid}")
            lock_failed += 1
        elif "coq_enforce" in vec_by_id[vid]:
            print(f"FAIL golden lock: {vid} must omit coq_enforce")
            lock_failed += 1
    for split in lock["surface_splits"]["rows"]:
        for vid in split["ids"]:
            if vid not in vec_by_id:
                print(f"FAIL golden lock: surface_split missing: {vid}")
                lock_failed += 1
            elif vec_by_id[vid]["module"] != split["family"]:
                print(
                    f"FAIL golden lock: {vid} module {vec_by_id[vid]['module']!r} "
                    f"!= {split['family']}"
                )
                lock_failed += 1
            elif vec_by_id[vid].get("coq_enforce") is False:
                print(f"FAIL golden lock: surface_split {vid} has coq_enforce:false")
                lock_failed += 1
    if lock_failed:
        sys.exit(1)

    stale = [(v["id"], r) for v in data["vectors"] if (r := stale_reason(v))]
    if stale:
        print(f"STALE {VECTORS.name}: predates SECURITY-001-A8 (Sigma-PoW-01)")
        for vid, reason in stale[:10]:
            print(f"  {vid}: {reason}")
        if len(stale) > 10:
            print(f"  ... and {len(stale) - 10} more")
        print("Regenerate with: python3 generate_phase3_vectors.py")
        sys.exit(3)

    failed = 0
    for vec in data["vectors"]:
        ok, msg = check_vector(vec)
        if not ok:
            print(f"FAIL {msg}")
            failed += 1
    print(f"PHASE-3 oracle: {len(data['vectors']) - failed}/{len(data['vectors'])} passed")
    if failed:
        sys.exit(1)


if __name__ == "__main__":
    main()

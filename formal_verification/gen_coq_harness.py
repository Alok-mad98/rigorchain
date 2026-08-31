#!/usr/bin/env python3
"""PHASE-4A1: generate a Coq vm_compute harness that runs the Coq consensus
kernel against the PHASE-3 golden vectors. Read-only w.r.t. all consensus code;
emits Kernel/GoldenHarness.v, which is part of the _CoqProject build.

Each vector is emitted as a `Lemma ... = true` closed by `vm_compute; reflexivity`,
so a vector that stops reproducing fails `make` rather than merely printing
`false` into build output that nobody reads.

Executable kernel surface:
  ConvergenceRoot -> convergence_root_from_txid_bytes
  DrawCandidateRoot -> draw_candidate_root_from_flags / e2e / contrast / shape /
                       e2e_mismatch / sigma_perturb (no N<2 shortcut; negatives
                       lock tamper, dup, exclusion mismatch, σ C/B/h change)
  InvalidCvg      -> classify_cvg_tx_reject / committed C / malformed preimage /
                       dup txid / empty-or-wrong C claim (P0-4 Convergence/Root rejects)
  ScriptMintNeg   -> is_res_script / is_allowed_script / PCORB stack shape /
                       spend-CVG2 (P1-1 abstract form; shape lift on apply_tx
                       is ReplayReorgCore/ScriptShape.v, not this harness)
  SigmaPoW01      -> sigma_bytes
  Inequality      -> uint256_of_bytes(h) <? T*f   (eligibility predicate)
  RunSelection    -> build_F / Sigma / eligible / select_winner  (full draw)
  EdgeCross      -> script+dual-pool / PCORB+dual-pool / schedule+mint/cap
                       (NegativeCoreHarness); malformed-root+mint/cap,
                       window+DCR+selection, and window+obligation
                       (GoldenHarness). TASK-FV-VECTOR-EDGE-AND-CROSS-FAMILY-001
                       + TASK-FV-VECTOR-SELECTIVE-DENSIFY-001.
Other modules (Replay, ReorgBind, SupplyScaffold, BlockTrace) are higher-layer
scaffolds with no executable kernel definition -> reported BLOCKED (out of scope).

NegativeCore is a cheap ReplayReorgCore family (dual-pool / mint-shape / cap / H-1
rejects). It is emitted to Kernel/NegativeCoreHarness.v so TxAbs record names do
not clash with Types.Tx in GoldenHarness.v. EdgeCross script/schedule rows share
that unit.

The vectors file may be given as argv[1]; it defaults to the checked-in PHASE-3 set.
"""
import json, os, sys
sys.path.insert(0, "spec")
import rigor_consensus_spec as spec

VEC = sys.argv[1] if len(sys.argv) > 1 else "phase3_golden_vectors.json"
OUT = os.environ.get("HARNESS_OUT", "coq/Kernel/GoldenHarness.v")
NEG_OUT = os.environ.get("NEG_HARNESS_OUT", "coq/Kernel/NegativeCoreHarness.v")
INDEX = os.environ.get("HARNESS_INDEX", "coq_harness_index.json")

def hexbytes(h):
    """hex string -> Coq 'list byte' literal of nat values."""
    if h is None:
        return "[]"
    b = bytes.fromhex(h)
    return "[" + ";".join(str(x) for x in b) + "]%nat"

def list_hexbytes(hs):
    return "[" + ";".join(hexbytes(h) for h in hs) + "]"

def sanitize(vid):
    """Vector id -> Coq-legal identifier fragment."""
    return "".join(c if c.isalnum() else "_" for c in vid)

def emit_check(P, idx, vid, expr):
    """Emit a build-enforcing assertion that `expr` evaluates to true."""
    P(f"Definition chk_{idx} : bool := {expr}.")
    P(f"Lemma golden_{sanitize(vid)} : chk_{idx} = true.")
    # vm_cast_no_check defers the single conversion check to the kernel's VM,
    # avoiding the duplicate evaluation that `vm_compute; reflexivity` incurs.
    P(f"Proof. vm_cast_no_check (eq_refl true). Qed.")

def coq_asset(a):
    return {"ORU": "AssetORU", "RES": "AssetRES", "NONE": "AssetNone"}[a]

def coq_out(o):
    spk = o.get("script_hex")
    if spk:
        return (
            f"(neg_out_spk ({int(o['value'])}) {coq_asset(o['asset'])} {hexbytes(spk)})"
        )
    return f"(neg_out ({int(o['value'])}) {coq_asset(o['asset'])})"

def coq_utxo(utxo):
    if not utxo:
        return "(@nil (OutPoint * TxOut))"
    items = "; ".join(
        f"(MkOutPoint (neg_h32 {int(e['id'])}%nat) {int(e['vout'])}%nat, "
        f"neg_out ({int(e['value'])}) {coq_asset(e['asset'])})"
        for e in utxo
    )
    return f"[ {items} ]"

def coq_tx(tx):
    spends = "; ".join(
        f"MkOutPoint (neg_h32 {int(s['id'])}%nat) {int(s['vout'])}%nat"
        for s in tx["spends"]
    )
    creates = "; ".join(coq_out(c) for c in tx["creates"])
    stack = tx.get("stack_hex")
    pcorb = f"Some {coq_hex_stack(stack)}" if stack else "None"
    return (
        f"{{| tx_id := neg_h32 {int(tx['id'])}%nat; "
        f"tx_spends := [{spends}]; "
        f"tx_creates := [{creates}]; "
        f"tx_pcorb := {pcorb} |}}"
    )

def coq_state(inp):
    height = int(inp["height"])
    supply = int(inp.get("res_supply", 0))
    utxo = coq_utxo(inp.get("utxo") or [])
    return (
        f"{{| rs_utxo := {utxo}; "
        f"rs_chain := {{| cm_height := {height - 1}%nat; cm_chainwork := 0; "
        f"cm_tip_hash := hash32_zero |}}; "
        f"rs_oru_supply := 0; rs_res_supply := {supply}; "
        f"rs_history := (@nil ReplaySnapshot) |}}"
    )

def coq_block(inp):
    height = int(inp["height"])
    won = "true" if inp.get("draw_won", False) else "false"
    n = int(inp.get("mint_count", 0))
    txs = inp.get("txs") or []
    txs_c = "[" + "; ".join(coq_tx(t) for t in txs) + "]" if txs else "(@nil TxAbs)"
    return (
        f"{{| blk_header := {{| hdr_height := {height}%nat; "
        f"hdr_prev_hash := hash32_zero; hdr_chainwork := 1; "
        f"hdr_nbits := 0; hdr_time := 0 |}}; "
        f"blk_txs := {txs_c}; "
        f"blk_mint := {{| cm_draw_won := {won}; cm_oru_subsidy := 0; "
        f"cm_res_mint_count := {n}%nat |}} |}}"
    )

def coq_ins_list(utxo, tx):
    by = {(int(e["id"]), int(e["vout"])): e for e in utxo}
    ins = []
    for s in tx["spends"]:
        e = by[(int(s["id"]), int(s["vout"]))]
        row = {"value": e["value"], "asset": e["asset"]}
        if "script_hex" in e:
            row["script_hex"] = e["script_hex"]
        ins.append(row)
    if not ins:
        return "(@nil TxOut)"
    return "[" + "; ".join(coq_out(o) for o in ins) + "]"

ICVG_REJECT_COQ = {
    "not_cvg2_script": "CvgTxNotScript",
    "amount_zero": "CvgTxAmountZero",
    "cvg_count": "CvgTxCount",
    "wrong_asset": "CvgTxWrongAsset",
    "amount_below_min": "CvgTxAmountBelowMin",
}

SMW_REJECT_COQ = {
    "bad_rigor_script_type": "SmwBadScriptType",
    "not_res_script": "SmwNotResScript",
    "not_oru_script": "SmwNotOruScript",
    "asset_tag_mismatch": "SmwAssetTagMismatch",
    "spend_cvg2": "SmwSpendCvg2",
    "pcorb_too_few": "SmwPcorbTooFew",
    "pcorb_field_size": "SmwPcorbFieldSize",
}

SMW_ASSET_COQ = {"ORU": "SmwORU", "RES": "SmwRES", "NONE": "SmwNone", "INVALID": "SmwInvalid"}


def coq_hex_stack(items):
    return "[" + ";".join(hexbytes(h) for h in items) + "]"


def emit_script_asset(P, idx, vid, inp, exp):
    """Emit asset_from_script / is_allowed / is_res pins (shared with C++)."""
    spk = hexbytes(inp["script_hex"])
    asset = SMW_ASSET_COQ[exp["asset"]]
    P(f"(* {vid} ScriptAsset asset={exp['asset']} allowed={exp['is_allowed']} is_res={exp['is_res']} *)")
    allow_pin = f"(is_allowed_script {spk})" if exp["is_allowed"] else f"(negb (is_allowed_script {spk}))"
    res_pin = f"(is_res_script {spk})" if exp["is_res"] else f"(negb (is_res_script {spk}))"
    expr = (
        f"andb (smw_asset_eqb (asset_from_script {spk}) {asset}) "
        f"(andb {allow_pin} {res_pin})"
    )
    emit_check(P, idx, vid, expr)


def emit_script_mint_neg(P, idx, vid, inp, exp):
    """Emit an abstract script / PCORB-shape reject check (ScriptMintWitness.v)."""
    kind = inp["kind"]
    reject = exp["reject"]
    ctor = SMW_REJECT_COQ[reject]
    P(f"(* {vid} ScriptMintNeg kind={kind} reject={reject} *)")
    if kind in ("script_class", "domain_cross"):
        spk = hexbytes(inp["script_hex"])
        claimed = inp.get("claimed_asset")
        claimed_c = "None" if not claimed else f"(Some {SMW_ASSET_COQ[claimed]})"
        expr = (
            f"match classify_script_reject {spk} {claimed_c} with "
            f"| Some r => smw_reject_eqb r {ctor} "
            f"| None => false end"
        )
        emit_check(P, idx, vid, expr)
        return
    if kind == "spend_cvg2":
        spk = hexbytes(inp["script_hex"])
        expr = (
            f"andb (is_cvg2_script {spk}) "
            f"(match classify_spend_cvg2 {spk} with "
            f"| Some r => smw_reject_eqb r {ctor} | None => false end)"
        )
        emit_check(P, idx, vid, expr)
        return
    if kind in ("pcorb_shape", "mint_witness_cross"):
        stack = coq_hex_stack(inp["stack_hex"])
        expr = (
            f"match classify_pcorb_reject {stack} with "
            f"| Some r => smw_reject_eqb r {ctor} "
            f"| None => false end"
        )
        emit_check(P, idx, vid, expr)
        return
    raise SystemExit(f"{vid}: unknown ScriptMintNeg kind {kind}")


def coq_cvg_asset(a):
    return {"ORU": "ORU", "RES": "RES"}[a]


def coq_cvg_out(o):
    return (
        f"{{| cvg_value := ({int(o['value'])}); "
        f"cvg_asset := {coq_cvg_asset(o['asset'])}; "
        f"cvg_script := {hexbytes(o['script_hex'])} |}}"
    )


def emit_invalid_cvg(P, idx, vid, inp, exp):
    """Emit a Convergence-tx / malformed-root reject check."""
    kind = inp["kind"]
    reject = exp["reject"]
    P(f"(* {vid} InvalidCvg kind={kind} reject={reject} *)")
    if kind == "cvg_tx":
        ctor = ICVG_REJECT_COQ[reject]
        outs = "[" + "; ".join(coq_cvg_out(o) for o in inp["outs"]) + "]"
        P(f"Definition icvg_outs_{idx} : list CvgOutAbs := {outs}.")
        expr = (
            f"match classify_cvg_tx_reject icvg_outs_{idx} with "
            f"| Some r => cvg_tx_reject_eqb r {ctor} "
            f"| None => false end"
        )
        emit_check(P, idx, vid, expr)
        return
    if kind == "committed_c":
        txids = list_hexbytes(inp["txids_hex"])
        claimed = hexbytes(inp["claimed_hex"])
        honest = hexbytes(exp["convergence_root"])
        expr = (
            f"andb (blist_eq (convergence_root_from_txid_bytes {txids}) {honest}) "
            f"(andb (Nat.eqb (length {claimed}) 32) "
            f"(negb (blist_eq {claimed} {honest})))"
        )
        emit_check(P, idx, vid, expr)
        return
    if kind == "malformed_preimage":
        txids = list_hexbytes(inp["txids_hex"])
        payload = hexbytes(inp["bad_payload_hex"])
        honest = hexbytes(exp["convergence_root"])
        bad = hexbytes(exp["bad_digest"])
        expr = (
            f"andb (blist_eq (convergence_root_from_txid_bytes {txids}) {honest}) "
            f"(andb (blist_eq (hash256_bytes {payload}) {bad}) "
            f"(negb (blist_eq {bad} {honest})))"
        )
        emit_check(P, idx, vid, expr)
        return
    if kind == "malformed_length":
        txids = list_hexbytes(inp["txids_hex"])
        claimed = hexbytes(inp["claimed_hex"])
        honest = hexbytes(exp["convergence_root"])
        expr = (
            f"andb (blist_eq (convergence_root_from_txid_bytes {txids}) {honest}) "
            f"(andb (Nat.eqb (length {honest}) 32) "
            f"(negb (Nat.eqb (length {claimed}) 32)))"
        )
        emit_check(P, idx, vid, expr)
        return
    if kind == "dup_txid":
        txids = list_hexbytes(inp["txids_hex"])
        collapsed = list_hexbytes(inp["collapsed_hex"])
        c_dup = hexbytes(exp["convergence_root"])
        c_one = hexbytes(exp["collapsed_root"])
        expr = (
            f"andb (blist_eq (convergence_root_from_txid_bytes {txids}) {c_dup}) "
            f"(andb (blist_eq (convergence_root_from_txid_bytes {collapsed}) {c_one}) "
            f"(negb (blist_eq {c_dup} {c_one})))"
        )
        emit_check(P, idx, vid, expr)
        return
    if kind == "nlt2_claim":
        txids = list_hexbytes(inp["txids_hex"])
        claimed = hexbytes(inp["claimed_hex"])
        honest = hexbytes(exp["convergence_root"])
        expr = (
            f"andb (blist_eq (convergence_root_from_txid_bytes {txids}) {honest}) "
            f"(negb (blist_eq {claimed} {honest}))"
        )
        emit_check(P, idx, vid, expr)
        return
    raise SystemExit(f"{vid}: unknown InvalidCvg kind {kind}")


XFAM_NEG_KINDS = {
    "script_dual_pool",
    "spend_cvg2_dual_pool",
    "pcorb_dual_pool",
    "schedule_mint",
    "schedule_cap",
}


def _andb(parts):
    expr = parts[0]
    for p in parts[1:]:
        expr = f"andb ({expr}) ({p})"
    return expr


def emit_edge_cross_neg(P, idx, vid, inp, exp):
    """TxAbs / dual-pool / schedule half of EdgeCross (NegativeCoreHarness)."""
    kind = inp["kind"]
    reject = exp["reject"]
    h = int(inp["height"])
    reward = f"(get_res_mint_reward {h})"
    P(f"(* {vid} EdgeCross kind={kind} reject={reject} *)")
    if kind in ("script_dual_pool", "spend_cvg2_dual_pool"):
        P(f"Definition neg_ins_{idx} : list TxOut := {coq_ins_list(inp['utxo'], inp['tx'])}.")
        P(f"Definition neg_tx_{idx} : TxAbs := {coq_tx(inp['tx'])}.")
        spk = hexbytes(inp["script_hex"])
        ctor = f"ScriptMintWitness.{SMW_REJECT_COQ[reject]}"
        if kind == "spend_cvg2_dual_pool":
            script_chk = (
                f"match ScriptMintWitness.classify_spend_cvg2 {spk} with "
                f"| Some r => ScriptMintWitness.smw_reject_eqb r {ctor} "
                f"| None => false end"
            )
        else:
            claimed = inp.get("claimed_asset")
            claimed_c = (
                "None" if not claimed else f"(Some ScriptMintWitness.{SMW_ASSET_COQ[claimed]})"
            )
            script_chk = (
                f"match ScriptMintWitness.classify_script_reject {spk} {claimed_c} with "
                f"| Some r => ScriptMintWitness.smw_reject_eqb r {ctor} "
                f"| None => false end"
            )
        expr = _andb(
            [
                script_chk,
                f"negb (andb (valid_dual_pool_on_ins neg_ins_{idx} neg_tx_{idx} {reward}) "
                f"(valid_script_shape_on_ins neg_ins_{idx} neg_tx_{idx}))",
            ]
        )
        emit_check(P, idx, vid, expr)
        return
    if kind == "pcorb_dual_pool":
        P(f"Definition neg_ins_{idx} : list TxOut := {coq_ins_list(inp['utxo'], inp['tx'])}.")
        P(f"Definition neg_tx_{idx} : TxAbs := {coq_tx(inp['tx'])}.")
        stack = coq_hex_stack(inp["tx"]["stack_hex"])
        ctor = f"ScriptMintWitness.{SMW_REJECT_COQ[reject]}"
        pcorb_chk = (
            f"match ScriptMintWitness.classify_pcorb_reject {stack} with "
            f"| Some r => ScriptMintWitness.smw_reject_eqb r {ctor} "
            f"| None => false end"
        )
        expr = _andb(
            [
                pcorb_chk,
                f"negb (valid_dual_pool_on_ins neg_ins_{idx} neg_tx_{idx} {reward})",
                f"negb (valid_script_shape_on_ins neg_ins_{idx} neg_tx_{idx})",
            ]
        )
        emit_check(P, idx, vid, expr)
        return
    if kind == "schedule_mint":
        mint = inp["mint"]
        P(f"Definition neg_ins_{idx} : list TxOut := {coq_ins_list(mint['utxo'], mint['tx'])}.")
        P(f"Definition neg_tx_{idx} : TxAbs := {coq_tx(mint['tx'])}.")
        parts = [
            f"negb (res_mint_shape neg_tx_{idx} neg_ins_{idx} {reward})",
            f"negb (valid_dual_pool_on_ins neg_ins_{idx} neg_tx_{idx} {reward})",
        ]
        neighbour = inp.get("neighbour_height")
        if neighbour is not None:
            parts.append(f"negb (Z.eqb {reward} (get_res_mint_reward {int(neighbour)}))")
        if reject == "unauthorized_mint":
            n = int(mint.get("mint_count", 1))
            parts.append(f"unauthorized_mint_at {h} {n}%nat")
            parts.append(f"negb (Z.gtb {reward} 0)")
        emit_check(P, idx, vid, _andb(parts))
        return
    if kind == "schedule_cap":
        shape = inp["shape"]
        cap = inp["cap"]
        P(f"Definition neg_ins_{idx} : list TxOut := {coq_ins_list(shape['utxo'], shape['tx'])}.")
        P(f"Definition neg_tx_{idx} : TxAbs := {coq_tx(shape['tx'])}.")
        supply = int(cap["res_supply"])
        expr = _andb(
            [
                f"Z.gtb {reward} 0",
                f"negb (res_mint_shape neg_tx_{idx} neg_ins_{idx} {reward})",
                f"negb (valid_dual_pool_on_ins neg_ins_{idx} neg_tx_{idx} {reward})",
                f"cap_overshoot_at ({supply}) {reward}",
            ]
        )
        emit_check(P, idx, vid, expr)
        return
    raise SystemExit(f"{vid}: unknown EdgeCross neg kind {kind}")


def invalid_cvg_bool_expr(inp, exp):
    """Bool expression matching emit_invalid_cvg, without emitting a lemma."""
    kind = inp["kind"]
    if kind == "committed_c":
        txids = list_hexbytes(inp["txids_hex"])
        claimed = hexbytes(inp["claimed_hex"])
        honest = hexbytes(exp["convergence_root"])
        return (
            f"andb (blist_eq (convergence_root_from_txid_bytes {txids}) {honest}) "
            f"(andb (Nat.eqb (length {claimed}) 32) "
            f"(negb (blist_eq {claimed} {honest})))"
        )
    if kind == "malformed_preimage":
        txids = list_hexbytes(inp["txids_hex"])
        payload = hexbytes(inp["bad_payload_hex"])
        honest = hexbytes(exp["convergence_root"])
        bad = hexbytes(exp["bad_digest"])
        return (
            f"andb (blist_eq (convergence_root_from_txid_bytes {txids}) {honest}) "
            f"(andb (blist_eq (hash256_bytes {payload}) {bad}) "
            f"(negb (blist_eq {bad} {honest})))"
        )
    if kind == "malformed_length":
        txids = list_hexbytes(inp["txids_hex"])
        claimed = hexbytes(inp["claimed_hex"])
        honest = hexbytes(exp["convergence_root"])
        return (
            f"andb (blist_eq (convergence_root_from_txid_bytes {txids}) {honest}) "
            f"(andb (Nat.eqb (length {honest}) 32) "
            f"(negb (Nat.eqb (length {claimed}) 32)))"
        )
    raise SystemExit(f"EdgeCross root kind {kind} not supported in GoldenHarness")


def emit_edge_cross_kernel(P, idx, vid, inp, exp):
    """Root / window / DCR / selection half of EdgeCross (GoldenHarness)."""
    kind = inp["kind"]
    reject = exp["reject"]
    P(f"(* {vid} EdgeCross kind={kind} reject={reject} *)")
    if kind in ("root_mint", "root_cap"):
        h = int(inp["height"])
        root_expr = invalid_cvg_bool_expr(inp["root"], exp)
        if kind == "root_mint":
            amount = int(inp["mint"]["tx"]["creates"][0]["value"])
            mint_expr = f"negb (Z.eqb ({amount}) (get_res_mint_reward {h}))"
        else:
            supply = int(inp["mint"]["res_supply"])
            n = int(inp["mint"]["mint_count"])
            attempted = "0" if n == 0 else f"(get_res_mint_reward {h})"
            mint_expr = f"negb (Z.leb ({supply} + {attempted}) RES_MAX_SUPPLY)"
        emit_check(P, idx, vid, f"andb ({root_expr}) ({mint_expr})")
        return
    if kind in ("window_dcr_sel", "window_obligation"):
        txids = inp["txids_hex"]
        fees = inp["fees"]
        h = inp["height"]
        prev = hexbytes(inp["prev_block_hash_hex"])
        T = spec.res_probability_target_from_bytes(bytes.fromhex(inp["T_hex"]))
        txs = "[" + ";".join(f"mkmint {hexbytes(t)} {w}" for t, w in zip(txids, fees)) + "]"
        paidb = list_hexbytes(inp.get("already_paid_hex") or [])
        survb = list_hexbytes(exp["surviving_hex"])
        expb = hexbytes(exp["draw_candidate_root"])
        cexp = hexbytes(exp["convergence_root"])
        wexp = exp["winner_index"]
        P(f"Definition F_{idx} := exclude_already_paid {txs} {paidb}.")
        P(f"Definition C_{idx} := ConvergeRoot F_{idx}.")
        P(
            f"Definition cand_{idx} := filter (fun t => if Z_lt_dec (Sigma t C_{idx} {prev} {h})"
            f" ({T} * weight t) then true else false) F_{idx}."
        )
        P(f"Definition Fw_{idx} := build_F {txs}.")
        P(f"Definition Cw_{idx} := ConvergeRoot Fw_{idx}.")
        P(
            f"Definition candw_{idx} := filter (fun t => if Z_lt_dec (Sigma t Cw_{idx} {prev} {h})"
            f" ({T} * weight t) then true else false) Fw_{idx}."
        )
        if wexp is not None and wexp >= 0:
            wexpb = hexbytes(txids[wexp])
            wchk = (
                f"match select_winner cand_{idx} C_{idx} {prev} {h} with"
                f" | Some w => blist_eq (tx_id_bytes w) {wexpb} | None => false end"
            )
        else:
            wchk = (
                f"match select_winner cand_{idx} C_{idx} {prev} {h} with"
                f" Some _ => false | None => true end"
            )
        parts = [
            f"llist_eq (map tx_id_bytes F_{idx}) {survb}",
            f"blist_eq C_{idx} {cexp}",
            f"blist_eq (DrawCandidateRoot cand_{idx}) {expb}",
            wchk,
        ]
        if exp.get("c_is_zero"):
            parts.append(f"blist_eq C_{idx} bytes32_zero")
        if exp.get("dcr_neq_zero"):
            parts.append(f"negb (blist_eq (DrawCandidateRoot cand_{idx}) bytes32_zero)")
        if exp.get("dcr_neq_c"):
            parts.append(f"negb (blist_eq (DrawCandidateRoot cand_{idx}) C_{idx})")
        if inp["mode"] == "drop_winner_moves":
            parts.append(
                f"negb (blist_eq (DrawCandidateRoot cand_{idx}) (DrawCandidateRoot candw_{idx}))"
            )
            ww = exp["full_winner_index"]
            if wexp >= 0 and ww >= 0:
                wwexpb = hexbytes(txids[ww])
                parts.append(
                    f"match select_winner cand_{idx} C_{idx} {prev} {h},"
                    f" select_winner candw_{idx} Cw_{idx} {prev} {h} with"
                    f" | Some w, Some ww =>"
                    f" andb (blist_eq (tx_id_bytes w) {wexpb})"
                    f" (andb (blist_eq (tx_id_bytes ww) {wwexpb})"
                    f" (negb (blist_eq (tx_id_bytes w) (tx_id_bytes ww))))"
                    f" | _, _ => false end"
                )
        elif inp["mode"] in ("singleton_nlt2", "singleton_hashes"):
            fwexp = hexbytes(exp["full_draw_candidate_root"])
            parts.append(
                f"negb (blist_eq (DrawCandidateRoot cand_{idx}) {fwexp})"
            )
        if kind == "window_obligation":
            won = "true" if inp["draw_won"] else "false"
            n = int(inp["mint_count"])
            ob = exp["obligation_reject"]
            if ob == "without_winner":
                parts.append(f"without_winner_at {won} {n}%nat")
            elif ob == "winner_mismatch":
                parts.append(f"winner_mismatch_at {h} {won} {n}%nat")
            else:
                raise SystemExit(f"{vid}: unmapped window_obligation reject {ob}")
        emit_check(P, idx, vid, _andb(parts))
        return
    raise SystemExit(f"{vid}: unknown EdgeCross kernel kind {kind}")


def emit_negative_core(P, idx, vid, inp, exp):
    """Emit a cheap ReplayReorgCore reject check. Kind selects the Coq predicate.

    Dual-pool / apply-body checks take the input TxOut list explicitly.
    gather_inputs / apply_block walk OutPoints with hash32_eq_dec, which is
    opaque (Qed in Hash32Order), so those paths are not vm_compute-able.
    The predicates below are the same booleans apply_tx uses after lookup.
    """
    kind = inp["kind"]
    reject = exp["reject"]
    h = int(inp["height"])
    P(f"(* {vid} NegativeCore kind={kind} reject={reject} *)")
    reward = f"(get_res_mint_reward {h})"
    if kind in ("dual_pool_tx", "mint_shape"):
        P(f"Definition neg_ins_{idx} : list TxOut := {coq_ins_list(inp['utxo'], inp['tx'])}.")
        P(f"Definition neg_tx_{idx} : TxAbs := {coq_tx(inp['tx'])}.")
        if kind == "mint_shape":
            expr = (
                f"andb (negb (res_mint_shape neg_tx_{idx} neg_ins_{idx} {reward})) "
                f"(negb (valid_dual_pool_on_ins neg_ins_{idx} neg_tx_{idx} {reward}))"
            )
        else:
            expr = f"negb (valid_dual_pool_on_ins neg_ins_{idx} neg_tx_{idx} {reward})"
        emit_check(P, idx, vid, expr)
        return
    if kind == "connect_cap":
        P(f"Definition neg_s_{idx} : ReplayState := {coq_state(inp)}.")
        P(f"Definition neg_b_{idx} : BlockAbs := {coq_block(inp)}.")
        expr = f"negb (valid_res_cap neg_s_{idx} neg_b_{idx})"
    elif kind == "connect_obligation":
        won = "true" if inp.get("draw_won", False) else "false"
        n = int(inp.get("mint_count", 0))
        if reject == "winner_mismatch":
            expr = f"winner_mismatch_at {h} {won} {n}%nat"
        elif reject == "unauthorized_mint":
            expr = f"unauthorized_mint_at {h} {n}%nat"
        elif reject == "without_winner":
            expr = f"without_winner_at {won} {n}%nat"
        else:
            raise SystemExit(f"{vid}: unmapped connect_obligation reject {reject}")
        emit_check(P, idx, vid, expr)
        return
    elif kind == "apply_block":
        # Pin the body dual-pool conjunct that makes apply_block return None.
        # valid_block_connect / apply_block themselves need hash32_eq_dec.
        txs = inp["txs"]
        utxo = inp["utxo"]
        parts = []
        for i, tx in enumerate(txs):
            P(f"Definition neg_bins_{idx}_{i} : list TxOut := {coq_ins_list(utxo, tx)}.")
            P(f"Definition neg_btx_{idx}_{i} : TxAbs := {coq_tx(tx)}.")
            parts.append(
                f"(valid_dual_pool_on_ins neg_bins_{idx}_{i} neg_btx_{idx}_{i} {reward})"
            )
        # Body is rejected iff not every tx passes dual-pool.
        if len(parts) == 1:
            expr = f"negb {parts[0]}"
        else:
            acc = parts[0]
            for p in parts[1:]:
                acc = f"andb {acc} {p}"
            expr = f"negb ({acc})"
    else:
        raise SystemExit(f"{vid}: unknown NegativeCore kind {kind}")
    emit_check(P, idx, vid, expr)

def main():
    d = json.load(open(VEC))
    vectors = d["vectors"]
    lines = []
    P = lines.append
    P("(* AUTO-GENERATED by gen_coq_harness.py - DO NOT EDIT. PHASE-4A1 golden execution. *)")
    P("(* Runs the Coq kernel against PHASE-3 golden vectors via vm_compute.          *)")
    P("(* Every vector is an enforced Lemma: a regressed vector fails the build.      *)")
    P("Require Import Coq.ZArith.ZArith.")
    P("Require Import Coq.Lists.List.")
    P("Import ListNotations.")
    P("Require Import Types.")
    P("From Rigor Require Import RigorBytes.")
    P("Include RigorBytes.")
    P("Require Import RigorHash.")
    P("From Rigor Require Import RigorSha256.")
    P("Require Import SigmaPoW01.")
    P("Require Import ConvergeRoot.")
    P("Require Import DrawCandidateRoot.")
    P("Require Import InvalidCvg.")
    P("Require Import ScriptMintWitness.")
    P("Require Import Sigma.")
    P("Require Import Selection.")
    P("Require Import Execution.")
    P("Require Import WindowExclusion.")
    P("From Rigor Require Import ResSchedule.")
    P("")
    P("Open Scope Z_scope.")
    P("")
    P("(* Z-height H-1 / without-winner pins for EdgeCross window_obligation rows.")
    P("   Same forms as NegativeCoreHarness; not a claim that cm_draw_won is the")
    P("   Selection winner (P1-2 remains open). *)")
    P("Definition res_mint_authorized_at (h : Z) : bool := Z.gtb (get_res_mint_reward h) 0.")
    P("Definition winner_mismatch_at (h : Z) (won : bool) (n : nat) : bool :=")
    P("  andb (res_mint_authorized_at h) (andb won (negb (Nat.eqb n 1%nat))).")
    P("Definition without_winner_at (won : bool) (n : nat) : bool :=")
    P("  andb (negb won) (negb (Nat.eqb n 0%nat)).")
    P("")
    P("Fixpoint blist_eq (a b : list nat) : bool :=")
    P("  match a, b with")
    P("  | nil, nil => true")
    P("  | x :: xs, y :: ys => andb (Nat.eqb x y) (blist_eq xs ys)")
    P("  | _, _ => false")
    P("  end.")
    P("")
    P("Fixpoint llist_eq (a b : list (list nat)) : bool :=")
    P("  match a, b with")
    P("  | nil, nil => true")
    P("  | x :: xs, y :: ys => andb (blist_eq x y) (llist_eq xs ys)")
    P("  | _, _ => false")
    P("  end.")
    P("")
    P("(* Build a mint (ORU) Tx with given txid bytes and weight (mint_cost). *)")
    P("Definition mkmint (idb : list byte) (w : Z) : Tx :=")
    P("  {| tx_id := 0; tx_id_bytes := idb; asset_in := ORU; fee := 0; mint_cost := w |}.")
    P("")

    idx = 0
    index_map = []  # (idx, vector_id, module, kind)
    neg_lines = []
    NP = neg_lines.append
    NP("(* AUTO-GENERATED by gen_coq_harness.py - DO NOT EDIT. NegativeCore / EdgeCross. *)")
    NP("(* Cheap ReplayReorgCore vm_compute: dual-pool / mint-shape / cap / H-1 plus   *)")
    NP("(* EdgeCross script+dual-pool and schedule-boundary rows.                     *)")
    NP("(* Separate compilation unit: Types.Tx.tx_id must not clash with TxAbs.      *)")
    NP("Require Import Coq.ZArith.ZArith.")
    NP("Require Import Coq.Lists.List.")
    NP("Require Import Coq.Bool.Bool.")
    NP("Import ListNotations.")
    NP("Require Import Rigor.ReplayReorgCore.Hash32Order.")
    NP("Require Import Rigor.ReplayReorgCore.ReplayState.")
    NP("Require Import Rigor.ReplayReorgCore.ConnectBlockMint.")
    NP("Require Import Rigor.ReplayReorgCore.StateTransition.")
    NP("Require Import Rigor.ReplayReorgCore.ScriptShape.")
    NP("From Rigor Require Import ResSchedule.")
    NP("Require ScriptMintWitness.")
    NP("")
    NP("Open Scope Z_scope.")
    NP("")
    NP("Definition neg_h32 (n : nat) : Hash32 := n :: repeat_byte 31%nat 0%nat.")
    NP("Definition neg_spk (k : AssetKind) : list nat :=")
    NP("  match k with")
    NP("  | AssetORU => ScriptMintWitness.canonical_p2wpkh")
    NP("  | AssetRES => ScriptMintWitness.canonical_native_res")
    NP("  | AssetNone => [ScriptMintWitness.RIGOR_MARKER_RETURN]")
    NP("  | AssetInvalid => ScriptMintWitness.canonical_p2pkh")
    NP("  end.")
    NP("Definition neg_out (v : Z) (k : AssetKind) : TxOut :=")
    NP("  {| out_value := v; out_script := neg_spk k |}.")
    NP("Definition neg_out_spk (v : Z) (_k : AssetKind) (spk : list nat) : TxOut :=")
    NP("  {| out_value := v; out_script := spk |}.")
    NP("Definition cap_overshoot_at (supply reward : Z) : bool :=")
    NP("  negb (Z.leb (supply + reward) RES_MAX_SUPPLY).")
    NP("")
    NP("(* Z-height forms of ConnectBlockMint obligation paths. hdr_height is nat,")
    NP("   and a 52_710_000 unary literal would blow vm_compute; the schedule itself")
    NP("   is a Z function, so these match unauthorized_mint_path / winner_mismatch_path")
    NP("   / without_winner_mint_path at get_res_mint_reward h. *)")
    NP("Definition res_mint_authorized_at (h : Z) : bool := Z.gtb (get_res_mint_reward h) 0.")
    NP("Definition unauthorized_mint_at (h : Z) (n : nat) : bool :=")
    NP("  andb (negb (res_mint_authorized_at h)) (negb (Nat.eqb n 0%nat)).")
    NP("Definition winner_mismatch_at (h : Z) (won : bool) (n : nat) : bool :=")
    NP("  andb (res_mint_authorized_at h) (andb won (negb (Nat.eqb n 1%nat))).")
    NP("Definition without_winner_at (won : bool) (n : nat) : bool :=")
    NP("  andb (negb won) (negb (Nat.eqb n 0%nat)).")
    NP("Definition valid_dual_pool_on_ins (ins : list TxOut) (tx : TxAbs) (reward : Z) : bool :=")
    NP("  if is_scheduled_mint_shape tx ins reward then true else valid_ordinary_dual_pool ins tx.")
    NP("")
    neg_idx = 0

    for v in vectors:
        vid = v["id"]; mod = v["module"]; inp = v["input"]; exp = v["expected"]
        kernel_mods = {
            "ConvergenceRoot", "DrawCandidateRoot", "InvalidCvg", "ScriptMintNeg",
            "ScriptAsset",
            "SigmaPoW01", "Inequality", "RunSelection", "WindowExclusion", "SigmaEncoder",
            "EdgeCross",
        }
        must_enforce = {
            "NegativeCore", "InvalidCvg", "ScriptMintNeg", "DrawCandidateRoot",
            "EdgeCross", "ScriptAsset",
        }
        enforce = v.get("coq_enforce", mod in kernel_mods)
        if mod in must_enforce and not enforce:
            raise SystemExit(
                f"{vid}: family {mod} must stay Coq-enforced "
                "(golden_lock.json must_enforce_families)"
            )
        if mod == "NegativeCore":
            if not enforce:
                index_map.append((None, vid, mod, "python_only"))
                continue
            emit_negative_core(NP, neg_idx, vid, inp, exp)
            index_map.append((neg_idx, vid, mod, "neg_core"))
            neg_idx += 1
            continue
        if mod == "EdgeCross":
            if not enforce:
                raise SystemExit(f"{vid}: EdgeCross must stay Coq-enforced")
            if inp["kind"] in XFAM_NEG_KINDS:
                emit_edge_cross_neg(NP, neg_idx, vid, inp, exp)
                index_map.append((neg_idx, vid, mod, "neg_core"))
                neg_idx += 1
                continue
            emit_edge_cross_kernel(P, idx, vid, inp, exp)
            index_map.append((idx, vid, mod, "xfam"))
            idx += 1
            continue
        if not enforce:
            index_map.append((None, vid, mod, "python_only"))
            continue
        if mod == "InvalidCvg":
            emit_invalid_cvg(P, idx, vid, inp, exp)
            index_map.append((idx, vid, mod, "icvg"))
            idx += 1
            continue
        if mod == "ScriptMintNeg":
            emit_script_mint_neg(P, idx, vid, inp, exp)
            index_map.append((idx, vid, mod, "smw"))
            idx += 1
            continue
        if mod == "ScriptAsset":
            emit_script_asset(P, idx, vid, inp, exp)
            index_map.append((idx, vid, mod, "sass"))
            idx += 1
            continue
        if mod == "ConvergenceRoot":
            txids = list_hexbytes(inp["txids_hex"])
            expb = hexbytes(exp["convergence_root"])
            P(f"(* {vid} {mod} *)")
            emit_check(P, idx, vid, f"blist_eq (convergence_root_from_txid_bytes {txids}) {expb}")
            index_map.append((idx, vid, mod, "root"))
            idx += 1
        elif mod == "DrawCandidateRoot":
            kind = inp.get("kind", "flags")
            P(f"(* {vid} {mod} kind={kind} *)")
            if kind == "incomplete":
                emit_check(
                    P, idx, vid,
                    "match maybe_draw_candidate_root false [] [] [] 0 0 with None => true | Some _ => false end",
                )
            elif kind == "genesis":
                expb = hexbytes(exp["draw_candidate_root"])
                emit_check(
                    P, idx, vid,
                    "match maybe_draw_candidate_root true [] [] [] 0 0 with"
                    f" | Some r => blist_eq r {expb} | None => false end",
                )
            elif kind == "exclusion":
                txs = "[" + ";".join(f"mkmint {hexbytes(t)} 1" for t in inp["candidates_hex"]) + "]"
                paidb = list_hexbytes(inp["already_paid_hex"])
                expb = hexbytes(exp["draw_candidate_root"])
                emit_check(
                    P, idx, vid,
                    f"blist_eq (DrawCandidateRoot (exclude_already_paid {txs} {paidb})) {expb}",
                )
            elif kind == "e2e":
                txids = inp["txids_hex"]
                fees = inp["fees"]
                h = inp["height"]
                prev = hexbytes(inp["prev_block_hash_hex"])
                T = spec.res_probability_target_from_bytes(bytes.fromhex(inp["T_hex"]))
                txs = "[" + ";".join(f"mkmint {hexbytes(t)} {w}" for t, w in zip(txids, fees)) + "]"
                expb = hexbytes(exp["draw_candidate_root"])
                cexp = hexbytes(exp["convergence_root"])
                wexp = exp["winner_index"]
                paid = inp.get("already_paid_hex") or []
                if paid:
                    paidb = list_hexbytes(paid)
                    P(f"Definition F_{idx} := exclude_already_paid {txs} {paidb}.")
                else:
                    P(f"Definition F_{idx} := build_F {txs}.")
                P(f"Definition C_{idx} := ConvergeRoot F_{idx}.")
                P(
                    f"Definition cand_{idx} := filter (fun t => if Z_lt_dec (Sigma t C_{idx} {prev} {h})"
                    f" ({T} * weight t) then true else false) F_{idx}."
                )
                if wexp is not None and wexp >= 0:
                    wexpb = hexbytes(txids[wexp])
                    wchk = (
                        f"match select_winner cand_{idx} C_{idx} {prev} {h} with"
                        f" | Some w => blist_eq (tx_id_bytes w) {wexpb} | None => false end"
                    )
                else:
                    wchk = (
                        f"match select_winner cand_{idx} C_{idx} {prev} {h} with"
                        f" Some _ => false | None => true end"
                    )
                emit_check(
                    P, idx, vid,
                    f"andb (blist_eq (DrawCandidateRoot cand_{idx}) {expb})"
                    f" (andb (blist_eq C_{idx} {cexp}) {wchk})",
                )
            elif kind == "contrast":
                txids = list_hexbytes(inp["txids_hex"])
                flags = "[" + ";".join("true" if f else "false" for f in inp["eligible"]) + "]"
                otx = list_hexbytes(inp["other_txids_hex"])
                oflags = "[" + ";".join("true" if f else "false" for f in inp["other_eligible"]) + "]"
                expb = hexbytes(exp["draw_candidate_root"])
                oexpb = hexbytes(exp["other_draw_candidate_root"])
                emit_check(
                    P, idx, vid,
                    f"andb (blist_eq (draw_candidate_root_from_flags {txids} {flags}) {expb})"
                    f" (andb (blist_eq (draw_candidate_root_from_flags {otx} {oflags}) {oexpb})"
                    f" (negb (blist_eq (draw_candidate_root_from_flags {txids} {flags})"
                    f" (draw_candidate_root_from_flags {otx} {oflags}))))",
                )
            elif kind == "shape":
                txids = list_hexbytes(inp["txids_hex"])
                flags = "[" + ";".join("true" if f else "false" for f in inp["eligible"]) + "]"
                expb = hexbytes(exp["draw_candidate_root"])
                parts = [
                    f"blist_eq (draw_candidate_root_from_flags {txids} {flags}) {expb}",
                ]
                if inp.get("neq_zero") or exp.get("neq_zero"):
                    parts.append(
                        f"negb (blist_eq (draw_candidate_root_from_flags {txids} {flags}) bytes32_zero)"
                    )
                if inp.get("neq_c") or exp.get("neq_c"):
                    parts.append(
                        f"negb (blist_eq (draw_candidate_root_from_flags {txids} {flags})"
                        f" (convergence_root_from_txid_bytes {txids}))"
                    )
                if inp.get("eq_c") or exp.get("eq_c"):
                    parts.append(
                        f"blist_eq (draw_candidate_root_from_flags {txids} {flags})"
                        f" (convergence_root_from_txid_bytes {txids})"
                    )
                expr = parts[0]
                for p in parts[1:]:
                    expr = f"andb ({expr}) ({p})"
                emit_check(P, idx, vid, expr)
            elif kind == "e2e_mismatch":
                txids = inp["txids_hex"]
                fees = inp["fees"]
                h = inp["height"]
                prev = hexbytes(inp["prev_block_hash_hex"])
                T = spec.res_probability_target_from_bytes(bytes.fromhex(inp["T_hex"]))
                txs = "[" + ";".join(f"mkmint {hexbytes(t)} {w}" for t, w in zip(txids, fees)) + "]"
                paidb = list_hexbytes(inp.get("already_paid_hex") or [])
                expb = hexbytes(exp["draw_candidate_root"])
                cexp = hexbytes(exp["convergence_root"])
                wexpb = hexbytes(txids[exp["winner_index"]])
                wwexpb = hexbytes(txids[exp["wrong_winner_index"]])
                wexpb_wrong_dcr = hexbytes(exp["wrong_draw_candidate_root"])
                P(f"Definition F_{idx} := exclude_already_paid {txs} {paidb}.")
                P(f"Definition C_{idx} := ConvergeRoot F_{idx}.")
                P(
                    f"Definition cand_{idx} := filter (fun t => if Z_lt_dec (Sigma t C_{idx} {prev} {h})"
                    f" ({T} * weight t) then true else false) F_{idx}."
                )
                P(f"Definition Fw_{idx} := build_F {txs}.")
                P(f"Definition Cw_{idx} := ConvergeRoot Fw_{idx}.")
                P(
                    f"Definition candw_{idx} := filter (fun t => if Z_lt_dec (Sigma t Cw_{idx} {prev} {h})"
                    f" ({T} * weight t) then true else false) Fw_{idx}."
                )
                wchk = (
                    f"match select_winner cand_{idx} C_{idx} {prev} {h},"
                    f" select_winner candw_{idx} Cw_{idx} {prev} {h} with"
                    f" | Some w, Some ww =>"
                    f" andb (blist_eq (tx_id_bytes w) {wexpb})"
                    f" (andb (blist_eq (tx_id_bytes ww) {wwexpb})"
                    f" (negb (blist_eq (tx_id_bytes w) (tx_id_bytes ww))))"
                    f" | _, _ => false end"
                )
                emit_check(
                    P, idx, vid,
                    f"andb (blist_eq (DrawCandidateRoot cand_{idx}) {expb})"
                    f" (andb (blist_eq C_{idx} {cexp})"
                    f" (andb (blist_eq (DrawCandidateRoot candw_{idx}) {wexpb_wrong_dcr}) {wchk}))",
                )
            elif kind == "sigma_perturb":
                txids = inp["txids_hex"]
                fees = inp["fees"]
                h = inp["height"]
                prev = hexbytes(inp["prev_block_hash_hex"])
                T = spec.res_probability_target_from_bytes(bytes.fromhex(inp["T_hex"]))
                txs = "[" + ";".join(f"mkmint {hexbytes(t)} {w}" for t, w in zip(txids, fees)) + "]"
                expb = hexbytes(exp["draw_candidate_root"])
                cexp = hexbytes(exp["convergence_root"])
                wexpb = hexbytes(txids[exp["winner_index"]])
                alt = inp["alt"]
                alt_h = int(alt.get("height", h))
                alt_prev = hexbytes(alt["prev_block_hash_hex"]) if "prev_block_hash_hex" in alt else prev
                P(f"Definition F_{idx} := build_F {txs}.")
                P(f"Definition C_{idx} := ConvergeRoot F_{idx}.")
                P(
                    f"Definition cand_{idx} := filter (fun t => if Z_lt_dec (Sigma t C_{idx} {prev} {h})"
                    f" ({T} * weight t) then true else false) F_{idx}."
                )
                if "convergence_root_hex" in alt:
                    P(f"Definition Calt_{idx} := {hexbytes(alt['convergence_root_hex'])}.")
                    calt = f"Calt_{idx}"
                else:
                    calt = f"C_{idx}"
                P(
                    f"Definition candalt_{idx} := filter (fun t => if Z_lt_dec (Sigma t {calt} {alt_prev} {alt_h})"
                    f" ({T} * weight t) then true else false) F_{idx}."
                )
                wchk = (
                    f"match select_winner cand_{idx} C_{idx} {prev} {h},"
                    f" select_winner candalt_{idx} {calt} {alt_prev} {alt_h} with"
                    f" | Some w, Some wa =>"
                    f" andb (blist_eq (tx_id_bytes w) {wexpb})"
                    f" (negb (blist_eq (tx_id_bytes w) (tx_id_bytes wa)))"
                    f" | _, _ => false end"
                )
                emit_check(
                    P, idx, vid,
                    f"andb (blist_eq (DrawCandidateRoot cand_{idx}) {expb})"
                    f" (andb (blist_eq C_{idx} {cexp}) {wchk})",
                )
            else:
                txids = list_hexbytes(inp["txids_hex"])
                flags = "[" + ";".join("true" if f else "false" for f in inp["eligible"]) + "]"
                expb = hexbytes(exp["draw_candidate_root"])
                emit_check(P, idx, vid, f"blist_eq (draw_candidate_root_from_flags {txids} {flags}) {expb}")
            index_map.append((idx, vid, mod, "dcr"))
            idx += 1
        elif mod == "SigmaPoW01":
            txid = hexbytes(inp["txid_hex"])
            croot = hexbytes(inp["convergence_root_hex"])
            prev = hexbytes(inp["prev_block_hash_hex"])
            h = inp["height"]
            expb = hexbytes(exp["sigma"])
            P(f"(* {vid} {mod} *)")
            emit_check(P, idx, vid, f"blist_eq (sigma_bytes {txid} {croot} {prev} {h}) {expb}")
            index_map.append((idx, vid, mod, "sigma"))
            idx += 1
        elif mod == "Inequality":
            hb = hexbytes(inp["h_hex"])
            # T travels as the same LE byte string consensus carries; `T_int` is the older
            # spelling, kept so pre-A9C vector files still generate.
            T = (inp["T_int"] if "T_int" in inp
                 else spec.res_probability_target_from_bytes(bytes.fromhex(inp["T_hex"])))
            f = inp["f"]
            elig = "true" if exp["eligible"] else "false"
            P(f"(* {vid} {mod} *)")
            emit_check(P, idx, vid, f"Bool.eqb (uint256_of_bytes {hb} <? ({T} * {f})) {elig}")
            index_map.append((idx, vid, mod, "ineq"))
            idx += 1
        elif mod == "RunSelection":
            txids = inp["txids_hex"]; fees = inp["fees"]
            h = inp["height"]; prev = hexbytes(inp["prev_block_hash_hex"])
            # threshold T: LE uint256 of T_hex (spec.res_probability_target_from_bytes)
            T = spec.res_probability_target_from_bytes(bytes.fromhex(inp["T_hex"]))
            txs = "[" + ";".join(f"mkmint {hexbytes(t)} {w}" for t, w in zip(txids, fees)) + "]"
            wexp = exp["winner_index"]
            P(f"(* {vid} {mod} winner_index={wexp} T={inp['T_hex'][:8]}.. *)")
            P(f"Definition F_{idx} := build_F {txs}.")
            P(f"Definition C_{idx} := ConvergeRoot F_{idx}.")
            P(f"Definition cand_{idx} := filter (fun t => if Z_lt_dec (Sigma t C_{idx} {prev} {h}) ({T} * weight t) then true else false) F_{idx}.")
            if wexp is not None and wexp >= 0:
                wexpb = hexbytes(txids[wexp])
                expr = (f"match select_winner cand_{idx} C_{idx} {prev} {h} with"
                        f" | Some w => blist_eq (tx_id_bytes w) {wexpb}"
                        f" | None => false end")
            else:
                expr = (f"match select_winner cand_{idx} C_{idx} {prev} {h} with"
                        f" Some _ => false | None => true end")
            emit_check(P, idx, vid, expr)
            index_map.append((idx, vid, mod, "winner"))
            idx += 1
        elif mod == "WindowExclusion":
            cands = inp["candidates_hex"]
            paid = inp["already_paid_hex"]
            surv = exp["surviving_hex"]
            txs = "[" + ";".join(f"mkmint {hexbytes(t)} 1" for t in cands) + "]"
            paidb = list_hexbytes(paid)
            survb = list_hexbytes(surv)
            P(f"(* {vid} {mod} dropped={exp.get('dropped')} *)")
            emit_check(P, idx, vid,
                       f"llist_eq (map tx_id_bytes (exclude_already_paid {txs} {paidb})) {survb}")
            index_map.append((idx, vid, mod, "a9c"))
            idx += 1
        elif mod == "SigmaEncoder":
            txid = hexbytes(inp["txid_hex"])
            croot = hexbytes(inp["convergence_root_hex"])
            prev = hexbytes(inp["prev_block_hash_hex"])
            h = inp["height"]
            hle = hexbytes(exp["height_le"])
            P(f"(* {vid} {mod} height={h} *)")
            expr = (
                f"andb (Nat.eqb (length (sigma_preimage_bytes {txid} {croot} {prev} {h})) 100) "
                f"(andb (blist_eq (firstn 32 (sigma_preimage_bytes {txid} {croot} {prev} {h})) {txid}) "
                f"(blist_eq (skipn 96 (sigma_preimage_bytes {txid} {croot} {prev} {h})) {hle}))"
            )
            emit_check(P, idx, vid, expr)
            index_map.append((idx, vid, mod, "encoder"))
            idx += 1
        elif mod == "HashKernel":
            payload = hexbytes(inp["payload_hex"])
            expb = hexbytes(exp["sha256d"])
            P(f"(* {vid} {mod} sha256d *)")
            emit_check(P, idx, vid, f"blist_eq (hash256_bytes {payload}) {expb}")
            index_map.append((idx, vid, mod, "hash256d"))
            idx += 1
        elif mod == "Sha256Fips":
            payload = hexbytes(inp["payload_hex"])
            expb = hexbytes(exp["sha256"])
            P(f"(* {vid} {mod} sha256 *)")
            emit_check(P, idx, vid, f"blist_eq (sha256_bytes {payload}) {expb}")
            index_map.append((idx, vid, mod, "sha256"))
            idx += 1
        else:
            # Not executable in kernel.
            index_map.append((None, vid, mod, "blocked"))

    # Fail-closed: never rewrite harness artifacts unless the living lock still
    # describes this index (TASK-FV-DISCREPANCY-AND-LOCK-MAINTENANCE-001).
    lock_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "golden_lock.json")
    lock = json.load(open(lock_path, encoding="utf-8"))
    execed = sum(1 for m in index_map if m[0] is not None)
    if len(index_map) != lock["shared_vectors"]:
        raise SystemExit(
            f"harness index length {len(index_map)} != golden_lock.json "
            f"shared_vectors {lock['shared_vectors']}; refusing to write"
        )
    if execed != lock["coq_enforced_lemmas"]:
        raise SystemExit(
            f"planned Coq-enforced lemmas {execed} != lock {lock['coq_enforced_lemmas']}; "
            "refusing to write"
        )
    if neg_idx != lock["negative_core_harness_lemmas"]:
        raise SystemExit(
            f"planned NegativeCoreHarness lemmas {neg_idx} != lock "
            f"{lock['negative_core_harness_lemmas']}; refusing to write"
        )
    if execed - neg_idx != lock["golden_harness_lemmas"]:
        raise SystemExit(
            f"planned GoldenHarness lemmas {execed - neg_idx} != lock "
            f"{lock['golden_harness_lemmas']}; refusing to write"
        )
    by_mod = {}
    for row in index_map:
        rec = by_mod.setdefault(row[2], {"vectors": 0, "coq_enforced": 0})
        rec["vectors"] += 1
        if row[0] is not None:
            rec["coq_enforced"] += 1
    for mod, exp in lock["families"].items():
        got = by_mod.get(mod, {"vectors": 0, "coq_enforced": 0})
        if got["vectors"] != exp["vectors"] or got["coq_enforced"] != exp["coq_enforced"]:
            raise SystemExit(
                f"planned {mod} vectors={got['vectors']} coq={got['coq_enforced']} "
                f"!= lock vectors={exp['vectors']} coq={exp['coq_enforced']}; refusing to write"
            )
    index_by_id = {row[1]: row for row in index_map}
    for split in lock["surface_splits"]["rows"]:
        for vid in split["ids"]:
            row = index_by_id.get(vid)
            if row is None or row[0] is None:
                raise SystemExit(f"surface_split {vid} not Coq-enforced; refusing to write")
            if row[2] != split["family"] or row[3] != split["harness_kind"]:
                raise SystemExit(
                    f"surface_split {vid} family/kind {row[2]}/{row[3]} != "
                    f"{split['family']}/{split['harness_kind']}; refusing to write"
                )
    for vid in lock["surface_splits"]["omit_coq_enforce_key_enforced"]:
        row = index_by_id.get(vid)
        if row is None or row[0] is None:
            raise SystemExit(f"omit-key {vid} not Coq-enforced; refusing to write")

    open(OUT, "w").write("\n".join(lines) + "\n")
    open(NEG_OUT, "w").write("\n".join(neg_lines) + "\n")
    json.dump(index_map, open(INDEX, "w"), indent=1)
    print(
        f"wrote {OUT}: {execed - neg_idx} kernel lemmas; "
        f"{NEG_OUT}: {neg_idx} NegativeCore lemmas; "
        f"{len(index_map)-execed} blocked"
    )

if __name__ == "__main__":
    main()

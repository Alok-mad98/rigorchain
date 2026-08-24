(** ReplayReorgCore/OpaqueBoundary.v
    TASK-FV-MODEL-RESIDUAL-OPAQUE-BOUNDARY-001.

    Residual abstract / opaque surfaces of the replay/connect model, stated as
    machine-checked *non-claims*. These lemmas exist so later work cannot
    treat the model as if it interpreted scripts (VM / P2WSH), authorized
    mints via owner / PCORB / Selection, or identified scalar [rs_res_supply]
    with the UTXO RES projection.

    This module stays in the ReplayReorgCore cone: no SHA-256, no Selection,
    no [draw_candidate_root]. Coinbase / Merkle / header bind of that digest
    is declared in [DrawCandidateRoot.v] (same original task).

    Pooling class **is** [asset_kind_of_script] (Task 4). Residual opaques
    are: no script VM, [cm_draw_won] is abstract, [rs_res_supply] is a
    scalar independent of [utxo_res].

    Honesty: not a C++ refinement. Does not flip freeze flags.
    Does not amend any Closed theorem in [ResMintObligation],
    [DualPoolConservation], or [SupplyConservation]. *)
From Coq Require Import ZArith.ZArith.
From Coq Require Import List.
From Coq Require Import Arith.
From Coq Require Import Bool.
Import ListNotations.
Require Import Rigor.ReplayReorgCore.ReplayState.
Require Import Rigor.ReplayReorgCore.ConnectBlockMint.
Require Import Rigor.ReplayReorgCore.StateTransition.
Require Import Rigor.ReplayReorgCore.ScriptShape.
Require Import Rigor.ReplayReorgCore.AssetFromScript.

(** * 1. Pooling class is [f(script)] — still no VM

    WE DO NOT CLAIM:
    - [out_script] is interpreted (no VM / P2WSH execution)
    - [apply_tx] is a C++ [EvalScript] refinement

    WE DO CLAIM (Task 4):
    - [output_asset o = asset_kind_of_script (out_script o)]
    - [txout_asset_value] reads that function, not an independent tag
    - same script bytes ⇒ same pooling class

    Finite *shape* lift remains:
    - non-empty created scripts must be [IsAllowedScript] or zero-value
      [OP_RETURN]; empty [out_script] is a shape placeholder (pools as ORU)
    - spent CVG2 scripts are rejected ([InvalidCvg.is_cvg2_script])
    - [tx_pcorb = Some stack] must parse PCORB field *sizes*
    - [tx_pcorb = None] skips the witness-shape conjunct *)

Lemma txout_asset_value_determined_by_script :
  forall (k : AssetKind) (v : Z) (s : list nat),
    txout_asset_value k {| out_value := v; out_script := s |} =
    (if asset_eqb (asset_kind_of_script s) k then v else 0%Z).
Proof. intros. unfold txout_asset_value, output_asset. reflexivity. Qed.

Theorem script_derives_pooling_class :
  forall o1 o2 : TxOut,
    out_script o1 = out_script o2 ->
    output_asset o1 = output_asset o2.
Proof. apply well_formed_output_asset_is_script_function. Qed.

Theorem different_scripts_can_pool_differently :
  exists o1 o2 : TxOut,
    out_value o1 = out_value o2 /\
    out_script o1 <> out_script o2 /\
    txout_asset_value AssetRES o1 <> txout_asset_value AssetRES o2.
Proof.
  exists (mk_res 1%Z), (mk_oru 1%Z).
  repeat split; try reflexivity.
  - unfold mk_res, mk_oru, spk_res, spk_oru. discriminate.
  - vm_compute. discriminate.
Qed.

Theorem well_formed_outputs_script_determines_asset :
  forall o1 o2 : TxOut,
    out_script o1 = out_script o2 ->
    output_asset o1 = output_asset o2.
Proof. apply well_formed_output_asset_is_script_function. Qed.

(** * 2. [cm_draw_won] is abstract — not owner / PCORB / Selection

    WE DO NOT CLAIM:
    - [cm_draw_won] equals the Selection kernel winner ([Some w])
    - [cm_draw_won] is [ConnectEvaluateCoinbaseResMint]
    - owner encoding, PCORB parse, or domain appear in [ConnectMintSpec]
    - [valid_res_mint_obligation] inspects [blk_txs]

    [ConnectMintSpec] has exactly three fields ([cm_draw_won],
    [cm_oru_subsidy], [cm_res_mint_count]). [TxAbs] has no σ / weight /
    Convergence_Root fields, so this cone cannot even *state* equality
    with [select_winner]. H-1 / winner-must-mint theorems quantify over
    this unconstrained boolean; they are Closed *under that abstraction*.

    [valid_res_mint_obligation] still ignores [blk_txs], and the theorem below still
    holds — but it is no longer the whole story for the mint count.
    [DualPoolConservation.valid_body_mint_count] is a separate conjunct that forces
    [cm_res_mint_count] to equal the mint-shape count observed in the body, so on the
    [apply_block_authorized] path the field can no longer lie about the transactions
    (that is what makes covenant-funded mints require a claimed win). The abstraction
    that stays is exactly [cm_draw_won]: coupling the count to the body means a body mint
    needs the block to *claim* a win, never that σ actually selected that owner.

    As shipped, [cm_draw_won] only ever bears on ORU-funded mints. [StateTransition.cvg_create_gate]
    forbids ticket creation and [StateTransition.apply_txs_preserves_cvg_free] carries
    ticket-freeness along a replay, so
    [StateTransition.scheduled_mint_shape_is_oru_funded_from_cvg_free] collapses
    [is_scheduled_mint_shape] to [res_mint_shape] — the covenant disjunct, the one carrying no
    winner authorization, is unreachable rather than merely rejected.

    THE MODEL OVER-APPROXIMATES THE NODE HERE. The covenant was retired from C++ outright: there
    is no OP_CVG_CLAIM opcode (0xbc is above MAX_OPCODE), no ASSET_CVG, no TxoutType::CVG_CLAIM,
    and no covenant shape check anywhere in consensus. A ticket could not authorize its own draw
    win, so a covenant-funded mint needed the same ConvergeProof as an ORU-funded one and bought
    nothing, while costing an asset class that was neither circulating ORU nor destroyed ORU.

    So this cone still *models* a class the node does not implement. That is the safe direction —
    the model admits strictly more transactions than the node accepts — and it is deliberate
    rather than drift: deleting [count_cvg] and friends would rewrite the core of [apply_tx] and
    discard proved theorems, whereas keeping them costs only this note. The C++ side satisfies
    [utxo_cvg_free] by construction, since no script it can classify is a covenant output, so
    every theorem downstream of that hypothesis applies to the node unconditionally.

    Read the covenant definitions as a design study of a rejected mechanism, not as a
    specification of shipped behaviour. Anything reviving them owes the node an opcode first.

    WE DO NOT CLAIM: that a covenant ticket placed in the *initial* UTXO set is harmless. The
    preservation theorems start from [utxo_cvg_free] and say nothing about a state that already
    holds a ticket; the [DualPoolConservation] covenant fixtures inject one on purpose. Since the
    node has no covenant script at all, no chain can reach such a state — but the gap is in the
    model, and whatever revives the mechanism inherits it along with the winner-authorization
    obligation. *)

Definition ob_hdr1 : BlockHeaderAbs :=
  {| hdr_height := 1%nat;
     hdr_prev_hash := hash32_zero;
     hdr_chainwork := 0%Z;
     hdr_nbits := 0%Z;
     hdr_time := 0%Z |}.

Definition ob_mint (won : bool) (n : nat) : ConnectMintSpec :=
  {| cm_draw_won := won; cm_oru_subsidy := 0%Z; cm_res_mint_count := n |}.

Definition ob_blk (won : bool) (n : nat) : BlockAbs :=
  {| blk_header := ob_hdr1; blk_txs := nil; blk_mint := ob_mint won n |}.

(** Two blocks may share header and body and still disagree on the winner
    flag. The flag is not a projection of [blk_txs]. *)
Theorem we_do_not_claim_cm_draw_won_determined_by_body :
  exists b1 b2 : BlockAbs,
    blk_header b1 = blk_header b2 /\
    blk_txs b1 = blk_txs b2 /\
    cm_draw_won (blk_mint b1) = true /\
    cm_draw_won (blk_mint b2) = false.
Proof.
  exists (ob_blk true 1), (ob_blk false 1).
  repeat split; reflexivity.
Qed.

(** Obligation looks at header height ([W(h)]) and the mint spec only. *)
Theorem valid_res_mint_obligation_ignores_body :
  forall (h : BlockHeaderAbs) (txs1 txs2 : list TxAbs) (m : ConnectMintSpec),
    valid_res_mint_obligation {| blk_header := h; blk_txs := txs1; blk_mint := m |} =
    valid_res_mint_obligation {| blk_header := h; blk_txs := txs2; blk_mint := m |}.
Proof. intros. reflexivity. Qed.

(** At a paying height, flipping the abstract flag flips H-1 obligation
    even when the scored mint count is the "right" one. That is the
    entire winner-authorization theory in this cone: a boolean, not
    owner / PCORB / Selection. *)
Theorem we_do_not_claim_cm_draw_won_is_owner_pcorb :
  valid_res_mint_obligation (ob_blk true 1) = true /\
  valid_res_mint_obligation (ob_blk false 1) = false.
Proof.
  split; vm_compute; reflexivity.
Qed.

(** * 3. [rs_res_supply] is a scalar — not [utxo_res]

    WE DO NOT CLAIM:
    - [rs_res_supply s = sum of RES values in [rs_utxo s]]
    - body application ([apply_tx]) updates the scalar
    - the mint-delta channel ([apply_connectblock_supply_delta]) updates UTXO

    Dual-pool [apply_block_res_invariant] is a statement about body net
    *plus* protocol mint on the scalar. It does not identify the two. *)

(** Same projection as [DualPoolConservation.utxo_res], inlined so this
    module does not depend on that file's theorem cone. *)
Definition utxo_res_proj (u : UTXOSet) : Z :=
  sum_asset AssetRES (map snd u).

Definition ob_diverged_state : ReplayState :=
  {| rs_utxo :=
       [(MkOutPoint hash32_zero 0%nat, mk_res 1%Z)];
     rs_chain := genesis_chain_meta;
     rs_oru_supply := 0%Z;
     rs_res_supply := 0%Z;
     rs_history := [] |}.

Theorem we_do_not_claim_rs_res_supply_equals_utxo_res :
  exists s : ReplayState,
    rs_res_supply s <> utxo_res_proj (rs_utxo s).
Proof.
  exists ob_diverged_state.
  unfold ob_diverged_state, utxo_res_proj.
  vm_compute. discriminate.
Qed.

Theorem apply_tx_does_not_update_rs_res_supply :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState),
    apply_tx s tx idx = Some s' ->
    rs_res_supply s' = rs_res_supply s.
Proof.
  intros s tx idx s' H.
  apply apply_tx_some_inv in H.
  destruct H as [u' [_ [_ [_ Hs]]]]. subst s'.
  unfold replay_set_utxo. reflexivity.
Qed.

Theorem apply_connectblock_supply_delta_does_not_update_utxo :
  forall (s : ReplayState) (b : BlockAbs),
    rs_utxo (apply_connectblock_supply_delta s b) = rs_utxo s.
Proof.
  intros s b.
  pose proof (apply_connectblock_supply_delta_preserves_other s b) as H.
  destruct H as [Hu _]. exact Hu.
Qed.

(** * 4. Shape vs classifier (no VM)

    Pooling consults [asset_kind_of_script] (which uses [is_res_script] at
    the shape layer). [created_script_shape_ok] is still a byte-template
    gate, not [EvalScript]. CVG2 scripts pool as ORU. *)
Require ScriptMintWitness.
Require InvalidCvg.

Theorem p2wpkh_is_not_res_script_but_shape_ok :
  ScriptMintWitness.is_res_script ScriptMintWitness.canonical_p2wpkh = false /\
  created_script_shape_ok
    {| out_value := 1%Z;
       out_script := ScriptMintWitness.canonical_p2wpkh |} = true /\
  output_asset {| out_value := 1%Z;
                  out_script := ScriptMintWitness.canonical_p2wpkh |} = AssetORU.
Proof. split; [|split]; vm_compute; reflexivity. Qed.

Theorem cvg2_script_pools_as_oru :
  InvalidCvg.is_cvg2_script InvalidCvg.canonical_cvg2_script = true /\
  output_asset {| out_value := 1%Z;
                  out_script := InvalidCvg.canonical_cvg2_script |} = AssetORU.
Proof. split; vm_compute; reflexivity. Qed.

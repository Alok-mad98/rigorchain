(** RIGOR v0.1 — unified mint / supply conservation (P0-036).

    Single transition (model of the ConnectBlock monetary slice):

      ledger_ORU'       = ledger_ORU + subsidy − converge
      circulating_ORU'  = circulating_ORU + subsidy − converge − fee_burn
      RES'              = RES + valid_mint

    Ledger follows RFC-002 ([m_chain_oru_total] does not subtract fee burn).
    Circulating follows RFC-018 (unclaimed burn never enters the UTXO set).

    Composes identities previously split across DualPool, fee-burn/BlockSupply,
    AssetStateTransition, and ConnectBlockSupply — in *this* state type.

    Not a C++ [ConnectBlock] refinement. No [Axiom] / [Admitted].
    Production W/T/reward/maturity/ScriptPubKey are not modified. *)
From Coq Require Import List.
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
Import ListNotations.
Require Import RigorV01.ResMintCorrectness.
Require Import RigorV01.WindowGeneric.
From Rigor Require Import ResSchedule.

Local Open Scope Z_scope.

(** * Unified supply state *)

Record UnifiedSupply := {
  us_ledger_oru : Z;
  us_circulating_oru : Z;
  us_res : Z;
  us_fee_burn_acc : Z
}.

Definition genesis_supply : UnifiedSupply :=
  {| us_ledger_oru := 0; us_circulating_oru := 0; us_res := 0; us_fee_burn_acc := 0 |}.

Definition total_ledger (s : UnifiedSupply) : Z :=
  us_ledger_oru s + us_res s.

Definition total_circulating (s : UnifiedSupply) : Z :=
  us_circulating_oru s + us_res s.

(** * One-block monetary delta *)

Record BlockDelta := {
  bd_height : Z;
  bd_subsidy : Z;
  bd_valid_mint : Z;
  bd_converge : Z;
  bd_fee_burn : Z;
  bd_ordinary : bool;
  bd_mint_authorized : bool;
  bd_has_invalid : bool
}.

Definition ordinary_delta (h sub conv burn : Z) : BlockDelta :=
  {| bd_height := h;
     bd_subsidy := sub;
     bd_valid_mint := 0;
     bd_converge := conv;
     bd_fee_burn := burn;
     bd_ordinary := true;
     bd_mint_authorized := false;
     bd_has_invalid := false |}.

Definition mint_cap_at (h : Z) : Z := get_res_mint_reward h.

Definition amounts_nonneg (d : BlockDelta) : bool :=
  (0 <=? bd_subsidy d) && (0 <=? bd_valid_mint d) &&
  (0 <=? bd_converge d) && (0 <=? bd_fee_burn d).

Definition mint_respects_authorization (d : BlockDelta) : bool :=
  if bd_mint_authorized d then
    Z.eqb (bd_valid_mint d) (mint_cap_at (bd_height d)) || Z.eqb (bd_valid_mint d) 0
  else
    Z.eqb (bd_valid_mint d) 0.

Definition ordinary_cannot_credit_mint (d : BlockDelta) : bool :=
  if bd_ordinary d then Z.eqb (bd_valid_mint d) 0 else true.

Definition delta_admissible (d : BlockDelta) : bool :=
  negb (bd_has_invalid d) &&
  amounts_nonneg d &&
  mint_respects_authorization d &&
  ordinary_cannot_credit_mint d &&
  (bd_valid_mint d <=? mint_cap_at (bd_height d)).

Definition apply_delta (s : UnifiedSupply) (d : BlockDelta)
  : option UnifiedSupply :=
  if delta_admissible d then
    Some {| us_ledger_oru := us_ledger_oru s + bd_subsidy d - bd_converge d;
            us_circulating_oru :=
              us_circulating_oru s + bd_subsidy d - bd_converge d - bd_fee_burn d;
            us_res := us_res s + bd_valid_mint d;
            us_fee_burn_acc := us_fee_burn_acc s + bd_fee_burn d |}
  else
    None.

Fixpoint apply_deltas (s : UnifiedSupply) (ds : list BlockDelta)
  : option UnifiedSupply :=
  match ds with
  | [] => Some s
  | d :: rest =>
      match apply_delta s d with
      | None => None
      | Some s' => apply_deltas s' rest
      end
  end.

(** * Conservation identity *)

Lemma apply_delta_some_admissible :
  forall s d s',
    apply_delta s d = Some s' ->
    delta_admissible d = true.
Proof.
  intros s d s' H. unfold apply_delta in H.
  destruct (delta_admissible d); [reflexivity | discriminate].
Qed.

Theorem mint_supply_conservation :
  forall s d s',
    apply_delta s d = Some s' ->
    total_circulating s' =
      total_circulating s + bd_subsidy d + bd_valid_mint d
        - bd_converge d - bd_fee_burn d.
Proof.
  intros s d s' H.
  unfold apply_delta in H.
  destruct (delta_admissible d); [|discriminate].
  inversion H. subst s'.
  unfold total_circulating. simpl. lia.
Qed.

Theorem ledger_conservation :
  forall s d s',
    apply_delta s d = Some s' ->
    total_ledger s' =
      total_ledger s + bd_subsidy d + bd_valid_mint d - bd_converge d.
Proof.
  intros s d s' H.
  unfold apply_delta in H.
  destruct (delta_admissible d); [|discriminate].
  inversion H. subst s'.
  unfold total_ledger. simpl. lia.
Qed.

Theorem fee_burn_does_not_enter_circulating :
  forall s d s',
    apply_delta s d = Some s' ->
    us_circulating_oru s' =
      us_circulating_oru s + bd_subsidy d - bd_converge d - bd_fee_burn d.
Proof.
  intros s d s' H.
  unfold apply_delta in H.
  destruct (delta_admissible d); [|discriminate].
  inversion H. subst s'. reflexivity.
Qed.

Theorem fee_burn_does_not_reduce_res :
  forall s d s',
    apply_delta s d = Some s' ->
    us_res s' = us_res s + bd_valid_mint d.
Proof.
  intros s d s' H.
  unfold apply_delta in H.
  destruct (delta_admissible d); [|discriminate].
  inversion H. subst s'. reflexivity.
Qed.

Theorem converge_reduces_oru :
  forall s d s',
    apply_delta s d = Some s' ->
    us_ledger_oru s' = us_ledger_oru s + bd_subsidy d - bd_converge d.
Proof.
  intros s d s' H.
  unfold apply_delta in H.
  destruct (delta_admissible d); [|discriminate].
  inversion H. subst s'. reflexivity.
Qed.

(** * Negative: ordinary tx cannot mint *)

Lemma ordinary_forces_zero_mint :
  forall d, delta_admissible d = true -> bd_ordinary d = true ->
            bd_valid_mint d = 0.
Proof.
  intros d Adm Ho.
  unfold delta_admissible in Adm.
  apply andb_true_iff in Adm. destruct Adm as [Adm Hcap].
  apply andb_true_iff in Adm. destruct Adm as [Adm Hord].
  unfold ordinary_cannot_credit_mint in Hord.
  rewrite Ho in Hord. apply Z.eqb_eq in Hord. exact Hord.
Qed.

Theorem ordinary_tx_cannot_mint :
  forall s d s',
    apply_delta s d = Some s' ->
    bd_ordinary d = true ->
    us_res s' = us_res s.
Proof.
  intros s d s' H Ho.
  pose proof (apply_delta_some_admissible s d s' H) as Adm.
  pose proof (ordinary_forces_zero_mint d Adm Ho) as Hz.
  pose proof (fee_burn_does_not_reduce_res s d s' H) as Hr.
  rewrite Hz in Hr. lia.
Qed.

(** * Negative: invalid mint / invalid tx cannot enter supply *)

Theorem invalid_tx_cannot_enter_supply :
  forall s d,
    bd_has_invalid d = true ->
    apply_delta s d = None.
Proof.
  intros s d Hinv.
  unfold apply_delta, delta_admissible.
  rewrite Hinv. reflexivity.
Qed.

Theorem unauthorized_mint_rejected :
  forall s d,
    bd_mint_authorized d = false ->
    bd_valid_mint d <> 0 ->
    apply_delta s d = None.
Proof.
  intros s d Hauth Hnz.
  unfold apply_delta, delta_admissible, mint_respects_authorization.
  rewrite Hauth.
  destruct (Z.eqb (bd_valid_mint d) 0) eqn:Hz.
  - apply Z.eqb_eq in Hz. contradiction.
  - simpl.
    destruct (negb (bd_has_invalid d)); [|reflexivity].
    destruct (amounts_nonneg d); [|reflexivity].
    reflexivity.
Qed.

Theorem invalid_mint_cannot_enter_supply :
  forall s d,
    bd_mint_authorized d = false ->
    bd_valid_mint d <> 0 ->
    apply_delta s d = None.
Proof. exact unauthorized_mint_rejected. Qed.

Theorem mint_cannot_exceed_schedule :
  forall s d,
    mint_cap_at (bd_height d) < bd_valid_mint d ->
    apply_delta s d = None.
Proof.
  intros s d Hlt.
  unfold apply_delta, delta_admissible.
  destruct (negb (bd_has_invalid d) && amounts_nonneg d &&
            mint_respects_authorization d && ordinary_cannot_credit_mint d)
    eqn:P; [|reflexivity].
  destruct (bd_valid_mint d <=? mint_cap_at (bd_height d)) eqn:Hle.
  - apply Z.leb_le in Hle. lia.
  - reflexivity.
Qed.

(** * Negative: immature mint cannot spend *)

Theorem immature_mint_cannot_spend :
  forall (l : ResMintLock) (coin_height spend_height : Z),
    spend_height < coin_height + coinbase_maturity ->
    mint_spend_height_ok l coin_height spend_height = false.
Proof. exact mint_cannot_bypass_coinbase_maturity. Qed.

Theorem immature_script_lock_cannot_spend :
  forall (l : ResMintLock) (h : Z),
    h < mint_maturity l ->
    script_lock_satisfied l h = false.
Proof. exact mint_script_lock_cannot_bypass_maturity_height. Qed.

(** Maturity is not a function of draw-window W. *)
Theorem maturity_independent_of_draw_window :
  coinbase_maturity = WindowGeneric.COINBASE_MATURITY /\
  WindowGeneric.COINBASE_MATURITY = 100.
Proof. split; reflexivity. Qed.

(** * Replay conservation *)

(** * Replay conservation *)

Fixpoint sum_subsidy (ds : list BlockDelta) : Z :=
  match ds with
  | [] => 0
  | d :: rest => bd_subsidy d + sum_subsidy rest
  end.

Fixpoint sum_mint (ds : list BlockDelta) : Z :=
  match ds with
  | [] => 0
  | d :: rest => bd_valid_mint d + sum_mint rest
  end.

Fixpoint sum_converge (ds : list BlockDelta) : Z :=
  match ds with
  | [] => 0
  | d :: rest => bd_converge d + sum_converge rest
  end.

Fixpoint sum_fee_burn (ds : list BlockDelta) : Z :=
  match ds with
  | [] => 0
  | d :: rest => bd_fee_burn d + sum_fee_burn rest
  end.

Theorem replay_conservation :
  forall ds s s',
    apply_deltas s ds = Some s' ->
    total_circulating s' =
      total_circulating s + sum_subsidy ds + sum_mint ds
        - sum_converge ds - sum_fee_burn ds.
Proof.
  induction ds as [|d rest IH]; intros s s' Happ.
  - simpl in Happ. inversion Happ. subst s'. simpl. lia.
  - simpl in Happ.
    destruct (apply_delta s d) as [s1|] eqn:H1; [|discriminate].
    specialize (IH s1 s' Happ).
    pose proof (mint_supply_conservation s d s1 H1) as Hstep.
    simpl. lia.
Qed.

(** * Pool identity recovered from the unified state *)

Theorem unified_matches_pool_identity :
  forall s d s',
    apply_delta s d = Some s' ->
    us_res s' = us_res s + bd_valid_mint d /\
    us_ledger_oru s' = us_ledger_oru s + bd_subsidy d - bd_converge d.
Proof.
  intros s d s' H. split.
  - eapply fee_burn_does_not_reduce_res; eauto.
  - eapply converge_reduces_oru; eauto.
Qed.

Theorem cpp_refinement_not_claimed_here :
  forall (P : Prop), P -> P.
Proof. intros P H. exact H. Qed.

(** Fee / reward / subsidy algebra. Schedule-side model only.

    Unifies old_supply + subsidy + fees - burn = new_supply.

    [economic_no_hidden_creation] used to be
    [split; reflexivity] — it asserted that two spellings of the
    economic step are syntactically equal. That detects a definitional
    divergence; it cannot detect hidden creation, which is a statement
    about how supply evolves over a chain of blocks. It is now the
    chain statement, proved by induction: net issuance across n blocks,
    with the fee flow subtracted out, is bounded by n times the maximum
    block subsidy.

    [subsidy_schedule_bound] used to be [intros e [Hs _]. exact Hs.] —
    the bound was a conjunct of the assumed [valid_econ]. It is now
    stated over a decision procedure, so discharging it needs the
    reflection lemma.

    NOT CLAIMED: runtime S_RES reject, C++ EvaluateSupply, node accounting.
*)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Bool.Bool.
From Coq Require Import Lia.
Import ListNotations.
From RigorKernel Require Import Supply.ORUModel.
From RigorKernel Require Import Supply.SupplySchedule.

Local Open Scope Z_scope.

Record EconomicStep := {
  es_old : Z;
  es_subsidy : Z;
  es_fees : Z;
  es_burn : Z
}.

(** Honest transition. M20 adds a second [es_fees] term (fees mint coins). *)
Definition apply_econ (e : EconomicStep) : Z :=
  es_old e + es_subsidy e + es_fees e - es_burn e.

Definition valid_econ (e : EconomicStep) : Prop :=
  0 <= es_subsidy e <= ORU_INITIAL_BLOCK_SUBSIDY /\
  0 <= es_fees e /\
  0 <= es_burn e <= es_fees e.

(** * Decision procedure *)

Definition check_econ (e : EconomicStep) : bool :=
  andb (0 <=? es_subsidy e)
    (andb (es_subsidy e <=? ORU_INITIAL_BLOCK_SUBSIDY)
      (andb (0 <=? es_fees e)
        (andb (0 <=? es_burn e) (es_burn e <=? es_fees e)))).

Theorem check_econ_reflects :
  forall e : EconomicStep, check_econ e = true <-> valid_econ e.
Proof.
  intros e. unfold check_econ, valid_econ. split.
  - intros H.
    apply andb_true_iff in H as [H1 H].
    apply andb_true_iff in H as [H2 H].
    apply andb_true_iff in H as [H3 H].
    apply andb_true_iff in H as [H4 H5].
    apply Z.leb_le in H1. apply Z.leb_le in H2. apply Z.leb_le in H3.
    apply Z.leb_le in H4. apply Z.leb_le in H5.
    repeat split; assumption.
  - intros [[H1 H2] [H3 [H4 H5]]].
    apply Z.leb_le in H1. apply Z.leb_le in H2. apply Z.leb_le in H3.
    apply Z.leb_le in H4. apply Z.leb_le in H5.
    rewrite H1, H2, H3, H4, H5. reflexivity.
Qed.

Corollary check_econ_false_iff :
  forall e : EconomicStep, check_econ e = false <-> ~ valid_econ e.
Proof.
  intros e. split.
  - intros Hf Hv. apply check_econ_reflects in Hv.
    rewrite Hf in Hv. discriminate.
  - intros Hn. destruct (check_econ e) eqn:E; [| reflexivity].
    exfalso. apply Hn, check_econ_reflects, E.
Qed.

(** * Single-step algebra *)

Theorem fees_only_transfer :
  forall e : EconomicStep,
    apply_econ e - (es_old e + es_subsidy e - es_burn e) = es_fees e.
Proof.
  intros e. unfold apply_econ. lia.
Qed.

Theorem burn_only_removal :
  forall e : EconomicStep,
    apply_econ e + es_burn e = es_old e + es_subsidy e + es_fees e.
Proof.
  intros e. unfold apply_econ. lia.
Qed.

Theorem subsidy_schedule_bound :
  forall e : EconomicStep,
    check_econ e = true ->
    0 <= es_subsidy e <= ORU_INITIAL_BLOCK_SUBSIDY.
Proof.
  intros e H. apply check_econ_reflects in H.
  destruct H as [Hs _]. exact Hs.
Qed.

(** The two spellings of the economic step agree. This is a
    definitional identity and is labelled as one; it is not an
    anti-inflation result. *)
Lemma apply_econ_matches_oru_transition :
  forall e : EconomicStep,
    apply_econ e = ORU_supply_transition (es_old e) (es_subsidy e)
                     (es_fees e) (es_burn e).
Proof.
  intros e. unfold apply_econ, ORU_supply_transition. reflexivity.
Qed.

(** * Chain accounting

    Supply evolution over a sequence of blocks. This is where "no
    hidden creation" can actually be stated. *)

Definition step_with_old (e : EconomicStep) (old : Z) : EconomicStep :=
  {| es_old := old;
     es_subsidy := es_subsidy e;
     es_fees := es_fees e;
     es_burn := es_burn e |}.

Fixpoint econ_chain (init : Z) (es : list EconomicStep) : Z :=
  match es with
  | [] => init
  | e :: rest => econ_chain (apply_econ (step_with_old e init)) rest
  end.

Fixpoint sum_subsidy (es : list EconomicStep) : Z :=
  match es with [] => 0 | e :: r => es_subsidy e + sum_subsidy r end.

Fixpoint sum_fees (es : list EconomicStep) : Z :=
  match es with [] => 0 | e :: r => es_fees e + sum_fees r end.

Fixpoint sum_burn (es : list EconomicStep) : Z :=
  match es with [] => 0 | e :: r => es_burn e + sum_burn r end.

Lemma econ_chain_conservation :
  forall (es : list EconomicStep) (init : Z),
    econ_chain init es
      = init + sum_subsidy es + sum_fees es - sum_burn es.
Proof.
  induction es as [| e rest IH]; intros init; simpl; [lia |].
  rewrite IH. unfold apply_econ, step_with_old. simpl. lia.
Qed.

Lemma sum_subsidy_bounded :
  forall es : list EconomicStep,
    Forall valid_econ es ->
    sum_subsidy es <= Z.of_nat (length es) * ORU_INITIAL_BLOCK_SUBSIDY.
Proof.
  induction es as [| e rest IH]; intros H.
  - simpl. lia.
  - inversion H as [| ? ? He Hrest]; subst.
    destruct He as [[Hs0 Hsmax] _].
    specialize (IH Hrest).
    replace (length (e :: rest)) with (S (length rest)) by reflexivity.
    rewrite Nat2Z.inj_succ, Z.mul_succ_l.
    simpl sum_subsidy.
    lia.
Qed.

Lemma sum_subsidy_nonneg :
  forall es : list EconomicStep,
    Forall valid_econ es -> 0 <= sum_subsidy es.
Proof.
  induction es as [| e rest IH]; intros H; simpl; [lia |].
  inversion H as [| ? ? He Hrest]; subst.
  destruct He as [[Hs0 _] _]. specialize (IH Hrest). lia.
Qed.

Lemma sum_burn_le_sum_fees :
  forall es : list EconomicStep,
    Forall valid_econ es -> sum_burn es <= sum_fees es.
Proof.
  induction es as [| e rest IH]; intros H; simpl; [lia |].
  inversion H as [| ? ? He Hrest]; subst.
  destruct He as [_ [_ [_ Hbf]]]. specialize (IH Hrest). lia.
Qed.

(** * No hidden creation

    Across any chain of valid blocks, the supply increase that is NOT
    accounted for by the fee flow is bounded by the number of blocks
    times the maximum block subsidy. There is no other source of
    coins. Proved by induction over the chain; adding a second fee
    term to [apply_econ] (M20) breaks [econ_chain_conservation] and
    therefore this. *)

Theorem economic_no_hidden_creation :
  forall (es : list EconomicStep) (init : Z),
    Forall valid_econ es ->
    econ_chain init es - init - (sum_fees es - sum_burn es)
      <= Z.of_nat (length es) * ORU_INITIAL_BLOCK_SUBSIDY.
Proof.
  intros es init H.
  rewrite econ_chain_conservation.
  pose proof (sum_subsidy_bounded es H) as Hb.
  lia.
Qed.

(** Net issuance is exactly the subsidy sum once fee flow is removed:
    fees are a transfer, not a source. *)
Theorem economic_net_issuance_is_subsidy :
  forall (es : list EconomicStep) (init : Z),
    econ_chain init es - init - (sum_fees es - sum_burn es)
      = sum_subsidy es.
Proof.
  intros es init. rewrite econ_chain_conservation. lia.
Qed.

(** A valid chain never destroys supply. *)
Theorem econ_chain_no_supply_loss :
  forall (es : list EconomicStep) (init : Z),
    Forall valid_econ es ->
    init <= econ_chain init es.
Proof.
  intros es init H.
  rewrite econ_chain_conservation.
  pose proof (sum_subsidy_nonneg es H).
  pose proof (sum_burn_le_sum_fees es H).
  lia.
Qed.

(** * Non-vacuity *)

Definition sample_block : EconomicStep :=
  {| es_old := 0; es_subsidy := 1; es_fees := 5; es_burn := 2 |}.

Definition invalid_block : EconomicStep :=
  {| es_old := 0; es_subsidy := -1; es_fees := 5; es_burn := 2 |}.

Theorem check_econ_accepts_something :
  check_econ sample_block = true.
Proof. reflexivity. Qed.

Theorem check_econ_rejects_negative_subsidy :
  check_econ invalid_block = false.
Proof. reflexivity. Qed.

Theorem check_econ_is_not_constant :
  (exists e, check_econ e = true) /\ (exists e, check_econ e = false).
Proof.
  split.
  - exists sample_block. exact check_econ_accepts_something.
  - exists invalid_block. exact check_econ_rejects_negative_subsidy.
Qed.

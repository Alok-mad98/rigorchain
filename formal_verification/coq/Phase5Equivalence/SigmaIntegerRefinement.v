(** Phase5Equivalence/SigmaIntegerRefinement.v
    PD-15 sigma integer refinement closure (TASK-PROOF-020).

    Formalizes PROOF-011 A-1 little-endian pin:

      H_i = Σ_{k=0}^{n-1} σ_bytes[k] · 256^k

    HONESTY BOUNDARY:
    - Coq refinement only; no consensus / runtime / FVR changes.
    - [equivalence_claimed] remains false.
    - Closes PD15 in the dependency catalog; PD14 / PD13 / PD4F_OB_02 stay open. *)

From Coq Require Import Arith.Arith List Bool ZArith Lia Ring.
Import ListNotations.
Require Import Rigor.RigorBytes.

(** H_i = Σ sigma[k] · 256^k  (little-endian, unsigned). *)
Fixpoint sigma_le_sum_nat (bs : list byte) : nat :=
  match bs with
  | nil => 0
  | b :: rest => (b + Nat.mul 256 (sigma_le_sum_nat rest))%nat
  end.

Definition sigma_byte_length : nat := 32%nat.

Fixpoint all_byte_okb (bs : list byte) : bool :=
  match bs with
  | nil => true
  | b :: rest => (b <=? 255)%nat && all_byte_okb rest
  end.

Definition is_sigma_bytes (bs : list byte) : bool :=
  is_bytes32 bs && all_byte_okb bs.

Lemma all_byte_okb_iff :
  forall bs, all_byte_okb bs = true <-> all_byte_ok bs.
Proof.
  induction bs as [|b bs IH]; simpl; [split; intros; auto |].
  split; intros H.
  - apply Bool.andb_true_iff in H. destruct H as [Hb Hrest].
    split; [exact Hb | apply IH; exact Hrest].
  - destruct H as [Hb Hrest]. apply Bool.andb_true_iff. split.
    + exact Hb.
    + apply IH. exact Hrest.
Qed.


Definition sigma_bytes_to_integer (bs : list byte) : Z :=
  Z.of_nat (sigma_le_sum_nat bs).

Section SigmaIntegerBridge.
Local Open Scope Z_scope.

Lemma sigma_le_sum_nat_eq_go :
  forall bs,
  Z.of_nat (sigma_le_sum_nat bs) = uint256_of_bytes_go bs 0 1.
Proof.
  induction bs as [|b bs IH].
  - simpl. reflexivity.
  - cbn [sigma_le_sum_nat].
    rewrite Nat2Z.inj_add.
    rewrite ofnat_mul256.
    rewrite IH.
    rewrite <- uint256_go_cons.
    reflexivity.
Qed.

End SigmaIntegerBridge.

Section SigmaPartA.
Local Open Scope Z_scope.

(** ================================================================
    Part A:  Little-endian sigma integer interpretation
    ================================================================ *)

Lemma sigma_bytes_to_integer_eq_go_all :
  forall bs,
  sigma_bytes_to_integer bs = uint256_of_bytes_go bs 0 1.
Proof.
  intros bs. unfold sigma_bytes_to_integer. apply sigma_le_sum_nat_eq_go.
Qed.

Lemma sigma_bytes_to_integer_as_le_sum :
  forall bs,
  sigma_bytes_to_integer bs = Z.of_nat (sigma_le_sum_nat bs).
Proof.
  intros bs. unfold sigma_bytes_to_integer. reflexivity.
Qed.

End SigmaPartA.

(** ================================================================
    Part B:  Nat decoder (left inverse at fixed length)
    ================================================================ *)

Section SigmaNatDecoder.
Local Close Scope Z_scope.
Local Close Scope string_scope.

Fixpoint sigma_decode_fuel_nat (n : nat) (fuel : nat) : list byte :=
  match fuel with
  | O => nil
  | S fuel' =>
      (Nat.modulo n 256) :: sigma_decode_fuel_nat (Nat.div n 256) fuel'
  end.

Definition sigma_decode32_nat (n : nat) : list byte :=
  sigma_decode_fuel_nat n sigma_byte_length.

Lemma sigma_decode_fuel_s :
  forall n fuel,
  sigma_decode_fuel_nat n (S fuel) =
  Nat.modulo n 256 :: sigma_decode_fuel_nat (Nat.div n 256) fuel.
Proof. intros. reflexivity. Qed.

Lemma nat_mod_div :
  forall (b n : nat),
  (b <=? 255)%nat = true ->
  Nat.modulo (b + Nat.mul 256 n) 256 = b /\
  Nat.div (b + Nat.mul 256 n) 256 = n.
Proof.
  intros b n Hb.
  assert (Hle : (b <= 255)%nat) by (apply Nat.leb_le; exact Hb).
  assert (H256 : 256 <> 0) by discriminate.
  split.
  - rewrite (Nat.mul_comm 256 n).
    rewrite (Nat.mod_add b n 256 H256).
    apply Nat.mod_small. lia.
  - rewrite (Nat.mul_comm 256 n).
    rewrite (Nat.div_add b n 256 H256).
    rewrite Nat.div_small; lia.
Qed.

Lemma sigma_cons_sum_parts_eq :
  forall b1 b2 s1 s2,
  (b1 <=? 255)%nat = true ->
  (b2 <=? 255)%nat = true ->
  (b1 + Nat.mul 256 s1 = b2 + Nat.mul 256 s2)%nat ->
  b1 = b2 /\ s1 = s2.
Proof.
  intros b1 b2 s1 s2 Hb1 Hb2 Heq.
  destruct (nat_mod_div b1 s1) as [Hmod1 Hdiv1]; auto.
  destruct (nat_mod_div b2 s2) as [Hmod2 Hdiv2]; auto.
  assert (Hb_eq : b1 = b2).
  {
    transitivity (Nat.modulo (b1 + Nat.mul 256 s1) 256).
    - symmetry. exact Hmod1.
    - rewrite Heq. exact Hmod2.
  }
  split; auto.
  subst b2. lia.
Qed.

Lemma sigma_le_sum_nat_injective :
  forall bs1 bs2,
  length bs1 = length bs2 ->
  (forall b, In b bs1 -> (b <=? 255)%nat = true) ->
  (forall b, In b bs2 -> (b <=? 255)%nat = true) ->
  sigma_le_sum_nat bs1 = sigma_le_sum_nat bs2 ->
  bs1 = bs2.
Proof.
  intros bs1 bs2. revert bs2.
  induction bs1 as [|b1 bs1 IH]; intros bs2 Hlen Hok1 Hok2 Heq.
  - destruct bs2 as [|b2 bs2]; simpl in Hlen; try discriminate.
    reflexivity.
  - destruct bs2 as [|b2 bs2]; simpl in Hlen; try discriminate.
    apply Nat.succ_inj in Hlen.
    cbn [sigma_le_sum_nat] in Heq.
    assert (Hb1 : (b1 <=? 255)%nat = true) by (apply Hok1; simpl; auto).
    assert (Hb2 : (b2 <=? 255)%nat = true) by (apply Hok2; simpl; auto).
    destruct (sigma_cons_sum_parts_eq b1 b2 (sigma_le_sum_nat bs1) (sigma_le_sum_nat bs2)
                Hb1 Hb2 Heq) as [Hb Htail].
    assert (Hok1' : forall b, In b bs1 -> (b <=? 255)%nat = true).
    { intros b Hin. apply Hok1. simpl. right. exact Hin. }
    assert (Hok2' : forall b, In b bs2 -> (b <=? 255)%nat = true).
    { intros b Hin. apply Hok2. simpl. right. exact Hin. }
    subst b1. f_equal.
    apply (IH bs2 Hlen Hok1' Hok2' Htail).
Qed.

Lemma sigma_le_sum_nat_bound :
  forall bs n,
  length bs = n ->
  (forall b, In b bs -> (b <=? 255)%nat = true) ->
  (sigma_le_sum_nat bs < Nat.pow 256 n)%nat.
Proof.
  induction bs as [|b bs IH]; intros n Hlen Hok.
  - destruct n as [|n']; [ | simpl in Hlen; discriminate ].
    cbn. apply Nat.lt_0_1.
  - simpl in Hlen.
    destruct n as [|n'] eqn:En; [discriminate |].
    inversion Hlen. subst n'.
    cbn [sigma_le_sum_nat].
    assert (Hb : (b <= 255)%nat) by (apply Nat.leb_le; apply Hok; simpl; auto).
    assert (Hrest : forall x, In x bs -> (x <=? 255)%nat = true).
    { intros x Hin. apply Hok. simpl. right. exact Hin. }
    assert (Hinner : (sigma_le_sum_nat bs < Nat.pow 256 (length bs))%nat).
    { apply IH; auto. }
    assert (Hpow : (Nat.pow 256 (S (length bs)) = Nat.mul 256 (Nat.pow 256 (length bs)))%nat).
    { apply Nat.pow_succ_r. lia. }
    rewrite Hpow. lia.
Qed.

Lemma sigma_le_sum_nat_bound_32 :
  forall bs,
  length bs = sigma_byte_length ->
  (forall b, In b bs -> (b <=? 255)%nat = true) ->
  (sigma_le_sum_nat bs < Nat.pow 256 sigma_byte_length)%nat.
Proof.
  intros bs Hlen Hok.
  assert (H32 : sigma_byte_length = 32%nat) by reflexivity.
  rewrite H32. apply sigma_le_sum_nat_bound with (n := 32); auto.
Qed.

Lemma pow2_8 : Nat.pow 2 8 = 256.
Proof. vm_compute. reflexivity. Qed.

Lemma pow256_to_pow2 :
  forall k, Nat.pow 256 k = Nat.pow 2 (8 * k).
Proof.
  induction k as [|k IH].
  - reflexivity.
  - cbn [Nat.pow]. rewrite IH.
    assert (H256 : 256 = Nat.pow 2 8) by exact pow2_8.
    rewrite H256. rewrite <- Nat.pow_add_r by lia.
    f_equal. lia.
Qed.

Lemma nat_pow256_32 : (Nat.pow 256 32 = Nat.pow 2 256).
Proof. exact (pow256_to_pow2 32). Qed.

End SigmaNatDecoder.

Section ZModBridge.
Local Open Scope Z_scope.

Lemma z_mod_identity (a b : Z) :
  (0 <= a)%Z -> (a < b)%Z -> (0 < b)%Z -> Z.modulo a b = a.
Proof.
  intros Ha Hlt Hbpos.
  assert (Hdiv : Z.div a b = 0) by (apply Z.div_small; auto).
  assert (Hb : b <> 0) by lia.
  assert (Heq : a = b * Z.div a b + Z.modulo a b) by (apply Z.div_mod; auto).
  rewrite Hdiv in Heq. lia.
Qed.

End ZModBridge.

Section SigmaGoModIdentity.
Local Close Scope Z_scope.
Local Close Scope string_scope.

Lemma sigma_go_mod_identity_32 :
  forall bs,
  length bs = sigma_byte_length ->
  (forall b, In b bs -> (b <=? 255)%nat = true) ->
  uint256_of_bytes_go bs 0 1 =
  Z.modulo (uint256_of_bytes_go bs 0 1) (Z.shiftl 1 256).
Proof.
  intros bs Hlen Hok.
  apply eq_sym.
  assert (H32 : sigma_byte_length = 32%nat) by reflexivity.
  assert (Hbound : (sigma_le_sum_nat bs < Nat.pow 256 32)%nat).
  { rewrite H32 in Hlen. apply sigma_le_sum_nat_bound_32; auto. }
  assert (H0 : (0 <= uint256_of_bytes_go bs 0 1)%Z).
  { rewrite <- sigma_le_sum_nat_eq_go. lia. }
  assert (Hlt : (uint256_of_bytes_go bs 0 1 < Z.shiftl 1 256)%Z).
  { rewrite <- sigma_le_sum_nat_eq_go.
    assert (Hz : (Z.of_nat (sigma_le_sum_nat bs) < Z.of_nat (Nat.pow 2 256))%Z).
    { apply Nat2Z.inj_lt. rewrite <- nat_pow256_32. exact Hbound. }
    assert (Heq : (Z.of_nat (Nat.pow 2 256) = Z.shiftl 1 256)%Z).
    { rewrite Nat2Z.inj_pow. reflexivity. }
    lia. }
  assert (Hpos : (0 < Z.shiftl 1 256)%Z) by (simpl; lia).
  exact (z_mod_identity (uint256_of_bytes_go bs 0 1) (Z.shiftl 1 256) H0 Hlt Hpos).
Qed.

End SigmaGoModIdentity.

Lemma sigma_bytes_to_integer_eq_le_sum_on_sigma32 :
  forall bs,
  is_sigma_bytes bs = true ->
  sigma_bytes_to_integer bs = Z.of_nat (sigma_le_sum_nat bs).
Proof.
  intros bs _. unfold sigma_bytes_to_integer. reflexivity.
Qed.

Lemma sigma_bytes_to_integer_eq_go_on_sigma32 :
  forall bs,
  is_sigma_bytes bs = true ->
  sigma_bytes_to_integer bs = uint256_of_bytes_go bs 0 1.
Proof.
  intros bs Hob.
  rewrite sigma_bytes_to_integer_eq_le_sum_on_sigma32; auto.
  rewrite sigma_le_sum_nat_eq_go. reflexivity.
Qed.

(** ================================================================
    Part C:  Runtime / spec consistency
    ================================================================ *)

Section SigmaPartC.
Local Open Scope Z_scope.

Lemma in_list_all_byte_ok_leb :
  forall bs b, all_byte_ok bs -> In b bs -> (b <=? 255)%nat = true.
Proof.
  induction bs as [|b bs IH]; intros b0 Hall Hin; simpl.
  - destruct Hin.
  - destruct Hin as [-> | Hin].
    + destruct Hall as [Hb _]. exact Hb.
    + destruct Hall as [_ Hall']. apply IH; auto.
Qed.

Lemma runtime_spec_consistency :
  forall (bs : list byte),
  is_sigma_bytes bs = true ->
  sigma_bytes_to_integer bs = uint256_of_bytes bs.
Proof.
  intros bs Hob.
  rewrite sigma_bytes_to_integer_eq_go_on_sigma32; auto.
  unfold uint256_of_bytes.
  apply sigma_go_mod_identity_32.
  - apply Bool.andb_true_iff in Hob. destruct Hob as [Hlen _].
    apply Nat.eqb_eq. exact Hlen.
  - intros b Hin.
    assert (Hall : all_byte_ok bs).
    { destruct (all_byte_okb_iff bs) as [Hto _].
      apply Hto.
      apply Bool.andb_true_iff in Hob. destruct Hob as [_ Hokb]. exact Hokb. }
    apply in_list_all_byte_ok_leb with (bs := bs); auto.
Qed.

Lemma runtime_spec_le_sum_consistency :
  forall bs,
  is_sigma_bytes bs = true ->
  Z.of_nat (sigma_le_sum_nat bs) = uint256_of_bytes bs.
Proof.
  intros bs Hob.
  rewrite <- sigma_bytes_to_integer_eq_le_sum_on_sigma32; auto.
  apply runtime_spec_consistency. exact Hob.
Qed.

(** ================================================================
    Part D:  Determinism
    ================================================================ *)

Lemma sigma_bytes_to_integer_deterministic :
  forall (bs1 bs2 : list byte),
  bs1 = bs2 ->
  sigma_bytes_to_integer bs1 = sigma_bytes_to_integer bs2.
Proof.
  intros bs1 bs2 Heq. subst bs2. reflexivity.
Qed.

(* VACUOUS-PLACEHOLDER (SECURITY-001-F / F-4): the statement below has the
   form [X = X]. It is trivially true by reflexivity and constrains nothing
   about [sigma_bytes_to_integer_functional]. Retained for catalog shape only. NOT EVIDENCE. *)
Lemma sigma_bytes_to_integer_functional :
  forall (bs : list byte), sigma_bytes_to_integer bs = sigma_bytes_to_integer bs.
Proof. intros. reflexivity. Qed.

(** ================================================================
    Part E:  Fixed-length injectivity (32-byte sigma digests)
    ================================================================ *)

Lemma sigma_bytes_to_integer_injective_length_fixed :
  forall (bs1 bs2 : list byte),
  length bs1 = sigma_byte_length ->
  length bs2 = sigma_byte_length ->
  is_sigma_bytes bs1 = true ->
  is_sigma_bytes bs2 = true ->
  sigma_bytes_to_integer bs1 = sigma_bytes_to_integer bs2 ->
  bs1 = bs2.
Proof.
  intros bs1 bs2 H1 H2 Hok1 Hok2 Heq.
  unfold is_sigma_bytes in Hok1, Hok2.
  apply Bool.andb_true_iff in Hok1. apply Bool.andb_true_iff in Hok2.
  destruct Hok1 as [_ Hok1b]. destruct Hok2 as [_ Hok2b].
  destruct (all_byte_okb_iff bs1) as [Hto1 _].
  destruct (all_byte_okb_iff bs2) as [Hto2 _].
  assert (Hok1 : all_byte_ok bs1) by (apply Hto1; exact Hok1b).
  assert (Hok2 : all_byte_ok bs2) by (apply Hto2; exact Hok2b).
  assert (Hok1' : forall b, In b bs1 -> (b <=? 255)%nat = true).
  { intros b Hin. apply in_list_all_byte_ok_leb with (bs := bs1); auto. }
  assert (Hok2' : forall b, In b bs2 -> (b <=? 255)%nat = true).
  { intros b Hin. apply in_list_all_byte_ok_leb with (bs := bs2); auto. }
  assert (Hnat1 : sigma_le_sum_nat bs1 = sigma_le_sum_nat bs2).
  { unfold sigma_bytes_to_integer in Heq. apply Nat2Z.inj. exact Heq. }
  assert (Hlen : length bs1 = length bs2) by (rewrite H1, H2; reflexivity).
  apply (sigma_le_sum_nat_injective bs1 bs2 Hlen Hok1' Hok2' Hnat1).
Qed.

End SigmaPartC.

Require Import Rigor.Phase5Equivalence.ProofObligationDependencyCatalog.

(** ================================================================
    Part F:  PD-15 closure predicate and soundness
    ================================================================ *)

Definition pd15_refinement_lemmas_discharged : Prop :=
  (forall bs1 bs2,
     bs1 = bs2 ->
     sigma_bytes_to_integer bs1 = sigma_bytes_to_integer bs2) /\
  (forall bs1 bs2,
     length bs1 = sigma_byte_length ->
     length bs2 = sigma_byte_length ->
     is_sigma_bytes bs1 = true ->
     is_sigma_bytes bs2 = true ->
     sigma_bytes_to_integer bs1 = sigma_bytes_to_integer bs2 ->
     bs1 = bs2) /\
  (forall bs,
     is_sigma_bytes bs = true ->
     sigma_bytes_to_integer bs = uint256_of_bytes bs).

Lemma pd15_refinement_lemmas_discharged_holds :
  pd15_refinement_lemmas_discharged.
Proof.
  repeat split; intros.
  - apply sigma_bytes_to_integer_deterministic; auto.
  - eapply sigma_bytes_to_integer_injective_length_fixed; eauto.
  - apply runtime_spec_consistency; auto.
Qed.

Definition pd15_closable : bool := true.

Lemma pd15_closable_true : pd15_closable = true.
Proof. reflexivity. Qed.

Definition pd15_permitted_status (closable : bool) : DependencyStatus :=
  if closable then DS_Closed else DS_Open.

Theorem pd15_refinement_soundness :
  pd15_closable = true ->
  pd15_permitted_status true = DS_Closed /\
  pd15_permitted_status true <> DS_Open /\
  ds_closed (pd15_permitted_status true) = true.
Proof.
  intros _. repeat split; simpl; auto.
  intro H. discriminate H.
Qed.

Lemma pd15_dependency_status_closed :
  ds_closed (status_of PD15_SigmaIntegerRefinement) = true.
Proof. vm_compute. reflexivity. Qed.

Definition pd15_baseline_closed : bool :=
  ds_closed (status_of PD15_SigmaIntegerRefinement).

Lemma pd15_baseline_closed_true : pd15_baseline_closed = true.
Proof. exact pd15_dependency_status_closed. Qed.

Definition pd14_baseline_closed : bool :=
  ds_closed (status_of PD14_Uint512MagnitudeBound).

Lemma pd14_dependency_status_closed :
  ds_closed (status_of PD14_Uint512MagnitudeBound) = true.
Proof. vm_compute. reflexivity. Qed.

Lemma pd14_baseline_closed_true : pd14_baseline_closed = true.
Proof. exact pd14_dependency_status_closed. Qed.

Definition pd13_baseline_closed : bool :=
  ds_closed (status_of PD13_BuildFEnumerationCorrectness).

Definition pd4f_ob02_baseline_ready : bool :=
  ds_ready (status_of PD4F_OB_02).

Definition pd4f_cpp01_baseline_open : bool :=
  ds_open (status_of PD4F_CPP_01).

Lemma pd13_dependency_status_closed :
  ds_closed (status_of PD13_BuildFEnumerationCorrectness) = true.
Proof. vm_compute. reflexivity. Qed.

Lemma pd13_baseline_closed_true : pd13_baseline_closed = true.
Proof. exact pd13_dependency_status_closed. Qed.

Lemma pd4f_ob02_dependency_status_ready :
  ds_ready (status_of PD4F_OB_02) = true.
Proof. vm_compute. reflexivity. Qed.

Lemma pd4f_ob02_baseline_ready_true : pd4f_ob02_baseline_ready = true.
Proof. exact pd4f_ob02_dependency_status_ready. Qed.

Lemma pd4f_cpp01_dependency_status_open :
  ds_open (status_of PD4F_CPP_01) = true.
Proof. vm_compute. reflexivity. Qed.

Lemma pd4f_cpp01_baseline_open_true : pd4f_cpp01_baseline_open = true.
Proof. exact pd4f_cpp01_dependency_status_open. Qed.

Lemma pd15_does_not_establish_equivalence :
  equivalence_claimed = false /\
  proof_obligation_02_closable_frozen = true /\
  ds_ready (status_of PD4F_OB_02) = true /\
  ds_open (status_of PD4F_CPP_01) = true.
Proof.
  split; [| split; [| split]].
  - exact equivalence_not_claimed.
  - exact proof_obligation_02_closable_at_baseline.
  - vm_compute. reflexivity.
  - vm_compute. reflexivity.
Qed.

Definition pd15_equivalence_claimed_flag : bool := equivalence_claimed.

Definition pd15_ob02_closable_flag : bool :=
  proof_obligation_02_closable_frozen.

Lemma pd15_equivalence_claimed_false : pd15_equivalence_claimed_flag = false.
Proof. vm_compute. reflexivity. Qed.

Lemma pd15_ob02_closable_flag_true : pd15_ob02_closable_flag = true.
Proof. vm_compute. reflexivity. Qed.

Definition pd15_closure_from_closable : Prop :=
  pd15_closable = true -> pd15_baseline_closed = true.

Lemma pd15_closure_from_closable_holds : pd15_closure_from_closable.
Proof. intros _. exact pd15_baseline_closed_true. Qed.

(* VACUOUS-PLACEHOLDER (SECURITY-001-F / F-4): [sigma_integer_refinement_established] is defined as
   [True], so any lemma concluding it is discharged by [exact I] regardless of
   the artifacts it names. Bookkeeping marker only. NOT EVIDENCE. *)
Definition sigma_integer_refinement_established : Prop := True.

Lemma sigma_integer_refinement_established_holds :
  sigma_integer_refinement_established.
Proof. exact I. Qed.

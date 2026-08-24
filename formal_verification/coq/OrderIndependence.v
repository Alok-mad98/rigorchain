(** SECURITY-001-F G-1 — order independence of the convergence root and the draw winner.

    Two honest nodes that receive the same transactions in different orders must commit the same
    ConvergenceRoot and pay the same winner. Without that, they fork on transaction arrival order
    alone. The corpus previously asserted this in prose only; [Permutation] appeared nowhere.

    Two results, and they are not equally strong:

    - [convergence_root_order_independent] is unconditional.
    - [select_winner_order_independent] needs positive weights and distinct txids. Both hold on
      chain — MIN_CONVERGENCE_AMOUNT is 10^8 satoshis and a draw set is keyed by txid — but they
      are hypotheses of the theorem rather than facts about [Tx], so they are stated explicitly.
      The weight hypothesis is not bookkeeping: the draw compares sigma_x/w_x against sigma_y/w_y
      by cross-multiplication, which stops being transitive the moment a weight can be zero. *)

From Coq Require Import Arith.Arith.
From Coq Require Import Bool.Bool.
From Coq Require Import Lia.
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Sorting.Permutation.
Import ListNotations.

Require Import Rigor.legacy_account_model.Types.
From Rigor Require Import RigorBytes.
Require Import Rigor.RigorBytesOrder.
Require Import Rigor.ConvergeRoot.
Require Import Rigor.Sigma.
Require Import Rigor.legacy_account_model.Selection.

Open Scope Z_scope.

(** * 1. The convergence root *)

(** Insertion into a sorted list commutes. This is the whole content of "insertion sort does not
    see the input order": the two orders of insertion differ only by where equal-comparing
    elements land, and [bytes_ltb] is antisymmetric, so they cannot differ at all. *)
Lemma insert_sorted_txid_comm :
  forall (x y : list byte) (l : list (list byte)),
    insert_sorted_txid x (insert_sorted_txid y l) =
    insert_sorted_txid y (insert_sorted_txid x l).
Proof.
  intros x y l. revert x y.
  induction l as [|a l IH]; intros x y.
  - simpl.
    destruct (bytes_ltb x y) eqn:Hxy; destruct (bytes_ltb y x) eqn:Hyx; simpl;
      rewrite ?Hxy, ?Hyx; try reflexivity.
    + (* both strict: impossible unless x = y, and then the two results coincide *)
      assert (x = y) as -> by (apply bytes_ltb_antisym; assumption). reflexivity.
    + (* neither strict: totality forces x = y *)
      destruct (bytes_ltb_total x y) as [E | [H | H]];
        [subst; reflexivity | congruence | congruence].
  - simpl.
    destruct (bytes_ltb y a) eqn:Hya.
    + (* y sits before a *)
      simpl. destruct (bytes_ltb x a) eqn:Hxa.
      * simpl. rewrite ?Hya.
        destruct (bytes_ltb x y) eqn:Hxy; destruct (bytes_ltb y x) eqn:Hyx; simpl;
          rewrite ?Hxa, ?Hya; try reflexivity.
        -- assert (x = y) as -> by (apply bytes_ltb_antisym; assumption). reflexivity.
        -- destruct (bytes_ltb_total x y) as [E | [H | H]];
             [subst; reflexivity | congruence | congruence].
      * simpl. rewrite ?Hya.
        destruct (bytes_ltb x y) eqn:Hxy.
        -- (* x < y < a would give x < a by transitivity, contradicting Hxa *)
           exfalso.
           assert (bytes_ltb x a = true) by (eapply bytes_ltb_trans; eassumption).
           congruence.
        -- simpl. rewrite ?Hxa. reflexivity.
    + (* a sits before y *)
      simpl. destruct (bytes_ltb x a) eqn:Hxa.
      * simpl. destruct (bytes_ltb y x) eqn:Hyx.
        -- (* y < x < a would give y < a, contradicting Hya *)
           exfalso.
           assert (bytes_ltb y a = true) by (eapply bytes_ltb_trans; eassumption).
           congruence.
        -- simpl. rewrite ?Hya. reflexivity.
      * simpl. rewrite ?Hya, ?Hxa. f_equal. apply IH.
Qed.

Lemma sort_txid_bytes_order_independent :
  forall l1 l2 : list (list byte),
    Permutation l1 l2 -> sort_txid_bytes l1 = sort_txid_bytes l2.
Proof.
  intros l1 l2 H. induction H; simpl.
  - reflexivity.
  - rewrite IHPermutation. reflexivity.
  - apply insert_sorted_txid_comm.
  - congruence.
Qed.

(** C is a function of the SET of candidate txids, not of the order they arrive in.

    This is the property the anti-fork argument rests on. The coinbase commits C, and C feeds
    sigma for the next block's draw, so a C that depended on arrival order would split the network
    on nothing more than propagation timing. *)
Theorem convergence_root_order_independent :
  forall l1 l2 : list (list byte),
    Permutation l1 l2 ->
    convergence_root_from_txid_bytes l1 = convergence_root_from_txid_bytes l2.
Proof.
  intros l1 l2 Hperm.
  assert (Hlen : length l1 = length l2) by (apply Permutation_length; assumption).
  (* Empty F is zeros; any nonempty F takes the concat-hash path. A permutation
     preserves emptiness, so both sides take the same branch. *)
  destruct l1 as [|a1 r1]; destruct l2 as [|a2 r2];
    simpl in Hlen; try discriminate; try reflexivity.
  unfold convergence_root_from_txid_bytes.
  f_equal. f_equal.
  apply sort_txid_bytes_order_independent. exact Hperm.
Qed.

(** The set-level corollary: same transactions, any order, same root. *)
Corollary ConvergeRoot_order_independent :
  forall F1 F2 : list Tx, Permutation F1 F2 -> ConvergeRoot F1 = ConvergeRoot F2.
Proof.
  intros F1 F2 H. unfold ConvergeRoot.
  apply convergence_root_order_independent.
  apply Permutation_map. exact H.
Qed.

(** * 2. The winner *)

(** [pick_better_tx] as a decision: keep x when it strictly precedes y. *)
Definition tx_lt (x y : Tx) (C B : list byte) (h : Z) : bool :=
  if sigma_product_lt x y C B h then true
  else if sigma_product_lt y x C B h then false
  else bytes_ltb (tx_id_bytes x) (tx_id_bytes y).

Lemma pick_better_tx_as_tx_lt :
  forall x y C B h,
    pick_better_tx x y C B h = if tx_lt x y C B h then x else y.
Proof.
  intros x y C B h. unfold pick_better_tx, tx_lt.
  destruct (sigma_product_lt x y C B h); [reflexivity|].
  destruct (sigma_product_lt y x C B h); [reflexivity|].
  destruct (bytes_ltb (tx_id_bytes x) (tx_id_bytes y)); reflexivity.
Qed.

(** The cross-product comparison is antisymmetric outright: sx*wy < sy*wx and sy*wx < sx*wy
    cannot both hold, whatever the weights are. *)
Lemma sigma_product_lt_antisym :
  forall x y C B h,
    sigma_product_lt x y C B h = true ->
    sigma_product_lt y x C B h = false.
Proof.
  intros x y C B h H. unfold sigma_product_lt in *.
  apply Z.ltb_lt in H. apply Z.ltb_ge. lia.
Qed.

(** Transitivity, and here the weights matter. sx*wy < sy*wx is sx/wx < sy/wy rearranged, and
    that rearrangement is only order-preserving for positive weights. With w = 0 admitted the
    relation is not transitive and no argmin is well defined. *)
Lemma sigma_product_lt_trans :
  forall x y z C B h,
    0 < weight x -> 0 < weight y -> 0 < weight z ->
    sigma_product_lt x y C B h = true ->
    sigma_product_lt y z C B h = true ->
    sigma_product_lt x z C B h = true.
Proof.
  intros x y z C B h Hwx Hwy Hwz Hxy Hyz.
  unfold sigma_product_lt in *.
  apply Z.ltb_lt in Hxy. apply Z.ltb_lt in Hyz. apply Z.ltb_lt.
  (* From sx*wy < sy*wx and sy*wz < sz*wy, multiply the first by wz and the second by wx:
       sx*wy*wz < sy*wx*wz  and  sy*wz*wx < sz*wy*wx
     so sx*wy*wz < sz*wy*wx, and wy > 0 cancels. *)
  assert (Hx : Sigma x C B h * weight y * weight z < Sigma y C B h * weight x * weight z)
    by (apply Zmult_lt_compat_r; lia).
  assert (Hy : Sigma y C B h * weight z * weight x < Sigma z C B h * weight y * weight x)
    by (apply Zmult_lt_compat_r; lia).
  nia.
Qed.

(** Trichotomy on distinct txids: exactly one of x, y is picked, whichever way round they come. *)
Lemma tx_lt_asym :
  forall x y C B h,
    tx_id_bytes x <> tx_id_bytes y ->
    tx_lt x y C B h = negb (tx_lt y x C B h).
Proof.
  intros x y C B h Hne. unfold tx_lt.
  destruct (sigma_product_lt x y C B h) eqn:Hxy.
  - rewrite (sigma_product_lt_antisym _ _ _ _ _ Hxy). reflexivity.
  - destruct (sigma_product_lt y x C B h) eqn:Hyx.
    + reflexivity.
    + destruct (bytes_ltb (tx_id_bytes x) (tx_id_bytes y)) eqn:Hbx.
      * assert (bytes_ltb (tx_id_bytes y) (tx_id_bytes x) = false) as ->.
        { destruct (bytes_ltb (tx_id_bytes y) (tx_id_bytes x)) eqn:Hby; [|reflexivity].
          exfalso. apply Hne. apply bytes_ltb_antisym; assumption. }
        reflexivity.
      * destruct (bytes_ltb_total (tx_id_bytes x) (tx_id_bytes y)) as [E | [Hlt | Hgt]].
        -- contradiction.
        -- congruence.
        -- rewrite Hgt. reflexivity.
Qed.

Lemma pick_better_tx_comm :
  forall x y C B h,
    tx_id_bytes x <> tx_id_bytes y ->
    pick_better_tx x y C B h = pick_better_tx y x C B h.
Proof.
  intros x y C B h Hne.
  rewrite !pick_better_tx_as_tx_lt.
  rewrite (tx_lt_asym x y C B h Hne).
  destruct (tx_lt y x C B h); reflexivity.
Qed.

(** [pick_better_tx] returns one of its arguments, which is what lets the fold's distinctness
    hypothesis survive a step. *)
Lemma pick_better_tx_in :
  forall x y C B h,
    pick_better_tx x y C B h = x \/ pick_better_tx x y C B h = y.
Proof.
  intros x y C B h. rewrite pick_better_tx_as_tx_lt.
  destruct (tx_lt x y C B h); [left|right]; reflexivity.
Qed.

Lemma tx_lt_trans :
  forall x y z C B h,
    0 < weight x -> 0 < weight y -> 0 < weight z ->
    tx_lt x y C B h = true ->
    tx_lt y z C B h = true ->
    tx_lt x z C B h = true.
Proof.
  intros x y z C B h Hwx Hwy Hwz Hxy Hyz. unfold tx_lt in *.
  destruct (sigma_product_lt x y C B h) eqn:Hsxy.
  - destruct (sigma_product_lt y z C B h) eqn:Hsyz.
    + rewrite (sigma_product_lt_trans _ y _ _ _ _ Hwx Hwy Hwz Hsxy Hsyz). reflexivity.
    + (* y and z tie on the cross product, so x beats z exactly as it beat y *)
      destruct (sigma_product_lt z y C B h) eqn:Hszy; [discriminate|].
      destruct (sigma_product_lt x z C B h) eqn:Hsxz; [reflexivity|].
      exfalso.
      (* sx*wy < sy*wx, sy*wz = sz*wy (neither strict), and sz*wx <= sx*wz: contradiction *)
      unfold sigma_product_lt in Hsxy, Hsyz, Hszy, Hsxz.
      apply Z.ltb_lt in Hsxy. apply Z.ltb_ge in Hsyz. apply Z.ltb_ge in Hszy.
      apply Z.ltb_ge in Hsxz. nia.
  - destruct (sigma_product_lt y x C B h) eqn:Hsyx; [discriminate|].
    destruct (sigma_product_lt y z C B h) eqn:Hsyz.
    + (* x ties y and y beats z, so x beats z *)
      destruct (sigma_product_lt x z C B h) eqn:Hsxz; [reflexivity|].
      exfalso.
      unfold sigma_product_lt in Hsxy, Hsyx, Hsyz, Hsxz.
      apply Z.ltb_ge in Hsxy. apply Z.ltb_ge in Hsyx. apply Z.ltb_lt in Hsyz.
      apply Z.ltb_ge in Hsxz. nia.
    + destruct (sigma_product_lt z y C B h) eqn:Hszy; [discriminate|].
      (* all three tie on the cross product; fall through to the txid order *)
      destruct (sigma_product_lt x z C B h) eqn:Hsxz; [reflexivity|].
      destruct (sigma_product_lt z x C B h) eqn:Hszx.
      * exfalso.
        unfold sigma_product_lt in Hsxy, Hsyx, Hsyz, Hszy, Hszx.
        apply Z.ltb_ge in Hsxy. apply Z.ltb_ge in Hsyx.
        apply Z.ltb_ge in Hsyz. apply Z.ltb_ge in Hszy. apply Z.ltb_lt in Hszx. nia.
      * eapply bytes_ltb_trans; eassumption.
Qed.

(** All txids in the list are distinct, and no member shares a txid with [x]. *)
Definition distinct_txids (l : list Tx) : Prop :=
  NoDup (map tx_id_bytes l).

Definition positive_weights (l : list Tx) : Prop :=
  forall t, In t l -> 0 < weight t.

Lemma select_winner_in :
  forall l C B h w, select_winner l C B h = Some w -> In w l.
Proof.
  induction l as [|x xs IH]; intros C B h w Hsel; simpl in Hsel; [discriminate|].
  destruct (select_winner xs C B h) as [y|] eqn:Hxs.
  - injection Hsel as <-.
    destruct (pick_better_tx_in x y C B h) as [-> | ->].
    + left. reflexivity.
    + right. apply (IH C B h y Hxs).
  - injection Hsel as <-. left. reflexivity.
Qed.

(** Left commutativity of the fold's combining step, which is what a swap in the input needs. *)
Lemma pick_better_tx_left_comm :
  forall x y z C B h,
    0 < weight x -> 0 < weight y -> 0 < weight z ->
    tx_id_bytes x <> tx_id_bytes y ->
    tx_id_bytes x <> tx_id_bytes z ->
    tx_id_bytes y <> tx_id_bytes z ->
    pick_better_tx y (pick_better_tx x z C B h) C B h =
    pick_better_tx x (pick_better_tx y z C B h) C B h.
Proof.
  intros x y z C B h Hwx Hwy Hwz Hxy Hxz Hyz.
  assert (Hyx : tx_id_bytes y <> tx_id_bytes x) by (intro E; apply Hxy; symmetry; exact E).
  assert (Hzy : tx_id_bytes z <> tx_id_bytes y) by (intro E; apply Hyz; symmetry; exact E).
  assert (Hzx : tx_id_bytes z <> tx_id_bytes x) by (intro E; apply Hxz; symmetry; exact E).
  rewrite !pick_better_tx_as_tx_lt.
  destruct (tx_lt x z C B h) eqn:Hxz_lt; destruct (tx_lt y z C B h) eqn:Hyz_lt.
  all: rewrite ?Hxz_lt, ?Hyz_lt.
  - (* both beat z; the survivor is whichever of x, y wins, either way round *)
    rewrite (tx_lt_asym y x C B h Hyx).
    destruct (tx_lt x y C B h); reflexivity.
  - (* x beats z, y does not: x beats y through z, so x survives on both sides *)
    assert (Hzy_lt : tx_lt z y C B h = true)
      by (rewrite (tx_lt_asym z y C B h Hzy), Hyz_lt; reflexivity).
    assert (Hxy_lt : tx_lt x y C B h = true)
      by (exact (tx_lt_trans x z y C B h Hwx Hwz Hwy Hxz_lt Hzy_lt)).
    assert (Hyx_lt : tx_lt y x C B h = false)
      by (rewrite (tx_lt_asym y x C B h Hyx), Hxy_lt; reflexivity).
    rewrite Hyx_lt. reflexivity.
  - (* mirror image *)
    assert (Hzx_lt : tx_lt z x C B h = true)
      by (rewrite (tx_lt_asym z x C B h Hzx), Hxz_lt; reflexivity).
    assert (Hyx_lt : tx_lt y x C B h = true)
      by (exact (tx_lt_trans y z x C B h Hwy Hwz Hwx Hyz_lt Hzx_lt)).
    assert (Hxy_lt : tx_lt x y C B h = false)
      by (rewrite (tx_lt_asym x y C B h Hxy), Hyx_lt; reflexivity).
    rewrite Hxy_lt. reflexivity.
  - (* neither beats z, so z survives on both sides *)
    reflexivity.
Qed.

(** The draw is a function of the SET of candidates.

    [select_winner] folds [pick_better_tx] over the list, so this says the fold is an argmin over
    a strict total order rather than something order-sensitive. Together with
    [convergence_root_order_independent] it closes G-1: two nodes holding the same candidate set
    commit the same C and pay the same transaction, whatever order the candidates reached them. *)
Theorem select_winner_order_independent :
  forall (l1 l2 : list Tx) (C B : list byte) (h : Z),
    Permutation l1 l2 ->
    positive_weights l1 ->
    distinct_txids l1 ->
    select_winner l1 C B h = select_winner l2 C B h.
Proof.
  intros l1 l2 C B h Hperm. revert C B h.
  induction Hperm as [ | x l1 l2 Hperm IH | x y l | l1 l2 l3 H12 IH12 H23 IH23 ];
    intros C B h Hw Hd.
  - reflexivity.
  - simpl. rewrite IH.
    + reflexivity.
    + intros t Ht. apply Hw. right. exact Ht.
    + unfold distinct_txids in *. simpl in Hd. inversion Hd; assumption.
  - (* the swap case: the two heads meet in the opposite order *)
    unfold distinct_txids in Hd. simpl in Hd.
    inversion Hd as [|? ? Hnin_y Hd']; subst.
    inversion Hd' as [|? ? Hnin_x Hd'']; subst.
    assert (Hyx : tx_id_bytes y <> tx_id_bytes x).
    { intro E. apply Hnin_y. left. symmetry. exact E. }
    simpl.
    destruct (select_winner l C B h) as [z|] eqn:Hz.
    + assert (Hin_z : In z l) by (apply (select_winner_in l C B h z Hz)).
      assert (Hxz : tx_id_bytes x <> tx_id_bytes z).
      { intro E. apply Hnin_x. apply in_map_iff. exists z. split; [symmetry; exact E| exact Hin_z]. }
      assert (Hyz : tx_id_bytes y <> tx_id_bytes z).
      { intro E. apply Hnin_y. right. apply in_map_iff.
        exists z. split; [symmetry; exact E| exact Hin_z]. }
      f_equal.
      apply (pick_better_tx_left_comm x y z C B h).
      * apply Hw. right. left. reflexivity.
      * apply Hw. left. reflexivity.
      * apply Hw. right. right. exact Hin_z.
      * intro E. apply Hyx. symmetry. exact E.
      * exact Hxz.
      * exact Hyz.
    + f_equal. apply pick_better_tx_comm. exact Hyx.
  - (* transitivity: the middle list inherits the hypotheses through the permutation *)
    assert (Hw2 : positive_weights l2).
    { intros t Ht. apply Hw. apply (Permutation_in _ (Permutation_sym H12)). exact Ht. }
    assert (Hd2 : distinct_txids l2).
    { unfold distinct_txids in *.
      apply (Permutation_NoDup (Permutation_map tx_id_bytes H12)). exact Hd. }
    rewrite (IH12 C B h Hw Hd). apply IH23; assumption.
Qed.

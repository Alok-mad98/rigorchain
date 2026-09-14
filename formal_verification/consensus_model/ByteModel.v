(** ConsensusModel.ByteModel

    Byte-level carrier for the serialization model. Not C++.
    Not ConnectBlock. Not extraction.

    A byte is a [nat] below 256, which is cheap to represent. The
    scalar values carried by the wire format are [N], not [nat],
    because they run up to 2^64: a unary [nat] of that size cannot be
    built, so a [nat]-valued codec cannot be run on real data and
    cannot be extracted usefully. Widths stay [nat] because they are
    small and drive structural recursion.

    CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED
*)
From Coq Require Import Arith.PeanoNat.
From Coq Require Import Bool.Bool.
From Coq Require Import List.
From Coq Require Import NArith.NArith.
From Coq Require Import Lia.
Import ListNotations.

Definition Byte := nat.

(** Width of a wire byte, in both carriers. [nbase] is the one the
    arithmetic runs on; [byte_base] is the one [byte_ok] is stated
    with, so that byte-level well-formedness keeps its [nat] shape. *)
Definition nbase : N := 256%N.
Definition byte_base : nat := 256.

Lemma byte_base_N : N.of_nat byte_base = nbase.
Proof. reflexivity. Qed.

Lemma nbase_nz : nbase <> 0%N.
Proof. unfold nbase. discriminate. Qed.

Lemma byte_base_nz : byte_base <> 0.
Proof.
  unfold byte_base. discriminate.
Qed.

Lemma byte_base_pos : 0 < byte_base.
Proof.
  unfold byte_base. apply Nat.lt_0_succ.
Qed.

(* [Opaque] below blocks [unfold byte_base] for the rest of this file,
   so the value is exposed as a rewrite instead. *)
Lemma byte_base_val : byte_base = 256.
Proof. reflexivity. Qed.

Opaque byte_base.

Definition byte_ok (b : Byte) : Prop := b < byte_base.

Definition byte_list := list Byte.

Definition beq_byte (a b : Byte) : bool := Nat.eqb a b.

Fixpoint bytes_ok (xs : byte_list) : Prop :=
  match xs with
  | nil => True
  | b :: rest => byte_ok b /\ bytes_ok rest
  end.

Fixpoint beq_byte_list (a b : byte_list) : bool :=
  match a, b with
  | nil, nil => true
  | x :: xs, y :: ys => andb (beq_byte x y) (beq_byte_list xs ys)
  | _, _ => false
  end.

Theorem byte_eq_dec :
  forall (a b : Byte), {a = b} + {a <> b}.
Proof.
  intros a b.
  destruct (Nat.eq_dec a b); [left | right]; assumption.
Qed.

Lemma beq_byte_true :
  forall a b, beq_byte a b = true -> a = b.
Proof.
  intros a b H.
  apply Nat.eqb_eq in H.
  exact H.
Qed.

(** * Fixed-width little-endian scalars

    [npow_base w] is 256^w as a binary natural. Unlike the [nat]
    version it evaluates, which is what makes the whole codec
    runnable. *)

Fixpoint npow_base (w : nat) : N :=
  match w with
  | 0 => 1%N
  | S w' => (nbase * npow_base w')%N
  end.

Lemma npow_base_S :
  forall w, npow_base (S w) = (nbase * npow_base w)%N.
Proof. intros w. reflexivity. Qed.

Lemma npow_base_pos : forall w, (0 < npow_base w)%N.
Proof.
  induction w as [|w IH]; cbn [npow_base]; unfold nbase in *; lia.
Qed.

Fixpoint encode_le (w : nat) (n : N) : byte_list :=
  match w with
  | 0 => nil
  | S w' => N.to_nat (N.modulo n nbase) :: encode_le w' (N.div n nbase)
  end.

Fixpoint decode_le (w : nat) (bs : byte_list) : option (N * byte_list) :=
  match w with
  | 0 => Some (0%N, bs)
  | S w' =>
      match bs with
      | nil => None
      | b :: rest =>
          if byte_base <=? b then None
          else
            match decode_le w' rest with
            | None => None
            | Some (hi, rest') => Some ((N.of_nat b + hi * nbase)%N, rest')
            end
      end
  end.

Lemma encode_le_length :
  forall w n, List.length (encode_le w n) = w.
Proof.
  induction w as [|w IH]; intros n; simpl.
  - reflexivity.
  - rewrite IH. reflexivity.
Qed.

Lemma nmod_base_lt :
  forall n, (N.modulo n nbase < nbase)%N.
Proof.
  intros n. apply N.mod_lt, nbase_nz.
Qed.

Lemma mod_base_byte_ok :
  forall n, byte_ok (N.to_nat (N.modulo n nbase)).
Proof.
  intros n.
  pose proof (nmod_base_lt n) as H.
  unfold byte_ok, nbase in *. rewrite byte_base_val. lia.
Qed.

Lemma encode_le_bytes_ok :
  forall w n, bytes_ok (encode_le w n).
Proof.
  induction w as [|w IH]; intros n; simpl.
  - exact I.
  - split; [apply mod_base_byte_ok | apply IH].
Qed.

Lemma decode_le_S_cons :
  forall w b rest,
    decode_le (S w) (b :: rest) =
      if byte_base <=? b then None
      else
        match decode_le w rest with
        | None => None
        | Some (hi, rest') => Some ((N.of_nat b + hi * nbase)%N, rest')
        end.
Proof.
  intros w b rest.
  reflexivity.
Qed.

Lemma byte_lt_nbase :
  forall b, b < byte_base -> (N.of_nat b < nbase)%N.
Proof.
  intros b H. rewrite byte_base_val in H. unfold nbase. lia.
Qed.

Lemma encode_le_S_recon :
  forall w b hi,
    b < byte_base ->
    encode_le (S w) (N.of_nat b + hi * nbase)%N = b :: encode_le w hi.
Proof.
  intros w b hi Hb.
  pose proof (byte_lt_nbase b Hb) as HbN.
  cbn [encode_le].
  rewrite N.mod_add by apply nbase_nz.
  rewrite (N.mod_small (N.of_nat b) nbase HbN).
  rewrite N.div_add by apply nbase_nz.
  rewrite (N.div_small (N.of_nat b) nbase HbN).
  rewrite N.add_0_l.
  rewrite Nat2N.id.
  reflexivity.
Qed.

Lemma decode_encode_le_app :
  forall w n rest,
    (n < npow_base w)%N ->
    decode_le w (encode_le w n ++ rest) = Some (n, rest).
Proof.
  induction w as [|w IH]; intros n rest Hn.
  - cbn [encode_le decode_le app].
    cbn [npow_base] in Hn.
    assert (n = 0%N) by lia. subst. reflexivity.
  - assert (Hdiv : (N.div n nbase < npow_base w)%N).
    { rewrite npow_base_S in Hn.
      apply N.div_lt_upper_bound; [apply nbase_nz | exact Hn]. }
    change (encode_le (S w) n ++ rest) with
      (N.to_nat (N.modulo n nbase) :: encode_le w (N.div n nbase) ++ rest).
    rewrite decode_le_S_cons.
    pose proof (mod_base_byte_ok n) as Hm. unfold byte_ok in Hm.
    destruct (byte_base <=? N.to_nat (N.modulo n nbase)) eqn:Hle.
    + apply Nat.leb_le in Hle. lia.
    + rewrite (IH (N.div n nbase) rest Hdiv).
      f_equal. f_equal.
      rewrite N2Nat.id.
      rewrite N.mul_comm, N.add_comm.
      symmetry. apply N.div_mod'.
Qed.

Lemma decode_encode_le :
  forall w n,
    (n < npow_base w)%N ->
    decode_le w (encode_le w n) = Some (n, nil).
Proof.
  intros w n Hn.
  pose proof (decode_encode_le_app w n nil Hn) as H.
  rewrite app_nil_r in H.
  exact H.
Qed.

Lemma byte_not_ge_base :
  forall b, (byte_base <=? b) = false -> b < byte_base.
Proof.
  intros b H.
  apply Nat.leb_gt in H.
  exact H.
Qed.

Lemma encode_le_via_eq :
  forall w b hi n,
    b < byte_base ->
    n = (N.of_nat b + hi * nbase)%N ->
    encode_le (S w) n = b :: encode_le w hi.
Proof.
  intros w b hi n Hb Heq.
  rewrite Heq.
  apply encode_le_S_recon.
  exact Hb.
Qed.

Lemma decode_le_1_split :
  forall bs n rest,
    decode_le 1 bs = Some (n, rest) ->
    encode_le 1 n ++ rest = bs.
Proof.
  intros bs n rest H.
  destruct bs as [|b rest0]; [simpl in H; discriminate|].
  rewrite decode_le_S_cons in H.
  destruct (byte_base <=? b) eqn:Hle; [discriminate|].
  injection H as Hn Hrest.
  subst rest.
  pose proof (byte_not_ge_base b Hle) as Hb.
  assert (Hn' : n = (N.of_nat b + 0 * nbase)%N).
  { rewrite <- Hn. reflexivity. }
  rewrite (encode_le_via_eq 0 b 0%N n Hb Hn').
  simpl.
  reflexivity.
Qed.

Lemma decode_le_2_split :
  forall bs n rest,
    decode_le 2 bs = Some (n, rest) ->
    encode_le 2 n ++ rest = bs.
Proof.
  intros bs n rest H.
  destruct bs as [|b rest0]; [simpl in H; discriminate|].
  rewrite decode_le_S_cons in H.
  destruct (byte_base <=? b) eqn:Hle; [discriminate|].
  destruct (decode_le 1 rest0) as [[hi rest']|] eqn:Hd; [|discriminate].
  injection H as Hn Hrest.
  subst rest.
  pose proof (byte_not_ge_base b Hle) as Hb.
  assert (Hn' : n = (N.of_nat b + hi * nbase)%N).
  { rewrite <- Hn. reflexivity. }
  rewrite (encode_le_via_eq 1 b hi n Hb Hn').
  cbn [app].
  f_equal.
  apply decode_le_1_split.
  exact Hd.
Qed.

Lemma decode_le_4_split :
  forall bs n rest,
    decode_le 4 bs = Some (n, rest) ->
    encode_le 4 n ++ rest = bs.
Proof.
  intros bs n rest H.
  destruct bs as [|b0 r0]; [simpl in H; discriminate|].
  rewrite decode_le_S_cons in H.
  destruct (byte_base <=? b0) eqn:L0; [discriminate|].
  destruct (decode_le 3 r0) as [[h0 r1]|] eqn:D0; [|discriminate].
  injection H as Hn Hrest.
  subst rest.
  pose proof (byte_not_ge_base b0 L0) as B0.
  assert (Hn' : n = (N.of_nat b0 + h0 * nbase)%N).
  { rewrite <- Hn. reflexivity. }
  rewrite (encode_le_via_eq 3 b0 h0 n B0 Hn').
  cbn [app].
  f_equal.
  destruct r0 as [|b1 r1']; [simpl in D0; discriminate|].
  rewrite decode_le_S_cons in D0.
  destruct (byte_base <=? b1) eqn:L1; [discriminate|].
  destruct (decode_le 2 r1') as [[h1 r2]|] eqn:D1; [|discriminate].
  injection D0 as Dh Dr.
  subst r1.
  pose proof (byte_not_ge_base b1 L1) as B1.
  assert (Dh' : h0 = (N.of_nat b1 + h1 * nbase)%N).
  { rewrite <- Dh. reflexivity. }
  rewrite (encode_le_via_eq 2 b1 h1 h0 B1 Dh').
  cbn [app].
  f_equal.
  apply decode_le_2_split.
  exact D1.
Qed.

Lemma decode_le_8_split :
  forall bs n rest,
    decode_le 8 bs = Some (n, rest) ->
    encode_le 8 n ++ rest = bs.
Proof.
  intros bs n rest H.
  destruct bs as [|b0 r0]; [simpl in H; discriminate|].
  rewrite decode_le_S_cons in H.
  destruct (byte_base <=? b0) eqn:L0; [discriminate|].
  destruct (decode_le 7 r0) as [[h0 r1]|] eqn:D0; [|discriminate].
  injection H as Hn Hrest.
  subst rest.
  pose proof (byte_not_ge_base b0 L0) as B0.
  assert (Hn' : n = (N.of_nat b0 + h0 * nbase)%N).
  { rewrite <- Hn. reflexivity. }
  rewrite (encode_le_via_eq 7 b0 h0 n B0 Hn').
  cbn [app].
  f_equal.
  destruct r0 as [|b1 r1']; [simpl in D0; discriminate|].
  rewrite decode_le_S_cons in D0.
  destruct (byte_base <=? b1) eqn:L1; [discriminate|].
  destruct (decode_le 6 r1') as [[h1 r2]|] eqn:D1; [|discriminate].
  injection D0 as Dh0 Dr0.
  subst r1.
  pose proof (byte_not_ge_base b1 L1) as B1.
  assert (Dh0' : h0 = (N.of_nat b1 + h1 * nbase)%N).
  { rewrite <- Dh0. reflexivity. }
  rewrite (encode_le_via_eq 6 b1 h1 h0 B1 Dh0').
  cbn [app].
  f_equal.
  destruct r1' as [|b2 r2']; [simpl in D1; discriminate|].
  rewrite decode_le_S_cons in D1.
  destruct (byte_base <=? b2) eqn:L2; [discriminate|].
  destruct (decode_le 5 r2') as [[h2 r3]|] eqn:D2; [|discriminate].
  injection D1 as Dh1 Dr1.
  subst r2.
  pose proof (byte_not_ge_base b2 L2) as B2.
  assert (Dh1' : h1 = (N.of_nat b2 + h2 * nbase)%N).
  { rewrite <- Dh1. reflexivity. }
  rewrite (encode_le_via_eq 5 b2 h2 h1 B2 Dh1').
  cbn [app].
  f_equal.
  destruct r2' as [|b3 r3']; [simpl in D2; discriminate|].
  rewrite decode_le_S_cons in D2.
  destruct (byte_base <=? b3) eqn:L3; [discriminate|].
  destruct (decode_le 4 r3') as [[h3 r4]|] eqn:D3; [|discriminate].
  injection D2 as Dh2 Dr2.
  subst r3.
  pose proof (byte_not_ge_base b3 L3) as B3.
  assert (Dh2' : h2 = (N.of_nat b3 + h3 * nbase)%N).
  { rewrite <- Dh2. reflexivity. }
  rewrite (encode_le_via_eq 4 b3 h3 h2 B3 Dh2').
  cbn [app].
  f_equal.
  apply decode_le_4_split.
  exact D3.
Qed.

Fixpoint take_n (n : nat) (bs : byte_list) : option (byte_list * byte_list) :=
  match n, bs with
  | 0, _ => Some (nil, bs)
  | S n', b :: rest =>
      match take_n n' rest with
      | None => None
      | Some (pre, post) => Some (b :: pre, post)
      end
  | S _, nil => None
  end.

Lemma take_n_app :
  forall (xs rest : byte_list),
    take_n (List.length xs) (xs ++ rest) = Some (xs, rest).
Proof.
  induction xs as [|x xs IH]; intros rest; simpl.
  - reflexivity.
  - rewrite IH. reflexivity.
Qed.

Lemma take_n_exact :
  forall xs,
    take_n (List.length xs) xs = Some (xs, nil).
Proof.
  intros xs.
  pose proof (take_n_app xs nil) as H.
  rewrite app_nil_r in H.
  exact H.
Qed.

Lemma bytes_ok_app :
  forall a b,
    bytes_ok (a ++ b) <-> bytes_ok a /\ bytes_ok b.
Proof.
  induction a as [|x xs IH]; intros b; simpl.
  - split.
    + intros H. split; [exact I | exact H].
    + intros [_ H]. exact H.
  - split.
    + intros [Hx Hrest].
      apply IH in Hrest.
      destruct Hrest as [Hxs Hb].
      split; [split; assumption | exact Hb].
    + intros [[Hx Hxs] Hb].
      split; [exact Hx|].
      apply IH.
      split; assumption.
Qed.

Lemma take_n_length :
  forall n bs pre post,
    take_n n bs = Some (pre, post) ->
    List.length pre = n.
Proof.
  induction n as [|n IH]; intros bs pre post Ht.
  - simpl in Ht. inversion Ht. subst. reflexivity.
  - destruct bs as [|b rest]; simpl in Ht; [discriminate|].
    destruct (take_n n rest) as [[pre' post']|] eqn:Ht'; [|discriminate].
    injection Ht as Hp Hq.
    subst pre post.
    simpl.
    rewrite (IH rest pre' post' Ht').
    reflexivity.
Qed.

Lemma take_n_split :
  forall n bs pre post,
    take_n n bs = Some (pre, post) ->
    bs = pre ++ post.
Proof.
  induction n as [|n IH]; intros bs pre post Ht.
  - simpl in Ht. inversion Ht. subst. reflexivity.
  - destruct bs as [|b rest]; simpl in Ht; [discriminate|].
    destruct (take_n n rest) as [[pre' post']|] eqn:Ht'; [|discriminate].
    injection Ht as Hp Hq.
    subst pre post.
    rewrite (IH rest pre' post' Ht').
    reflexivity.
Qed.

Lemma take_n_bytes_ok :
  forall n bs pre post,
    take_n n bs = Some (pre, post) ->
    bytes_ok bs ->
    bytes_ok pre /\ bytes_ok post.
Proof.
  intros n bs pre post Ht Hok.
  apply take_n_split in Ht.
  subst bs.
  apply bytes_ok_app in Hok.
  exact Hok.
Qed.

(** [take_n] fails exactly when the input is too short. The [None]
    direction is what lets a caller bound work by the input length
    instead of by a decoded count. *)
Lemma take_n_none_short :
  forall n bs, List.length bs < n -> take_n n bs = None.
Proof.
  induction n as [|n IH]; intros bs Hlen; [cbn in Hlen; lia|].
  destruct bs as [|b rest]; [reflexivity|].
  cbn [take_n].
  cbn [List.length] in Hlen.
  rewrite (IH rest ltac:(lia)).
  reflexivity.
Qed.

Lemma decode_le_bound :
  forall w bs n rest,
    decode_le w bs = Some (n, rest) ->
    (n < npow_base w)%N.
Proof.
  induction w as [|w IH]; intros bs n rest H.
  - cbn [decode_le] in H. injection H as Hn _. subst n.
    cbn [npow_base]. lia.
  - destruct bs as [|b rest0]; [cbn [decode_le] in H; discriminate|].
    rewrite decode_le_S_cons in H.
    destruct (byte_base <=? b) eqn:Hle; [discriminate|].
    destruct (decode_le w rest0) as [[hi rest']|] eqn:Hd; [|discriminate].
    injection H as Hn Hrest.
    subst n rest.
    pose proof (byte_not_ge_base b Hle) as Hb.
    pose proof (byte_lt_nbase b Hb) as HbN.
    pose proof (IH rest0 hi rest' Hd) as Hhi.
    rewrite npow_base_S.
    rewrite (N.mul_comm nbase (npow_base w)).
    assert (Hstep : ((hi + 1) * nbase <= npow_base w * nbase)%N)
      by (apply N.mul_le_mono_r; lia).
    rewrite N.mul_add_distr_r, N.mul_1_l in Hstep.
    lia.
Qed.

(** Decoding a fixed-width field consumes exactly that many bytes, so
    a caller can bound the tail without looking at the value. *)
Lemma decode_le_consumes :
  forall w bs n rest,
    decode_le w bs = Some (n, rest) ->
    List.length bs = w + List.length rest.
Proof.
  induction w as [|w IH]; intros bs n rest H.
  - cbn [decode_le] in H. injection H as _ Hr. subst rest. reflexivity.
  - destruct bs as [|b rest0]; [cbn [decode_le] in H; discriminate|].
    rewrite decode_le_S_cons in H.
    destruct (byte_base <=? b); [discriminate|].
    destruct (decode_le w rest0) as [[hi rest']|] eqn:Hd; [|discriminate].
    injection H as _ Hr. subst rest'.
    cbn [List.length].
    rewrite (IH rest0 hi rest Hd).
    reflexivity.
Qed.

(** A count read off the wire is [N] and can be astronomically large.
    Converting it to a unary [nat] index would be the whole problem all
    over again, so the length check comes first. [take_n_N_spec] says
    the guard changes nothing: the guarded branch was already [None]. *)

Definition take_n_N (n : N) (bs : byte_list) : option (byte_list * byte_list) :=
  if (N.of_nat (List.length bs) <? n)%N then None else take_n (N.to_nat n) bs.

Theorem take_n_N_spec :
  forall n bs, take_n_N n bs = take_n (N.to_nat n) bs.
Proof.
  intros n bs. unfold take_n_N.
  destruct (N.of_nat (List.length bs) <? n)%N eqn:E; [|reflexivity].
  apply N.ltb_lt in E.
  symmetry. apply take_n_none_short. lia.
Qed.

Lemma encode_le_nonempty : forall w n, w <> 0 -> encode_le w n <> nil.
Proof.
  intros [|w] n Hw; [contradiction|]. cbn [encode_le]. discriminate.
Qed.

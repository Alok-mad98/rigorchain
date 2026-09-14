(** FeeBurn/ReplaySafety.v
    RFC-018 fee-burn replay / rollback / reorg safety.

    Same transaction on the same fee class and fee amount produces the
    same burn. Disconnect restores the pre-state snapshot (ORU supply,
    RES supply, burn accumulator, miner-claim accumulator). A reorg
    that disconnects A then connects B records B's burn, not A+B.

    Model of the accounting scalars, not a C++ [DisconnectBlock]
    refinement. *)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From Coq Require Import List.
Import ListNotations.
Require Import Rigor.FeeBurn.Accounting.
Require Import Rigor.FeeBurn.AssetDomain.

Local Open Scope Z_scope.

Definition FeeTx : Type := (TxFeeClass * Z)%type.

Record FeeBurnState : Set := {
  fbs_oru : Z;
  fbs_res : Z;
  fbs_burn : Z;
  fbs_miner : Z
}.

Definition genesis_fee_state (oru res : Z) : FeeBurnState :=
  {| fbs_oru := oru; fbs_res := res; fbs_burn := 0; fbs_miner := 0 |}.

Definition apply_one (s : FeeBurnState) (tx : FeeTx) : option FeeBurnState :=
  let c := fst tx in
  let fee := snd tx in
  if fee <? 0 then None
  else
    let b := calculate_fee_burn fee c in
    let m := calculate_miner_fee fee c in
    Some {| fbs_oru := fbs_oru s - b;
            fbs_res := fbs_res s;
            fbs_burn := fbs_burn s + b;
            fbs_miner := fbs_miner s + m |}.

Fixpoint apply_txs (s : FeeBurnState) (txs : list FeeTx)
  : option FeeBurnState :=
  match txs with
  | [] => Some s
  | tx :: rest =>
      match apply_one s tx with
      | None => None
      | Some s1 => apply_txs s1 rest
      end
  end.

Definition tx_burn (tx : FeeTx) : Z :=
  calculate_fee_burn (snd tx) (fst tx).

Fixpoint txs_burn (txs : list FeeTx) : Z :=
  match txs with
  | [] => 0
  | tx :: rest => tx_burn tx + txs_burn rest
  end.

Fixpoint txs_miner (txs : list FeeTx) : Z :=
  match txs with
  | [] => 0
  | tx :: rest => calculate_miner_fee (snd tx) (fst tx) + txs_miner rest
  end.

Lemma apply_one_res_unchanged :
  forall s tx s',
    apply_one s tx = Some s' ->
    fbs_res s' = fbs_res s.
Proof.
  intros s tx s' H.
  unfold apply_one in H.
  destruct (snd tx <? 0); [discriminate H|].
  inversion H. reflexivity.
Qed.

Lemma apply_one_burn_delta :
  forall s tx s',
    apply_one s tx = Some s' ->
    fbs_burn s' = fbs_burn s + tx_burn tx
    /\ fbs_oru s' = fbs_oru s - tx_burn tx
    /\ fbs_miner s' = fbs_miner s + calculate_miner_fee (snd tx) (fst tx).
Proof.
  intros s tx s' H.
  unfold apply_one, tx_burn in H.
  destruct (snd tx <? 0); [discriminate H|].
  inversion H. repeat split; reflexivity.
Qed.

Lemma apply_txs_nil :
  forall s, apply_txs s [] = Some s.
Proof. reflexivity. Qed.

(** Replay: the same list on the same pre-state is a function. *)
Lemma fee_burn_replay_deterministic :
  forall (s : FeeBurnState) (txs : list FeeTx) (s1 s2 : FeeBurnState),
    apply_txs s txs = Some s1 ->
    apply_txs s txs = Some s2 ->
    s1 = s2.
Proof.
  intros s txs s1 s2 H1 H2.
  rewrite H1 in H2. inversion H2. reflexivity.
Qed.

Lemma apply_txs_res_unchanged :
  forall txs s s',
    apply_txs s txs = Some s' ->
    fbs_res s' = fbs_res s.
Proof.
  induction txs as [|tx rest IH]; intros s s' H; simpl in H.
  - inversion H. reflexivity.
  - destruct (apply_one s tx) as [s1|] eqn:Ho; [|discriminate H].
    pose proof (apply_one_res_unchanged s tx s1 Ho) as Hr.
    pose proof (IH s1 s' H) as IH'.
    rewrite IH'. exact Hr.
Qed.

Lemma apply_txs_burn_acc :
  forall txs s s',
    apply_txs s txs = Some s' ->
    fbs_burn s' = fbs_burn s + txs_burn txs
    /\ fbs_oru s' = fbs_oru s - txs_burn txs
    /\ fbs_miner s' = fbs_miner s + txs_miner txs
    /\ fbs_res s' = fbs_res s.
Proof.
  induction txs as [|tx rest IH]; intros s s' H; simpl in H.
  - inversion H. subst s'. simpl. lia.
  - destruct (apply_one s tx) as [s1|] eqn:Ho; [|discriminate H].
    destruct (apply_one_burn_delta s tx s1 Ho) as [Hb [Ho' Hm]].
    pose proof (apply_one_res_unchanged s tx s1 Ho) as Hr.
    destruct (IH s1 s' H) as [Hb2 [Ho2 [Hm2 Hr2]]].
    simpl.
    repeat split; lia.
Qed.

Lemma same_tx_replayed_same_burn :
  forall (s : FeeBurnState) (tx : FeeTx) (s1 s2 : FeeBurnState),
    apply_one s tx = Some s1 ->
    apply_one s tx = Some s2 ->
    fbs_burn s1 = fbs_burn s2
    /\ fbs_oru s1 = fbs_oru s2
    /\ fbs_res s1 = fbs_res s2
    /\ fbs_miner s1 = fbs_miner s2.
Proof.
  intros s tx s1 s2 H1 H2.
  rewrite H1 in H2. inversion H2. subst s2.
  repeat split; reflexivity.
Qed.

(** * Rollback via snapshot *)

Record FeeBurnSnapshot : Set := {
  snap_oru : Z;
  snap_res : Z;
  snap_burn : Z;
  snap_miner : Z
}.

Definition snapshot_of (s : FeeBurnState) : FeeBurnSnapshot :=
  {| snap_oru := fbs_oru s;
     snap_res := fbs_res s;
     snap_burn := fbs_burn s;
     snap_miner := fbs_miner s |}.

Definition restore (snap : FeeBurnSnapshot) : FeeBurnState :=
  {| fbs_oru := snap_oru snap;
     fbs_res := snap_res snap;
     fbs_burn := snap_burn snap;
     fbs_miner := snap_miner snap |}.

Lemma snapshot_restore :
  forall s, restore (snapshot_of s) = s.
Proof. intros []. reflexivity. Qed.

Definition apply_block (s : FeeBurnState) (txs : list FeeTx)
  : option (FeeBurnState * FeeBurnSnapshot) :=
  match apply_txs s txs with
  | None => None
  | Some s' => Some (s', snapshot_of s)
  end.

Definition rollback (_s : FeeBurnState) (snap : FeeBurnSnapshot)
  : FeeBurnState :=
  restore snap.

Lemma rollback_restores_fee_burn_state :
  forall (s s' : FeeBurnState) (txs : list FeeTx) (snap : FeeBurnSnapshot),
    apply_block s txs = Some (s', snap) ->
    rollback s' snap = s.
Proof.
  intros s s' txs snap H.
  unfold apply_block in H.
  destruct (apply_txs s txs); [|discriminate H].
  inversion H. subst.
  unfold rollback.
  apply snapshot_restore.
Qed.

Lemma rollback_restores_oru_res_burn :
  forall s s' txs snap,
    apply_block s txs = Some (s', snap) ->
    fbs_oru (rollback s' snap) = fbs_oru s
    /\ fbs_res (rollback s' snap) = fbs_res s
    /\ fbs_burn (rollback s' snap) = fbs_burn s
    /\ fbs_miner (rollback s' snap) = fbs_miner s.
Proof.
  intros s s' txs snap H.
  rewrite (rollback_restores_fee_burn_state s s' txs snap H).
  repeat split; reflexivity.
Qed.

(** * Reorg: disconnect A, connect B — burn is B's, not A+B *)

Lemma reorg_does_not_create_or_destroy_extra_burn :
  forall (s sA sB : FeeBurnState) (txsA txsB : list FeeTx)
         (snapA : FeeBurnSnapshot),
    apply_block s txsA = Some (sA, snapA) ->
    apply_txs (rollback sA snapA) txsB = Some sB ->
    fbs_burn sB = fbs_burn s + txs_burn txsB
    /\ fbs_oru sB = fbs_oru s - txs_burn txsB
    /\ fbs_res sB = fbs_res s
    /\ (fbs_burn sB <> fbs_burn s + txs_burn txsA + txs_burn txsB
        \/ txs_burn txsA = 0).
Proof.
  intros s sA sB txsA txsB snapA HA HB.
  pose proof (rollback_restores_fee_burn_state s sA txsA snapA HA) as Hr.
  rewrite Hr in HB.
  destruct (apply_txs_burn_acc txsB s sB HB) as [Hb [Ho [_ Hs]]].
  split; [|split; [|split]].
  - exact Hb.
  - exact Ho.
  - exact Hs.
  - destruct (Z.eq_dec (txs_burn txsA) 0) as [Hz|Hnz].
    + right. exact Hz.
    + left. rewrite Hb. lia.
Qed.

Lemma reorg_burn_equals_new_branch :
  forall s sA sB txsA txsB snapA,
    apply_block s txsA = Some (sA, snapA) ->
    apply_txs (rollback sA snapA) txsB = Some sB ->
    fbs_burn sB = fbs_burn s + txs_burn txsB.
Proof.
  intros s sA sB txsA txsB snapA HA HB.
  pose proof (reorg_does_not_create_or_destroy_extra_burn
                s sA sB txsA txsB snapA HA HB) as [H _].
  exact H.
Qed.

(** Multi-block history: connect then rollback-1 is identity. *)
Lemma connect_rollback_identity :
  forall s txs s' snap,
    apply_block s txs = Some (s', snap) ->
    restore snap = s /\ snapshot_of (restore snap) = snapshot_of s.
Proof.
  intros s txs s' snap H.
  pose proof (rollback_restores_fee_burn_state s s' txs snap H) as Hr.
  unfold rollback in Hr.
  split.
  - exact Hr.
  - rewrite Hr. reflexivity.
Qed.

(** Same block body replayed twice from the same snapshot agrees. *)
Lemma replay_block_same_burn :
  forall s txs s1 snap1 s2 snap2,
    apply_block s txs = Some (s1, snap1) ->
    apply_block s txs = Some (s2, snap2) ->
    fbs_burn s1 = fbs_burn s2
    /\ snap1 = snap2
    /\ s1 = s2.
Proof.
  intros s txs s1 snap1 s2 snap2 H1 H2.
  rewrite H1 in H2. inversion H2. subst.
  repeat split; reflexivity.
Qed.

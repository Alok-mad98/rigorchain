(** RigorKernel.Kernel.Chain.ForkChoice — most-work tip selection.

    Scope: consensus kernel only.

    Tips share a named prefix. Each block carries compact [nBits].
    The per-block increment is [GetBlockProof] = 2^256 / (target+1)
    after SetCompact guards ([Pow.BlockProof]). Among a finite set of
    suffixes, the live tip is a maximal-work one; a tie keeps the
    already-active suffix (no reorg for nothing). The two-tip rule
    [select_suffix] is the binary special case.

    The common prefix's work cannot flip the verdict, because work
    is a sum.

    NOT CLAIMED:
      k-deep common-prefix bound (honest-majority)
      C++ ActivateBestChain / arith_uint256 bit-exactness
      OP-R1 closed
      C++ DisconnectBlock
      GetBlockProofEquivalentTime
*)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
Import ListNotations.
From RigorKernel Require Import Supply.SupplySchedule.
From RigorKernel Require Import Pow.BlockProof.

Local Open Scope Z_scope.

Record work_block := {
  wb_nbits : Z;
  wb_height : Z
}.

Definition wb_work (b : work_block) : Z := get_block_proof (wb_nbits b).

Fixpoint chain_work (bs : list work_block) : Z :=
  match bs with
  | nil => 0
  | b :: rest => wb_work b + chain_work rest
  end.

Fixpoint chain_work_from_nbits (bs : list work_block) : Z :=
  match bs with
  | nil => 0
  | b :: rest => get_block_proof (wb_nbits b) + chain_work_from_nbits rest
  end.

(** Most work wins. A tie keeps the first argument — the already
    active suffix — so equal work is not a reorg. *)
Definition select_suffix (a b : list work_block) : list work_block :=
  if chain_work b <? chain_work a then a
  else if chain_work a <? chain_work b then b
  else a.

Definition select_by_total
    (prefix a b : list work_block) : list work_block :=
  if chain_work (prefix ++ b) <? chain_work (prefix ++ a) then a
  else if chain_work (prefix ++ a) <? chain_work (prefix ++ b) then b
  else a.

Fixpoint fold_work_max (acc : Z) (cs : list (list work_block)) : Z :=
  match cs with
  | nil => acc
  | s :: rest =>
      let w := chain_work s in
      fold_work_max (if acc <? w then w else acc) rest
  end.

Definition pool_max_work
    (active : list work_block) (cands : list (list work_block)) : Z :=
  fold_work_max (chain_work active) cands.

Fixpoint first_at_work (w : Z) (cs : list (list work_block))
    : option (list work_block) :=
  match cs with
  | nil => None
  | s :: rest =>
      if chain_work s =? w then Some s else first_at_work w rest
  end.

(** Finite most-work: keep the active tip if it is already maximal,
    otherwise the first candidate that attains the pool max. *)
Definition select_tip
    (active : list work_block) (cands : list (list work_block))
    : list work_block :=
  let wstar := pool_max_work active cands in
  if chain_work active =? wstar then active
  else
    match first_at_work wstar cands with
    | Some s => s
    | None => active
    end.

Fixpoint oru_issued (bs : list work_block) : Z :=
  match bs with
  | nil => 0
  | b :: rest => oru_subsidy (wb_height b) + oru_issued rest
  end.

(** Statement: suffix work is the sum of [GetBlockProof] on each
    block's nBits. A free [wb_work] field is a different function.
    Assumptions: none. Consensus kernel only: yes. *)
Theorem chain_work_uses_block_proof :
  forall bs,
    chain_work bs = chain_work_from_nbits bs.
Proof.
  induction bs as [|b bs IH]; [reflexivity|].
  simpl. rewrite IH. reflexivity.
Qed.

(** Statement: accumulated work is the sum of the increments.
    A last-only or first-only fold is not a chainwork. Assumptions:
    none. Consensus kernel only: yes. Increments are GetBlockProof. *)
Theorem chain_work_app :
  forall xs ys,
    chain_work (xs ++ ys) = chain_work xs + chain_work ys.
Proof.
  induction xs as [|x xs IH]; intros ys; simpl; [lia|].
  rewrite IH. lia.
Qed.

(** Statement: the strictly heavier suffix is selected, regardless
    of how many blocks it has. Assumptions: [chain_work b < chain_work a].
    Consensus kernel only: yes. *)
Theorem more_work_wins :
  forall a b,
    chain_work b < chain_work a ->
    select_suffix a b = a.
Proof.
  intros a b Hlt.
  unfold select_suffix.
  destruct (Z.ltb_spec (chain_work b) (chain_work a)) as [H|H];
    [reflexivity | lia].
Qed.

(** Statement: equal work keeps the already-active suffix.
    Assumptions: [chain_work a = chain_work b]. Consensus kernel
    only: yes. *)
Theorem equal_work_keeps_active :
  forall a b,
    chain_work a = chain_work b ->
    select_suffix a b = a.
Proof.
  intros a b Heq.
  unfold select_suffix.
  destruct (Z.ltb_spec (chain_work b) (chain_work a)) as [H1|H1]; [lia|].
  destruct (Z.ltb_spec (chain_work a) (chain_work b)) as [H2|H2]; [lia|].
  reflexivity.
Qed.

Theorem prefix_work_cancels :
  forall prefix a b,
    (chain_work (prefix ++ b) <? chain_work (prefix ++ a))
    = (chain_work b <? chain_work a).
Proof.
  intros prefix a b.
  rewrite !chain_work_app.
  destruct (Z.ltb_spec (chain_work b) (chain_work a));
    destruct (Z.ltb_spec (chain_work prefix + chain_work b)
                         (chain_work prefix + chain_work a));
    try reflexivity; lia.
Qed.

Theorem select_by_total_eq_suffix :
  forall prefix a b,
    select_by_total prefix a b = select_suffix a b.
Proof.
  intros prefix a b.
  unfold select_by_total, select_suffix.
  rewrite !prefix_work_cancels.
  reflexivity.
Qed.

Theorem select_suffix_is_a_or_b :
  forall a b,
    select_suffix a b = a \/ select_suffix a b = b.
Proof.
  intros a b.
  unfold select_suffix.
  destruct (chain_work b <? chain_work a); [left; reflexivity|].
  destruct (chain_work a <? chain_work b); [right; reflexivity | left; reflexivity].
Qed.

Theorem selected_oru_follows_winner :
  forall a b,
    chain_work b < chain_work a ->
    oru_issued (select_suffix a b) = oru_issued a.
Proof.
  intros a b Hlt.
  rewrite (more_work_wins a b Hlt).
  reflexivity.
Qed.

(** One block of work 3 (nBits 0x20400000) beats two of work 1
    (nBits 0x2100ffff). Longest-chain selection would have picked
    the other side. *)
Definition heavy_block : work_block :=
  {| wb_nbits := nbits_work_3; wb_height := 1 |}.
Definition light_block : work_block :=
  {| wb_nbits := nbits_work_1; wb_height := 1 |}.

Theorem longer_need_not_win :
  select_suffix [heavy_block] [light_block; light_block] = [heavy_block].
Proof.
  unfold select_suffix.
  replace (chain_work [heavy_block]) with 3.
  2: {
    unfold chain_work, heavy_block, wb_work.
    change (wb_nbits {| wb_nbits := nbits_work_3; wb_height := 1 |})
      with nbits_work_3.
    rewrite get_block_proof_work_3. reflexivity.
  }
  replace (chain_work [light_block; light_block]) with 2.
  2: {
    unfold chain_work, light_block, wb_work.
    change (wb_nbits {| wb_nbits := nbits_work_1; wb_height := 1 |})
      with nbits_work_1.
    rewrite get_block_proof_work_1. reflexivity.
  }
  reflexivity.
Qed.

Lemma fold_work_max_ge_acc :
  forall cs acc, acc <= fold_work_max acc cs.
Proof.
  induction cs as [|s cs IH]; intros acc; simpl; [lia|].
  destruct (Z.ltb_spec acc (chain_work s)) as [Hlt|Hge].
  - pose proof (IH (chain_work s)) as Hfold. lia.
  - pose proof (IH acc) as Hfold. lia.
Qed.

Lemma fold_work_max_ge_in :
  forall cs acc s,
    In s cs -> chain_work s <= fold_work_max acc cs.
Proof.
  induction cs as [|x cs IH]; intros acc s Hin; simpl in Hin; [contradiction|].
  simpl.
  destruct Hin as [Heq|Hin].
  - subst s.
    destruct (Z.ltb_spec acc (chain_work x)) as [Hlt|Hge].
    + pose proof (fold_work_max_ge_acc cs (chain_work x)) as Hfold. lia.
    + pose proof (fold_work_max_ge_acc cs acc) as Hfold. lia.
  - destruct (Z.ltb_spec acc (chain_work x)); apply IH; exact Hin.
Qed.

Lemma fold_work_max_attained :
  forall cs acc,
    fold_work_max acc cs = acc
    \/ exists s, In s cs /\ chain_work s = fold_work_max acc cs.
Proof.
  induction cs as [|x cs IH]; intros acc; simpl.
  - left. reflexivity.
  - destruct (Z.ltb_spec acc (chain_work x)) as [Hlt|Hge].
    + destruct (IH (chain_work x)) as [Heq|[s [Hin Hs]]].
      * right. exists x. split; [left; reflexivity|].
        rewrite Heq. reflexivity.
      * right. exists s. split; [right; exact Hin| exact Hs].
    + destruct (IH acc) as [Heq|[s [Hin Hs]]].
      * left. exact Heq.
      * right. exists s. split; [right; exact Hin| exact Hs].
Qed.

Lemma first_at_work_some :
  forall cs w s,
    first_at_work w cs = Some s ->
    In s cs /\ chain_work s = w.
Proof.
  induction cs as [|x cs IH]; intros w s H; simpl in H; [discriminate|].
  destruct (Z.eqb_spec (chain_work x) w) as [Heq|Hne].
  - inversion H. subst s. split; [left; reflexivity| exact Heq].
  - apply IH in H. destruct H as [Hin Hw].
    split; [right; exact Hin| exact Hw].
Qed.

Lemma first_at_work_finds :
  forall cs w,
    (exists s, In s cs /\ chain_work s = w) ->
    first_at_work w cs <> None.
Proof.
  induction cs as [|x cs IH]; intros w [s [Hin Heq]]; simpl in Hin;
    [contradiction|].
  simpl.
  destruct Hin as [Hx|Hin].
  - subst x. rewrite Heq. rewrite Z.eqb_refl. discriminate.
  - destruct (Z.eqb_spec (chain_work x) w); [discriminate|].
    apply IH. exists s. split; [exact Hin| exact Heq].
Qed.

Lemma select_tip_work_is_max :
  forall a cs,
    chain_work (select_tip a cs) = pool_max_work a cs.
Proof.
  intros a cs.
  unfold select_tip, pool_max_work.
  set (wstar := fold_work_max (chain_work a) cs).
  destruct (Z.eqb_spec (chain_work a) wstar) as [Heq|Hne]; [exact Heq|].
  destruct (first_at_work wstar cs) as [s|] eqn:Hf.
  - apply first_at_work_some in Hf. apply Hf.
  - exfalso.
    destruct (fold_work_max_attained cs (chain_work a)) as [Hacc|Hex].
    + apply Hne. unfold wstar. symmetry. exact Hacc.
    + apply (first_at_work_finds cs wstar).
      * destruct Hex as [s [Hin Hs]]. exists s. split; [exact Hin|].
        unfold wstar. exact Hs.
      * exact Hf.
Qed.

(** Statement: the selected tip's work is at least every candidate's
    work, including the already-active one. Assumptions: none.
    Consensus kernel only: yes. Not ActivateBestChain, not OP-R1. *)
Theorem select_tip_maximal :
  forall a cs x,
    In x (a :: cs) ->
    chain_work x <= chain_work (select_tip a cs).
Proof.
  intros a cs x Hin.
  unfold select_tip, pool_max_work.
  set (wstar := fold_work_max (chain_work a) cs).
  assert (Hx : chain_work x <= wstar).
  { destruct Hin as [Ha|Hin'].
    - subst x. apply fold_work_max_ge_acc.
    - apply fold_work_max_ge_in. exact Hin'. }
  destruct (Z.eqb_spec (chain_work a) wstar) as [Heq|Hne].
  - rewrite Heq. exact Hx.
  - destruct (first_at_work wstar cs) as [s|] eqn:Hf.
    + apply first_at_work_some in Hf.
      rewrite (proj2 Hf). exact Hx.
    + exfalso.
      destruct (fold_work_max_attained cs (chain_work a)) as [Hacc|Hex].
      * apply Hne. unfold wstar. symmetry. exact Hacc.
      * apply (first_at_work_finds cs wstar).
        -- destruct Hex as [s [Hin' Hs]]. exists s. split; [exact Hin'|].
           unfold wstar. exact Hs.
        -- exact Hf.
Qed.

(** Statement: if the active tip already has the pool max, it stays
    selected. Equal work is not a reorg. Assumptions:
    [chain_work a = pool_max_work a cs].
    Consensus kernel only: yes. *)
Theorem select_tip_keeps_active :
  forall a cs,
    chain_work a = pool_max_work a cs ->
    select_tip a cs = a.
Proof.
  intros a cs Heq.
  unfold select_tip.
  destruct (Z.eqb_spec (chain_work a) (pool_max_work a cs)) as [H|H];
    [reflexivity|].
  congruence.
Qed.

(** Statement: the two-tip rule is the binary special case of
    finite-tip selection. Assumptions: none.
    Consensus kernel only: yes. *)
Theorem select_tip_binary :
  forall a b,
    select_tip a [b] = select_suffix a b.
Proof.
  intros a b.
  unfold select_tip, select_suffix, pool_max_work.
  simpl.
  destruct (Z.ltb_spec (chain_work a) (chain_work b)) as [Hlt|Hge].
  - destruct (Z.eqb_spec (chain_work a) (chain_work b)) as [Heq|Hne]; [lia|].
    destruct (Z.eqb_spec (chain_work b) (chain_work b)) as [Hb|Hb]; [|lia].
    destruct (Z.ltb_spec (chain_work b) (chain_work a)) as [Hba|Hba]; [lia|].
    destruct (Z.ltb_spec (chain_work a) (chain_work b)) as [Hab|Hab];
      [reflexivity|lia].
  - destruct (Z.eqb_spec (chain_work a) (chain_work a)) as [Ha|Ha]; [|lia].
    destruct (Z.ltb_spec (chain_work b) (chain_work a)) as [Hba|Hba];
      [reflexivity|].
    destruct (Z.ltb_spec (chain_work a) (chain_work b)) as [Hab|Hab];
      [lia|reflexivity].
Qed.

(** Statement: the selected tip is one of the candidates, including
    the already-active suffix. Assumptions: none.
    Consensus kernel only: yes. *)
Theorem select_tip_total :
  forall a cs,
    In (select_tip a cs) (a :: cs).
Proof.
  intros a cs.
  unfold select_tip.
  destruct (Z.eqb_spec (chain_work a) (pool_max_work a cs)) as [Heq|Hne].
  - left. reflexivity.
  - destruct (first_at_work (pool_max_work a cs) cs) as [s|] eqn:Hf.
    + right. apply first_at_work_some in Hf. apply Hf.
    + left. reflexivity.
Qed.

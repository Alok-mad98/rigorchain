(** ReplayReorgCore/StateTransition.v — structural ApplyBlock / ApplyTx. *)
From Coq Require Import ZArith.ZArith.
From Coq Require Import List.
From Coq Require Import Arith.
From Coq Require Import Bool.
From Coq Require Import Lia.
Import ListNotations.
Require Import Rigor.ReplayReorgCore.ReplayState.

(** Decidable OutPoint equality (needed for list UTXO updates). *)
Definition outpoint_eqb (a b : OutPoint) : bool :=
  match a, b with
  | MkOutPoint ha ia, MkOutPoint hb ib =>
      andb (Nat.eqb ia ib)
           (match hash32_eq_dec ha hb with left _ => true | right _ => false end)
  end.

Fixpoint utxo_lookup (s : UTXOSet) (op : OutPoint) : option TxOut :=
  match s with
  | nil => None
  | (op', o) :: t =>
      if outpoint_eqb op op' then Some o else utxo_lookup t op
  end.

Definition utxo_remove (s : UTXOSet) (op : OutPoint) : UTXOSet :=
  filter (fun p => negb (outpoint_eqb (fst p) op)) s.

Definition utxo_insert (s : UTXOSet) (op : OutPoint) (o : TxOut) : UTXOSet :=
  utxo_remove s op ++ [(op, o)].

(** All spends must exist; outputs appended with synthetic OutPoints (structural). *)
Definition apply_tx_utxo (s : UTXOSet) (tx : TxAbs) (tx_index : nat)
  : option UTXOSet :=
  let spends_ok :=
    forallb (fun op => match utxo_lookup s op with Some _ => true | None => false end)
            (tx_spends tx) in
  if spends_ok then
    let s1 := fold_left (fun u op => utxo_remove u op) (tx_spends tx) s in
    let s2 :=
      fold_left
        (fun (acc : UTXOSet * nat) o =>
           let (u, idx) := acc in
           let op := MkOutPoint (hash32_placeholder) idx in
           (utxo_insert u op o, S idx))
        (tx_creates tx)
        (s1, tx_index) in
    Some (fst s2)
  else
    None.

(** Header connects to current tip (height + prev hash). *)
Definition valid_header_connect (cm : ChainMeta) (hdr : BlockHeaderAbs) : bool :=
  andb (Nat.eqb (hdr_height hdr) (S (cm_height cm)))
       (match hash32_eq_dec (hdr_prev_hash hdr) (cm_tip_hash cm) with
        | left _ => true
        | right _ => false
        end).

(** Chainwork strictly increases along the active chain (INV-01 structural). *)
Definition valid_chainwork_extend (cm : ChainMeta) (hdr : BlockHeaderAbs) : bool :=
  Z.gtb (hdr_chainwork hdr) (cm_chainwork cm).

Require Import Rigor.ReplayReorgCore.ConnectBlockMint.

(** SECURITY-001-F G-2: the third conjunct is new. Without it the model let a block mint any
    amount of ORU it liked, because nothing else inspected [cm_oru_subsidy]. Production has always
    rejected an over-paying coinbase (`bad-cb-amount`), so this closes a gap between the model and
    the code rather than a hole in the code. *)
Definition valid_block_connect (s : ReplayState) (b : BlockAbs) : bool :=
  let hdr := blk_header b in
  andb (andb (valid_header_connect (rs_chain s) hdr)
             (valid_chainwork_extend (rs_chain s) hdr))
       (cb_oru_subsidy_ok (hdr_height hdr) (blk_mint b)).

(** The issuance bound a connected block obeys, stated on the delta actually applied. *)
Lemma valid_block_connect_bounds_oru_mint :
  forall (s : ReplayState) (b : BlockAbs),
    valid_block_connect s b = true ->
    (cb_oru_mint_delta s b <= cb_ORU_INITIAL_SUBSIDY)%Z.
Proof.
  intros s b H. unfold valid_block_connect in H.
  apply andb_prop in H as [_ Hsub].
  unfold cb_oru_subsidy_ok in Hsub. apply Z.leb_le in Hsub.
  unfold cb_oru_mint_delta, cb_oru_subsidy.
  pose proof (cb_expected_oru_subsidy_le_initial (hdr_height (blk_header b))) as Hsched.
  unfold cb_ORU_INITIAL_SUBSIDY in *. lia.
Qed.

(** ConnectBlock-aligned supply delta (ORU subsidy + capped RES mint). *)
Definition apply_supply_delta (s : ReplayState) (b : BlockAbs) : ReplayState :=
  apply_connectblock_supply_delta s b.

(** Snapshot before extension (rollback anchor). *)
Definition push_history (s_pre s_post : ReplayState) : ReplayState :=
  {| rs_utxo := rs_utxo s_post;
     rs_chain := rs_chain s_post;
     rs_oru_supply := rs_oru_supply s_post;
     rs_res_supply := rs_res_supply s_post;
     rs_history := snapshot_of_state s_pre :: rs_history s_post |}.

(** Apply one transaction; [tx_index] disambiguates synthetic outpoints. *)
Definition apply_tx (s : ReplayState) (tx : TxAbs) (tx_index : nat)
  : option ReplayState :=
  match apply_tx_utxo (rs_utxo s) tx tx_index with
  | None => None
  | Some u' =>
      Some (replay_set_utxo s u')
  end.

Fixpoint apply_txs (s : ReplayState) (txs : list TxAbs) (idx : nat)
  : option ReplayState :=
  match txs with
  | nil => Some s
  | t :: rest =>
      match apply_tx s t idx with
      | None => None
      | Some s' => apply_txs s' rest (S idx)
      end
  end.

(** [ApplyBlock]: total on valid inputs; [None] = rejection (ConnectBlock ⊥). *)
Definition apply_block_body (s : ReplayState) (b : BlockAbs) : option ReplayState :=
  match apply_txs s (blk_txs b) 0 with
  | None => None
  | Some s_tx =>
      let hdr := blk_header b in
      let cm' :=
        {| cm_height := hdr_height hdr;
           cm_chainwork := hdr_chainwork hdr;
           cm_tip_hash := hash32_placeholder |} in
      let s' :=
        apply_supply_delta (replay_set_chain s_tx cm') b in
      Some (push_history s s')
  end.

Definition apply_block (s : ReplayState) (b : BlockAbs) : option ReplayState :=
  if valid_block_connect s b then apply_block_body s b else None.

(** Fold a block sequence from [s0] (deterministic when defined). *)
Fixpoint fold_apply_blocks (s : ReplayState) (bs : list BlockAbs)
  : option ReplayState :=
  match bs with
  | nil => Some s
  | b :: rest =>
      match apply_block s b with
      | None => None
      | Some s' => fold_apply_blocks s' rest
      end
  end.

(** Structural validity: every block connects to the running tip. *)
Fixpoint valid_block_sequence (s : ReplayState) (bs : list BlockAbs) : bool :=
  match bs with
  | nil => true
  | b :: rest =>
      if valid_block_connect s b then
        match apply_block_body s b with
        | None => false
        | Some s' => valid_block_sequence s' rest
        end
      else
        false
  end.

Lemma apply_block_deterministic :
  forall (s : ReplayState) (b : BlockAbs) (r1 r2 : ReplayState),
    apply_block s b = Some r1 ->
    apply_block s b = Some r2 ->
    r1 = r2.
Proof.
  intros s b r1 r2 H1 H2.
  congruence.
Qed.

Lemma valid_block_connect_apply_block :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    valid_block_connect s b = true ->
    apply_block_body s b = Some s' ->
    apply_block s b = Some s'.
Proof.
  intros s b s' Hc Hbody.
  unfold apply_block. rewrite Hc. exact Hbody.
Qed.

Lemma valid_block_sequence_head :
  forall (s : ReplayState) (b : BlockAbs) (rest : list BlockAbs),
    valid_block_sequence s (b :: rest) = true ->
    exists s',
      apply_block_body s b = Some s' /\
      valid_block_sequence s' rest = true.
Proof.
  intros s b rest H.
  simpl in H.
  destruct (valid_block_connect s b) eqn:Hc; [|discriminate H].
  destruct (apply_block_body s b) as [s'|] eqn:Hb; [|discriminate H].
  exists s'. split; eauto.
Qed.

Lemma fold_apply_blocks_cons :
  forall (s : ReplayState) (b : BlockAbs) (rest : list BlockAbs)
         (s' s'' : ReplayState),
    apply_block s b = Some s' ->
    fold_apply_blocks s' rest = Some s'' ->
    fold_apply_blocks s (b :: rest) = Some s''.
Proof.
  intros s b rest s' s'' Hab Hfold.
  simpl. rewrite Hab. exact Hfold.
Qed.

Theorem valid_block_sequence_fold_some :
  forall (s : ReplayState) (blocks : list BlockAbs),
    valid_block_sequence s blocks = true ->
    exists s', fold_apply_blocks s blocks = Some s'.
Proof.
  intros s blocks H.
  revert s H.
  induction blocks as [|b rest IH]; intros s H; simpl in H.
  - exists s. reflexivity.
  - destruct (valid_block_connect s b) eqn:Hc; [|discriminate].
    destruct (apply_block_body s b) as [s1|] eqn:Hb; [|discriminate].
    destruct (valid_block_sequence s1 rest) eqn:Hrest; [|discriminate].
    destruct (IH s1 Hrest) as [s'' Hfold].
    exists s''.
    eapply fold_apply_blocks_cons; eauto.
    eapply valid_block_connect_apply_block; eauto.
Qed.

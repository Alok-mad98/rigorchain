(** Draw candidate root — byte-accurate counterpart of Convergence_Root.

    C++: [HashConcatenatedSortedTxids] / [ConvergenceDrawEligibleRoot]
         (core/src/consensus/convergence_logic.cpp).
    Python L1: [convergence_draw_eligible_root] (rigor_consensus_spec.py).

    Same sorted-concat-then-SHA-256d shape as [convergence_root_from_txid_bytes]
    for every nonempty list. The remaining distinction is the empty set:
    C is 32 zero bytes (no hash); DCR hashes the empty concatenation
    (SHA-256d of ``[]``). A singleton of either function is Hash256(txid).

    The hashed list is the subset that actually enters selection: inequality-
    eligible candidates after F (and, under A-9-C, after already-paid exclusion)
    is established. [WindowExclusion.v] composes this hash with [assemble_F].

    Nothing here claims Coq = C++. These are theorems about the Coq model.
    Correspondence to the node is held by golden vectors. *)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Sorting.Permutation.
Require Import Coq.Arith.PeanoNat.
From Coq Require Import Lia.
Import ListNotations.
Require Import Rigor.legacy_account_model.Types.
From Rigor Require Import RigorBytes.
Include RigorBytes.
Require Import RigorHash.
Require Import ConvergeRoot.
Require Import Sigma.
Require Import Rigor.legacy_account_model.Selection.
Require Import Rigor.OrderIndependence.

(** * 1. HashConcatenatedSortedTxids (empty hashes ``[]``; C does not) *)

(** Authoritative byte-level eligible-subset root. Matches
    ``sha256d(b"".join(sorted(eligible_txids)))``. *)
Definition hash_concatenated_sorted_txids (txids : list (list byte)) : list byte :=
  hash256_bytes (concat_txid_lists (sort_txid_bytes txids)).

Definition draw_candidate_root_from_txid_bytes (txids : list (list byte)) : list byte :=
  hash_concatenated_sorted_txids txids.

(** Python ``zip(txids, eligible)``: extra flags or extra txids are ignored. *)
Fixpoint filter_by_flags (txids : list (list byte)) (flags : list bool) : list (list byte) :=
  match txids, flags with
  | t :: ts, f :: fs =>
      if f then t :: filter_by_flags ts fs else filter_by_flags ts fs
  | _, _ => nil
  end.

Definition draw_candidate_root_from_flags
    (txids : list (list byte)) (elig : list bool) : list byte :=
  draw_candidate_root_from_txid_bytes (filter_by_flags txids elig).

(** Bridge: already-filtered [Tx] list using [tx_id_bytes]. *)
Definition DrawCandidateRoot (eligible : list Tx) : list byte :=
  draw_candidate_root_from_txid_bytes (map tx_id_bytes eligible).

(** Public digest of the candidates that actually enter selection for this draw.
    [C] is typically [ConvergeRoot F]; this function does not recompute C. *)
Definition draw_candidate_root (F : list Tx) (C B : list byte) (h : Z) : list byte :=
  DrawCandidateRoot (eligible_candidates F C B h).

(** * 2. Length, empty set, singleton *)

Lemma app_bytes_nil : forall xs, app_bytes xs [] = xs.
Proof. induction xs as [|x xs IH]; simpl; [reflexivity | rewrite IH; reflexivity]. Qed.

Theorem draw_candidate_root_length :
  forall txids, length (draw_candidate_root_from_txid_bytes txids) = 32%nat.
Proof. intros. apply hash256_bytes_length. Qed.

Theorem DrawCandidateRoot_length :
  forall F, length (DrawCandidateRoot F) = 32%nat.
Proof. intros. apply draw_candidate_root_length. Qed.

Theorem draw_candidate_root_of_F_length :
  forall F C B h, length (draw_candidate_root F C B h) = 32%nat.
Proof. intros. apply DrawCandidateRoot_length. Qed.

Theorem draw_candidate_root_empty_is_hash_of_empty :
  draw_candidate_root_from_txid_bytes [] = hash256_bytes [].
Proof. reflexivity. Qed.

Theorem DrawCandidateRoot_empty :
  DrawCandidateRoot [] = hash256_bytes [].
Proof. reflexivity. Qed.

(** Empty eligible set is NOT the all-zero digest. That is the empty-F shortcut
    [ConvergeRoot] takes and this function must not. *)
Theorem draw_candidate_root_empty_neq_zero :
  draw_candidate_root_from_txid_bytes [] <> bytes32_zero.
Proof. vm_compute. discriminate. Qed.

Theorem DrawCandidateRoot_empty_neq_zero :
  DrawCandidateRoot [] <> bytes32_zero.
Proof. exact draw_candidate_root_empty_neq_zero. Qed.

Theorem draw_candidate_root_singleton :
  forall t, DrawCandidateRoot [t] = hash256_bytes (tx_id_bytes t).
Proof.
  intros t.
  unfold DrawCandidateRoot, draw_candidate_root_from_txid_bytes, hash_concatenated_sorted_txids.
  simpl. rewrite app_bytes_nil. reflexivity.
Qed.

Theorem singleton_roots_agree :
  forall t,
    ConvergeRoot [t] = hash256_bytes (tx_id_bytes t) /\
    DrawCandidateRoot [t] = hash256_bytes (tx_id_bytes t).
Proof.
  intros t. split.
  - unfold ConvergeRoot, convergence_root_from_txid_bytes. simpl.
    rewrite app_bytes_nil. reflexivity.
  - apply draw_candidate_root_singleton.
Qed.

(** * 3. Agreement with Convergence_Root when F is nonempty *)

Theorem draw_candidate_root_agrees_with_C_when_nonempty :
  forall a rest,
    draw_candidate_root_from_txid_bytes (a :: rest) =
    convergence_root_from_txid_bytes (a :: rest).
Proof.
  intros a rest.
  unfold draw_candidate_root_from_txid_bytes, hash_concatenated_sorted_txids,
         convergence_root_from_txid_bytes.
  reflexivity.
Qed.

Theorem draw_candidate_root_agrees_with_C_when_large :
  forall a b rest,
    draw_candidate_root_from_txid_bytes (a :: b :: rest) =
    convergence_root_from_txid_bytes (a :: b :: rest).
Proof.
  intros a b rest. apply draw_candidate_root_agrees_with_C_when_nonempty.
Qed.

Theorem DrawCandidateRoot_agrees_with_ConvergeRoot_nonempty :
  forall a rest,
    DrawCandidateRoot (a :: rest) = ConvergeRoot (a :: rest).
Proof.
  intros a rest.
  unfold DrawCandidateRoot, ConvergeRoot.
  apply draw_candidate_root_agrees_with_C_when_nonempty.
Qed.

Theorem DrawCandidateRoot_agrees_with_ConvergeRoot_large :
  forall a b rest,
    DrawCandidateRoot (a :: b :: rest) = ConvergeRoot (a :: b :: rest).
Proof.
  intros a b rest. apply DrawCandidateRoot_agrees_with_ConvergeRoot_nonempty.
Qed.

(** * 4. Order independence (same candidate set → same root) *)

(** Unconditional: DCR always hashes. [convergence_root_order_independent]
    still cases on empty vs nonempty because C is zeros only when F is empty. *)
Theorem draw_candidate_root_order_independent :
  forall l1 l2 : list (list byte),
    Permutation l1 l2 ->
    draw_candidate_root_from_txid_bytes l1 = draw_candidate_root_from_txid_bytes l2.
Proof.
  intros l1 l2 Hperm.
  unfold draw_candidate_root_from_txid_bytes, hash_concatenated_sorted_txids.
  f_equal. f_equal.
  apply sort_txid_bytes_order_independent. exact Hperm.
Qed.

Corollary DrawCandidateRoot_order_independent :
  forall F1 F2 : list Tx,
    Permutation F1 F2 -> DrawCandidateRoot F1 = DrawCandidateRoot F2.
Proof.
  intros F1 F2 H. unfold DrawCandidateRoot.
  apply draw_candidate_root_order_independent.
  apply Permutation_map. exact H.
Qed.

(** * 5. Eligible subset ⊆ F (the Convergence set) *)

Lemma filter_by_flags_in :
  forall txids flags t,
    In t (filter_by_flags txids flags) -> In t txids.
Proof.
  induction txids as [|x xs IH]; intros flags t Hin.
  - destruct flags; simpl in Hin; contradiction.
  - destruct flags as [|f fs]; simpl in Hin; [contradiction|].
    destruct f.
    + destruct Hin as [Heq | Hin].
      * left. exact Heq.
      * right. eapply IH. exact Hin.
    + right. eapply IH. exact Hin.
Qed.

Theorem draw_candidate_root_flags_subset :
  forall txids flags t,
    In t (filter_by_flags txids flags) -> In t txids.
Proof. apply filter_by_flags_in. Qed.

Theorem eligible_candidates_subset_of_F :
  forall F C B h t,
    In t (eligible_candidates F C B h) -> In t F.
Proof.
  intros F C B h t H.
  unfold eligible_candidates in H.
  apply filter_In in H. apply H.
Qed.

(** Every txid hashed into [draw_candidate_root F C B h] is a member of F.
    That is the relation to Convergence_Root: C commits F, and this root
    commits a subset of F. *)
Theorem draw_candidate_root_hashes_subset_of_F :
  forall F C B h,
    incl (map tx_id_bytes (eligible_candidates F C B h)) (map tx_id_bytes F).
Proof.
  intros F C B h id Hin.
  apply in_map_iff in Hin. destruct Hin as [t [Heq Hin]].
  apply in_map_iff. exists t. split; [exact Heq|].
  eapply eligible_candidates_subset_of_F. exact Hin.
Qed.

(** * 6. Winner selection uses the same set the root hashes *)

Lemma list_filter_permutation :
  forall (A : Type) (f : A -> bool) l1 l2,
    Permutation l1 l2 -> Permutation (filter f l1) (filter f l2).
Proof.
  intros A f l1 l2 Hperm. induction Hperm; simpl.
  - constructor.
  - destruct (f x); [constructor; assumption | assumption].
  - destruct (f x) eqn:Hx; destruct (f y) eqn:Hy; simpl.
    + apply perm_swap.
    + constructor. apply Permutation_refl.
    + constructor. apply Permutation_refl.
    + apply Permutation_refl.
  - eapply Permutation_trans; eassumption.
Qed.

Lemma eligible_candidates_perm :
  forall F1 F2 C B h,
    Permutation F1 F2 ->
    Permutation (eligible_candidates F1 C B h) (eligible_candidates F2 C B h).
Proof.
  intros F1 F2 C B h H. unfold eligible_candidates. apply list_filter_permutation. exact H.
Qed.

Lemma in_map_filter_in_map_dcr :
  forall (A B : Type) (f : A -> bool) (g : A -> B) l y,
    In y (map g (filter f l)) -> In y (map g l).
Proof.
  intros A B f g l y H.
  apply in_map_iff in H. destruct H as [x [Heq Hin]].
  apply filter_In in Hin. destruct Hin as [Hin _].
  apply in_map_iff. exists x. split; assumption.
Qed.

Lemma distinct_txids_filter_dcr :
  forall (f : Tx -> bool) l,
    distinct_txids l -> distinct_txids (filter f l).
Proof.
  intros f l. unfold distinct_txids.
  induction l as [|x xs IH]; simpl; [intros H; exact H|].
  intros Hd. inversion Hd as [|? ? Hnin Hrest]; subst.
  destruct (f x) eqn:Hf; simpl.
  - constructor.
    + intros Hin. apply Hnin. eapply in_map_filter_in_map_dcr. exact Hin.
    + apply IH. exact Hrest.
  - apply IH. exact Hrest.
Qed.

Lemma positive_weights_filter_dcr :
  forall (f : Tx -> bool) l,
    positive_weights l -> positive_weights (filter f l).
Proof.
  intros f l Hw t Hin. apply Hw.
  apply filter_In in Hin. apply Hin.
Qed.

(** The winner, if any, is a member of the list [draw_candidate_root] hashes. *)
Theorem winner_in_draw_eligible_set :
  forall F C B h w,
    select_winner (eligible_candidates F C B h) C B h = Some w ->
    In w (eligible_candidates F C B h).
Proof. intros F C B h w H. apply select_winner_in in H. exact H. Qed.

Theorem winner_txid_is_hashed :
  forall F C B h w,
    select_winner (eligible_candidates F C B h) C B h = Some w ->
    In (tx_id_bytes w) (map tx_id_bytes (eligible_candidates F C B h)).
Proof.
  intros F C B h w H. apply in_map. eapply winner_in_draw_eligible_set. exact H.
Qed.

Theorem select_winner_none_iff_empty_eligible :
  forall F C B h,
    select_winner (eligible_candidates F C B h) C B h = None <->
    eligible_candidates F C B h = [].
Proof.
  intros F C B h. split.
  - destruct (eligible_candidates F C B h) as [|x xs]; intros H; [reflexivity|].
    simpl in H. destruct (select_winner xs C B h); discriminate.
  - intros ->. reflexivity.
Qed.

Theorem empty_eligible_root_is_empty_concat :
  forall F C B h,
    eligible_candidates F C B h = [] ->
    draw_candidate_root F C B h = hash256_bytes [].
Proof.
  intros F C B h H. unfold draw_candidate_root. rewrite H. apply DrawCandidateRoot_empty.
Qed.

(** Same candidate *set* ⇒ same eligible-subset root *and* same winner.
    The winner theorem needs the usual positive-weight / distinct-txid hypotheses;
    the root equality does not. *)
Theorem draw_and_winner_order_independent :
  forall F1 F2 C B h,
    Permutation F1 F2 ->
    positive_weights F1 ->
    distinct_txids F1 ->
    draw_candidate_root F1 C B h = draw_candidate_root F2 C B h /\
    select_winner (eligible_candidates F1 C B h) C B h =
    select_winner (eligible_candidates F2 C B h) C B h.
Proof.
  intros F1 F2 C B h Hperm Hw Hd. split.
  - unfold draw_candidate_root.
    apply DrawCandidateRoot_order_independent.
    apply eligible_candidates_perm. exact Hperm.
  - apply select_winner_order_independent.
    + apply eligible_candidates_perm. exact Hperm.
    + unfold eligible_candidates. apply positive_weights_filter_dcr. exact Hw.
    + unfold eligible_candidates. apply distinct_txids_filter_dcr. exact Hd.
Qed.

(** * 7. Strengthened relation to Convergence_Root *)

Lemma filter_all_true :
  forall (A : Type) (f : A -> bool) (l : list A),
    (forall x, In x l -> f x = true) -> filter f l = l.
Proof.
  intros A f l H. induction l as [|x xs IH]; [reflexivity|].
  simpl. rewrite (H x (or_introl eq_refl)).
  f_equal. apply IH. intros y Hin. apply H. right. exact Hin.
Qed.

(** If every member of F is inequality-eligible, the hashed list is F itself. *)
Theorem all_eligible_draw_candidate_root_eq_F :
  forall F C B h,
    (forall t, In t F -> eligible_bool t C B h = true) ->
    draw_candidate_root F C B h = DrawCandidateRoot F.
Proof.
  intros F C B h Hall.
  unfold draw_candidate_root, eligible_candidates.
  rewrite filter_all_true; [reflexivity | exact Hall].
Qed.

(** If every member of F is eligible and F is nonempty, the two roots coincide. *)
Theorem all_eligible_agrees_with_C_when_nonempty :
  forall F C B h,
    (forall t, In t F -> eligible_bool t C B h = true) ->
    F <> [] ->
    draw_candidate_root F C B h = ConvergeRoot F.
Proof.
  intros F C B h Hall Hne.
  rewrite all_eligible_draw_candidate_root_eq_F; [|exact Hall].
  destruct F as [|a rest]; [contradiction|].
  apply DrawCandidateRoot_agrees_with_ConvergeRoot_nonempty.
Qed.

Theorem all_eligible_agrees_with_C_when_large :
  forall F C B h,
    (forall t, In t F -> eligible_bool t C B h = true) ->
    (1 <= length F)%nat ->
    draw_candidate_root F C B h = ConvergeRoot F.
Proof.
  intros F C B h Hall Hlen.
  destruct F as [|a rest]; [simpl in Hlen; lia|].
  apply all_eligible_agrees_with_C_when_nonempty; [exact Hall|discriminate].
Qed.

(** Precise nonempty-eligible statement: DCR of F is C of the *eligible subset*,
    not of F. They agree on F itself only when everyone is eligible. *)
Theorem draw_candidate_root_is_C_of_eligible_when_nonempty :
  forall F C B h,
    eligible_candidates F C B h <> [] ->
    draw_candidate_root F C B h = ConvergeRoot (eligible_candidates F C B h).
Proof.
  intros F C B h Hne. unfold draw_candidate_root.
  destruct (eligible_candidates F C B h) as [|a rest] eqn:He; [contradiction|].
  apply DrawCandidateRoot_agrees_with_ConvergeRoot_nonempty.
Qed.

Theorem draw_candidate_root_is_C_of_eligible_when_large :
  forall F C B h,
    (1 <= length (eligible_candidates F C B h))%nat ->
    draw_candidate_root F C B h = ConvergeRoot (eligible_candidates F C B h).
Proof.
  intros F C B h Hlen. unfold draw_candidate_root.
  destruct (eligible_candidates F C B h) as [|a rest] eqn:He.
  - simpl in Hlen. lia.
  - apply DrawCandidateRoot_agrees_with_ConvergeRoot_nonempty.
Qed.

Theorem eligible_empty_C_is_zero :
  forall F C B h,
    eligible_candidates F C B h = [] ->
    ConvergeRoot (eligible_candidates F C B h) = bytes32_zero.
Proof.
  intros F C B h Heq. rewrite Heq. reflexivity.
Qed.

(** * 8. End-to-end pipeline: F → eligible (via σ) → DCR / winner

    The draw kernel is one pipeline on a shared [(F, C, B, h)]:

    - Eligibility is [σ(t, C, B, h) < T · weight t]. [C] here is the caller's
      Convergence_Root of F (see [Execute]); it is never [draw_candidate_root].
    - [draw_candidate_root] hashes that eligible list.
    - [select_winner] ranks the same list, still using [C] for σ products.

    Coinbase / RPC commitment of the digest is C++ ([ComputeConvergenceDrawCandidateRoot]);
    this module does not model that bind. See §9. *)

Theorem eligible_bool_iff_sigma_lt :
  forall t C B h,
    eligible_bool t C B h = true <->
    Z.lt (Sigma t C B h) (RES_PROBABILITY_TARGET * weight t).
Proof.
  intros t C B h. unfold eligible_bool.
  destruct (Z_lt_dec (Sigma t C B h) (RES_PROBABILITY_TARGET * weight t)) as [Hlt | Hn].
  - split; [intros _; exact Hlt | intros _; reflexivity].
  - split; [intros H; discriminate H | intros H; exfalso; exact (Hn H)].
Qed.

Theorem draw_candidate_root_hashes_eligible_list :
  forall F C B h,
    draw_candidate_root F C B h = DrawCandidateRoot (eligible_candidates F C B h).
Proof. intros. reflexivity. Qed.

Theorem winner_is_eligible_and_in_F :
  forall F C B h w,
    select_winner (eligible_candidates F C B h) C B h = Some w ->
    eligible_bool w C B h = true /\
    In w (eligible_candidates F C B h) /\
    In w F.
Proof.
  intros F C B h w H.
  assert (Hin : In w (eligible_candidates F C B h)) by (eapply winner_in_draw_eligible_set; exact H).
  split; [|split].
  - apply filter_In in Hin. apply Hin.
  - exact Hin.
  - eapply eligible_candidates_subset_of_F. exact Hin.
Qed.

(** Same inputs ⇒ the hashed list, the DCR digest, and the winner are aligned.
    [select_winner_order_independent] is used unchanged through
    [draw_and_winner_order_independent]. *)
Theorem draw_pipeline_e2e :
  forall F C B h,
    draw_candidate_root F C B h = DrawCandidateRoot (eligible_candidates F C B h) /\
    (select_winner (eligible_candidates F C B h) C B h = None <->
     eligible_candidates F C B h = []) /\
    (forall w,
      select_winner (eligible_candidates F C B h) C B h = Some w ->
      In w (eligible_candidates F C B h) /\
      eligible_bool w C B h = true /\
      In w F).
Proof.
  intros F C B h. split; [|split].
  - apply draw_candidate_root_hashes_eligible_list.
  - apply select_winner_none_iff_empty_eligible.
  - intros w Hw. apply winner_is_eligible_and_in_F in Hw.
    destruct Hw as [Helig [Hin HinF]]. split; [|split]; assumption.
Qed.

(** * 9. Residual opaque boundary — coinbase / Merkle / header (not in scope)

    TASK-FV-MODEL-RESIDUAL-OPAQUE-BOUNDARY-001.

    WE DO NOT CLAIM:
    - that this digest is written into coinbase
    - that it is a Merkle root of the transaction tree
    - that it appears in the block header
    - Coq = C++ [ComputeConvergenceDrawCandidateRoot]

    The type of [draw_candidate_root] is
    [list Tx -> list byte -> list byte -> Z -> list byte]. There is no
    coinbase, Merkle, or header argument. The proved equality below *is*
    the entire Coq claim: the digest of the eligible list. Inserting that
    digest into coinbase / the tx Merkle tree / the header is
    implementation-level C++. Replay-cone opaques ([out_script],
    [cm_draw_won], [rs_res_supply]) are in [OpaqueBoundary.v]. *)
Theorem draw_candidate_root_scope_is_eligible_digest :
  forall F C B h,
    draw_candidate_root F C B h =
    DrawCandidateRoot (eligible_candidates F C B h).
Proof. intros. apply draw_candidate_root_hashes_eligible_list. Qed.

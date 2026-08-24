(** ReplayReorgCore/CvgCovenant.v
    W3 one-shot covenant recognizers. [StateTransition.apply_tx] requires
    [valid_cvg_covenant_spend] (via [valid_cvg_spend_tx]): zero or one CVG
    input; a CVG spend cannot recreate CVG and must couple [cvg_owner_hash160]
    to the unique [OP_RES_MINT] pubkey hash.

    RETIRED MECHANISM — DESIGN STUDY ONLY. The C++ node has no OP_CVG_CLAIM: 0xbc sits above
    MAX_OPCODE, and ASSET_CVG, TxoutType::CVG_CLAIM and the covenant shape checks were all
    deleted. A ticket could not authorize its own draw win, so a covenant-funded mint needed the
    same ConvergeProof as an ORU-funded one; CVG2 burn does the job without an asset class that
    is neither circulating nor destroyed ORU. This module therefore models a class the node
    cannot represent, which is over-approximation (safe direction) and is recorded as such in
    [OpaqueBoundary]. Nothing here corresponds to shipped behaviour; see
    [StateTransition.cvg_create_gate] and the [utxo_cvg_free] theorems for why the covenant
    disjunct is unreachable even inside the model.

    Template (same local-check pattern as [OP_RES_MINT]):

      scriptPubKey = <20-byte HASH160(ed25519 pubkey)> OP_CVG_CLAIM
      OP_CVG_CLAIM = 0xbc (188)

    This module is the byte-level source of truth. [ScriptMintWitness.asset_from_script]
    maps the template to [SmwCVG]; [StateTransition.asset_kind_of_script] maps that
    to [AssetCVG]. Legacy witness-v1 CVG2 scripts still classify as ORU until a
    later C++ migration. Not a script interpreter, not [ConnectEvaluateResMintTx],
    not a C++ refinement. Honesty flags stay false. No [Axiom] / [Admitted]. *)
From Coq Require Import List.
From Coq Require Import Arith.
From Coq Require Import Bool.
From Coq Require Import ZArith.ZArith.
Import ListNotations.
Require Import Rigor.ReplayReorgCore.ReplayState.

(** C++ opcode is not landed. Model constant only. *)
Definition OP_CVG_CLAIM : nat := 188%nat.
Definition OP_PUSH20 : nat := 20%nat.

Record CvgCovenant := {
  cvg_owner_hash160 : list nat
}.

Definition parse_cvg_claim_script (spk : list nat) : option CvgCovenant :=
  match spk with
  | plen :: rest =>
      if Nat.eqb plen OP_PUSH20 then
        match rev rest with
        | op :: hash_rev =>
            if andb (Nat.eqb op OP_CVG_CLAIM)
                    (Nat.eqb (length hash_rev) 20%nat)
            then Some {| cvg_owner_hash160 := rev hash_rev |}
            else None
        | nil => None
        end
      else None
  | nil => None
  end.

Definition is_cvg_claim_script (spk : list nat) : bool :=
  match parse_cvg_claim_script spk with
  | Some _ => true
  | None => false
  end.

Definition is_cvg_covenant_output (o : TxOut) : bool :=
  is_cvg_claim_script (out_script o).

Definition count_cvg (outs : list TxOut) : nat :=
  length (filter is_cvg_covenant_output outs).

Definition cvg_unique_owner_hash (outs : list TxOut) : option (list nat) :=
  match filter is_cvg_covenant_output outs with
  | o :: nil =>
      match parse_cvg_claim_script (out_script o) with
      | Some c => Some (cvg_owner_hash160 c)
      | None => None
      end
  | _ => None
  end.

(** * Ticket face value floor

    Same value as C++ [Consensus::MIN_CONVERGENCE_AMOUNT] (1 ORU at COIN scale) and
    [InvalidCvg.icvg_min_convergence_amount], restated here so the covenant module does not
    depend on the legacy CVG2 module for a constant.

    A zero-value covenant output would be a permanent UTXO-set entry that costs its creator
    nothing, so the floor belongs to the creation rule rather than to relay policy. *)
Definition MIN_CVG_TICKET_VALUE : Z := 100000000%Z.

Definition cvg_ticket_value_ok (o : TxOut) : bool :=
  Z.geb (out_value o) MIN_CVG_TICKET_VALUE.

(** Every covenant output in the list clears the floor. Non-CVG outputs are unconstrained. *)
Definition cvg_creates_value_ok (outs : list TxOut) : bool :=
  forallb (fun o => if is_cvg_covenant_output o then cvg_ticket_value_ok o else true) outs.

Lemma cvg_creates_value_ok_nil : cvg_creates_value_ok nil = true.
Proof. reflexivity. Qed.

Lemma cvg_creates_value_ok_forall :
  forall (outs : list TxOut) (o : TxOut),
    cvg_creates_value_ok outs = true ->
    In o outs ->
    is_cvg_covenant_output o = true ->
    (MIN_CVG_TICKET_VALUE <= out_value o)%Z.
Proof.
  intros outs o Hok Hin Hcvg.
  unfold cvg_creates_value_ok in Hok.
  rewrite forallb_forall in Hok.
  specialize (Hok o Hin). rewrite Hcvg in Hok.
  unfold cvg_ticket_value_ok in Hok.
  apply Z.geb_le. exact Hok.
Qed.

Lemma cvg_creates_value_ok_rejects_below_floor :
  forall (o : TxOut) (rest : list TxOut),
    is_cvg_covenant_output o = true ->
    (out_value o < MIN_CVG_TICKET_VALUE)%Z ->
    cvg_creates_value_ok (o :: rest) = false.
Proof.
  intros o rest Hcvg Hlt.
  unfold cvg_creates_value_ok. simpl.
  apply andb_false_iff. left.
  rewrite Hcvg. unfold cvg_ticket_value_ok.
  rewrite Z.geb_leb. apply Z.leb_gt. exact Hlt.
Qed.

Definition canonical_cvg_owner_hash : list nat :=
  repeat 0%nat 20%nat.

Definition canonical_cvg_claim_script : list nat :=
  OP_PUSH20 :: canonical_cvg_owner_hash ++ [OP_CVG_CLAIM].

Definition mk_cvg (v : Z) : TxOut :=
  {| out_value := v; out_script := canonical_cvg_claim_script |}.

(** Spend-shape: no CVG input, or exactly one CVG input and zero CVG creates.
    Owner-hash coupling to [OP_RES_MINT] lives in [StateTransition.valid_cvg_covenant_spend]. *)
Definition cvg_spend_ok (ins : list TxOut) (tx : TxAbs) : bool :=
  match count_cvg ins with
  | 0%nat => true
  | 1%nat => Nat.eqb (count_cvg (tx_creates tx)) 0%nat
  | _ => false
  end.

Definition convergence_creates_one_cvg (tx : TxAbs) : bool :=
  Nat.eqb (count_cvg (tx_creates tx)) 1%nat.

Lemma mk_cvg_is_cvg_output : forall v : Z, is_cvg_covenant_output (mk_cvg v) = true.
Proof. intros v. vm_compute. reflexivity. Qed.

Lemma mk_cvg_below_floor_rejected : cvg_creates_value_ok [mk_cvg 1%Z] = false.
Proof. vm_compute. reflexivity. Qed.

Lemma mk_cvg_at_floor_accepted :
  cvg_creates_value_ok [mk_cvg MIN_CVG_TICKET_VALUE] = true.
Proof. vm_compute. reflexivity. Qed.

(** * Recognizer proofs *)

Lemma is_cvg_claim_script_iff_parse :
  forall spk : list nat,
    is_cvg_claim_script spk = true <->
    exists c : CvgCovenant, parse_cvg_claim_script spk = Some c.
Proof.
  intros spk. unfold is_cvg_claim_script.
  destruct (parse_cvg_claim_script spk) as [c|]; split; intros H.
  - exists c. reflexivity.
  - reflexivity.
  - discriminate H.
  - destruct H as [c Hc]. discriminate Hc.
Qed.

Lemma is_cvg_claim_script_false_iff_parse_none :
  forall spk : list nat,
    is_cvg_claim_script spk = false <->
    parse_cvg_claim_script spk = None.
Proof.
  intros spk. unfold is_cvg_claim_script.
  destruct (parse_cvg_claim_script spk); split; intros H;
    try reflexivity; try discriminate H.
Qed.

Lemma parse_cvg_claim_script_inv :
  forall (spk : list nat) (c : CvgCovenant),
    parse_cvg_claim_script spk = Some c ->
    spk = OP_PUSH20 :: cvg_owner_hash160 c ++ [OP_CVG_CLAIM] /\
    length (cvg_owner_hash160 c) = 20%nat.
Proof.
  intros spk c H.
  unfold parse_cvg_claim_script in H.
  destruct spk as [|plen rest]; [discriminate H|].
  destruct (Nat.eqb plen OP_PUSH20) eqn:Ep; [|discriminate H].
  apply Nat.eqb_eq in Ep. subst plen.
  destruct (rev rest) as [|op hash_rev] eqn:Er; [discriminate H|].
  destruct (andb (Nat.eqb op OP_CVG_CLAIM) (Nat.eqb (length hash_rev) 20%nat))
    eqn:Ea; [|discriminate H].
  apply andb_true_iff in Ea as [Eo El].
  apply Nat.eqb_eq in Eo. apply Nat.eqb_eq in El. subst op.
  inversion H. subst c. clear H.
  split.
  - assert (Hrest : rest = rev (OP_CVG_CLAIM :: hash_rev)).
    { rewrite <- (rev_involutive rest). rewrite Er. reflexivity. }
    rewrite Hrest. simpl.
    reflexivity.
  - cbn [cvg_owner_hash160]. rewrite rev_length. exact El.
Qed.

Lemma parse_cvg_claim_script_hash_len :
  forall (spk : list nat) (c : CvgCovenant),
    parse_cvg_claim_script spk = Some c ->
    length (cvg_owner_hash160 c) = 20%nat.
Proof.
  intros spk c H. apply parse_cvg_claim_script_inv in H. apply H.
Qed.

Lemma parse_cvg_claim_script_nil :
  parse_cvg_claim_script nil = None.
Proof. reflexivity. Qed.

Lemma is_cvg_claim_script_nil :
  is_cvg_claim_script nil = false.
Proof. reflexivity. Qed.

Lemma parse_cvg_claim_script_sound :
  forall (hash : list nat),
    length hash = 20%nat ->
    parse_cvg_claim_script (OP_PUSH20 :: hash ++ [OP_CVG_CLAIM]) =
      Some {| cvg_owner_hash160 := hash |}.
Proof.
  intros hash Hl.
  unfold parse_cvg_claim_script, OP_PUSH20, OP_CVG_CLAIM.
  rewrite Nat.eqb_refl.
  rewrite rev_app_distr. simpl.
  rewrite rev_length. rewrite Hl.
  rewrite Nat.eqb_refl.
  rewrite rev_involutive.
  reflexivity.
Qed.

Lemma is_cvg_claim_script_sound :
  forall hash : list nat,
    length hash = 20%nat ->
    is_cvg_claim_script (OP_PUSH20 :: hash ++ [OP_CVG_CLAIM]) = true.
Proof.
  intros hash Hl.
  unfold is_cvg_claim_script.
  rewrite (parse_cvg_claim_script_sound hash Hl).
  reflexivity.
Qed.

Lemma canonical_cvg_owner_hash_len :
  length canonical_cvg_owner_hash = 20%nat.
Proof. reflexivity. Qed.

Lemma canonical_cvg_claim_script_parses :
  parse_cvg_claim_script canonical_cvg_claim_script =
    Some {| cvg_owner_hash160 := canonical_cvg_owner_hash |}.
Proof.
  unfold canonical_cvg_claim_script.
  apply parse_cvg_claim_script_sound.
  exact canonical_cvg_owner_hash_len.
Qed.

Lemma canonical_cvg_claim_script_ok :
  is_cvg_claim_script canonical_cvg_claim_script = true.
Proof.
  unfold is_cvg_claim_script.
  rewrite canonical_cvg_claim_script_parses.
  reflexivity.
Qed.

Lemma is_cvg_covenant_output_script :
  forall (v : Z) (spk : list nat),
    is_cvg_covenant_output {| out_value := v; out_script := spk |} =
    is_cvg_claim_script spk.
Proof. intros v spk. reflexivity. Qed.

Lemma is_cvg_covenant_output_canonical :
  is_cvg_covenant_output (mk_cvg 1%Z) = true.
Proof.
  unfold mk_cvg, is_cvg_covenant_output. cbn [out_script].
  exact canonical_cvg_claim_script_ok.
Qed.

Lemma count_cvg_nil : count_cvg nil = 0%nat.
Proof. reflexivity. Qed.

Lemma count_cvg_canonical_cons :
  forall rest : list TxOut,
    count_cvg (mk_cvg 1%Z :: rest) = S (count_cvg rest).
Proof.
  intros rest. unfold count_cvg, mk_cvg, is_cvg_covenant_output.
  cbn [out_script filter length].
  rewrite canonical_cvg_claim_script_ok.
  reflexivity.
Qed.

Lemma parse_cvg_claim_script_rejects_wrong_opcode :
  forall hash : list nat,
    length hash = 20%nat ->
    parse_cvg_claim_script (OP_PUSH20 :: hash ++ [187%nat]) = None.
Proof.
  intros hash Hl.
  unfold parse_cvg_claim_script, OP_PUSH20, OP_CVG_CLAIM.
  rewrite Nat.eqb_refl.
  rewrite rev_app_distr. simpl.
  reflexivity.
Qed.

Lemma is_cvg_claim_script_rejects_empty_hash :
  is_cvg_claim_script [OP_PUSH20; OP_CVG_CLAIM] = false.
Proof. reflexivity. Qed.

Lemma cvg_spend_ok_no_cvg :
  forall (ins : list TxOut) (tx : TxAbs),
    count_cvg ins = 0%nat ->
    cvg_spend_ok ins tx = true.
Proof.
  intros ins tx H. unfold cvg_spend_ok. rewrite H. reflexivity.
Qed.

Lemma count_cvg_eq_filter_length :
  forall os : list TxOut,
    count_cvg os = length (filter is_cvg_covenant_output os).
Proof. intros os. reflexivity. Qed.

Lemma cvg_unique_owner_hash_of_count_1 :
  forall os : list TxOut,
    count_cvg os = 1%nat ->
    exists h : list nat, cvg_unique_owner_hash os = Some h /\ length h = 20%nat.
Proof.
  intros os Hc.
  unfold count_cvg, cvg_unique_owner_hash in *.
  destruct (filter is_cvg_covenant_output os) as [|o rest] eqn:Ef;
    [discriminate Hc|].
  destruct rest as [|o2 rest2]; [|discriminate Hc].
  assert (Hin : In o (filter is_cvg_covenant_output os)).
  { rewrite Ef. left. reflexivity. }
  apply filter_In in Hin. destruct Hin as [_ Ho].
  unfold is_cvg_covenant_output in Ho.
  apply is_cvg_claim_script_iff_parse in Ho.
  destruct Ho as [c Hp]. rewrite Hp.
  exists (cvg_owner_hash160 c). split; [reflexivity|].
  apply parse_cvg_claim_script_hash_len in Hp. exact Hp.
Qed.

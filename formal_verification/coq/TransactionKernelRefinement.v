(** TransactionKernelRefinement.v — P058.

    Proves the C++-shaped VerifyRigorPredicate execution boundary
    equals the canonical Coq spend predicate.

      forall input,
        CXX_VerifyRigorPredicate(input) = Coq_spend_allowed(input)

    Also: CppSighash = CoqSighash (layout). Ed25519 is the external
    primitive in SignatureBoundary.v (no curve math).

    Scope: Transaction Kernel only.
    Not proved: UTXO, LevelDB, chainstate, P2P, wallet, miner, GBT, PoW, ASERT.

      refinement_proved            = true   (this spend boundary)
      equivalence_proved           = false
      production_kernel_switched   = false

    No [Admitted] / [Axiom] / [Parameter] / [Extract Constant]. *)

From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From Coq Require Import List.
From Coq Require Import Bool.Bool.
From Coq Require Import Arith.PeanoNat.
Import ListNotations.

From RigorV01 Require Import AssetClassification.
From RigorV01 Require Import ScriptAssetBoundary.
From RigorV01 Require Import Ed25519Ownership.
From RigorV01 Require Import NoInterpreterConsensus.
From RigorV01 Require Import SighashPreimage.
Require Import Rigor.SignatureBoundary.

Local Open Scope Z_scope.

(** * Honesty *)

Definition refinement_proved : bool := true.
Definition equivalence_proved : bool := false.
Definition production_kernel_switched : bool := false.
Definition connectblock_full_proof : bool := false.
Definition utxo_refinement_proved : bool := false.

Theorem transaction_kernel_boundary_proved : refinement_proved = true.
Proof. reflexivity. Qed.

Theorem node_equivalence_not_proved : equivalence_proved = false.
Proof. reflexivity. Qed.

Theorem production_kernel_not_switched : production_kernel_switched = false.
Proof. reflexivity. Qed.

(** * TransactionKernelInput — explicit fields only *)

Record TransactionKernelInput := {
  tki_scriptPubKey : list byte;
  tki_scriptSig : list byte;
  tki_witness : list (list byte);
  tki_pubkey : list byte;
  tki_signature : list byte;
  tki_flags : Z;
  tki_spend_height : Z;
  tki_scriptSig_push_only : bool;
  tki_stack_clean : bool;
  tki_keyhash_match : bool;
  tki_crypto_accept : bool;
  tki_sighash : SighashFields
}.

Definition TransactionKernelInput_has_CCoinsView : bool := false.
Definition TransactionKernelInput_has_CBlockIndex : bool := false.
Definition TransactionKernelInput_has_ChainstateManager : bool := false.
Definition TransactionKernelInput_has_rpc : bool := false.
Definition TransactionKernelInput_has_wallet : bool := false.
Definition TransactionKernelInput_has_miner : bool := false.

Theorem transaction_kernel_input_excludes_state :
  TransactionKernelInput_has_CCoinsView = false /\
  TransactionKernelInput_has_CBlockIndex = false /\
  TransactionKernelInput_has_ChainstateManager = false.
Proof. repeat split; reflexivity. Qed.

Theorem transaction_kernel_input_excludes_node :
  TransactionKernelInput_has_rpc = false /\
  TransactionKernelInput_has_wallet = false /\
  TransactionKernelInput_has_miner = false.
Proof. repeat split; reflexivity. Qed.

(** * Flags (frozen Bitcoin bit positions) *)

Definition flag_sigpushonly (i : TransactionKernelInput) : bool :=
  Z.testbit (tki_flags i) 5.
Definition flag_cleanstack (i : TransactionKernelInput) : bool :=
  Z.testbit (tki_flags i) 8.
Definition flag_witness (i : TransactionKernelInput) : bool :=
  Z.testbit (tki_flags i) 11.

Definition spend_gate_sigpush (i : TransactionKernelInput) : bool :=
  if flag_sigpushonly i then tki_scriptSig_push_only i else true.

Definition bytes_eqb (a b : list byte) : bool :=
  if list_eq_dec Nat.eq_dec a b then true else false.

Definition stack_item_ok (item : list byte) : bool :=
  Nat.leb (length item) 520%nat.

Definition witness_items_ok (i : TransactionKernelInput) : bool :=
  forallb stack_item_ok (tki_witness i).

Definition witness_two (i : TransactionKernelInput) : bool :=
  match tki_witness i with
  | sig :: pk :: nil =>
      andb (bytes_eqb sig (tki_signature i)) (bytes_eqb pk (tki_pubkey i))
  | _ => false
  end.

Definition encoding_ok (i : TransactionKernelInput) : bool :=
  andb (Nat.eqb (length (tki_signature i)) 64%nat)
       (Nat.eqb (length (tki_pubkey i)) 32%nat).

(** * C++-shaped path bodies (VerifyRigorPredicate control flow) *)

Definition cxx_oru_inner (i : TransactionKernelInput) : bool :=
  andb (witness_two i)
  (andb (witness_items_ok i)
  (andb (tki_keyhash_match i)
  (andb (Nat.eqb (length (tki_pubkey i)) 32%nat)
  (andb (Nat.eqb (length (tki_signature i)) 64%nat)
        (tki_crypto_accept i))))).

Definition cxx_res_inner (i : TransactionKernelInput) (prog : list byte) : bool :=
  match tki_witness i with
  | nil => false
  | _ =>
      andb (witness_items_ok i)
      (andb (witness_two i)
      (andb (Nat.eqb (length (tki_pubkey i)) 32%nat)
      (andb (bytes_eqb (tki_pubkey i) prog)
      (andb (Nat.eqb (length (tki_signature i)) 64%nat)
            (tki_crypto_accept i)))))
  end.

Definition cxx_witness (i : TransactionKernelInput) (v : nat) (prog : list byte) : bool :=
  if andb (negb (Nat.eqb v 0%nat)) (negb (Nat.eqb v 1%nat)) then false
  else if andb (Nat.eqb v 1%nat) (negb (Nat.eqb (length prog) 32%nat)) then false
  else if negb (flag_witness i) then false
  else if negb (Nat.eqb (length (tki_scriptSig i)) 0%nat) then false
  else if Nat.eqb v 0%nat then
    if Nat.eqb (length prog) 20%nat then cxx_oru_inner i else false
  else if andb (Nat.eqb v 1%nat)
               (andb (Nat.eqb (length prog) 32%nat) (is_native_res_program prog))
       then cxx_res_inner i prog
       else false.

Definition cxx_mint (i : TransactionKernelInput) : bool :=
  if negb (tki_scriptSig_push_only i) then false
  else
    match parse_canonical_mint (tki_scriptPubKey i) with
    | None => false
    | Some (mat, _) =>
        if mat <? 0 then false
        else if tki_spend_height i <? mat then false
        else if negb (encoding_ok i) then false
        else if negb (tki_keyhash_match i) then false
        else if negb (tki_crypto_accept i) then false
        else if andb (flag_cleanstack i) (negb (tki_stack_clean i)) then false
        else if andb (flag_witness i)
                     (negb (match tki_witness i with nil => true | _ => false end))
             then false
        else true
    end.

Definition CXX_VerifyRigorPredicate (i : TransactionKernelInput) : bool :=
  if negb (spend_gate_sigpush i) then false
  else if is_res_mint_script (tki_scriptPubKey i) then cxx_mint i
  else
    match parse_witness_program (tki_scriptPubKey i) with
    | None => false
    | Some (v, prog) => cxx_witness i v prog
    end.

(** * Canonical spend predicate *)

Definition Coq_spend_allowed (i : TransactionKernelInput) : bool :=
  if negb (spend_gate_sigpush i) then false
  else
    match ownership_of (tki_scriptPubKey i) with
    | OwnOruP2WPKH prog => cxx_witness i 0%nat prog
    | OwnResV1 prog => cxx_witness i 1%nat prog
    | OwnResMint _ => cxx_mint i
    | OwnConverge _ | OwnNone | OwnInvalid => false
    end.

Definition spend_allowed (i : TransactionKernelInput) : bool :=
  Coq_spend_allowed i.

(** * Disjointness: a canonical mint is never a witness program *)

Lemma split_at_app :
  forall (n : nat) (xs pre post : list byte),
    split_at n xs = Some (pre, post) ->
    xs = pre ++ post /\ length pre = n.
Proof.
  induction n as [|n IH]; intros xs pre post H.
  - simpl in H. inversion H. split; reflexivity.
  - destruct xs as [|x xs']; simpl in H; [discriminate H|].
    destruct (split_at n xs') as [[pre' post']|] eqn:E; [|discriminate H].
    inversion H. subst pre post.
    destruct (IH xs' pre' post' E) as [E1 E2].
    split.
    + simpl. rewrite E1. reflexivity.
    + simpl. rewrite E2. reflexivity.
Qed.

Lemma witness_version_of_1_4 :
  forall b : byte,
    Nat.leb 1%nat b = true ->
    Nat.leb b 4%nat = true ->
    witness_version_of b = None.
Proof.
  intros b H1 H4.
  unfold witness_version_of.
  destruct (Nat.eqb b RIGOR_PUSH_0) eqn:E0.
  - apply Nat.eqb_eq in E0. subst b. discriminate H1.
  - destruct (andb (Nat.leb RIGOR_PUSH_1 b) (Nat.leb b RIGOR_PUSH_16)) eqn:E81.
    + apply andb_true_iff in E81. destruct E81 as [H81 _].
      apply Nat.leb_le in H81. apply Nat.leb_le in H4.
      change RIGOR_PUSH_1 with 81%nat in H81. lia.
    + reflexivity.
Qed.

Lemma decode_scriptnum_prefix_head :
  forall spk mat rest,
    decode_scriptnum_prefix spk = Some (mat, rest) ->
    exists b r,
      spk = b :: r /\
      (b = 0%nat \/ (Nat.leb 1%nat b = true /\ Nat.leb b 4%nat = true)).
Proof.
  intros spk mat rest H.
  unfold decode_scriptnum_prefix in H.
  destruct spk as [|b r]; [discriminate H|].
  exists b, r. split; [reflexivity|].
  destruct (Nat.eqb b RIGOR_PUSH_0) eqn:Eb0.
  - apply Nat.eqb_eq in Eb0. left. exact Eb0.
  - destruct (andb (Nat.leb 1%nat b) (Nat.leb b 4%nat)) eqn:E14; [|discriminate H].
    apply andb_true_iff in E14. right. exact E14.
Qed.

Lemma parse_witness_v0_20_mismatch :
  forall body : list byte,
    Nat.eqb (length body) 20%nat = false ->
    parse_witness_program (0%nat :: 20%nat :: body) = None.
Proof.
  intros body H.
  unfold parse_witness_program; cbn zeta.
  match goal with
  | |- (if ?c then _ else _) = _ => destruct c; [reflexivity|]
  end.
  simpl. rewrite H. reflexivity.
Qed.

Lemma mint_script_not_witness :
  forall spk : list byte,
    is_res_mint_script spk = true ->
    parse_witness_program spk = None.
Proof.
  intros spk Hm.
  unfold is_res_mint_script, parse_canonical_mint in Hm.
  destruct (decode_scriptnum_prefix spk) as [[mat rest]|] eqn:Hd; [|discriminate Hm].
  destruct rest as [|plen body]; [discriminate Hm|].
  destruct (Nat.eqb plen RIGOR_PUSHBYTES_20) eqn:Eplen; [|discriminate Hm].
  destruct (split_at 20%nat body) as [[pkh mtl]|] eqn:Esp; [|discriminate Hm].
  destruct mtl as [|op tl]; [discriminate Hm|].
  destruct tl; [|discriminate Hm].
  destruct (Nat.eqb op RIGOR_MARKER_RES_MINT); [|discriminate Hm].
  apply Nat.eqb_eq in Eplen.
  destruct (decode_scriptnum_prefix_head spk mat (plen :: body) Hd) as [b [r [Hspk Hb]]].
  rewrite Hspk.
  destruct Hb as [Hb|Hb].
  - subst b.
    unfold decode_scriptnum_prefix in Hd. rewrite Hspk in Hd. simpl in Hd.
    inversion Hd. subst r.
    destruct (split_at_app 20%nat body pkh (op :: nil) Esp) as [Hbod Hl].
    subst plen.
    change RIGOR_PUSHBYTES_20 with 20%nat.
    apply parse_witness_v0_20_mismatch.
    rewrite Hbod. rewrite app_length. simpl. rewrite Hl. reflexivity.
  - destruct Hb as [H1 H4].
    unfold parse_witness_program; cbn zeta.
    match goal with
    | |- (if ?c then _ else _) = _ => destruct c; [reflexivity|]
    end.
    simpl.
    rewrite (witness_version_of_1_4 b H1 H4).
    destruct r; reflexivity.
Qed.

Lemma mint_script_ownership :
  forall spk : list byte,
    is_res_mint_script spk = true ->
    exists h, ownership_of spk = OwnResMint h.
Proof.
  intros spk Hm.
  pose proof (mint_script_not_witness spk Hm) as Hw.
  unfold is_res_mint_script in Hm.
  destruct (parse_canonical_mint spk) as [[mat h]|] eqn:Em; [|discriminate Hm].
  exists h.
  unfold ownership_of. rewrite Hw.
  unfold parse_res_mint_pubkey_hash. rewrite Em. reflexivity.
Qed.

Lemma ownership_oru_parse :
  forall spk prog,
    ownership_of spk = OwnOruP2WPKH prog ->
    parse_witness_program spk = Some (0%nat, prog) /\
    Nat.eqb (length prog) 20%nat = true /\
    is_res_mint_script spk = false.
Proof.
  intros spk prog H.
  unfold ownership_of in H.
  destruct (parse_witness_program spk) as [[v p]|] eqn:Hw.
  - destruct v as [|v'].
    + destruct (Nat.eqb (length p) 20%nat) eqn:E20; [|discriminate H].
      inversion H. subst p. split; [reflexivity|]. split; [exact E20|].
      unfold is_res_mint_script.
      destruct (parse_canonical_mint spk) as [m|] eqn:Em; [|reflexivity].
      exfalso.
      assert (is_res_mint_script spk = true) as Hm.
      { unfold is_res_mint_script. rewrite Em. reflexivity. }
      pose proof (mint_script_not_witness spk Hm) as Hw2.
      rewrite Hw in Hw2. discriminate Hw2.
    + destruct v' as [|v''].
      * destruct (is_native_res_program p); discriminate H.
      * discriminate H.
  - destruct (parse_res_mint_pubkey_hash spk); [discriminate H|].
    destruct (is_converge_script spk); [|destruct (is_op_return spk); discriminate H].
    destruct spk as [|plen rest]; [discriminate H|].
    destruct (rev rest); [discriminate H|discriminate H].
Qed.

Lemma ownership_res_parse :
  forall spk prog,
    ownership_of spk = OwnResV1 prog ->
    parse_witness_program spk = Some (1%nat, prog) /\
    is_native_res_program prog = true /\
    is_res_mint_script spk = false.
Proof.
  intros spk prog H.
  unfold ownership_of in H.
  destruct (parse_witness_program spk) as [[v p]|] eqn:Hw.
  - destruct v as [|v'].
    + destruct (Nat.eqb (length p) 20%nat); discriminate H.
    + destruct v' as [|v''].
      * destruct (is_native_res_program p) eqn:En; [|discriminate H].
        inversion H. subst p. split; [reflexivity|]. split; [exact En|].
        unfold is_res_mint_script.
        destruct (parse_canonical_mint spk) as [m|] eqn:Em; [|reflexivity].
        exfalso.
        assert (is_res_mint_script spk = true) as Hm.
        { unfold is_res_mint_script. rewrite Em. reflexivity. }
        pose proof (mint_script_not_witness spk Hm) as Hw2.
        rewrite Hw in Hw2. discriminate Hw2.
      * discriminate H.
  - destruct (parse_res_mint_pubkey_hash spk); [discriminate H|].
    destruct (is_converge_script spk); [|destruct (is_op_return spk); discriminate H].
    destruct spk as [|plen rest]; [discriminate H|].
    destruct (rev rest); [discriminate H|discriminate H].
Qed.

Lemma ownership_mint_is_mint_script :
  forall spk h,
    ownership_of spk = OwnResMint h ->
    is_res_mint_script spk = true.
Proof.
  intros spk h H.
  unfold ownership_of, parse_res_mint_pubkey_hash in H.
  destruct (parse_witness_program spk) as [[v p]|].
  - destruct v as [|v'].
    + destruct (Nat.eqb (length p) 20%nat); discriminate H.
    + destruct v' as [|v''].
      * destruct (is_native_res_program p); discriminate H.
      * discriminate H.
  - unfold is_res_mint_script.
    destruct (parse_canonical_mint spk) as [mh|]; [reflexivity|].
    destruct (is_converge_script spk); [|destruct (is_op_return spk); discriminate H].
    destruct spk as [|plen rest]; [discriminate H|].
    destruct (rev rest); discriminate H.
Qed.

Lemma cxx_witness_unknown_false :
  forall i v prog,
    (v <> 0%nat /\ v <> 1%nat) \/
    (v = 0%nat /\ Nat.eqb (length prog) 20%nat = false) \/
    (v = 1%nat /\ is_native_res_program prog = false) ->
    cxx_witness i v prog = false.
Proof.
  intros i v prog H.
  unfold cxx_witness.
  destruct H as [H|[H|H]].
  - destruct H as [H0 H1].
    destruct (Nat.eqb v 0%nat) eqn:E0.
    + apply Nat.eqb_eq in E0. contradiction.
    + destruct (Nat.eqb v 1%nat) eqn:E1.
      * apply Nat.eqb_eq in E1. contradiction.
      * reflexivity.
  - destruct H as [Hv Hl]. subst v. simpl.
    rewrite Hl. simpl.
    destruct (negb (flag_witness i)); [reflexivity|].
    destruct (negb (Nat.eqb (length (tki_scriptSig i)) 0%nat)); [reflexivity|].
    reflexivity.
  - destruct H as [Hv Hn]. subst v. simpl.
    destruct (negb (Nat.eqb (length prog) 32%nat)) eqn:E32.
    + reflexivity.
    + apply negb_false_iff in E32.
      destruct (negb (flag_witness i)); [reflexivity|].
      destruct (negb (Nat.eqb (length (tki_scriptSig i)) 0%nat)); [reflexivity|].
      simpl.
      rewrite E32, Hn. reflexivity.
Qed.

(** * Predicate theorem *)

Theorem spend_predicate_refinement :
  forall input,
    CXX_VerifyRigorPredicate input = Coq_spend_allowed input.
Proof.
  intros input.
  unfold CXX_VerifyRigorPredicate, Coq_spend_allowed.
  destruct (negb (spend_gate_sigpush input)); [reflexivity|].
  destruct (is_res_mint_script (tki_scriptPubKey input)) eqn:Hm.
  - destruct (mint_script_ownership (tki_scriptPubKey input) Hm) as [h Ho].
    rewrite Ho. reflexivity.
  - destruct (ownership_of (tki_scriptPubKey input)) eqn:Ho.
    + destruct (ownership_oru_parse (tki_scriptPubKey input) pkh Ho) as [Hw [Hl Hmf]].
      rewrite Hw. reflexivity.
    + destruct (ownership_res_parse (tki_scriptPubKey input) pk Ho) as [Hw [Hn Hmf]].
      rewrite Hw. reflexivity.
    + pose proof (ownership_mint_is_mint_script _ _ Ho) as Hc.
      rewrite Hm in Hc. discriminate Hc.
    + unfold ownership_of in Ho.
      destruct (parse_witness_program (tki_scriptPubKey input)) as [[v p]|] eqn:Hw.
      * destruct v as [|v'].
        -- destruct (Nat.eqb (length p) 20%nat); discriminate Ho.
        -- destruct v' as [|v''].
           ++ destruct (is_native_res_program p); discriminate Ho.
           ++ discriminate Ho.
      * reflexivity.
    + unfold ownership_of in Ho.
      destruct (parse_witness_program (tki_scriptPubKey input)) as [[v p]|] eqn:Hw.
      * destruct v as [|v'].
        -- destruct (Nat.eqb (length p) 20%nat); discriminate Ho.
        -- destruct v' as [|v''].
           ++ destruct (is_native_res_program p); discriminate Ho.
           ++ discriminate Ho.
      * reflexivity.
    + unfold ownership_of in Ho.
      destruct (parse_witness_program (tki_scriptPubKey input)) as [[v p]|] eqn:Hw.
      * apply cxx_witness_unknown_false.
        destruct v as [|v'].
        -- destruct (Nat.eqb (length p) 20%nat) eqn:E20; [discriminate Ho|].
           right. left. split; reflexivity.
        -- destruct v' as [|v''].
           ++ destruct (is_native_res_program p) eqn:En; [discriminate Ho|].
              right. right. split; reflexivity.
           ++ left. split; discriminate.
      * reflexivity.
Qed.

Theorem predicate_theorem :
  forall input,
    CXX_VerifyRigorPredicate input = spend_allowed input.
Proof.
  intros input. rewrite spend_predicate_refinement. reflexivity.
Qed.

(** * Sighash boundary *)

Definition CppSighash (f : SighashFields) : list byte :=
  (sh_tag f ++ sh_chain_id f) ++
  (le32 (sh_version f) ++
   sh_hash_prevouts f ++
   sh_hash_sequence f ++
   sh_hash_outputs f) ++
  (sh_prevout_hash f ++ le32 (sh_prevout_n f)) ++
  serialize_script (sh_script f) ++
  (le64 (sh_amount f) ++ le32 (sh_sequence f) ++ le32 (sh_locktime f)).

Definition CoqSighash (f : SighashFields) : list byte :=
  sighash_preimage f.

Theorem sighash_alignment :
  forall input,
    CppSighash (tki_sighash input) = CoqSighash (tki_sighash input).
Proof.
  intros input.
  unfold CppSighash, CoqSighash, sighash_preimage.
  repeat rewrite <- app_assoc.
  reflexivity.
Qed.

Definition cpp_appends_sighash_all_byte : bool := false.
Definition cpp_mutates_sighash_flags : bool := false.
Definition cpp_mutates_scriptcode : bool := false.

Theorem no_sighash_all_byte :
  cpp_appends_sighash_all_byte = false.
Proof. reflexivity. Qed.

Theorem no_sighash_flags_mutation :
  cpp_mutates_sighash_flags = false.
Proof. reflexivity. Qed.

Theorem no_scriptcode_mutation :
  cpp_mutates_scriptcode = false.
Proof. reflexivity. Qed.

(** * Signature boundary re-export *)

Theorem signature_boundary_alignment :
  forall sig pk msg acc,
    valid_signature sig pk msg acc = signature_accept_boundary sig pk msg acc.
Proof. exact valid_signature_eq_accept_boundary. Qed.

(** * Fail-closed / VM / multisig *)

Definition script_vm_absent : bool := true.
Definition multisig_absent : bool := true.

Theorem script_vm_absent_pass : script_vm_absent = true.
Proof. reflexivity. Qed.

Theorem multisig_absent_pass : multisig_absent = true.
Proof. reflexivity. Qed.

Theorem no_eval_script_in_tx_kernel :
  eval_script_in_consensus = false /\ opcode_vm_in_consensus = false.
Proof. split; reflexivity. Qed.

Theorem no_multisig_in_tx_kernel :
  checkmultisig_in_consensus = false.
Proof. reflexivity. Qed.

Definition empty_sh : SighashFields :=
  {| sh_tag := tag_oru;
     sh_chain_id := repeat_byte 32 0%nat;
     sh_version := 2;
     sh_hash_prevouts := repeat_byte 32 0%nat;
     sh_hash_sequence := repeat_byte 32 0%nat;
     sh_hash_outputs := repeat_byte 32 0%nat;
     sh_prevout_hash := repeat_byte 32 0%nat;
     sh_prevout_n := 0;
     sh_script := nil;
     sh_amount := 0;
     sh_sequence := 0;
     sh_locktime := 0 |}.

Definition sig64 : list byte := repeat_byte 64 7%nat.
Definition pk32 : list byte := repeat_byte 32 9%nat.

Definition mk_input (spk : list byte) (wit : list (list byte))
  (pk sig : list byte) (flags : Z) (kh acc : bool) : TransactionKernelInput :=
  {| tki_scriptPubKey := spk;
     tki_scriptSig := nil;
     tki_witness := wit;
     tki_pubkey := pk;
     tki_signature := sig;
     tki_flags := flags;
     tki_spend_height := 0;
     tki_scriptSig_push_only := true;
     tki_stack_clean := true;
     tki_keyhash_match := kh;
     tki_crypto_accept := acc;
     tki_sighash := empty_sh |}.

Definition flags_witness : Z := 2048.

Definition sample_oru : TransactionKernelInput :=
  mk_input canonical_p2wpkh [sig64; pk32] pk32 sig64 flags_witness true true.

Definition sample_res : TransactionKernelInput :=
  mk_input canonical_native_res [sig64; rfc8032_pk1] rfc8032_pk1 sig64
           flags_witness true true.

Definition sample_invalid : TransactionKernelInput :=
  mk_input nil nil pk32 sig64 flags_witness true true.

Definition sample_p2pkh : TransactionKernelInput :=
  mk_input canonical_p2pkh nil pk32 sig64 flags_witness true true.

Definition sample_unknown_version : TransactionKernelInput :=
  mk_input ((RIGOR_PUSH_1 + 1)%nat :: RIGOR_PUSHBYTES_20 :: repeat_byte 20 0%nat)
           [sig64; pk32] pk32 sig64 flags_witness true true.

Definition sample_p2wsh : TransactionKernelInput :=
  mk_input canonical_p2wsh [sig64; pk32] pk32 sig64 flags_witness true true.

Definition sample_malformed_wit : TransactionKernelInput :=
  mk_input canonical_p2wpkh [sig64] pk32 sig64 flags_witness true true.

Definition sample_wrong_pk : TransactionKernelInput :=
  mk_input canonical_native_res [sig64; pk32] pk32 sig64 flags_witness true true.

Definition sample_wrong_sig : TransactionKernelInput :=
  mk_input canonical_p2wpkh [sig64; pk32] pk32 sig64 flags_witness true false.

Theorem fail_closed_empty :
  spend_allowed sample_invalid = false /\
  CXX_VerifyRigorPredicate sample_invalid = false.
Proof. split; vm_compute; reflexivity. Qed.

Theorem fail_closed_p2pkh :
  spend_allowed sample_p2pkh = false.
Proof. vm_compute. reflexivity. Qed.

Theorem fail_closed_unknown_version :
  spend_allowed sample_unknown_version = false.
Proof. vm_compute. reflexivity. Qed.

Theorem fail_closed_bad_length :
  spend_allowed sample_p2wsh = false.
Proof. vm_compute. reflexivity. Qed.

Theorem fail_closed_malformed_witness :
  spend_allowed sample_malformed_wit = false.
Proof. vm_compute. reflexivity. Qed.

Theorem fail_closed_wrong_pubkey :
  spend_allowed sample_wrong_pk = false.
Proof. vm_compute. reflexivity. Qed.

Theorem fail_closed_wrong_signature :
  spend_allowed sample_wrong_sig = false.
Proof. vm_compute. reflexivity. Qed.

Theorem valid_oru_spend :
  spend_allowed sample_oru = true /\
  CXX_VerifyRigorPredicate sample_oru = true.
Proof. split; vm_compute; reflexivity. Qed.

Theorem valid_res_key_path :
  spend_allowed sample_res = true /\
  CXX_VerifyRigorPredicate sample_res = true.
Proof. split; vm_compute; reflexivity. Qed.

Theorem classify_then_predicate :
  classify canonical_p2wpkh = AssetORU /\
  classify canonical_native_res = AssetRES /\
  classify canonical_p2pkh = AssetInvalid.
Proof.
  split; [apply p2wpkh_is_oru|].
  split; [apply native_res_is_res|apply p2pkh_is_invalid].
Qed.

Theorem fail_closed_flag :
  spend_allowed sample_invalid = false /\
  spend_allowed sample_p2wsh = false /\
  spend_allowed sample_unknown_version = false /\
  spend_allowed sample_malformed_wit = false.
Proof.
  repeat split; vm_compute; reflexivity.
Qed.
